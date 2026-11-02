// audren: el renderizador de audio, el que usan los juegos (nn::audio) y libnx (audrv).
// En cada actualización (RequestUpdate) el programa manda el estado de sus memorias, canales, voces,
// efectos, splitters, mezclas y sinks, y recibe el de sus voces, memorias, efectos y sinks, los
// errores y los cuadros hechos. El formato depende de la revisión que pide al abrirlo (REV1 a REV15):
// se sigue el StateUpdater y el BehaviourInfo de Ryujinx (MIT, src/Ryujinx.Audio/Renderer/Server).
// Un hilo genera un cuadro cada 5 ms (200 Hz, como la consola) con nxaudren.c y lo manda a nxaudio.c.
// También está IAudioDevice (nombres, volumen y eventos de los dispositivos de salida).
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxaudio.h"
#include "nxaudren.h"
#include "nxsvc.h"

#define OK 0

static uint32_t u32at(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t u64at(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static float f32at(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static uint64_t al(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

// ---------- parámetros de apertura (AudioRendererParameter del IPC, 0x34 bytes) ----------
typedef struct {
    uint32_t rate, samples, mix_buffers, submixes, voices, sinks, effects, perf, splitters, dests;
    uint8_t drop, device, mode;
    int32_t revision;
} ArParam;

static ArParam param_read(const uint8_t *p) {
    ArParam a = {u32at(p), u32at(p + 4), u32at(p + 8), u32at(p + 0xC), u32at(p + 0x10), u32at(p + 0x14), u32at(p + 0x18),
                 u32at(p + 0x1C), u32at(p + 0x24), u32at(p + 0x28), p[0x20], p[0x22], p[0x23], (int32_t)u32at(p + 0x30)};
    return a;
}

// "REVn" en little endian: el número va en el byte alto. Se aceptan REV1 a REV15.
static int rev_number(int32_t rev) {
    uint32_t r = (uint32_t)rev;
    int n = (int)(r >> 24) - '0';
    return (r & 0xFFFFFF) == 0x564552 && n >= 1 && n <= 15 ? n : 0;
}

// AudioRenderSystem.GetWorkBufferSize de Ryujinx, con los tamaños de sus estructuras: VoiceInfo 0x218,
// VoiceChannelResource 0xD0, VoiceState 0xD8, MixInfo 0x940, MemoryPoolInfo 0x20, SplitterState 0x20,
// SplitterDestination 0xE0 (0x128 desde REV12), BiquadFilterState 0x20.
static uint64_t work_size(const ArParam *p) {
    int rev = rev_number(p->revision);
    uint64_t mixes = (uint64_t)p->submixes + 1, pools = (uint64_t)p->effects + (uint64_t)p->voices * 4, s = 0;
    s = al(s, 16) + 4ull * (uint32_t)(p->samples * (6 + p->mix_buffers));                  // buffers de mezcla
    s = al(s, 16) + 4ull * (uint32_t)(240 * (6 + p->mix_buffers) * (p->sinks + p->submixes)); // remuestreo
    s = al(s, 64) + 4 * al(p->mix_buffers, 64);                                            // depop
    s = al(s, 16) + 0x218ull * p->voices;
    s = al(s, 16) + 4ull * p->voices;
    s = al(s, 16) + 0xD0ull * p->voices;
    s = al(s, 16) + 0xD8ull * p->voices;
    s = al(s, 16) + 0x940 * mixes;
    s = al(s, 16) + 4 * (uint64_t)p->effects * mixes;
    s = al(s, 16) + 4 * mixes;
    if (rev >= 2) s += al(mixes * mixes * 4 + 0xC * mixes + 3 * al(mixes * mixes, 64), 16); // estados de nodos y aristas
    s = al(s, 16) + 0x20 * pools;
    if (rev >= 2) {
        s = al(s, 16) + 0x20ull * p->splitters;
        s = al(s, 16) + (rev >= 12 ? 0x128ull : 0xE0ull) * p->dests;
        if (rev >= 5) s = al(s, 16) + 4ull * p->dests;
    }
    if (rev >= 12 && p->splitters && p->dests) s = al(s, 16) + 0x20ull * p->dests * 4;
    s = al(s, 16) + 0xD8ull * p->voices; // estados de las voces del lado del DSP
    if (p->perf) {
        uint64_t entries = (uint64_t)p->voices + p->effects + p->submixes + p->sinks + 1;
        uint64_t frame = rev >= 5 ? 0x18 * entries + 0x18 * 100 + 0x30 : 0x10 * entries + 0x10 * 100 + 0x18;
        s += al(frame * ((uint64_t)p->perf + 1) + 0xC, 0x100);
    }
    return al(s, 0x1000);
}

// ---------- memorias y direcciones (PoolMapper) ----------
static void error_add(Renderer *R, uint32_t code, uint64_t extra) {
    if (code && R->nerr < 10) R->err[R->nerr].code = code, R->err[R->nerr].pad = 0, R->err[R->nerr++].extra = extra;
    if (code && getenv("NX_TRAZA_AUDREN")) fprintf(stderr, "[nxrt] audren: error 0x%x (0x%llx)\n", code, (unsigned long long)extra);
}

static void addr_reset(AddrInfo *a) { *a = (AddrInfo){0, 0, -1, 0}; }

// TryAttachBuffer: la dirección vale si cae en una memoria adjunta; con mapeo forzado, igual se usa.
static int attach(Renderer *R, AddrInfo *a, uint64_t cpu, uint64_t size, int report) {
    *a = (AddrInfo){cpu, size, -1, 0};
    if (cpu) {
        for (uint32_t i = 0; i < R->npools; i++) {
            Pool *p = &R->pools[i];
            if (p->cpu <= cpu && size + cpu <= p->size + p->cpu) {
                a->pool = (int)i;
                return 1;
            }
        }
        if (R->flags & 1) a->forced = cpu;
    }
    if (report) error_add(R, AR_R_INVALID_ADDRESS, cpu);
    return (R->flags & 1) != 0;
}

// ---------- lectura de la entrada y escritura de la salida ----------
typedef struct { const uint8_t *p; uint64_t size, pos; } Lector;
typedef struct { uint8_t *p; uint64_t size, pos; } Escritor;
static const uint8_t *tomar(Lector *l, uint64_t n) { return l->pos + n <= l->size ? (l->pos += n, l->p + l->pos - n) : NULL; }
static uint8_t *poner(Escritor *e, uint64_t n) { return e->pos + n <= e->size ? (e->pos += n, e->p + e->pos - n) : NULL; }

static void voice_init(Voice *v) { // VoiceInfo.Initialize
    uint8_t in_use = v->in_use;
    memset(v, 0, sizeof *v);
    v->in_use = in_use;
    v->play = v->prev_play = PLAY_STOPPED;
    v->priority = 0xFF;
    v->mix = AR_SIN_MEZCLA, v->splitter = AR_SIN_SPLITTER;
    addr_reset(&v->ds);
    for (int i = 0; i < AR_ONDAS; i++) addr_reset(&v->wb[i].buf), addr_reset(&v->wb[i].ctx), v->wb[i].sent = 1;
}

static uint64_t adpcm_size(uint32_t n) { return 8ull * (n / 14) + (n % 14 ? (n % 14) / 2 + 1 + (n % 2) : 0); }

static Biquad biquad_read(const uint8_t *p, int v2) {
    Biquad b = {p[0], {0}, {0}};
    for (int i = 0; i < 5; i++) {
        float x = v2 ? f32at(p + 4 + 4 * i) : (int16_t)(p[2 + 2 * i] | p[3 + 2 * i] << 8) / 16384.0f; // Q14
        if (i < 3) b.num[i] = x;
        else b.den[i - 3] = x;
    }
    return b;
}

// Una voz (VoiceInParameter: 0x170 bytes, 0x188 desde REV15 con los biquad en float).
static int voice_update(Renderer *R, Voice *v, const uint8_t *p, int v2, uint8_t *out) {
    int d = v2 ? 0x18 : 0;
    uint32_t nch = u32at(p + 0x18);
    if (nch > AR_CANALES) return AR_R_INVALID_UPDATE;
    for (uint32_t c = 0; c < nch; c++)
        if (u32at(p + 0x140 + d + 4 * c) >= R->nvoices) return AR_R_INVALID_UPDATE;
    if (p[8]) voice_init(v);
    v->in_use = 1;
    v->id = (int32_t)u32at(p), v->node = (int32_t)u32at(p + 4);
    uint8_t up = p[0xA]; // estado del juego: 0 iniciar, 1 detener, 2 pausar
    v->prev_play = v->play;
    if (up == 0) v->play = PLAY_STARTED;
    else if (up == 1 && v->play != PLAY_STOPPED) v->play = PLAY_STOPPING;
    else if (up == 2) v->play = PLAY_PAUSED;
    v->src_quality = p[0x15E + d], v->priority = u32at(p + 0x10), v->sorting = u32at(p + 0x14);
    v->rate = u32at(p + 0xC), v->format = p[0xB], v->channels = nch;
    v->pitch = f32at(p + 0x1C), v->volume = f32at(p + 0x20);
    for (int i = 0; i < 2; i++) v->bq[i] = biquad_read(p + 0x24 + i * (v2 ? 0x18 : 0xC), v2);
    v->wb_count = u32at(p + 0x3C + d), v->wb_index = u32at(p + 0x40 + d);
    if (ar_rev(R, 5)) v->flush += p[0x159 + d];
    v->mix = (int32_t)u32at(p + 0x58 + d);
    v->splitter = ar_rev(R, 2) ? u32at(p + 0x5C + d) : AR_SIN_SPLITTER;
    for (uint32_t c = 0; c < AR_CANALES; c++) v->chan[c] = (int32_t)u32at(p + 0x140 + d + 4 * c);
    v->decoding = ar_rev(R, 5) ? (uint16_t)(p[0x15C + d] | p[0x15D + d] << 8) : 0;
    if (p[0x158 + d]) v->drop = 0;
    uint64_t ds = u64at(p + 0x48 + d), ds_size = u64at(p + 0x50 + d);
    if (v->ds.cpu != ds || v->ds.size != ds_size) v->ds_unmapped = !attach(R, &v->ds, ds, ds_size, 1);
    // buffers de onda
    VoiceState *st0 = nch ? &R->vstates[v->chan[0]] : NULL;
    if (p[8]) {
        for (int i = 0; i < AR_ONDAS; i++) v->wb[i].start = v->wb[i].end = 0, v->wb[i].loop = v->wb[i].eos = 0, v->wb[i].sent = 1;
        for (uint32_t c = 0; c < nch; c++) memset(R->vstates[v->chan[c]].valid, 0, AR_ONDAS);
    }
    for (int i = 0; st0 && i < AR_ONDAS; i++) {
        const uint8_t *w = p + 0x60 + d + i * 0x38;
        WaveBuf *b = &v->wb[i];
        if (!st0->valid[i] && b->sent && b->buf.cpu) addr_reset(&b->buf);
        if (w[0x1A] && !v->buf_unmapped) continue; // ya enviado
        uint64_t size = u64at(w + 8);
        uint32_t s0 = u32at(w + 0x10), s1 = u32at(w + 0x14);
        int es = v->format == FMT_PCM8 ? 1 : v->format == FMT_PCM16 ? 2 : v->format == FMT_PCM32 || v->format == FMT_FLOAT ? 4 : 0;
        int ok = v->format == FMT_ADPCM ? adpcm_size(s0) <= size && adpcm_size(s1) <= size : es && (uint64_t)s0 * es <= size && (uint64_t)s1 * es <= size;
        if (!ok) {
            error_add(R, AR_R_INVALID_ADDRESS, u64at(w));
            continue;
        }
        b->sent = 0, b->start = s0, b->end = s1, b->loop = w[0x18], b->eos = w[0x19];
        b->loop_start = u32at(w + 0x30), b->loop_end = u32at(w + 0x34), b->loop_count = (int32_t)u32at(w + 0x1C);
        v->buf_unmapped = !attach(R, &b->buf, u64at(w), size, 1);
        if (v->format == FMT_ADPCM && ar_rev(R, 2) && u64at(w + 0x20)) {
            int ctx = attach(R, &b->ctx, u64at(w + 0x20), u64at(w + 0x28), 1);
            v->buf_unmapped = !ctx || v->buf_unmapped;
        } else addr_reset(&b->ctx);
    }
    // estado de salida: una voz nueva todavía no tocó nada
    if (p[8] || v->is_new) {
        v->is_new = 1;
        return OK;
    }
    if (st0) {
        memcpy(out, &st0->played, 8);
        memcpy(out + 8, &st0->consumed, 4);
    }
    out[0xC] = v->drop;
    return OK;
}

// ---------- efectos ----------
static void effect_reset(Effect *e, int type) {
    free(e->delay);
    memset(e, 0, sizeof *e);
    e->type = (uint8_t)type, e->mix = AR_SIN_MEZCLA, e->order = 0xFFFFFFFFu;
    addr_reset(&e->work[0]), addr_reset(&e->work[1]);
}

static int channels_ok(int n) { return n == 1 || n == 2 || n == 4 || n == 6; }

// EffectInParameter (0xC0): tipo, nuevo, activo, mezcla, buffer de trabajo, orden y 0xA0 de datos.
static void effect_update(Renderer *R, Effect *e, const uint8_t *p) {
    const uint8_t *data = p + 0x20;
    int is_new = p[1], status_at = 0;
    switch (e->type) {
    case FX_BUFFER_MIX: case FX_COMPRESSOR:
        e->mix = (int32_t)u32at(p + 4), e->order = u32at(p + 0x18), e->enabled = p[2];
        memcpy(e->param, data, 0xA0);
        break;
    case FX_AUX: case FX_CAPTURE: { // AuxiliaryBufferParameter
        e->mix = (int32_t)u32at(p + 4), e->order = u32at(p + 0x18), e->enabled = p[2];
        memcpy(e->param, data, 0xA0);
        if (!e->unmapped && !is_new) break;
        uint64_t size = 4ull * u32at(data + 0x38) + 0x80;
        int send = attach(R, &e->work[0], u64at(data + 0x40), size, e->type == FX_CAPTURE);
        if (e->type == FX_CAPTURE) {
            e->unmapped = !send;
            if (send) {
                uint64_t s = ar_addr(R, &e->work[0], 0);
                e->send_info = s, e->send_base = s + 0x80, e->ret_info = e->ret_base = 0;
            }
            break;
        }
        int ret = attach(R, &e->work[1], u64at(data + 0x50), size, 1);
        e->unmapped = !send && !ret;
        if (!e->unmapped) {
            uint64_t s = ar_addr(R, &e->work[0], 0), r = ar_addr(R, &e->work[1], 0);
            e->send_info = s ? s + 0x40 : 0, e->send_base = s ? s + 0x80 : 0;
            e->ret_info = r ? r + 0x40 : 0, e->ret_base = r ? r + 0x80 : 0;
        }
        break;
    }
    case FX_BIQUAD: { // se guarda siempre en float: antes de REV15 llega en Q14
        e->mix = (int32_t)u32at(p + 4), e->order = u32at(p + 0x18), e->enabled = p[2];
        if (ar_rev(R, 15)) memcpy(e->param, data, 0x28);
        else {
            memset(e->param, 0, 0x28);
            memcpy(e->param, data, 12);
            for (int i = 0; i < 5; i++) {
                float x = (int16_t)(data[0xC + 2 * i] | data[0xD + 2 * i] << 8) / 16384.0f;
                memcpy(e->param + 0x10 + 4 * i, &x, 4);
            }
            e->param[0x24] = data[0x16], e->param[0x25] = data[0x17];
        }
        break;
    }
    case FX_DELAY: status_at = 0x34; goto con_estado;
    case FX_REVERB: status_at = 0x40; goto con_estado;
    case FX_REVERB3D: status_at = 0x48;
    con_estado: { // valen si la cuenta máxima de canales es 1, 2, 4 o 6
        if (!channels_ok(data[0xC] | data[0xD] << 8)) break;
        e->mix = (int32_t)u32at(p + 4), e->order = u32at(p + 0x18);
        uint8_t old = e->param[status_at];
        memcpy(e->param, data, 0xA0);
        if (!channels_ok(data[0xE] | data[0xF] << 8)) break;
        e->enabled = p[2];
        if (old != USO_ENABLED) e->param[status_at] = old;
        if (e->unmapped || is_new) {
            e->usage = USO_NEW, e->param[status_at] = USO_INVALID;
            e->unmapped = !attach(R, &e->work[0], u64at(p + 8), u64at(p + 0x10), 1);
        }
        break;
    }
    case FX_LIMITER:
        e->mix = (int32_t)u32at(p + 4), e->order = u32at(p + 0x18), e->enabled = p[2];
        memcpy(e->param, data, 0xA0);
        if (e->unmapped || is_new) {
            e->usage = USO_NEW, e->param[0x40] = USO_INVALID;
            e->unmapped = !attach(R, &e->work[0], u64at(p + 8), u64at(p + 0x10), 1);
        }
        break;
    }
}

// Estadísticas del limitador y del compresor (EffectResultState) cuando el efecto es nuevo.
static void result_init(Effect *e, uint8_t *r) {
    memset(r, 0, 0x80);
    float one = 1.0f;
    if (e->type == FX_LIMITER)
        for (int c = 0; c < 6; c++) memcpy(r + 0x18 + 4 * c, &one, 4);
    else if (e->type == FX_COMPRESSOR) memcpy(r + 4, &one, 4);
}

// ---------- splitters ----------
static void splitter_set(Renderer *R, Splitter *s, const uint8_t *p, Lector *l) {
    for (int d = s->first, k = 0; d >= 0 && k <= (int)R->ndests; k++) { // ClearLinks
        int n = R->dests[d].next;
        R->dests[d].next = -1;
        d = n;
    }
    s->first = -1;
    int32_t want = (int32_t)u32at(p + 0xC), n = want;
    if (!ar_rev(R, 5)) { // antes del arreglo de REV5 se limitaba a destinos por splitter
        int32_t per = (int32_t)(R->ndests / R->nsplitters);
        if (per < n) n = per;
    }
    int prev = -1;
    for (int32_t i = 0; i < n; i++) {
        const uint8_t *q = tomar(l, 4);
        if (!q) return;
        uint32_t id = u32at(q);
        if (id >= R->ndests) continue;
        if (prev < 0) s->first = (int)id;
        else R->dests[prev].next = (int)id;
        R->dests[id].next = -1;
        prev = (int)id;
    }
    if (n > 0) s->count = n;
    if (want > n) tomar(l, 4ull * (uint32_t)(want - n));
    s->rate = u32at(p + 8), s->new_conn = 1;
}

static int splitter_update(Renderer *R, Lector *l, uint32_t size) {
    uint64_t start = l->pos;
    if (!R->nsplitters || !R->ndests) {
        l->pos = start + size;
        return OK;
    }
    const uint8_t *h = tomar(l, 0x20);
    if (!h || u32at(h) != 0x48444E53) return AR_R_INVALID_UPDATE; // "SNDH"
    for (uint32_t i = 0; i < R->nsplitters; i++) R->splitters[i].new_conn = 0;
    for (uint32_t i = 0; i < u32at(h + 4); i++) {
        const uint8_t *p = tomar(l, 0x10);
        if (!p) break;
        if (u32at(p) != 0x49444E53) { l->pos -= 0x10; break; } // "SNDI"
        uint32_t id = u32at(p + 4);
        if (id < R->nsplitters) splitter_set(R, &R->splitters[id], p, l);
        tomar(l, 0xC); // 12 bytes sin uso después de los destinos
    }
    // destinos: 0x70 bytes; desde REV12 con dos biquad (Q14, 0x90), y en float desde REV15 (0xA8)
    int bq = ar_rev(R, 12), bq2 = ar_rev(R, 15);
    uint32_t dsize = bq ? (bq2 ? 0xA8 : 0x90) : 0x70, tail = 0x6C + (bq ? (bq2 ? 0x30 : 0x18) : 0);
    for (uint32_t i = 0; i < u32at(h + 8); i++) {
        const uint8_t *p = tomar(l, dsize);
        if (!p) break;
        if (u32at(p) != 0x44444E53) { l->pos -= dsize; break; } // "SNDD"
        uint32_t id = u32at(p + 4);
        if (id >= R->ndests) continue;
        SplitDest *d = &R->dests[id];
        d->dest = (int32_t)u32at(p + 0x68);
        memcpy(d->mix, p + 8, sizeof d->mix);
        if (bq)
            for (int k = 0; k < 2; k++) d->bq[k] = biquad_read(p + 0x6C + k * (bq2 ? 0x18 : 0xC), bq2);
        int used = p[tail], reset = ar_rev(R, 13) ? p[tail + 1] : !d->used && used;
        if (reset) memcpy(d->prev, d->mix, sizeof d->prev), d->need_update = 0;
        d->used = (uint8_t)used;
    }
    l->pos = start + size;
    return OK;
}

// ---------- mezclas ----------
static void edges_remove(Renderer *R, uint32_t src) {
    if (R->edges && src < R->nmixes) memset(R->edges + (size_t)src * R->nmixes, 0, R->nmixes);
}
static void edge_add(Renderer *R, int32_t a, int32_t b) {
    if (R->edges && a >= 0 && b >= 0 && (uint32_t)a < R->nmixes && (uint32_t)b < R->nmixes) R->edges[(size_t)a * R->nmixes + (uint32_t)b] = 1;
}

// MixInfo.Update: devuelve si cambiaron las conexiones.
static int mix_set(Renderer *R, Mix *m, const uint8_t *p) {
    m->volume = f32at(p), m->rate = u32at(p + 4), m->buffers = u32at(p + 8), m->used = p[0xC];
    m->id = (int32_t)u32at(p + 0x10), m->node = (int32_t)u32at(p + 0x18);
    memcpy(m->vol, p + 0x24, sizeof m->vol);
    int32_t dest = (int32_t)u32at(p + 0x924);
    uint32_t dsplit = u32at(p + 0x928);
    int dirty;
    if (ar_rev(R, 2)) {
        int fresh = m->dest_splitter < R->nsplitters && R->splitters[m->dest_splitter].new_conn;
        dirty = !(m->dest_mix == dest && m->dest_splitter == dsplit && !fresh);
        if (dirty) {
            edges_remove(R, (uint32_t)m->id);
            if (dest == AR_SIN_MEZCLA) {
                if (dsplit < R->nsplitters) {
                    int d = R->splitters[dsplit].first;
                    for (int i = 0; i < R->splitters[dsplit].count && d >= 0; i++, d = R->dests[d].next)
                        if (R->dests[d].dest != AR_SIN_MEZCLA) edge_add(R, m->id, R->dests[d].dest);
                }
            } else edge_add(R, m->id, dest);
            m->dest_mix = dest, m->dest_splitter = dsplit;
        }
    } else {
        dirty = m->dest_mix != dest;
        m->dest_mix = dest, m->dest_splitter = AR_SIN_SPLITTER;
    }
    for (uint32_t i = 0; i < R->neffects; i++) m->order[i] = -1;
    for (uint32_t i = 0; i < R->neffects; i++) {
        Effect *e = &R->effects[i];
        if (e->mix != m->id) continue;
        if (e->order > R->neffects) return dirty;
        if (e->order < R->neffects) m->order[e->order] = (int32_t)i;
    }
    return dirty;
}

static void mix_offsets(Renderer *R) {
    uint32_t off = 0;
    for (uint32_t i = 0; i < R->nmixes; i++) R->mixes[i].offset = off, off += R->mixes[i].buffers;
}

// Sin splitters: por distancia a la mezcla final, de la más lejana a la más cercana.
static void mix_sort_distance(Renderer *R) {
    uint32_t n = R->nmixes, inval = 0x80000000u;
    for (uint32_t i = 0; i < n; i++) R->mixes[i].distance = inval;
    for (uint32_t i = 0; i < n; i++) {
        Mix *m = &R->mixes[i];
        if (!m->used) continue;
        uint32_t dist = inval;
        if (m->id != 0) {
            int32_t id = m->id;
            for (dist = 0; dist < n; dist++) {
                if (id == AR_SIN_MEZCLA || id < 0 || (uint32_t)id >= n) { dist = inval; break; }
                Mix *x = &R->mixes[id];
                if (x->distance != inval) { dist = x->distance + 1; break; }
                id = x->dest_mix;
                if (id == 0) break;
            }
            if (dist > n) dist = inval;
        }
        m->distance = dist;
    }
    for (uint32_t i = 0; i < n; i++) { // estable, de mayor a menor distancia
        int32_t k = (int32_t)i, j = (int32_t)i - 1;
        while (j >= 0 && R->mixes[R->sorted_mixes[j]].distance < R->mixes[k].distance) R->sorted_mixes[j + 1] = R->sorted_mixes[j], j--;
        R->sorted_mixes[j + 1] = k;
    }
    mix_offsets(R);
}

// Con splitters: orden topológico del grafo de mezclas (NodeStates); un ciclo es un error.
static int mix_sort_graph(Renderer *R) {
    uint32_t n = R->nmixes, top = 0, done = 0;
    uint8_t *state = calloc(n, 1);    // 0 sin ver, 1 descubierto, 2 terminado
    uint32_t *stack = malloc(sizeof(uint32_t) * (n * n + 1));
    int32_t *res = malloc(sizeof(int32_t) * n);
    int ok = 1;
    for (uint32_t i = 0; i < n && ok; i++) {
        if (state[i]) continue;
        stack[top++] = i;
        while (top && ok) {
            uint32_t t = stack[top - 1];
            if (state[t] == 1) state[t] = 2, res[done++] = (int32_t)t, top--;
            else if (state[t] == 2) top--;
            else {
                state[t] = 1;
                for (uint32_t j = 0; j < n; j++)
                    if (R->edges[(size_t)t * n + j]) {
                        if (!state[j] && top < n * n) stack[top++] = j;
                        else if (state[j] == 1) ok = 0;
                    }
            }
        }
    }
    if (ok) {
        for (uint32_t i = 0; i < done; i++) R->sorted_mixes[i] = res[done - 1 - i];
        mix_offsets(R);
    }
    free(state), free(stack), free(res);
    return ok;
}

static int mixes_update(Renderer *R, Lector *l, uint32_t size) {
    uint32_t count = R->nmixes, in_size = 0;
    int dirty_only = ar_rev(R, 7);
    if (dirty_only) { // MixInParameterDirtyOnlyUpdate: solo las mezclas que cambiaron
        const uint8_t *h = tomar(l, 0x20);
        if (!h) return AR_R_INVALID_UPDATE;
        count = u32at(h + 4), in_size = 0x20;
    }
    uint64_t mix_size = (uint64_t)count * 0x930;
    if (in_size + mix_size != size) return AR_R_INVALID_UPDATE;
    const uint8_t *base = tomar(l, mix_size);
    if (!base) return AR_R_INVALID_UPDATE;
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *p = base + (uint64_t)i * 0x930;
        int64_t dest = (int32_t)u32at(p + 0x924);
        if (!p[0xC]) continue;
        if (dest != AR_SIN_MEZCLA && dest > (int64_t)R->nmixes && u32at(p + 0x10) != 0) return AR_R_INVALID_UPDATE;
        total += u32at(p + 8);
    }
    if (total > R->mix_buffers) return AR_R_INVALID_UPDATE;
    int dirty = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *p = base + (uint64_t)i * 0x930;
        uint32_t id = dirty_only ? u32at(p + 0x10) : i;
        if (id >= R->nmixes) return AR_R_INVALID_UPDATE;
        Mix *m = &R->mixes[id];
        if (p[0xC] != m->used) {
            m->used = p[0xC];
            if (m->used)
                for (uint32_t k = 0; k < R->neffects; k++) m->order[k] = -1;
            dirty = 1;
        }
        if (m->used) dirty |= mix_set(R, m, p);
    }
    if (dirty) {
        if (ar_rev(R, 2) && R->nsplitters && R->ndests) {
            if (!mix_sort_graph(R)) return AR_R_INVALID_SORTING;
        } else mix_sort_distance(R);
    }
    return OK;
}

// ---------- sinks ----------
static void sink_reset(Sink *s, int type) {
    memset(s, 0, sizeof *s);
    s->type = (uint8_t)type;
    addr_reset(&s->circ);
}

static void sink_update(Renderer *R, Sink *s, const uint8_t *p, uint8_t *out) {
    const uint8_t *data = p + 0x20;
    if (s->type == SINK_DEVICE) { // DeviceParameter: nombre, entradas y coeficientes de mezcla a estéreo
        if (p[1] != s->used) s->used = p[1], s->node = (int32_t)u32at(p + 4), memcpy(s->param, data, 0x120);
        else s->param[0x10B] = data[0x10B], memcpy(s->param + 0x10C, data + 0x10C, 16);
        memcpy(s->downmix, s->param + 0x10C, 16);
    } else if (s->type == SINK_CIRCULAR) { // CircularBufferParameter
        if (p[1] != s->used || s->skip) {
            if (p[1] != s->used) s->used = p[1], s->node = (int32_t)u32at(p + 4);
            if (p[1]) s->skip = !attach(R, &s->circ, u64at(data), u32at(data + 8), 1);
            memcpy(s->param, data, 0x24);
        }
        memcpy(out, &s->last_written, 4);
    }
}

// ---------- la actualización (StateUpdater) ----------
static uint32_t update(Renderer *R, const uint8_t *in, uint64_t in_size, uint8_t *out, uint64_t out_size, uint8_t *perf, uint64_t perf_size) {
    Lector l = {in, in_size, 0};
    Escritor w = {out, out_size, 0};
    if (out_size) memset(out, 0, out_size);
    const uint8_t *h = tomar(&l, 0x40);
    uint8_t *oh = poner(&w, 0x40);
    if (!h || !oh) return AR_R_INVALID_UPDATE;
    uint32_t sz_beh = u32at(h + 4), sz_mp = u32at(h + 8), sz_vo = u32at(h + 0xC), sz_ch = u32at(h + 0x10), sz_fx = u32at(h + 0x14);
    uint32_t sz_mix = u32at(h + 0x18), sz_sink = u32at(h + 0x1C), sz_perf = u32at(h + 0x20), sz_split = u32at(h + 0x24);
    uint32_t osz[11] = {0}; // tamaños de la salida, en el orden de la cabecera
    uint8_t *q;
    // comportamiento: la revisión tiene que ser la del renderizador
    const uint8_t *b = tomar(&l, 0x10);
    if (!b || rev_number((int32_t)u32at(b)) == 0 || (int32_t)u32at(b) != R->revision) return AR_R_INVALID_UPDATE;
    R->nerr = 0;
    R->flags = u64at(b + 8);
    if (sz_beh != 0x10) return AR_R_INVALID_UPDATE;
    // memorias: pedir adjuntar → adjunta, pedir soltar → suelta (si ningún comando la usó)
    if ((uint64_t)R->npools * 0x20 != sz_mp) return AR_R_INVALID_UPDATE;
    for (uint32_t i = 0; i < R->npools; i++) {
        const uint8_t *p = tomar(&l, 0x20);
        if (!p || !(q = poner(&w, 0x10))) return AR_R_INVALID_UPDATE;
        uint64_t cpu = u64at(p), size = u64at(p + 8);
        uint32_t st = u32at(p + 0x10), ns;
        Pool *mp = &R->pools[i];
        if (st != 4 && st != 2) continue;
        if (!cpu || cpu & 0xFFF || !size || size & 0xFFF) return AR_R_INVALID_UPDATE;
        if (st == 4) mp->cpu = cpu, mp->size = size, mp->mapped = 1, ns = 5;
        else {
            if (mp->cpu != cpu || mp->size != size) return AR_R_INVALID_UPDATE;
            if (mp->used) continue;
            mp->cpu = mp->size = 0, mp->mapped = 0, ns = 3;
        }
        memcpy(q, &ns, 4);
    }
    osz[2] = R->npools * 0x10;
    // recursos de canal: los factores de mezcla de cada canal de voz
    if ((uint64_t)R->nvoices * 0x70 != sz_ch) return AR_R_INVALID_UPDATE;
    for (uint32_t i = 0; i < R->nvoices; i++) {
        const uint8_t *p = tomar(&l, 0x70);
        if (!p) return AR_R_INVALID_UPDATE;
        R->chans[i].id = u32at(p);
        memcpy(R->chans[i].mix, p + 4, sizeof R->chans[i].mix);
        R->chans[i].used = p[0x64];
    }
    // voces
    int v2 = ar_rev(R, 15);
    uint32_t vsize = v2 ? 0x188 : 0x170;
    if ((uint64_t)R->nvoices * vsize != sz_vo) return AR_R_INVALID_UPDATE;
    for (uint32_t i = 0; i < R->nvoices; i++) R->voices[i].in_use = 0;
    for (uint32_t i = 0; i < R->nvoices; i++) {
        const uint8_t *p = tomar(&l, vsize);
        if (!p || !(q = poner(&w, 0x10))) return AR_R_INVALID_UPDATE;
        if (p[9]) {
            uint32_t r = voice_update(R, &R->voices[i], p, v2, q);
            if (r) return r;
        }
    }
    osz[3] = R->nvoices * 0x10;
    // efectos: entrada de 0xC0; salida de 0x10, o de 0x90 con las estadísticas desde REV9
    int fx2 = ar_rev(R, 9);
    uint32_t fxo = fx2 ? 0x90 : 0x10;
    if ((uint64_t)R->neffects * 0xC0 != sz_fx) return AR_R_INVALID_UPDATE;
    for (uint32_t i = 0; i < R->neffects; i++) {
        const uint8_t *p = tomar(&l, 0xC0);
        if (!p || !(q = poner(&w, fxo))) return AR_R_INVALID_UPDATE;
        Effect *e = &R->effects[i];
        if (p[0] != e->type) effect_reset(e, p[0] <= FX_COMPRESSOR ? p[0] : FX_INVALID);
        effect_update(R, e, p);
        q[0] = R->active ? (e->usage == USO_DISABLED ? 4 : 3) : (e->usage == USO_NEW ? 3 : 4);
        if (fx2) {
            if (p[1]) result_init(e, e->result), result_init(e, e->result_dsp);
            if (e->type == FX_LIMITER || e->type == FX_COMPRESSOR) memcpy(q + 0x10, e->result, 0x80);
        }
    }
    osz[5] = R->neffects * fxo;
    if (ar_rev(R, 2)) {
        uint32_t r = splitter_update(R, &l, sz_split);
        if (r) return r;
    }
    uint32_t r = mixes_update(R, &l, sz_mix);
    if (r) return r;
    // sinks: entrada de 0x140, salida de 0x20
    if ((uint64_t)R->nsinks * 0x140 != sz_sink) return AR_R_INVALID_UPDATE;
    for (uint32_t i = 0; i < R->nsinks; i++) {
        const uint8_t *p = tomar(&l, 0x140);
        if (!p || !(q = poner(&w, 0x20))) return AR_R_INVALID_UPDATE;
        Sink *s = &R->sinks[i];
        if (p[0] != s->type) sink_reset(s, p[0] <= SINK_CIRCULAR ? p[0] : SINK_INVALID);
        sink_update(R, s, p, q);
    }
    osz[7] = R->nsinks * 0x20;
    // rendimiento: las cabeceras de los cuadros guardados ("PERF"), y una vacía al final
    const uint8_t *pp = tomar(&l, 0x10);
    if (sz_perf != 0x10 || !pp || !(q = poner(&w, 0x10))) return AR_R_INVALID_UPDATE;
    uint32_t hist = 0;
    if (R->perf_frames && perf && perf_size) {
        uint32_t hs = ar_rev(R, 5) ? 0x30 : 0x18, n = R->perf_frames < 64 ? R->perf_frames : 64;
        while (R->perf_r != R->perf_w && hist < perf_size && perf_size - hist >= 2 * hs) {
            uint8_t *f = perf + hist;
            memset(f, 0, hs);
            uint32_t magic = 0x46524550, next = hist + hs;
            memcpy(f, &magic, 4), memcpy(f + 0xC, &next, 4);
            if (hs == 0x30) memcpy(f + 0x18, &R->perf_start[R->perf_r], 8), memcpy(f + 0x20, &R->perf_idx[R->perf_r], 4);
            hist = next;
            R->perf_r = (R->perf_r + 1) % n;
        }
        if (perf_size - hist >= hs) memset(perf + hist, 0, hs);
    }
    memcpy(q, &hist, 4);
    osz[8] = 0x10;
    // errores (BehaviourErrorInfoOutStatus) y cuadros hechos (desde REV5)
    if (!(q = poner(&w, 0xB0))) return AR_R_INVALID_UPDATE;
    memcpy(q, R->err, sizeof R->err);
    memcpy(q + 0xA0, &R->nerr, 4);
    osz[1] = 0xB0;
    if (ar_rev(R, 5)) {
        if (!(q = poner(&w, 0x10))) return AR_R_INVALID_UPDATE;
        memcpy(q, &R->elapsed, 8);
        osz[10] = 0x10;
    }
    // todo lo de la entrada tiene que haberse leído
    if (l.pos != u32at(h + 0x3C)) return AR_R_INVALID_UPDATE;
    uint32_t hdr[16] = {(uint32_t)R->revision};
    for (int k = 1; k < 11; k++) hdr[k] = osz[k];
    hdr[15] = (uint32_t)w.pos;
    memcpy(oh, hdr, 0x40);
    nx_kernel_lock(); // el evento del renderizador se limpia con cada actualización
    R->event->signaled = 0;
    nx_kernel_unlock();
    return OK;
}

// ---------- hilo de mezcla ----------
static void *render_thread(void *arg) {
    Renderer *R = arg;
    uint64_t clock = nx_now_ns();
    for (;;) {
        pthread_mutex_lock(&R->lock);
        int active = R->active, ch = active ? ar_frame(R) : 0;
        pthread_mutex_unlock(&R->lock);
        if (ch) nx_audio_write(R->out, AR_SALIDA);
        if (active) nx_event_signal(R->event);
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

static uint32_t renderer(ServiceObject *self, NxRequest *r) {
    Renderer *R = self->state;
    uint32_t res = OK;
    switch (r->cmd) {
    case 0: nx_out32(r, R->rate); return OK;          // GetSampleRate
    case 1: nx_out32(r, R->samples); return OK;       // GetSampleCount
    case 2: nx_out32(r, R->mix_buffers); return OK;   // GetMixBufferCount
    case 3: nx_out32(r, R->active ? 0 : 1); return OK; // GetState
    case 4: case 10: { // RequestUpdate(Auto): salida, rendimiento, entrada
        NxBuffer *in = nx_in_buffer(r, 0), *out = nx_out_buffer(r, 0), *perf = nx_out_buffer(r, 1);
        if (!in || !out) return AR_R_INVALID_UPDATE;
        pthread_mutex_lock(&R->lock);
        res = update(R, (const uint8_t *)(uintptr_t)in->addr, in->size, (uint8_t *)(uintptr_t)out->addr, out->size,
                     perf ? (uint8_t *)(uintptr_t)perf->addr : NULL, perf ? perf->size : 0);
        pthread_mutex_unlock(&R->lock);
        if (res && getenv("NX_TRAZA_AUDREN")) fprintf(stderr, "[nxrt] audren: actualización rechazada (0x%x)\n", res);
        return res;
    }
    case 5: case 6: // Start / Stop
        pthread_mutex_lock(&R->lock);
        if (r->cmd == 5) R->elapsed = 0;
        R->active = r->cmd == 5;
        pthread_mutex_unlock(&R->lock);
        return OK;
    case 7: // QuerySystemEvent: se señala con cada cuadro
        if (R->manual) return AR_R_UNSUPPORTED;
        nx_out_copy_handle(r, R->event_h);
        return OK;
    case 8: R->time_limit = nx_in32(r, 0); return OK;  // SetRenderingTimeLimit
    case 9: nx_out32(r, R->time_limit); return OK;     // GetRenderingTimeLimit
    case 11: return R->manual && R->cpu ? AR_R_INVALID_EXECUTION : AR_R_UNSUPPORTED; // ExecuteAudioRendererRendering: no con el DSP
    case 12: { float v = f32at(r->in); R->drop_param = v < 0 ? 0 : v > 4 ? 4 : v; return OK; } // SetVoiceDropParameter
    case 13: { uint32_t v; memcpy(&v, &R->drop_param, 4); nx_out32(r, v); return OK; }        // GetVoiceDropParameter
    }
    return NX_R_UNKNOWN_CMD;
}

static ServiceObject *renderer_open(NxRequest *r, int manual, uint32_t *res) {
    ArParam a = param_read(r->in);
    int rev = rev_number(a.revision);
    *res = OK;
    if (!rev || !a.samples || a.samples > 0x1000) {
        *res = AR_R_OPERATION_FAILED;
        return NULL;
    }
    if (!manual && work_size(&a) > nx_in64(r, 0x38)) {
        *res = AR_R_WORK_BUFFER_SMALL;
        return NULL;
    }
    Renderer *R = calloc(1, sizeof *R);
    pthread_mutex_init(&R->lock, NULL);
    R->rate = a.rate, R->samples = a.samples, R->mix_buffers = a.mix_buffers, R->submixes = a.submixes;
    R->nvoices = a.voices, R->nsinks = a.sinks, R->neffects = a.effects, R->perf_frames = a.perf;
    R->revision = a.revision, R->rev = rev, R->voice_drop = a.drop, R->manual = (uint8_t)manual, R->cpu = a.device == 1;
    R->time_limit = 100, R->drop_param = 1.0f;
    R->npools = a.effects + a.voices * 4;
    R->pools = calloc(R->npools + 1, sizeof *R->pools);
    R->voices = calloc(a.voices + 1, sizeof *R->voices);
    R->chans = calloc(a.voices + 1, sizeof *R->chans);
    R->vstates = calloc(a.voices + 1, sizeof *R->vstates);
    R->sorted_voices = calloc(a.voices + 1, sizeof *R->sorted_voices);
    for (uint32_t i = 0; i < a.voices; i++) voice_init(&R->voices[i]), R->chans[i].id = i;
    R->nmixes = a.submixes + 1;
    R->mixes = calloc(R->nmixes, sizeof *R->mixes);
    R->sorted_mixes = calloc(R->nmixes, sizeof *R->sorted_mixes);
    R->mix_order = malloc(sizeof(int32_t) * ((size_t)a.effects * R->nmixes + 1));
    for (uint32_t i = 0; i < R->nmixes; i++) {
        Mix *m = &R->mixes[i];
        m->id = i ? AR_SIN_MEZCLA : 0, m->distance = 0x80000000u, m->dest_mix = AR_SIN_MEZCLA, m->dest_splitter = AR_SIN_SPLITTER;
        m->long_predelay = rev >= 3;
        m->order = R->mix_order + (size_t)i * a.effects;
        for (uint32_t k = 0; k < a.effects; k++) m->order[k] = -1;
        R->sorted_mixes[i] = (int32_t)i;
    }
    if (rev >= 2) R->edges = calloc((size_t)R->nmixes * R->nmixes, 1);
    if (rev >= 2 && a.splitters && a.dests) {
        R->nsplitters = a.splitters, R->ndests = a.dests;
        R->splitters = calloc(a.splitters, sizeof *R->splitters);
        R->dests = calloc(a.dests, sizeof *R->dests);
        for (uint32_t i = 0; i < a.splitters; i++) R->splitters[i].id = (int32_t)i, R->splitters[i].first = -1;
        for (uint32_t i = 0; i < a.dests; i++) R->dests[i].id = (int32_t)i, R->dests[i].dest = AR_SIN_MEZCLA, R->dests[i].next = -1;
        if (rev >= 12) R->dest_bq = calloc((size_t)a.dests * 4, sizeof *R->dest_bq);
    }
    R->effects = calloc(a.effects + 1, sizeof *R->effects);
    for (uint32_t i = 0; i < a.effects; i++) effect_reset(&R->effects[i], FX_INVALID);
    R->sinks = calloc(a.sinks + 1, sizeof *R->sinks);
    for (uint32_t i = 0; i < a.sinks; i++) sink_reset(&R->sinks[i], SINK_INVALID);
    R->nbufs = a.mix_buffers + AR_CANALES;
    R->buf = calloc((size_t)R->nbufs * a.samples, sizeof(float));
    R->depop = calloc(R->nbufs, sizeof(float));
    R->event_h = nx_event_new(&R->event);
    if (getenv("NX_TRAZA_AUDREN"))
        fprintf(stderr, "[nxrt] audren: REV%d, %u Hz, %u muestras, %u voces, %u efectos, %u submezclas, %u buffers, %u splitters\n", rev, a.rate,
                a.samples, a.voices, a.effects, a.submixes, a.mix_buffers, a.splitters);
    pthread_t t;
    pthread_create(&t, NULL, render_thread, R);
    return nx_object("IAudioRenderer", renderer, R);
}

// ---------- IAudioDevice ----------
// Los dispositivos de Ryujinx (VirtualDevice); el activo es la TV en la base y el parlante en modo portátil.
static const char *dev_names[5] = {"AudioStereoJackOutput", "AudioBuiltInSpeakerOutput", "AudioTvOutput", "AudioUsbDeviceOutput", "AudioExternalOutput"};
static float dev_volume[5] = {1, 1, 1, 1, 1};
static int dev_active(void) { return nx_en_base() ? 2 : 1; }
float ar_device_volume(void) { return dev_volume[dev_active()]; }

typedef struct { int usb; KEvent *ev[3]; uint32_t h[3]; } AudioDevice;

static int dev_find(NxBuffer *b) {
    if (!b || !b->addr) return -1;
    const char *s = (const char *)(uintptr_t)b->addr;
    size_t n = strnlen(s, b->size);
    for (int i = 0; i < 5; i++)
        if (strlen(dev_names[i]) == n && !memcmp(dev_names[i], s, n)) return i;
    return -1;
}

static void name_out(NxBuffer *b, const char *s) {
    if (!b || !b->addr || !b->size) return;
    size_t n = strlen(s) + 1 < b->size ? strlen(s) + 1 : b->size;
    memset((void *)(uintptr_t)b->addr, 0, b->size < 0x100 ? b->size : 0x100);
    memcpy((void *)(uintptr_t)b->addr, s, n);
}

static uint32_t audio_device(ServiceObject *self, NxRequest *r) {
    AudioDevice *d = self->state;
    switch (r->cmd) {
    case 0: case 6: case 14: { // ListAudioDeviceName(Auto), ListAudioOutputDeviceName
        NxBuffer *b = nx_out_buffer(r, 0);
        const char *names[5];
        uint32_t n = 0;
        for (int i = 0; i < 5; i++) {
            if (i == 3 && !d->usb) continue;
            const char *s = r->cmd == 14 && (i == 0 || i == 3) ? dev_names[4] : dev_names[i]; // salidas externas
            int dup = 0;
            for (uint32_t k = 0; k < n; k++) dup |= !strcmp(names[k], s);
            if (!dup) names[n++] = s;
        }
        uint32_t fit = b && b->addr ? (uint32_t)(b->size / 0x100) : 0, k = 0;
        for (; k < n && k < fit; k++) {
            memset((void *)(uintptr_t)(b->addr + k * 0x100), 0, 0x100);
            memcpy((void *)(uintptr_t)(b->addr + k * 0x100), names[k], strlen(names[k]));
        }
        nx_out32(r, k);
        return OK;
    }
    case 1: case 7: { // SetAudioDeviceOutputVolume(Auto)
        int i = dev_find(nx_in_buffer(r, 0));
        float v = f32at(r->in);
        if (i < 0) return AR_R_DEVICE_NOT_FOUND;
        dev_volume[i] = v < 0 ? 0 : v > 1 ? 1 : v;
        return OK;
    }
    case 2: case 8: { // GetAudioDeviceOutputVolume(Auto)
        int i = dev_find(nx_in_buffer(r, 0));
        if (i < 0) return AR_R_DEVICE_NOT_FOUND;
        uint32_t v;
        memcpy(&v, &dev_volume[i], 4);
        nx_out32(r, v);
        return OK;
    }
    case 3: case 10: name_out(nx_out_buffer(r, 0), dev_names[dev_active()]); return OK; // GetActiveAudioDeviceName(Auto)
    case 13: name_out(nx_out_buffer(r, 0), dev_names[dev_active()]); return OK;         // GetActiveAudioOutputDeviceName
    case 4: case 11: case 12: // QueryAudioDeviceSystemEvent, InputEvent, OutputEvent
        nx_out_copy_handle(r, d->h[r->cmd == 4 ? 0 : r->cmd == 11 ? 1 : 2]);
        return OK;
    case 5: nx_out32(r, 2); return OK; // GetActiveChannelCount: la salida es estéreo
    }
    return NX_R_UNKNOWN_CMD;
}

static ServiceObject *device_open(int32_t revision) {
    AudioDevice *d = calloc(1, sizeof *d);
    d->usb = rev_number(revision) >= 4; // la salida USB se lista desde REV4
    for (int i = 0; i < 3; i++) d->h[i] = nx_event_new(&d->ev[i]);
    nx_event_signal(d->ev[0]); // como Ryujinx: el evento del sistema empieza señalado
    return nx_object("IAudioDevice", audio_device, d);
}

uint32_t nx_audren(ServiceObject *self, NxRequest *r) {
    (void)self;
    uint32_t res;
    switch (r->cmd) {
    case 0: case 3: { // OpenAudioRenderer(parámetros, tamaño de trabajo, aruid), ...ForManualExecution(parámetros, dirección, aruid)
        ServiceObject *o = renderer_open(r, r->cmd == 3, &res);
        if (o) nx_out_object(r, o);
        return res;
    }
    case 1: { // GetWorkBufferSize
        ArParam a = param_read(r->in);
        if (!rev_number(a.revision)) return AR_R_OPERATION_FAILED;
        nx_out64(r, work_size(&a));
        return OK;
    }
    case 2: nx_out_object(r, device_open(0)); return OK;                      // GetAudioDeviceService
    case 4: nx_out_object(r, device_open((int32_t)nx_in32(r, 0))); return OK; // GetAudioDeviceServiceWithRevisionInfo
    }
    return NX_R_UNKNOWN_CMD;
}
