// Renderizador de audio: cada cuadro recorre las voces, las submezclas, la mezcla final y los sinks
// en el orden del CommandGenerator de Ryujinx (MIT, src/Ryujinx.Audio/Renderer/Server) y ejecuta
// cada comando en el momento, con la matemática de sus comandos del DSP (Renderer/Dsp): lectura de
// PCM y DSP-ADPCM, remuestreador con sus tablas, rampas de volumen, filtros biquad, depop, mezclas,
// splitters, efectos y remuestreo de 32 a 48 kHz. Las muestras son float en escala de 16 bits y
// cada multiplicación se redondea al entero más cercano, como en Ryujinx.
// Efectos: mezcla de buffers, AUX, captura, biquad y limitador completos; delay, reverb, reverb 3D y
// compresor dejan pasar la señal (con su estado y su salida de estado correctos).
#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "nxaudio.h"
#include "nxaudren.h"
#include "nxaudren_lut.h"

#pragma STDC FP_CONTRACT OFF

static float *buf(Renderer *R, uint32_t i) { return i < R->nbufs ? R->buf + (size_t)i * R->samples : NULL; }
static float mul_round(float a, float b) { return rintf(a * b); } // FloatingPointHelper.MultiplyRoundUp
static uint32_t rd32(uint64_t a) { uint32_t v; memcpy(&v, (const void *)(uintptr_t)a, 4); return v; }
static void wr32(uint64_t a, uint32_t v) { memcpy((void *)(uintptr_t)a, &v, 4); }

uint64_t ar_addr(Renderer *R, AddrInfo *a, int mark) {
    if (a->pool < 0 || a->pool >= (int)R->npools || !R->pools[a->pool].mapped) return a->forced;
    Pool *p = &R->pools[a->pool];
    if (mark) p->used = 1;
    return p->cpu <= a->cpu && a->cpu + a->size <= p->cpu + p->size ? a->cpu : 0;
}

// ---------- lectura de muestras ----------
// DSP-ADPCM (AdpcmHelper): cuadros de 8 bytes, cabecera (predictor << 4 | escala) y 14 muestras.
static int adpcm_decode(int16_t *out, const uint8_t *in, uint64_t size, int start, int end, int offset, int count,
                        const int16_t *coef, int ncoef, int16_t ctx[3]) {
    if (!in || !size || end < start) return 0;
    uint8_t ps = (uint8_t)ctx[0];
    int scale = ps & 15, ci = ps >> 4 & 15;
    int16_t h0 = ctx[1], h1 = ctx[2];
    int c0 = ci * 2 < ncoef ? coef[ci * 2] : 0, c1 = ci * 2 + 1 < ncoef ? coef[ci * 2 + 1] : 0;
    int n = count < end - start - offset ? count : end - start - offset;
    if (n < 0) n = 0;
    int s = offset + start;
    int nib = s % 14 + 16 * (s / 14) + (s % 14 ? 2 : 0);
    uint64_t pos = (uint64_t)nib / 2;
    int done = 0;
    while (done < n) {
        if (pos >= size) break;
        int k;
        if (nib % 16 == 0) {
            ps = in[pos++];
            scale = ps & 15, ci = ps >> 4 & 15;
            c0 = ci * 2 < ncoef ? coef[ci * 2] : 0, c1 = ci * 2 + 1 < ncoef ? coef[ci * 2 + 1] : 0;
            nib += 2;
            k = n - done < 14 ? n - done : 14;
        } else k = 1;
        int sf = 2048 << scale;
        for (int i = 0; i < k && pos < size; i++) {
            int v = in[pos];
            int x = nib & 1 ? (int32_t)((uint32_t)v << 28) >> 28 : (int32_t)((uint32_t)v << 24) >> 28;
            if (nib & 1) pos++;
            nib++;
            int y = (x * sf + c0 * h0 + c1 * h1 + 1024) >> 11;
            int16_t z = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
            h1 = h0, h0 = z;
            out[done++] = z;
        }
    }
    ctx[0] = ps, ctx[1] = h0, ctx[2] = h1;
    return done;
}

// PCM intercalado (PcmHelper.Decode): canal ch de cada muestra, pasado a 16 bits.
static int pcm_decode(int16_t *out, uint64_t addr, uint64_t size, int fmt, int start, int end, int offset, int count, int ch, int nch) {
    if (!addr || !size || end < start) return 0;
    int n = count < end - start - offset ? count : end - start - offset;
    if (n <= 0) return 0;
    int es = fmt == FMT_PCM8 ? 1 : fmt == FMT_PCM16 ? 2 : 4;
    uint64_t base = (uint64_t)es * nch * (uint64_t)(start + offset);
    if (base + (uint64_t)es * nch * n > size) n = base >= size ? 0 : (int)((size - base) / ((uint64_t)es * nch));
    const uint8_t *p = (const uint8_t *)(uintptr_t)(addr + base);
    for (int i = 0; i < n; i++) {
        const uint8_t *q = p + ((uint64_t)i * nch + ch) * es;
        switch (fmt) {
        case FMT_PCM8: out[i] = (int16_t)((int8_t)q[0] * 256); break;
        case FMT_PCM16: memcpy(&out[i], q, 2); break;
        case FMT_PCM32: { int32_t v; memcpy(&v, q, 4); out[i] = (int16_t)(v >> 16); break; }
        case FMT_FLOAT: { // ConvertSampleToPcmInt16: saturar s * 32767 y truncar
            float v; memcpy(&v, q, 4);
            v *= 32767.0f;
            out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            break;
        }
        }
    }
    return n;
}

