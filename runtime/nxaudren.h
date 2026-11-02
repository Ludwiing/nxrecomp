// El renderizador de audio (audren): estado del servidor, compartido entre nxsvc_audren.c (IPC y
// lectura de cada actualización) y nxaudren.c (generar y ejecutar los comandos de cada cuadro).
// Sigue el servidor de Ryujinx (MIT, src/Ryujinx.Audio/Renderer/Server y Dsp), con sus estructuras,
// sus revisiones (REV1 a REV15) y su matemática. Las muestras viven como float en escala de 16 bits.
#pragma once
#include <pthread.h>
#include <stdint.h>

#include "nxkern.h"

#define AR_CANALES 6          // canales por voz y por sink (VoiceChannelCountMax)
#define AR_BUFS 24            // buffers de mezcla por mezcla (MixBufferCountMax)
#define AR_ONDAS 4            // buffers de onda por voz
#define AR_SIN_MEZCLA 0x7FFFFFFF
#define AR_SIN_SPLITTER 0xFFFFFFFFu
#define AR_SALIDA 240         // muestras por cuadro a 48 kHz
#define AR_TEMP 0x3F00        // muestras decodificadas por voz y cuadro, como máximo

#define AR_R_DEVICE_NOT_FOUND 0x299u
#define AR_R_OPERATION_FAILED 0x499u
#define AR_R_WORK_BUFFER_SMALL 0x899u
#define AR_R_INVALID_UPDATE 0x5299u
#define AR_R_INVALID_ADDRESS 0x5499u
#define AR_R_INVALID_SORTING 0x5699u
#define AR_R_UNSUPPORTED 0x40299u
#define AR_R_INVALID_EXECUTION 0x40499u

enum { FMT_INVALID, FMT_PCM8, FMT_PCM16, FMT_PCM24, FMT_PCM32, FMT_FLOAT, FMT_ADPCM };
enum { PLAY_STARTED, PLAY_STOPPED, PLAY_STOPPING, PLAY_PAUSED }; // estado de la voz en el servidor
enum { FX_INVALID, FX_BUFFER_MIX, FX_AUX, FX_DELAY, FX_REVERB, FX_REVERB3D, FX_BIQUAD, FX_LIMITER, FX_CAPTURE, FX_COMPRESSOR };
enum { USO_INVALID, USO_NEW, USO_ENABLED, USO_DISABLED };
enum { SINK_INVALID, SINK_DEVICE, SINK_CIRCULAR };
enum { SRC_DEFAULT, SRC_HIGH, SRC_LOW };

typedef struct { float s[8]; } BqState;                 // BiquadFilterState
typedef struct { uint8_t enable; float num[3], den[2]; } Biquad; // BiquadFilterParameter2

// Dirección que el juego pasa al servidor: vale solo si cae en una memoria adjunta (o si el juego
// pidió mapeo forzado). La "dirección del DSP" es la misma del juego.
typedef struct { uint64_t cpu, size; int pool; uint64_t forced; } AddrInfo;
typedef struct { uint64_t cpu, size; int mapped, used; } Pool;

typedef struct {
    AddrInfo buf, ctx;
    uint32_t start, end, loop_start, loop_end;
    int32_t loop_count;
    uint8_t loop, eos, sent;    // sent: IsSendToAudioProcessor
} WaveBuf;

typedef struct {                // VoiceState: uno por recurso de canal
    BqState bq[2];
    uint64_t played;
    int32_t offset;
    uint32_t wb_index, consumed;
    uint8_t valid[AR_ONDAS];
    int16_t pitch[8];           // historia del remuestreador
    float fraction;
    int16_t loop_ctx[3];        // ADPCM: predictor y escala, historia 0 y 1
    float last[AR_BUFS];        // últimas muestras mezcladas (para el depop)
    int32_t loop_count;
} VoiceState;

typedef struct { float mix[AR_BUFS], prev[AR_BUFS]; uint32_t id; uint8_t used; } ChannelRes;

typedef struct {
    uint8_t in_use, is_new, was_playing, format, src_quality, drop, ds_unmapped, buf_unmapped, flush;
    uint8_t bq_init[2];         // BiquadFilterNeedInitialization: el filtro estaba activo el cuadro anterior
    int play, prev_play;
    uint32_t rate, channels, priority, sorting, wb_count, wb_index, splitter;
    int32_t id, node, mix;
    float pitch, volume, prev_volume;
    Biquad bq[2];
    uint16_t decoding;          // 1: reiniciar la cuenta al repetir, 2: sin remuestreo
    AddrInfo ds;                // coeficientes ADPCM
    WaveBuf wb[AR_ONDAS];
    int32_t chan[AR_CANALES];
} Voice;

