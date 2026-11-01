// audren: el renderizador de audio, el que usan los juegos (nn::audio) y libnx (audrv).
// El programa manda en cada actualización el estado de sus voces (formato, frecuencia, tono,
// volumen, buffers de muestras), las memorias, los canales con sus factores de mezcla, las
// mezclas y la salida. Un hilo mezcla 240 muestras cada 5 ms (200 Hz, como la consola) y las
// manda a nxaudio.c; en cada actualización se devuelve el estado de las voces y las memorias.
// Formato de libnx (REV4): estructuras medidas con el compilador de devkitPro.
// Formatos de voz: PCM de 8/16/32 bits, float y DSP-ADPCM de Nintendo.
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxaudio.h"
#include "nxkern.h"
#include "nxsvc.h"

#define OK 0
#define MAX_VOICES 128
#define MAX_CHANNELS 256
#define MAX_MIXES 32
#define MAX_BUFS 24       // buffers de mezcla
#define FRAME 240         // muestras por cuadro de audio a 48 kHz
#define FINAL_MIX 0

enum { PCM_INT8 = 1, PCM_INT16 = 2, PCM_INT32 = 4, PCM_FLOAT = 5, PCM_ADPCM = 6 };
enum { PLAY_STARTED, PLAY_STOPPED, PLAY_PAUSED };

typedef struct {
    uint64_t addr, size, ctx_addr;
    int32_t start, end;
    int looping, pending;
    int16_t *decoded;           // ADPCM decodificado entero al empezar
} WaveBuf;

typedef struct {
    int used, state, format, channels;
    uint32_t rate, mix_id, chan_ids[6];
    float pitch, volume;
    uint64_t extra;             // coeficientes ADPCM
    WaveBuf wb[4];
    uint32_t consumed;          // buffers terminados (acumulado, lo lee el programa)
    uint64_t played;
    double pos;                 // posición dentro del buffer actual, en muestras
} Voice;

typedef struct {
    int used;
    float mix[MAX_BUFS];
} ChannelRes;

typedef struct {
    int used;
    uint32_t id, buffers, dest;
    float volume, mat[MAX_BUFS][MAX_BUFS];
} Mix;

typedef struct {
    pthread_mutex_t lock;
    int started, nvoices, nmempools, nsinks, nchannels;
    uint32_t sample_rate, mix_buffers;
    Voice voices[MAX_VOICES];
    ChannelRes chans[MAX_CHANNELS];
    Mix mixes[MAX_MIXES];
    int nmixes, sink_inputs[6], sink_count;
    KEvent *event;
    uint32_t event_h;
} Renderer;