// ---------- remuestreo (ResamplerHelper) ----------
// Con razón 1 y sin fase pendiente se copia la muestra del centro del filtro: la misma latencia que el
// filtro de Ryujinx (tres muestras en calidad normal) pero sin su ganancia de 0,996, así un PCM a la
// frecuencia del renderizador sale intacto.
static void resample(float *out, const int16_t *in, float ratio, float *fraction, int n, int quality) {
    float f = *fraction;
    int idx = 0;
    if (quality == SRC_LOW) {
        for (int i = 0; i < n; i++) {
            out[i] = f > 1.0f ? in[idx + 1] : in[idx];
            f += ratio;
            idx += (int)f;
            f -= (int)f;
        }
    } else if (ratio == 1.0f && f == 0.0f) {
        int c = quality == SRC_HIGH ? 3 : 1;
        for (int i = 0; i < n; i++) out[i] = in[i + c];
    } else if (quality == SRC_HIGH) {
        const int16_t *lut = ratio <= 1.0f ? lut_high1 : ratio > 1.333313f ? lut_high0 : lut_high2;
        for (int i = 0; i < n; i++) {
            const int16_t *p = lut + (int)(f * 128) * 8, *x = in + idx;
            float a = x[0] * (p[0] / 32768.0f) + x[1] * (p[1] / 32768.0f), b = x[2] * (p[2] / 32768.0f) + x[3] * (p[3] / 32768.0f);
            float c = x[4] * (p[4] / 32768.0f) + x[5] * (p[5] / 32768.0f), d = x[6] * (p[6] / 32768.0f) + x[7] * (p[7] / 32768.0f);
            out[i] = rintf((a + b) + (c + d));
            f += ratio;
            idx += (int)f;
            f -= (int)f;
        }
    } else {
        const int16_t *lut = ratio <= 1.0f ? lut_normal1 : ratio > 1.333313f ? lut_normal0 : lut_normal2;
        for (int i = 0; i < n; i++) { // como el camino SSE de Ryujinx: sumas de a pares
            const int16_t *p = lut + (int)(f * 128) * 4, *x = in + idx;
            float a = x[0] * (p[0] / 32768.0f) + x[1] * (p[1] / 32768.0f), b = x[2] * (p[2] / 32768.0f) + x[3] * (p[3] / 32768.0f);
            out[i] = rintf(a + b);
            f += ratio;
            idx += (int)f;
            f -= (int)f;
        }
    }
    *fraction = f;
}

typedef struct {
    uint64_t buf, buf_size, ctx;
    uint32_t start, end, loop_start, loop_end;
    int32_t loop_count;
    uint8_t loop, eos;
} DspWave;

static void end_of_buffer(VoiceState *st, const DspWave *w, int *wbi, uint32_t *consumed, uint64_t *played) {
    st->valid[*wbi] = 0;
    (*wbi)++;
    st->loop_count = 0;
    (*consumed)++;
    if (*wbi >= AR_ONDAS) *wbi = 0;
    if (w->eos) *played = 0;
}

// DataSourceHelper.ProcessWaveBuffers: decodifica lo que hace falta de los buffers de onda (en una
// memoria temporal precedida por la historia del remuestreador) y remuestrea a la salida.
static void process_waves(Renderer *R, float *out, const Voice *v, const DspWave *waves, VoiceState *st, int ch, int nch,
                          uint64_t coef, uint64_t coef_size, int quality) {
    enum { TEMP = AR_TEMP };
    int16_t *temp = R->temp;
    int count = (int)R->samples;
    float ratio = (float)v->rate / (float)R->rate * v->pitch;
    float fraction = st->fraction;
    int wbi = (int)st->wb_index, offset = st->offset;
    uint64_t played = st->played;
    uint32_t consumed = st->consumed;
    int pmax = quality == SRC_HIGH ? 8 : 4, skip = v->decoding & 2;
    int total = (int)truncf(fraction + ratio * count);
    if (total + pmax > TEMP || total < 0 || ratio != ratio) { // más muestras de las que caben: silencio
        memset(out, 0, (size_t)count * sizeof(float));
        return;
    }
    int per = ratio > 0 ? (int)fminf(truncf((TEMP - fraction) / ratio), (float)count) : count;
    if (per < 1) per = 1;
    for (int i = 0; i < count;) {
        int ti = 0;
        if (!skip) memcpy(temp, st->pitch, (size_t)pmax * 2), ti = pmax;
        int n = count - i < per ? count - i : per;
        int need = (int)truncf(fraction + ratio * n), y = 0;
        while (y < need) {
            if (wbi >= AR_ONDAS) wbi = 0, played = 0;
            if (!st->valid[wbi]) break; // sin datos: lo que falta es silencio
            const DspWave *w = &waves[wbi];
            if (offset == 0 && v->format == FMT_ADPCM && w->ctx) memcpy(st->loop_ctx, (const void *)(uintptr_t)w->ctx, 6);
            int s0 = (int)w->start, s1 = (int)w->end;
            if (st->loop_count > 0 && w->loop_start && w->loop_end && w->loop_start <= w->loop_end) s0 = (int)w->loop_start, s1 = (int)w->loop_end;
            int dec;
            if (v->format == FMT_ADPCM)
                dec = adpcm_decode(temp + ti + y, w->buf ? (const uint8_t *)(uintptr_t)w->buf : NULL, w->buf_size, s0, s1, offset,
                                   need - y, coef ? (const int16_t *)(uintptr_t)coef : NULL, (int)(coef_size / 2), st->loop_ctx);
            else if (v->format == FMT_PCM8 || v->format == FMT_PCM16 || v->format == FMT_PCM32 || v->format == FMT_FLOAT)
                dec = pcm_decode(temp + ti + y, w->buf, w->buf_size, v->format, s0, s1, offset, need - y, ch, nch);
            else dec = -1;
            if (dec < 0) end_of_buffer(st, w, &wbi, &consumed, &played), dec = 0;
            y += dec, offset += dec, played += (uint32_t)dec;
            if (offset >= s1 - s0 || dec == 0) {
                offset = 0;
                if (w->loop) {
                    st->loop_count++;
                    if (w->loop_count >= 0 && (dec == 0 || st->loop_count > w->loop_count)) end_of_buffer(st, w, &wbi, &consumed, &played);
                    if (dec == 0) break;
                    if (v->decoding & 1) played = 0;
                } else end_of_buffer(st, w, &wbi, &consumed, &played);
            }
        }
        if (skip) {
            for (int j = 0; j < n; j++) out[i + j] = j < y ? temp[j] : 0;
        } else {
            memset(temp + ti + y, 0, (size_t)(need - y) * 2);
            resample(out + i, temp, ratio, &fraction, n, quality);
            memcpy(st->pitch, temp + need, (size_t)pmax * 2);
        }
        i += n;
    }
    st->consumed = consumed, st->offset = offset, st->played = played, st->wb_index = (uint32_t)wbi, st->fraction = fraction;
}

