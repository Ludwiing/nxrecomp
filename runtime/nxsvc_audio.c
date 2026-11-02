// audout: la salida de audio simple. El programa agrega buffers de PCM (AudioOutBuffer, con la
// dirección de las muestras); un hilo los reproduce en tiempo real y, al terminar cada uno, lo
// pasa a la lista de liberados y señala el evento que el programa espera.
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxaudio.h"
#include "nxkern.h"
#include "nxsvc.h"

#define OK 0
#define MAX_BUFS 64

typedef struct {
    uint64_t tag, addr, size;
} AudBuf;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    AudBuf queue[MAX_BUFS], released[MAX_BUFS];
    int nqueue, nreleased, started;
    uint64_t played;
    float volume;
    KEvent *event;
    uint32_t event_h;
} AudioOut;

static void sleep_ns(uint64_t ns) {
    struct timespec ts = {(time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull)};
    nanosleep(&ts, NULL);
}

// Reproduce los buffers en orden, al ritmo real de 48 kHz.
static void *player(void *arg) {
    AudioOut *ao = arg;
    uint64_t clock = nx_now_ns();
    for (;;) {
        pthread_mutex_lock(&ao->lock);
        while (!ao->started || !ao->nqueue) pthread_cond_wait(&ao->cond, &ao->lock);
        AudBuf b = ao->queue[0];
        pthread_mutex_unlock(&ao->lock);

        size_t frames = b.size / (2 * (size_t)nx_audio_channels());
        int16_t *tmp = malloc(b.size ? b.size : 1);
        memcpy(tmp, (const void *)(uintptr_t)b.addr, b.size);
        if (ao->volume != 1.0f)
            for (size_t k = 0; k < frames * (size_t)nx_audio_channels(); k++) tmp[k] = (int16_t)(tmp[k] * ao->volume);
        nx_audio_write(tmp, frames);
        free(tmp);

        uint64_t now = nx_now_ns();
        if (clock < now) clock = now;
        clock += frames * 1000000000ull / (uint64_t)nx_audio_rate();
        sleep_ns(clock - nx_now_ns() < 1000000000ull ? clock - nx_now_ns() : 0);

        pthread_mutex_lock(&ao->lock);
        if (ao->nqueue && ao->queue[0].tag == b.tag) { // si un Stop no lo devolvió ya
            memmove(ao->queue, ao->queue + 1, (size_t)--ao->nqueue * sizeof *ao->queue);
            if (ao->nreleased < MAX_BUFS) ao->released[ao->nreleased++] = b;
        }
        ao->played += frames;
        pthread_mutex_unlock(&ao->lock);
        nx_event_signal(ao->event);
    }
    return NULL;
}

static uint32_t audio_out(ServiceObject *self, NxRequest *r) {
    AudioOut *ao = self->state;
    switch (r->cmd) {
    case 0: nx_out32(r, ao->started ? 0 : 1); return OK; // GetAudioOutState: 0 andando, 1 detenido
    case 1: case 2: // StartAudioOut / StopAudioOut
        pthread_mutex_lock(&ao->lock);
        ao->started = r->cmd == 1;
        if (r->cmd == 2) { // detener devuelve todo lo pendiente (FMOD esperaba esos buffers y se colgaba)
            for (int k = 0; k < ao->nqueue && ao->nreleased < MAX_BUFS; k++) ao->released[ao->nreleased++] = ao->queue[k];
            ao->nqueue = 0;
        }
        pthread_cond_signal(&ao->cond);
        pthread_mutex_unlock(&ao->lock);
        if (r->cmd == 2) nx_event_signal(ao->event);
        return OK;
    case 3: case 7: { // AppendAudioOutBuffer(tag) + el AudioOutBuffer
        NxBuffer *in = nx_in_buffer(r, 0);
        if (!in || in->size < 0x28) return 0x7F4u;
        const uint64_t *aob = (const uint64_t *)(uintptr_t)in->addr; // next, buffer, buffer_size, data_size, data_offset
        pthread_mutex_lock(&ao->lock);
        int lleno = ao->nqueue >= MAX_BUFS;
        if (!lleno) ao->queue[ao->nqueue++] = (AudBuf){nx_in64(r, 0), aob[1] + aob[4], aob[3]};
        pthread_cond_signal(&ao->cond);
        pthread_mutex_unlock(&ao->lock);
        return lleno ? 0x1099u : OK; // audio 8: la cola está llena (antes se perdía sin avisar)
    }
    case 4: nx_out_copy_handle(r, ao->event_h); return OK; // RegisterBufferEvent
    case 5: case 8: { // GetReleasedAudioOutBuffer → los tags liberados
        NxBuffer *out = nx_out_buffer(r, 0);
        uint32_t n = 0;
        pthread_mutex_lock(&ao->lock);
        uint32_t max = out ? (uint32_t)(out->size / 8) : 0;
        while (n < max && n < (uint32_t)ao->nreleased) {
            memcpy((void *)(uintptr_t)(out->addr + 8 * n), &ao->released[n].tag, 8);
            n++;
        }
        memmove(ao->released, ao->released + n, (size_t)(ao->nreleased - (int)n) * sizeof *ao->released);
        ao->nreleased -= (int)n;
        pthread_mutex_unlock(&ao->lock);
        nx_out32(r, n);
        return OK;
    }
    case 6: { // ContainsAudioOutBuffer
        uint64_t tag = nx_in64(r, 0);
        int found = 0;
        pthread_mutex_lock(&ao->lock);
        for (int k = 0; k < ao->nqueue; k++) found |= ao->queue[k].tag == tag;
        pthread_mutex_unlock(&ao->lock);
        nx_out8(r, (uint8_t)found);
        return OK;
    }
    case 9: nx_out32(r, (uint32_t)ao->nqueue); return OK;   // GetAudioOutBufferCount
    case 10: nx_out64(r, ao->played); return OK;            // GetAudioOutPlayedSampleCount
    case 11: nx_out8(r, 1); return OK;                      // FlushAudioOutBuffers
    case 12: memcpy(&ao->volume, r->in, 4); return OK;      // SetAudioOutVolume
    case 13: nx_out_bytes(r, &ao->volume, 4); return OK;    // GetAudioOutVolume
    }
    return NX_R_UNKNOWN_CMD;
}

static void device_name(NxBuffer *b) {
    if (b && b->size >= 10) memcpy((void *)(uintptr_t)b->addr, "DeviceOut", 10);
}

uint32_t nx_audout(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 2: // ListAudioOuts
        device_name(nx_out_buffer(r, 0));
        nx_out32(r, 1);
        return OK;
    case 1: case 3: { // OpenAudioOut(frecuencia, canales, aruid) → frecuencia, canales, formato, estado
        uint32_t want_rate = nx_in32(r, 0), want_ch = nx_in32(r, 4);
        int ch = want_ch == 6 ? 6 : 2;
        (void)want_rate;
        nx_audio_open(48000, ch); // la Switch siempre sale a 48 kHz
        AudioOut *ao = calloc(1, sizeof *ao);
        pthread_mutex_init(&ao->lock, NULL);
        pthread_cond_init(&ao->cond, NULL);
        ao->volume = 1.0f;
        ao->event_h = nx_event_new(&ao->event);
        pthread_t t;
        pthread_create(&t, NULL, player, ao);
        device_name(nx_out_buffer(r, 0));
        nx_out32(r, 48000);
        nx_out32(r, (uint32_t)ch);
        nx_out32(r, 2); // PcmFormat_Int16
        nx_out32(r, 1); // detenido
        nx_out_object(r, nx_object("IAudioOut", audio_out, ao));
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}