static uint32_t u32at(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t u64at(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static float f32at(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }

// ---------- DSP-ADPCM ----------
// Cuadros de 8 bytes: cabecera (predictor << 4 | escala) y 14 muestras de 4 bits.
static int16_t *adpcm_decode(const uint8_t *data, uint64_t size, const int16_t coef[16], int32_t count,
                             const uint8_t *ctx) {
    int16_t *out = calloc((size_t)count + 1, 2);
    int32_t h1 = 0, h2 = 0;
    if (ctx) { // contexto: índice (u16), historia 0 y 1
        h1 = (int16_t)(ctx[2] | ctx[3] << 8);
        h2 = (int16_t)(ctx[4] | ctx[5] << 8);
    }
    int32_t n = 0;
    for (uint64_t f = 0; f + 1 < size && n < count; f += 8) {
        int ps = data[f], pred = ps >> 4 & 7, scale = 1 << (ps & 15);
        int c1 = coef[pred * 2], c2 = coef[pred * 2 + 1];
        for (int k = 0; k < 14 && n < count; k++) {
            uint64_t b = f + 1 + (uint64_t)k / 2;
            if (b >= size) break;
            int nib = k & 1 ? data[b] & 15 : data[b] >> 4;
            if (nib >= 8) nib -= 16;
            int32_t s = ((nib * scale) << 11) + 1024 + c1 * h1 + c2 * h2;
            s >>= 11;
            s = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
            out[n++] = (int16_t)s;
            h2 = h1;
            h1 = s;
        }
    }
    return out;
}

// Muestra i (desde el inicio del buffer) del canal c, como float en [-1, 1].
static float sample_at(const Voice *v, const WaveBuf *w, int32_t i, int c) {
    int32_t idx = w->start + i;
    if (idx >= w->end) idx = w->end - 1;
    const uint8_t *p = (const uint8_t *)(uintptr_t)w->addr;
    int ch = v->channels;
    switch (v->format) {
    case PCM_INT16: { int16_t s; memcpy(&s, p + ((uint64_t)idx * ch + c) * 2, 2); return s / 32768.0f; }
    case PCM_INT8: return (int8_t)p[(uint64_t)idx * ch + c] / 128.0f;
    case PCM_INT32: { int32_t s; memcpy(&s, p + ((uint64_t)idx * ch + c) * 4, 4); return s / 2147483648.0f; }
    case PCM_FLOAT: { float s; memcpy(&s, p + ((uint64_t)idx * ch + c) * 4, 4); return s; }
    case PCM_ADPCM: return w->decoded ? w->decoded[i] / 32768.0f : 0;
    }
    return 0;
}

static void prepare(Voice *v, WaveBuf *w) {
    if (v->format == PCM_ADPCM && !w->decoded && v->extra) {
        int16_t coef[16];
        memcpy(coef, (const void *)(uintptr_t)v->extra, sizeof coef);
        // el ADPCM direcciona por muestra desde el inicio del buffer: se decodifica desde 0
        int16_t *all = adpcm_decode((const uint8_t *)(uintptr_t)w->addr, w->size, coef, w->end,
                                    w->ctx_addr ? (const uint8_t *)(uintptr_t)w->ctx_addr : NULL);
        w->decoded = all;
        if (w->start) { // dejar decoded[i] = muestra start + i
            memmove(all, all + w->start, (size_t)(w->end - w->start) * 2);
        }
    }
}

// ---------- mezcla de un cuadro ----------
static void mix_frame(Renderer *R, float bufs[MAX_BUFS][FRAME]) {
    memset(bufs, 0, sizeof(float) * MAX_BUFS * FRAME);
    float sub[MAX_MIXES][MAX_BUFS][FRAME];
    memset(sub, 0, sizeof sub);
    for (int k = 0; k < R->nvoices; k++) {
        Voice *v = &R->voices[k];
        if (!v->used || v->state != PLAY_STARTED || v->channels < 1 || v->channels > 6) continue;
        double step = (double)v->rate * v->pitch / R->sample_rate;
        float (*dst)[FRAME] = bufs;
        if (v->mix_id != FINAL_MIX)
            for (int m = 0; m < R->nmixes; m++)
                if (R->mixes[m].used && R->mixes[m].id == v->mix_id && m < MAX_MIXES) dst = sub[m];
        for (int s = 0; s < FRAME; s++) {
            WaveBuf *w = &v->wb[v->consumed & 3];
            while (w->pending && v->pos >= w->end - w->start) { // terminó este buffer
                v->pos -= w->end - w->start;
                if (w->looping) continue;
                w->pending = 0;
                free(w->decoded);
                w->decoded = NULL;
                v->consumed++;
                w = &v->wb[v->consumed & 3];
            }
            if (!w->pending) break; // sin muestras: la voz se queda esperando
            prepare(v, w);
            int32_t i0 = (int32_t)v->pos;
            float frac = (float)(v->pos - i0);
            int32_t len = w->end - w->start;
            for (int c = 0; c < v->channels; c++) {
                float a = sample_at(v, w, i0, c), b = i0 + 1 < len ? sample_at(v, w, i0 + 1, c) : a;
                float x = (a + (b - a) * frac) * v->volume;
                ChannelRes *cr = v->chan_ids[c] < MAX_CHANNELS ? &R->chans[v->chan_ids[c]] : NULL;
                if (!cr) continue;
                for (uint32_t d = 0; d < R->mix_buffers && d < MAX_BUFS; d++)
                    if (cr->mix[d] != 0) dst[d][s] += x * cr->mix[d];
            }
            double before = v->pos;
            v->pos += step;
            v->played += (uint64_t)v->pos - (uint64_t)before;
        }
    }
    // submezclas hacia la mezcla final, con su matriz
    for (int m = 0; m < R->nmixes && m < MAX_MIXES; m++) {
        Mix *mx = &R->mixes[m];
        if (!mx->used || mx->id == FINAL_MIX) continue;
        for (uint32_t i = 0; i < mx->buffers && i < MAX_BUFS; i++)
            for (uint32_t o = 0; o < R->mix_buffers && o < MAX_BUFS; o++)
                if (mx->mat[i][o] != 0)
                    for (int s = 0; s < FRAME; s++) bufs[o][s] += sub[m][i][s] * mx->mat[i][o] * mx->volume;
    }
    for (int m = 0; m < R->nmixes; m++)
        if (R->mixes[m].used && R->mixes[m].id == FINAL_MIX && R->mixes[m].volume != 1.0f)
            for (uint32_t o = 0; o < R->mix_buffers && o < MAX_BUFS; o++)
                for (int s = 0; s < FRAME; s++) bufs[o][s] *= R->mixes[m].volume;
}

static int16_t to16(float x) {
    float v = x * 32768.0f;
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : lrintf(v));
}

static void *render_thread(void *arg) {
    Renderer *R = arg;
    static float bufs[MAX_BUFS][FRAME];
    int16_t out[FRAME * 2];
    uint64_t clock = nx_now_ns();
    for (;;) {
        pthread_mutex_lock(&R->lock);
        int started = R->started;
        if (started) {
            mix_frame(R, bufs);
            int nin = R->sink_count;
            for (int s = 0; s < FRAME; s++) {
                float l = 0, r = 0;
                if (nin >= 6) { // 5.1 → estéreo
                    float fl = bufs[R->sink_inputs[0]][s], fr = bufs[R->sink_inputs[1]][s];
                    float c = bufs[R->sink_inputs[2]][s], bl = bufs[R->sink_inputs[4]][s], br = bufs[R->sink_inputs[5]][s];
                    l = fl + 0.707f * c + 0.707f * bl;
                    r = fr + 0.707f * c + 0.707f * br;
                } else if (nin >= 2) {
                    l = bufs[R->sink_inputs[0]][s];
                    r = bufs[R->sink_inputs[1]][s];
                } else if (nin == 1) {
                    l = r = bufs[R->sink_inputs[0]][s];
                }
                out[2 * s] = to16(l);
                out[2 * s + 1] = to16(r);
            }
        }
        pthread_mutex_unlock(&R->lock);
        if (started) nx_audio_write(out, FRAME);
        nx_event_signal(R->event);
        clock += 5000000; // 5 ms
        uint64_t now = nx_now_ns();
        if (clock > now) {
            struct timespec ts = {0, (long)(clock - now)};
            nanosleep(&ts, NULL);
        } else if (now - clock > 100000000) {
            clock = now; // nos atrasamos mucho (el programa se detuvo): no recuperar de golpe
        }
    }
    return NULL;
}

// ---------- actualización ----------
static uint32_t update(Renderer *R, const uint8_t *in, uint64_t in_size, uint8_t *out, uint64_t out_size) {
    if (in_size < 0x40) return 0x7F4u;
    uint32_t sz_beh = u32at(in + 4), sz_mp = u32at(in + 8), sz_vo = u32at(in + 0xC), sz_ch = u32at(in + 0x10);
    uint32_t sz_fx = u32at(in + 0x14), sz_mix = u32at(in + 0x18), sz_sink = u32at(in + 0x1C);
    const uint8_t *p = in + 0x40 + sz_beh;
    const uint8_t *mempools = p;
    int nmp = (int)(sz_mp / 0x20);
    p += sz_mp;
    const uint8_t *chans = p;
    int nch = (int)(sz_ch / 0x70);
    p += sz_ch;
    const uint8_t *voices = p;
    int nvo = (int)(sz_vo / 0x170);
    p += sz_vo + sz_fx;
    const uint8_t *mixes = p;
    int nmix = (int)(sz_mix / 0x930);
    p += sz_mix;
    const uint8_t *sinks = p;
    int nsink = (int)(sz_sink / 0x140);

    pthread_mutex_lock(&R->lock);
    for (int k = 0; k < nch && k < MAX_CHANNELS; k++) {
        const uint8_t *c = chans + k * 0x70;
        uint32_t id = u32at(c);
        if (id >= MAX_CHANNELS) continue;
        R->chans[id].used = c[0x64];
        memcpy(R->chans[id].mix, c + 4, sizeof R->chans[id].mix);
    }
    if (nvo > MAX_VOICES) nvo = MAX_VOICES;
    R->nvoices = nvo;
    for (int k = 0; k < nvo; k++) {
        const uint8_t *vi = voices + k * 0x170;
        Voice *v = &R->voices[k];
        if (vi[8]) { // nueva: se reinicia el estado del servidor
            for (int b = 0; b < 4; b++) free(v->wb[b].decoded);
            memset(v, 0, sizeof *v);
        }
        v->used = vi[9];
        v->state = vi[0xA];
        v->format = vi[0xB];
        v->rate = u32at(vi + 0xC);
        v->channels = (int)u32at(vi + 0x18);
        v->pitch = f32at(vi + 0x1C);
        v->volume = f32at(vi + 0x20);
        v->extra = u64at(vi + 0x48);
        v->mix_id = u32at(vi + 0x58);
        for (int c = 0; c < 6; c++) v->chan_ids[c] = u32at(vi + 0x140 + 4 * c);
        for (int b = 0; b < 4; b++) {
            const uint8_t *w = vi + 0x60 + b * 0x38;
            if (!w[0x1A]) { // sent_to_server en falso: buffer recién agregado
                WaveBuf *wb = &v->wb[b];
                free(wb->decoded);
                *wb = (WaveBuf){u64at(w), u64at(w + 8), u64at(w + 0x20), (int32_t)u32at(w + 0x10), (int32_t)u32at(w + 0x14),
                                w[0x18], 1, NULL};
                if (wb->end <= wb->start) wb->pending = 0;
            }
        }
        if (v->state == PLAY_STOPPED) v->pos = 0;
    }
    R->nmixes = nmix < MAX_MIXES ? nmix : MAX_MIXES;
    for (int k = 0; k < R->nmixes; k++) {
        const uint8_t *m = mixes + k * 0x930;
        Mix *mx = &R->mixes[k];
        mx->volume = f32at(m);
        mx->buffers = u32at(m + 8);
        mx->used = m[0xC];
        mx->id = u32at(m + 0x10);
        mx->dest = u32at(m + 0x924);
        for (int i = 0; i < MAX_BUFS; i++) memcpy(mx->mat[i], m + 0x24 + i * 24 * 4, MAX_BUFS * 4);
    }
    for (int k = 0; k < nsink; k++) {
        const uint8_t *s = sinks + k * 0x140;
        if (s[0] == 1 && s[1]) { // sink de dispositivo en uso
            R->sink_count = (int)u32at(s + 0x20 + 0x100);
            if (R->sink_count > 6) R->sink_count = 6;
            for (int c = 0; c < R->sink_count; c++) R->sink_inputs[c] = s[0x20 + 0x104 + c] % MAX_BUFS;
        }
    }

    // Salida: cabecera, memorias, voces, sinks, rendimiento y comportamiento.
    uint64_t need = 0x40 + (uint64_t)nmp * 0x10 + (uint64_t)nvo * 0x10 + (uint64_t)nsink * 0x20 + 0x10 + 0xB0;
    if (out_size < need) {
        pthread_mutex_unlock(&R->lock);
        return 0x7F4u;
    }
    memset(out, 0, need);
    uint32_t hdr[16] = {u32at(in), 0xB0, (uint32_t)nmp * 0x10, (uint32_t)nvo * 0x10, 0, 0, 0, (uint32_t)nsink * 0x20, 0x10};
    hdr[15] = (uint32_t)need;
    memcpy(out, hdr, 0x40);
    uint8_t *q = out + 0x40;
    for (int k = 0; k < nmp; k++, q += 0x10) { // estados de las memorias: pedir adjuntar → adjunta
        uint32_t st = u32at(mempools + k * 0x20 + 0x10);
        uint32_t ns = st == 4 ? 5 : st == 2 ? 3 : st;
        memcpy(q, &ns, 4);
    }
    for (int k = 0; k < nvo; k++, q += 0x10) {
        memcpy(q, &R->voices[k].played, 8);
        memcpy(q + 8, &R->voices[k].consumed, 4);
    }
    pthread_mutex_unlock(&R->lock);
    return OK;
}

static uint32_t renderer(ServiceObject *self, NxRequest *r) {
    Renderer *R = self->state;
    switch (r->cmd) {
    case 0: nx_out32(r, R->sample_rate); return OK;   // GetSampleRate
    case 1: nx_out32(r, FRAME); return OK;            // GetSampleCount
    case 2: nx_out32(r, R->mix_buffers); return OK;   // GetMixBufferCount
    case 3: nx_out32(r, R->started ? 0 : 1); return OK; // GetState
    case 4: case 10: { // RequestUpdate: salida, rendimiento, entrada
        NxBuffer *in = nx_in_buffer(r, 0), *out = nx_out_buffer(r, 0);
        if (!in || !out) return 0x7F4u;
        return update(R, (const uint8_t *)(uintptr_t)in->addr, in->size, (uint8_t *)(uintptr_t)out->addr, out->size);
    }
    case 5: case 6: R->started = r->cmd == 5; return OK; // Start / Stop
    case 7: nx_out_copy_handle(r, R->event_h); return OK; // QuerySystemEvent
    case 8: return OK;                                  // SetRenderingTimeLimit
    case 9: nx_out32(r, 100); return OK;                // GetRenderingTimeLimit
    }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_audren(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // OpenAudioRenderer(parámetros, tamaño de trabajo, aruid) + memoria de trabajo y proceso
        Renderer *R = calloc(1, sizeof *R);
        pthread_mutex_init(&R->lock, NULL);
        R->sample_rate = nx_in32(r, 0) ? nx_in32(r, 0) : 48000;
        R->mix_buffers = nx_in32(r, 8) > MAX_BUFS ? MAX_BUFS : nx_in32(r, 8);
        R->sink_inputs[0] = 0, R->sink_inputs[1] = 1, R->sink_count = 2;
        R->event_h = nx_event_new(&R->event);
        nx_audio_open(48000, 2);
        pthread_t t;
        pthread_create(&t, NULL, render_thread, R);
        nx_out_object(r, nx_object("IAudioRenderer", renderer, R));
        return OK;
    }
    case 1: nx_out64(r, 0x100000); return OK; // GetWorkBufferSize
    }
    return NX_R_UNKNOWN_CMD;
}