// ---------- filtros biquad (BiquadFilterHelper) ----------
static void biquad(const Biquad *p, BqState *s, float *out, const float *in, int n) { // forma directa II transpuesta
    for (int i = 0; i < n; i++) {
        float x = in[i], y = x * p->num[0] + s->s[0];
        s->s[0] = x * p->num[1] + y * p->den[0] + s->s[1];
        s->s[1] = x * p->num[2] + y * p->den[1];
        out[i] = y;
    }
}

static float df1(const Biquad *p, BqState *s, float x) { // forma directa I (multi-tap y biquad con mezcla)
    float y = x * p->num[0] + s->s[0] * p->num[1] + s->s[1] * p->num[2] + s->s[2] * p->den[0] + s->s[3] * p->den[1];
    s->s[1] = s->s[0], s->s[0] = x, s->s[3] = s->s[2], s->s[2] = y;
    return y;
}

// BiquadFilterAndMix / MultiTapBiquadFilterAndMix: filtra y suma a la salida, con o sin rampa.
static float biquad_mix(const Biquad *p0, const Biquad *p1, BqState *s0, BqState *s1, float *out, const float *in, int n,
                        float v0, float v1, int ramp) {
    float vol = ramp ? v0 : v1, step = ramp ? (v1 - v0) / n : 0, last = 0;
    for (int i = 0; i < n; i++) {
        float y = df1(p0, s0, in[i]);
        if (p1) y = df1(p1, s1, y);
        last = mul_round(y, vol);
        out[i] += last;
        vol += step;
    }
    return last;
}

// ---------- comandos simples ----------
static float mix_ramp(float *out, const float *in, int n, float v0, float v1) {
    float ramp = (v1 - v0) / n, vol = v0, last = 0;
    for (int i = 0; i < n; i++) {
        last = mul_round(in[i], vol);
        out[i] += last;
        vol += ramp;
    }
    return last;
}

static void mix(Renderer *R, uint32_t in, uint32_t out, float vol) {
    float *a = buf(R, in), *b = buf(R, out);
    if (a && b)
        for (uint32_t i = 0; i < R->samples; i++) b[i] += mul_round(a[i], vol);
}

static void copy_buf(Renderer *R, uint32_t in, uint32_t out) {
    float *a = buf(R, in), *b = buf(R, out);
    if (a && b && a != b) memcpy(b, a, R->samples * sizeof(float));
}

static void depop_prepare(Renderer *R, VoiceState *st, uint32_t count, uint32_t offset) {
    for (uint32_t i = 0; i < count && i < AR_BUFS; i++)
        if (st->last[i] != 0) {
            if (offset + i < R->nbufs) R->depop[offset + i] += st->last[i];
            st->last[i] = 0;
        }
}

// DepopForMixBuffers: lo que quedó de una voz cortada decae exponencialmente sobre su mezcla.
static void depop_mix(Renderer *R, uint32_t offset, uint32_t count, uint32_t rate) {
    float decay = rate == 48000 ? 0.962189f : 0.943695f;
    uint32_t end = offset + count < R->nbufs ? offset + count : R->nbufs;
    for (uint32_t i = offset; i < end; i++) {
        float d = R->depop[i];
        if (d == 0) continue;
        float *b = buf(R, i), sign = d < 0 ? -1.0f : 1.0f;
        d = fabsf(d);
        for (uint32_t k = 0; k < R->samples; k++) {
            d = rintf(decay * d);
            b[k] += sign * d;
        }
        R->depop[i] = sign * d;
    }
}

// ---------- splitters ----------
static SplitDest *dest_at(Renderer *R, uint32_t splitter, int index) {
    if (splitter >= R->nsplitters) return NULL;
    int d = R->splitters[splitter].first;
    if (index > (int)R->ndests) return NULL; // una cadena con un ciclo no se recorre para siempre
    for (int i = 0; i < index && d >= 0; i++) d = R->dests[d].next;
    return d >= 0 ? &R->dests[d] : NULL;
}

static int dest_ok(Renderer *R, SplitDest *d) { return d->used && d->dest != AR_SIN_MEZCLA && d->dest >= 0 && (uint32_t)d->dest < R->nmixes; }

static BqState *dest_states(Renderer *R, SplitDest *d) {
    return R->dest_bq && d->id >= 0 && (uint32_t)d->id < R->ndests ? R->dest_bq + (size_t)d->id * 4 : NULL;
}

// Biquad del destino de un splitter (REV12+): con un filtro, estados 0 (actual) y 1 (anterior); con
// dos, 0 y 1 actuales y 2 y 3 anteriores. El primer buffer guarda el estado, los demás lo rebobinan.
static float dest_biquad_mix(Renderer *R, SplitDest *d, uint32_t in, uint32_t out, float v0, float v1, int ramp, int first) {
    BqState *s = dest_states(R, d);
    float *a = buf(R, in), *b = buf(R, out);
    if (!s || !a || !b) return 0;
    int both = d->bq[0].enable && d->bq[1].enable, init = !d->bq_prev[0];
    const Biquad *p = d->bq[0].enable ? &d->bq[0] : &d->bq[1];
    for (int k = 0; k < (both ? 2 : 1); k++) {
        BqState *cur = &s[k], *prev = &s[both ? k + 2 : 1];
        if (init) memset(cur, 0, sizeof *cur);
        else if (first) *prev = *cur;
        else *cur = *prev;
    }
    float last = biquad_mix(both ? &d->bq[0] : p, both ? &d->bq[1] : NULL, &s[0], both ? &s[1] : NULL, b, a, (int)R->samples, v0, v1, ramp);
    if (both) d->bq_prev[0] = d->bq[0].enable, d->bq_prev[1] = d->bq[1].enable;
    else d->bq_prev[d->bq[0].enable ? 0 : 1] = p->enable;
    return last;
}

