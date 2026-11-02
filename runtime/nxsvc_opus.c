// hwopus: el decodificador Opus "por hardware" de la consola. Los juegos le mandan paquetes con una
// cabecera de 8 bytes (largo y "final range", big endian) y reciben PCM de 16 bits intercalado. Aquí lo
// hace libopus. Tears of the Kingdom y los juegos con sonido de CRI lo usan (nxnative). La estructura
// de los comandos sigue a Ryujinx (Ryujinx.Horizon/Sdk/Codec, MIT); la memoria de trabajo que manda el
// juego no se usa.
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxrt.h"
#include "nxsvc.h"

#define OK 0u
#define R_LARGO 0xE06Fu    // codec 111, 7: largo inválido
#define R_CANALES 0x206Fu  // codec 111, 16: cantidad de canales inválida
#define R_FRECUENCIA 0x206Fu
#define R_PAQUETE 0x1006Fu // codec 111, 128: paquete Opus inválido

typedef struct {
    OpusDecoder *d;
    OpusMSDecoder *ms;
    int canales, frecuencia;
} Decodificador;

static int frecuencia_valida(int f) { return f == 8000 || f == 12000 || f == 16000 || f == 24000 || f == 48000; }

static uint32_t decodificar(Decodificador *o, NxRequest *r, int con_tiempo, int reinicio) {
    NxBuffer *in = nx_in_buffer(r, 0), *out = nx_out_buffer(r, 0);
    if (!in || !out || in->size < 8) return R_LARGO;
    const uint8_t *p = (const uint8_t *)(uintptr_t)in->addr;
    uint32_t largo = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    if (largo + 8ull > in->size) return R_LARGO;
    int muestras = opus_packet_get_nb_samples(p + 8, (opus_int32)largo, o->frecuencia);
    if (muestras < 0) return R_PAQUETE;
    if ((uint64_t)muestras * o->canales * 2 > out->size) return R_LARGO;
    if (reinicio) {
        if (o->d) opus_decoder_ctl(o->d, OPUS_RESET_STATE);
        if (o->ms) opus_multistream_decoder_ctl(o->ms, OPUS_RESET_STATE);
    }
    nx_vigia_tocar(out->addr, out->size);
    opus_int16 *pcm = (opus_int16 *)(uintptr_t)out->addr;
    int n = o->ms ? opus_multistream_decode(o->ms, p + 8, (opus_int32)largo, pcm, muestras, 0)
                  : opus_decode(o->d, p + 8, (opus_int32)largo, pcm, muestras, 0);
    if (n < 0) return n == OPUS_BUFFER_TOO_SMALL ? R_LARGO : R_PAQUETE;
    nx_out32(r, largo + 8); // consumido
    nx_out32(r, (uint32_t)n); // muestras por canal
    if (con_tiempo) nx_out64(r, 0); // tiempo del DSP
    return OK;
}

// IHardwareOpusDecoder: 0 y 2 sin tiempo; 4 y 5 con tiempo; 6 y 7 además reinician; 8 y 9 (7.0.0+)
// llevan el reinicio como parámetro.
static uint32_t decodificador(ServiceObject *self, NxRequest *r) {
    Decodificador *o = self->state;
    switch (r->cmd) {
    case 0: case 2: return decodificar(o, r, 0, 0);
    case 1: case 3: return OK; // SetContext
    case 4: case 5: return decodificar(o, r, 1, 0);
    case 6: case 7: case 8: case 9: return decodificar(o, r, 1, r->in_size ? r->in[0] & 1 : 0);
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t abrir(NxRequest *r, int frecuencia, int canales, int flujos, int estereo, const uint8_t *mapa) {
    if (!frecuencia_valida(frecuencia)) return R_FRECUENCIA;
    Decodificador *o = calloc(1, sizeof *o);
    int err = 0;
    o->canales = canales, o->frecuencia = frecuencia;
    if (flujos) {
        if (canales < 1 || canales > 255) return free(o), R_CANALES;
        o->ms = opus_multistream_decoder_create(frecuencia, canales, flujos, estereo, mapa, &err);
    } else {
        if (canales < 1 || canales > 2) return free(o), R_CANALES;
        o->d = opus_decoder_create(frecuencia, canales, &err);
    }
    if (err != OPUS_OK) return free(o), R_CANALES;
    nx_out_object(r, nx_object("IHardwareOpusDecoder", decodificador, o));
    return OK;
}

static uint32_t tamano(int canales, int flujos, int estereo) {
    int n = flujos ? opus_multistream_decoder_get_size(flujos, estereo) : opus_decoder_get_size(canales);
    return (uint32_t)(n > 0 ? n : 0x4000) + 0x8000 + (uint32_t)canales * 1920 * 2; // de sobra: no se usa
}

// Parámetros de varios flujos (0x110, o 0x118 con banderas): frecuencia, canales, flujos, flujos estéreo,
// [banderas, reservado], y el mapa de canales; llegan en un buffer.
static const uint8_t *multiflujo(NxRequest *r, int ex, int *f, int *c, int *s, int *e) {
    NxBuffer *b = nx_in_buffer(r, 0);
    if (!b || b->size < (ex ? 0x118u : 0x110u)) return NULL;
    const int32_t *v = (const int32_t *)(uintptr_t)b->addr;
    *f = v[0], *c = v[1], *s = v[2], *e = v[3];
    return (const uint8_t *)(uintptr_t)b->addr + (ex ? 0x18 : 0x10);
}

uint32_t nx_hwopus(ServiceObject *self, NxRequest *r) {
    (void)self;
    int f, c, s, e;
    const uint8_t *mapa;
    switch (r->cmd) {
    case 0: case 4: return abrir(r, (int)nx_in32(r, 0), (int)nx_in32(r, 4), 0, 0, NULL); // Open(Ex)
    case 1: case 5: case 8: nx_out32(r, tamano((int)nx_in32(r, 4), 0, 0)); return OK; // GetWorkBufferSize(Ex, ExEx)
    case 2: case 6: // OpenForMultiStream(Ex)
        if (!(mapa = multiflujo(r, r->cmd == 6, &f, &c, &s, &e))) return R_LARGO;
        return abrir(r, f, c, s, e, mapa);
    case 3: case 7: case 9: // GetWorkBufferSizeForMultiStream(Ex, ExEx)
        if (!multiflujo(r, r->cmd != 3, &f, &c, &s, &e)) return R_LARGO;
        nx_out32(r, tamano(c, s, e));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}