typedef struct {
    float volume;
    uint32_t rate, buffers, offset, distance, dest_splitter;
    int32_t id, node, dest_mix;
    uint8_t used, long_predelay;
    int32_t *order;             // efectos en orden de proceso (-1 = fin)
    float vol[AR_BUFS * AR_BUFS];
} Mix;

typedef struct {
    int32_t id, dest, next;     // next: siguiente destino del splitter (-1 = fin)
    float mix[AR_BUFS], prev[AR_BUFS];
    uint8_t used, need_update, bq_prev[2];
    Biquad bq[2];
} SplitDest;

typedef struct { int32_t id, count, first; uint32_t rate; uint8_t new_conn; } Splitter;

typedef struct {
    uint8_t type, enabled, unmapped, usage;
    int32_t mix;
    uint32_t order;
    AddrInfo work[2];
    uint8_t param[0xA0];        // datos del tipo, como llegan (el BiquadFilter de REV<15, pasado a float)
    uint64_t send_info, send_base, ret_info, ret_base; // AUX y captura
    BqState bq[AR_CANALES];
    uint8_t result[0x80], result_dsp[0x80]; // EffectResultState (REV9+): estadísticas del limitador
    float *delay;               // limitador: muestras demoradas y promedios
    int32_t delay_pos[AR_CANALES], delay_max;
    float detector[AR_CANALES], gain[AR_CANALES];
} Effect;

typedef struct {
    uint8_t type, used, skip;
    int32_t node;
    uint8_t param[0x120];
    float downmix[4];
    int up_used;                // remuestreo de 32 a 48 kHz
    struct { float hist[20]; int phase, init; float scale; } up[AR_CANALES];
    float upbuf[AR_CANALES][AR_SALIDA];
    AddrInfo circ;
    uint32_t last_written, old_written, write_off;
} Sink;

typedef struct Renderer {
    pthread_mutex_t lock;
    uint32_t rate, samples, mix_buffers, submixes, nvoices, nsinks, neffects, perf_frames, nsplitters, ndests;
    int32_t revision;
    int rev;                    // número de revisión (1-15)
    uint8_t voice_drop, manual, cpu; // cpu: el juego pidió renderizar en la CPU
    uint64_t flags;             // del comportamiento: bit 0, mapeo forzado
    struct { uint32_t code, pad; uint64_t extra; } err[10];
    uint32_t nerr;
    int active;
    uint64_t elapsed;
    uint32_t time_limit;
    float drop_param;
    uint32_t npools;
    Pool *pools;
    Voice *voices;
    ChannelRes *chans;
    VoiceState *vstates;
    int32_t *sorted_voices;
    uint32_t nmixes;
    Mix *mixes;
    int32_t *sorted_mixes, *mix_order;
    uint8_t *edges;             // matriz de conexiones entre mezclas (para ordenarlas con splitters)
    Splitter *splitters;
    SplitDest *dests;
    BqState *dest_bq;           // 4 estados por destino (REV12+)
    Effect *effects;
    Sink *sinks;
    float *buf, *depop;         // buffers de mezcla: los de las mezclas y después 6 de la voz
    uint32_t nbufs;
    // rendimiento: solo las cabeceras de cada cuadro (sin tiempos medidos)
    uint32_t perf_r, perf_w, perf_index;
    uint64_t perf_start[64];
    uint32_t perf_idx[64];
    int16_t out[AR_SALIDA * AR_CANALES];
    int16_t temp[AR_TEMP + 16];  // muestras decodificadas, con la historia del remuestreador delante
    int dev_channels;
    KEvent *event;
    uint32_t event_h;
} Renderer;

static inline int ar_rev(const Renderer *R, int n) { return R->rev >= n; }
// Dirección del DSP de un AddrInfo (0 si su memoria no está adjunta); marca la memoria como usada.
uint64_t ar_addr(Renderer *R, AddrInfo *a, int mark);
// Genera y ejecuta un cuadro. Devuelve los canales escritos en R->out (0 si no hay sink de dispositivo).
int ar_frame(Renderer *R);
// Volumen del dispositivo de salida activo (IAudioDevice).
float ar_device_volume(void);