// ---------- voces ----------
static void voice_mix(Renderer *R, const float *vol, const float *prev, VoiceState *st, uint32_t offset, uint32_t count, uint32_t in) {
    float *a = buf(R, in);
    if (!a) return;
    for (uint32_t i = 0; i < count && i < AR_BUFS; i++) {
        float *b = buf(R, offset + i);
        if (vol[i] != 0 || prev[i] != 0) {
            if (b) st->last[i] = mix_ramp(b, a, (int)R->samples, prev[i], vol[i]);
        } else if (count > AR_CANALES) st->last[i] = 0; // MixRampGrouped
    }
}

// VoiceInfo.UpdateForCommandGeneration: pasa los buffers nuevos al "DSP" y resuelve los cambios de
// estado. Devuelve si la voz genera comandos este cuadro.
static int voice_prepare(Renderer *R, Voice *v) {
    VoiceState *st[AR_CANALES];
    for (uint32_t c = 0; c < v->channels; c++) st[c] = &R->vstates[v->chan[c]];
    if (v->is_new) { // ResetResources
        for (uint32_t c = 0; c < v->channels; c++) {
            memset(st[c], 0, sizeof *st[c]);
            memcpy(R->chans[v->chan[c]].prev, R->chans[v->chan[c]].mix, sizeof R->chans[0].prev);
        }
        v->prev_volume = v->volume;
        v->is_new = 0;
    }
    if (v->flush) { // FlushWaveBuffers
        uint32_t k = v->wb_index;
        for (int i = 0; i < v->flush; i++) {
            v->wb[k].sent = 1;
            for (uint32_t c = 0; c < v->channels; c++)
                if (st[c]->valid[k]) st[c]->wb_index = (st[c]->wb_index + 1) % AR_ONDAS, st[c]->consumed++, st[c]->valid[k] = 0;
            k = (k + 1) % AR_ONDAS;
        }
        v->flush = 0;
    }
    switch (v->play) {
    case PLAY_STARTED:
        for (int i = 0; i < AR_ONDAS; i++)
            if (!v->wb[i].sent) {
                for (uint32_t c = 0; c < v->channels; c++) st[c]->valid[i] = 1;
                v->wb[i].sent = 1;
            }
        v->was_playing = 0;
        for (int i = 0; i < AR_ONDAS; i++)
            if (st[0]->valid[i]) return 1;
        return 0;
    case PLAY_STOPPING:
        for (int i = 0; i < AR_ONDAS; i++) {
            v->wb[i].sent = 1;
            for (uint32_t c = 0; c < v->channels; c++) {
                if (st[c]->valid[i]) st[c]->wb_index = (st[c]->wb_index + 1) % AR_ONDAS, st[c]->consumed++;
                st[c]->valid[i] = 0;
            }
        }
        for (uint32_t c = 0; c < v->channels; c++) {
            st[c]->offset = 0, st[c]->played = 0, st[c]->fraction = 0;
            memset(st[c]->pitch, 0, sizeof st[c]->pitch);
            memset(st[c]->loop_ctx, 0, sizeof st[c]->loop_ctx);
        }
        v->play = PLAY_STOPPED;
        return v->was_playing = v->prev_play == PLAY_STARTED;
    default: // detenida o en pausa: sus memorias siguen en uso
        for (int i = 0; i < AR_ONDAS; i++) ar_addr(R, &v->wb[i].buf, 1), ar_addr(R, &v->wb[i].ctx, 1);
        if (v->format == FMT_ADPCM && v->ds.cpu) ar_addr(R, &v->ds, 1);
        return v->was_playing = v->prev_play == PLAY_STARTED;
    }
}

static void voice_render(Renderer *R, Voice *v) {
    int v2 = ar_rev(R, 8); // WaveBuffer versión 2: repeticiones con inicio y fin propios, calidad de remuestreo
    DspWave waves[AR_ONDAS];
    for (int i = 0; i < AR_ONDAS; i++) {
        WaveBuf *w = &v->wb[i];
        waves[i] = (DspWave){ar_addr(R, &w->buf, 1), w->buf.size, w->ctx.cpu ? ar_addr(R, &w->ctx, 1) : 0, w->start, w->end,
                             v2 ? w->loop_start : 0, v2 ? w->loop_end : 0, v2 ? w->loop_count : -1, w->loop, w->eos};
    }
    uint64_t coef = 0, coef_size = 0;
    if (v->format == FMT_ADPCM) coef = ar_addr(R, &v->ds, 1), coef_size = v->ds.size;
    uint32_t mb = R->mix_buffers;
    for (uint32_t c = 0; c < v->channels; c++) {
        VoiceState *st = &R->vstates[v->chan[c]];
        ChannelRes *cr = &R->chans[v->chan[c]];
        float *vb = buf(R, mb + c);
        // depop de lo que dejó la voz si la cortaron, y la lectura de sus muestras
        if (v->mix != AR_SIN_MEZCLA) {
            if ((uint32_t)v->mix < R->nmixes && v->was_playing) depop_prepare(R, st, R->mixes[v->mix].buffers, R->mixes[v->mix].offset);
        } else if (v->splitter != AR_SIN_SPLITTER) {
            SplitDest *d;
            for (int k = 0; (d = dest_at(R, v->splitter, k)); k++)
                if (dest_ok(R, d)) {
                    if (v->was_playing) depop_prepare(R, st, R->mixes[d->dest].buffers, R->mixes[d->dest].offset);
                    d->need_update = 1;
                }
        }
        if (!v->was_playing && vb)
            process_waves(R, vb, v, waves, st, v->format == FMT_ADPCM && !v2 ? 0 : (int)c, v->format == FMT_ADPCM && !v2 ? 1 : (int)v->channels,
                          coef, coef_size, v2 ? v->src_quality : SRC_DEFAULT);
        if (v->was_playing) v->prev_volume = 0;
        else if ((v->mix != AR_SIN_MEZCLA || v->splitter != AR_SIN_SPLITTER) && vb) {
            // filtros de la voz: dos encadenados (REV10+, forma directa I) o uno por uno
            if (ar_rev(R, 10) && v->bq[0].enable && v->bq[1].enable) {
                for (int i = 0; i < 2; i++)
                    if (!v->bq_init[i]) memset(&st->bq[i], 0, sizeof st->bq[i]);
                for (uint32_t k = 0; k < R->samples; k++) vb[k] = df1(&v->bq[1], &st->bq[1], df1(&v->bq[0], &st->bq[0], vb[k]));
            } else
                for (int i = 0; i < 2; i++)
                    if (v->bq[i].enable) {
                        if (!v->bq_init[i]) memset(&st->bq[i], 0, sizeof st->bq[i]);
                        biquad(&v->bq[i], &st->bq[i], vb, vb, (int)R->samples);
                    }
            // rampa del volumen de la voz (el anterior pasa a ser el actual desde el primer canal)
            float vol = v->prev_volume, ramp = (v->volume - v->prev_volume) / (float)R->samples;
            for (uint32_t k = 0; k < R->samples; k++) vb[k] = mul_round(vb[k], vol), vol += ramp;
            v->prev_volume = v->volume;
            if (v->mix == AR_SIN_MEZCLA) {
                SplitDest *d;
                for (int k = (int)c; (d = dest_at(R, v->splitter, k)); k += (int)v->channels) {
                    if (!dest_ok(R, d)) continue;
                    Mix *m = &R->mixes[d->dest];
                    if (d->bq[0].enable || d->bq[1].enable) {
                        int first = 1;
                        for (uint32_t i = 0; i < m->buffers && i < AR_BUFS; i++)
                            if (d->mix[i] != 0 || d->prev[i] != 0) {
                                st->last[i] = dest_biquad_mix(R, d, mb + c, m->offset + i, d->prev[i], d->mix[i], 1, first);
                                first = 0;
                            }
                    } else voice_mix(R, d->mix, d->prev, st, m->offset, m->buffers, mb + c);
                    d->need_update = 1;
                }
            } else if ((uint32_t)v->mix < R->nmixes) {
                Mix *m = &R->mixes[v->mix];
                voice_mix(R, cr->mix, cr->prev, st, m->offset, m->buffers, mb + c);
                memcpy(cr->prev, cr->mix, sizeof cr->prev);
            }
            for (int i = 0; i < 2; i++) v->bq_init[i] = v->bq[i].enable;
        }
    }
}

// ---------- efectos ----------
static uint32_t u32p(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float f32p(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }

// AuxiliaryBufferCommand: manda la mezcla al juego por un buffer circular (send) y la recibe de otro
// (return). Cada buffer empieza con dos AuxiliaryBufferInfo (del CPU y del DSP, 0x40 cada una): el
// renderizador mueve el puntero de escritura del send y el de lectura del return.
static void aux_frame(Renderer *R, Effect *e, uint32_t in_i, uint32_t out_i, uint32_t count_max, uint32_t update, uint32_t write_off) {
    float *in = buf(R, in_i), *out = buf(R, out_i);
    if (!in || !out) return;
    uint32_t n = R->samples;
    if (e->enabled) {
        for (uint32_t k = 0; k < n; k++) in[k] = (float)(int32_t)lrintf(in[k]); // a enteros, como los lee el juego
        if (count_max && e->send_base) { // escritura en el send
            uint32_t pos = write_off + rd32(e->send_info + 4);
            if (pos <= count_max) {
                for (uint32_t k = 0; k < n; k++, pos = (pos + 1) % count_max) wr32(e->send_base + (uint64_t)(pos % count_max) * 4, (uint32_t)(int32_t)in[k]);
                if (update) wr32(e->send_info + 4, (rd32(e->send_info + 4) + update) % count_max);
            }
        }
        uint32_t got = 0;
        if (count_max && e->ret_base) { // lectura del return
            uint32_t pos = write_off + rd32(e->ret_info);
            if (pos <= count_max) {
                for (uint32_t k = 0; k < n; k++, pos = (pos + 1) % count_max) out[k] = (float)(int32_t)rd32(e->ret_base + (uint64_t)(pos % count_max) * 4);
                if (update) wr32(e->ret_info, (rd32(e->ret_info) + update) % count_max);
                got = n;
            }
        }
        for (uint32_t k = got; k < n; k++) out[k] = 0;
    } else {
        uint64_t z = 0;
        memcpy((void *)(uintptr_t)e->send_info, &z, 8), wr32(e->send_info + 12, 0);
        memcpy((void *)(uintptr_t)e->ret_info, &z, 8), wr32(e->ret_info + 12, 0);
        copy_buf(R, in_i, out_i);
    }
}

// CaptureBufferCommand: como el send del AUX, con la cuenta total y la de muestras perdidas.
static void capture_frame(Renderer *R, Effect *e, uint32_t in_i, uint32_t count_max, uint32_t update, uint32_t write_off) {
    float *in = buf(R, in_i);
    uint64_t cpu = e->send_info, dsp = e->send_info + 0x40;
    if (!in) return;
    if (!e->enabled) {
        uint64_t z = 0;
        memcpy((void *)(uintptr_t)dsp, &z, 8), wr32(dsp + 12, 0);
        return;
    }
    for (uint32_t k = 0; k < R->samples; k++) in[k] = (float)(int32_t)lrintf(in[k]);
    if (!count_max || !e->send_base) return;
    uint32_t pos = write_off + rd32(dsp + 4);
    if (pos > count_max) return;
    for (uint32_t k = 0; k < R->samples; k++, pos = (pos + 1) % count_max) wr32(e->send_base + (uint64_t)(pos % count_max) * 4, (uint32_t)(int32_t)in[k]);
    if (!update) return;
    uint32_t diff = rd32(dsp + 12) - rd32(cpu + 12);
    if (diff >= count_max) {
        uint32_t ldiff = rd32(dsp + 8) - rd32(cpu + 8), nl = ldiff + update;
        if (ldiff > nl) nl = rd32(cpu + 8) - 1;
        wr32(dsp + 8, nl);
    }
    uint32_t nw = (rd32(dsp + 4) + update) % count_max;
    wr32(dsp + 4, nw);
    wr32(dsp + 12, diff + nw);
}

// LimiterCommandVersion1/2: detector y ganancia con promedios exponenciales y una línea de demora.
static void limiter_frame(Renderer *R, Effect *e, uint32_t offset) {
    const uint8_t *p = e->param;
    int nch = p[0xE] | p[0xF] << 8, valid = nch == 1 || nch == 2 || nch == 4 || nch == 6;
    int dmax = (int32_t)u32p(p + 0x3C), dmin = (int32_t)u32p(p + 0x38);
    if (e->enabled && valid && (p[0x40] == USO_INVALID || !e->delay)) { // estado nuevo
        free(e->delay);
        e->delay_max = dmax > 0 ? dmax : 1;
        e->delay = calloc((size_t)nch * (size_t)e->delay_max, sizeof(float));
        for (int c = 0; c < AR_CANALES; c++) e->delay_pos[c] = 0, e->detector[c] = 0, e->gain[c] = 1;
    }
    if (!e->enabled || !valid || !e->delay) {
        for (int c = 0; c < nch && c < AR_CANALES; c++) copy_buf(R, offset + p[c], offset + p[6 + c]);
        return;
    }
    float in_gain = f32p(p + 0x30), out_gain = f32p(p + 0x34), thr = f32p(p + 0x2C), att = f32p(p + 0x24), rel = f32p(p + 0x28);
    int stats = ar_rev(R, 9) && p[0x41];
    float *imax = (float *)e->result_dsp, *gmin = (float *)e->result_dsp + 6;
    if (stats && p[0x42])
        for (int c = 0; c < 6; c++) imax[c] = 0, gmin[c] = 1;
    for (int c = 0; c < nch; c++) {
        float *in = buf(R, offset + p[c]), *out = buf(R, offset + p[6 + c]);
        if (!in || !out) continue;
        for (uint32_t k = 0; k < R->samples; k++) {
            float x = in[k] / 32767.0f * in_gain, ax = fabsf(x);
            e->detector[c] += (ax > e->detector[c] ? att : rel) * (ax - e->detector[c]);
            float a = e->detector[c] > thr ? thr / e->detector[c] : 1.0f;
            e->gain[c] += (e->gain[c] > a ? att : rel) * (a - e->gain[c]);
            float *d = &e->delay[c * e->delay_max + e->delay_pos[c]];
            out[k] = *d * e->gain[c] * out_gain * 32767.0f;
            *d = x;
            e->delay_pos[c]++;
            if (dmin > 0)
                while (e->delay_pos[c] >= dmin) e->delay_pos[c] -= dmin;
            if (e->delay_pos[c] >= e->delay_max) e->delay_pos[c] = 0;
            if (stats) {
                if (ax > imax[c]) imax[c] = ax;
                if (e->gain[c] < gmin[c]) gmin[c] = e->gain[c];
            }
        }
    }
}

static void effect_render(Renderer *R, Mix *m, Effect *e) {
    uint32_t off = m->offset;
    const uint8_t *p = e->param;
    switch (e->type) {
    case FX_BUFFER_MIX: // BufferMixParameter: entradas, salidas, volúmenes y cantidad
        if (e->enabled)
            for (uint32_t i = 0; i < u32p(p + 0x90) && i < AR_BUFS; i++)
                if (f32p(p + 0x30 + 4 * i) != 0) mix(R, off + p[i], off + p[0x18 + i], f32p(p + 0x30 + 4 * i));
        break;
    case FX_AUX: case FX_CAPTURE: { // AuxiliaryBufferParameter: los canales van uno detrás de otro
        if (e->enabled) {
            ar_addr(R, &e->work[0], 1);
            if (e->type == FX_AUX) ar_addr(R, &e->work[1], 1);
        }
        if (!e->send_base || (e->type == FX_AUX && !e->ret_base)) break;
        uint32_t nch = u32p(p + 0x30), wo = 0;
        for (uint32_t c = 0; c < nch && c < AR_BUFS; c++) {
            uint32_t next = wo + R->samples;
            if (e->type == FX_AUX) aux_frame(R, e, off + p[c], off + p[0x18 + c], u32p(p + 0x38), c + 1 == nch ? next : 0, wo);
            else capture_frame(R, e, off + p[c], u32p(p + 0x38), c + 1 == nch ? next : 0, wo);
            wo = next;
        }
        break;
    }
    case FX_BIQUAD: { // parámetro en float: entradas, salidas, numerador, denominador, canales, estado
        int nch = p[0x24];
        if (e->enabled) {
            int init = p[0x25] == USO_INVALID || (p[0x25] == USO_NEW && !ar_rev(R, 6));
            Biquad q = {1, {f32p(p + 0x10), f32p(p + 0x14), f32p(p + 0x18)}, {f32p(p + 0x1C), f32p(p + 0x20)}};
            for (int c = 0; c < nch && c < AR_CANALES; c++) {
                float *in = buf(R, off + p[c]), *out = buf(R, off + p[6 + c]);
                if (init) memset(&e->bq[c], 0, sizeof e->bq[c]);
                if (in && out) biquad(&q, &e->bq[c], out, in, (int)R->samples);
            }
        } else
            for (int c = 0; c < nch && c < AR_CANALES; c++) copy_buf(R, off + p[c], off + p[6 + c]);
        break;
    }
    case FX_LIMITER:
        if (e->enabled) ar_addr(R, &e->work[0], 1);
        limiter_frame(R, e, off);
        break;
    case FX_DELAY: case FX_REVERB: case FX_REVERB3D: case FX_COMPRESSOR: { // dejan pasar la señal
        if (e->enabled && e->type != FX_COMPRESSOR) ar_addr(R, &e->work[0], 1);
        int nch = p[0xE] | p[0xF] << 8;
        for (int c = 0; c < nch && c < AR_CANALES; c++) copy_buf(R, off + p[c], off + p[6 + c]);
        break;
    }
    }
    // UpdateForCommandGeneration: el estado interno del parámetro pasa a "activo"
    e->usage = e->enabled ? USO_ENABLED : USO_DISABLED;
    switch (e->type) {
    case FX_BIQUAD: e->param[0x25] = USO_ENABLED; break;
    case FX_DELAY: e->param[0x34] = USO_ENABLED; break;
    case FX_REVERB: e->param[0x40] = USO_ENABLED; break;
    case FX_REVERB3D: e->param[0x48] = USO_ENABLED; break;
    case FX_LIMITER: e->param[0x40] = USO_ENABLED, e->param[0x42] = 0; break;
    case FX_COMPRESSOR: e->param[0x34] = USO_ENABLED, e->param[0x37] = 0; break;
    }
}

static void effects_render(Renderer *R, Mix *m) {
    for (uint32_t i = 0; i < R->neffects; i++) {
        int k = m->order[i];
        if (k < 0) break;
        if ((uint32_t)k < R->neffects && !R->effects[k].unmapped) effect_render(R, m, &R->effects[k]);
    }
}

// ---------- mezclas ----------
static void mix_render(Renderer *R, Mix *m) {
    if (m->dest_mix == AR_SIN_MEZCLA) {
        if (m->dest_splitter == AR_SIN_SPLITTER || !m->buffers) return;
        SplitDest *d;
        for (int k = 0; (d = dest_at(R, m->dest_splitter, k)); k++) {
            if (!dest_ok(R, d)) continue;
            Mix *t = &R->mixes[d->dest];
            uint32_t in = m->offset + (uint32_t)k % m->buffers;
            int first = 1;
            for (uint32_t o = 0; o < t->buffers && o < AR_BUFS; o++) {
                float vol = m->volume * d->mix[o];
                if (vol == 0) continue;
                if (d->bq[0].enable || d->bq[1].enable) dest_biquad_mix(R, d, in, t->offset + o, 0, vol, 0, first), first = 0;
                else mix(R, in, t->offset + o, vol);
            }
        }
    } else if ((uint32_t)m->dest_mix < R->nmixes) {
        Mix *t = &R->mixes[m->dest_mix];
        for (uint32_t i = 0; i < m->buffers && i < AR_BUFS; i++)
            for (uint32_t o = 0; o < t->buffers && o < AR_BUFS; o++) {
                float vol = m->volume * m->vol[i * AR_BUFS + o];
                if (vol != 0) mix(R, m->offset + i, t->offset + o, vol);
            }
    }
}

// ---------- sinks ----------
// UpsamplerHelper: polifase con sinc y ventana de Blackman, 20 muestras de historia.
static float up_bank[6][20];
static pthread_once_t up_once = PTHREAD_ONCE_INIT;
static void up_banks_init(void) {
    for (int b = 0; b < 6; b++)
        for (int i = 0; i < 20; i++) {
            float x = (float)(9 - i) + b / 6.0f, w = x / 20.0f + 0.5f;
            float sinc = x == 0 ? 1.0f : sinf((float)M_PI * x) / ((float)M_PI * x);
            up_bank[b][i] = sinc * (0.41f - 0.5f * cosf(2 * (float)M_PI * w) + 0.09f * cosf(4 * (float)M_PI * w));
        }
}

static void upsample(Sink *s, int c, float *out, const float *in, int nin) {
    __typeof__(s->up[0]) *u = &s->up[c];
    if (!u->init) u->scale = nin == 40 ? 6.0f : nin == 80 ? 3.0f : 1.5f, u->init = 1;
    int ii = 0;
    for (int i = 0; i < AR_SALIDA; i++) {
        int phases = u->scale == 1.5f ? 3 : (int)u->scale, bank = 0, push = u->phase == 0;
        if (u->scale == 6.0f) bank = u->phase;
        else if (u->scale == 3.0f) bank = u->phase * 2;
        else bank = u->phase == 1 ? 4 : u->phase == 2 ? 2 : 0, push |= u->phase == 2;
        if (push && ii < nin) memmove(u->hist, u->hist + 1, 19 * sizeof(float)), u->hist[19] = in[ii++];
        if (u->phase == 0) out[i] = u->hist[9];
        else {
            float r = 0;
            for (int k = 0; k < 20; k++) r += up_bank[bank][k] * u->hist[k];
            out[i] = r;
        }
        u->phase = (u->phase + 1) % phases;
    }
}

static int16_t saturate(float x) { return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }

static void device_sink(Renderer *R, Sink *s, uint32_t offset, int *wrote) {
    const uint8_t *p = s->param;
    uint32_t nin = u32p(p + 0x100);
    if (nin > AR_CANALES) nin = AR_CANALES;
    if (!R->dev_channels) {
        nx_audio_open(48000, nin == 6 ? 6 : 2);
        R->dev_channels = nx_audio_channels();
    }
    int dev = R->dev_channels;
    uint32_t idx[AR_CANALES];
    for (uint32_t i = 0; i < AR_CANALES; i++) idx[i] = offset + p[0x104 + i];
    // de 5.1 a estéreo: con los coeficientes del juego o con los de Ryujinx (1, 0,707, 0,251, 0,707)
    if (dev == 2 && nin == 6) {
        static const float def[4] = {1.0f, 0.707f, 0.251f, 0.707f};
        const float *k = p[0x10B] ? s->downmix : def;
        float *fl = buf(R, idx[0]), *fr = buf(R, idx[1]), *fc = buf(R, idx[2]), *lfe = buf(R, idx[3]), *bl = buf(R, idx[4]), *br = buf(R, idx[5]);
        if (fl && fr && fc && lfe && bl && br) {
            for (uint32_t i = 0; i < R->samples; i++) {
                float l = rintf(k[3] * bl[i] + k[2] * lfe[i] + k[1] * fc[i] + k[0] * fl[i]);
                float r = rintf(k[3] * br[i] + k[2] * lfe[i] + k[1] * fc[i] + k[0] * fr[i]);
                fl[i] = l, fr[i] = r;
            }
            for (int c = 2; c < 6; c++) memset(buf(R, idx[c]), 0, R->samples * sizeof(float));
        }
    }
    const float *src[AR_CANALES] = {0};
    for (uint32_t c = 0; c < nin; c++) {
        float *b = buf(R, idx[c]);
        if (b && R->rate != 48000) { // UpsampleCommand
            pthread_once(&up_once, up_banks_init);
            upsample(s, (int)c, s->upbuf[c], b, (int)R->samples);
            src[c] = s->upbuf[c];
        } else if (b && R->samples == AR_SALIDA) src[c] = b;
    }
    if (*wrote) return; // un solo sink de dispositivo por cuadro llega a la salida
    float vol = ar_device_volume();
    for (int c = 0; c < dev; c++) {
        const float *b = nin == 1 ? src[0] : (uint32_t)c < nin ? src[c] : NULL; // un sink mono suena en los dos lados
        for (int i = 0; i < AR_SALIDA; i++) R->out[i * dev + c] = b ? saturate(vol == 1.0f ? b[i] : b[i] * vol) : 0;
    }
    *wrote = dev;
}

// CircularBufferSinkCommand: cada canal, un bloque de muestras de 16 bits en el buffer del juego.
static void circular_sink(Renderer *R, Sink *s, uint32_t offset) {
    const uint8_t *p = s->param;
    uint32_t nin = u32p(p + 0xC), size = u32p(p + 8), cur = s->write_off;
    uint64_t base = ar_addr(R, &s->circ, 1);
    if (base && size)
        for (uint32_t c = 0; c < nin && c < AR_CANALES; c++) {
            float *b = buf(R, offset + p[0x1C + c]);
            if (b)
                for (uint32_t i = 0; i < R->samples; i++) {
                    int16_t v = saturate(b[i]);
                    memcpy((void *)(uintptr_t)(base + cur + (uint64_t)i * 2), &v, 2);
                }
            cur += R->samples * 2;
            if (cur >= size) cur = 0;
        }
    uint32_t frame = 2 * u32p(p + 0x10) * nin; // UpdateForCommandGeneration
    s->last_written = s->old_written, s->old_written = s->write_off;
    s->write_off += frame;
    if (size) s->write_off %= size;
}

// ---------- un cuadro ----------
int ar_frame(Renderer *R) {
    for (uint32_t i = 0; i < R->npools; i++) R->pools[i].used = 0;
    // rendimiento: se guarda la cabecera del cuadro (sin entradas: los tiempos no se miden)
    if (R->perf_frames) {
        uint32_t n = R->perf_frames < 64 ? R->perf_frames : 64, k = R->perf_w;
        R->perf_w = (R->perf_w + 1) % n;
        R->perf_idx[k] = R->perf_index++;
        R->perf_start[k] = nx_now_ns() * 12 / 625; // ticks de 19,2 MHz
    }
    memset(R->buf, 0, (size_t)R->nbufs * R->samples * sizeof(float));
    // voces, por prioridad y orden (orden estable)
    for (uint32_t i = 0; i < R->nvoices; i++) {
        int32_t k = (int32_t)i, j = (int32_t)i - 1;
        Voice *a = &R->voices[k];
        while (j >= 0) {
            Voice *b = &R->voices[R->sorted_voices[j]];
            if (b->priority < a->priority || (b->priority == a->priority && b->sorting <= a->sorting)) break;
            R->sorted_voices[j + 1] = R->sorted_voices[j];
            j--;
        }
        R->sorted_voices[j + 1] = k;
    }
    for (uint32_t i = 0; i < R->nvoices; i++) {
        Voice *v = &R->voices[R->sorted_voices[i]];
        if (!v->in_use || !v->wb_count || v->ds_unmapped || v->buf_unmapped || v->drop || !v->channels) continue;
        if (voice_prepare(R, v)) voice_render(R, v);
    }
    for (uint32_t i = 0; i < R->nsplitters; i++) // UpdateInternalState
        for (int d = R->splitters[i].first, n = 0; d >= 0 && n <= (int)R->ndests; d = R->dests[d].next, n++) {
            SplitDest *x = &R->dests[d];
            if (x->used && x->need_update) memcpy(x->prev, x->mix, sizeof x->prev);
            x->need_update = 0;
        }
    // submezclas en orden, la mezcla final y los sinks
    for (uint32_t i = 0; i < R->nmixes; i++) {
        Mix *m = &R->mixes[R->sorted_mixes[i]];
        if (!m->used || m->id == 0) continue;
        depop_mix(R, m->offset, m->buffers, m->rate);
        effects_render(R, m);
        mix_render(R, m);
    }
    Mix *fin = &R->mixes[0];
    depop_mix(R, fin->offset, fin->buffers, fin->rate);
    effects_render(R, fin);
    if (fin->volume != 1.0f)
        for (uint32_t i = 0; i < fin->buffers && i < AR_BUFS; i++) {
            float *b = buf(R, fin->offset + i);
            if (b)
                for (uint32_t k = 0; k < R->samples; k++) b[k] = mul_round(b[k], fin->volume);
        }
    int wrote = 0;
    for (uint32_t i = 0; i < R->nsinks; i++) {
        Sink *s = &R->sinks[i];
        if (!s->used || s->skip) continue;
        if (s->type == SINK_DEVICE) device_sink(R, s, fin->offset, &wrote);
        else if (s->type == SINK_CIRCULAR) circular_sink(R, s, fin->offset);
    }
    if (ar_rev(R, 9))
        for (uint32_t i = 0; i < R->neffects; i++)
            if (R->effects[i].type == FX_LIMITER || R->effects[i].type == FX_COMPRESSOR) memcpy(R->effects[i].result, R->effects[i].result_dsp, 0x80);
    R->elapsed++;
    return wrote;
}
