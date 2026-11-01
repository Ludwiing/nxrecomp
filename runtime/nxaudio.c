// Salida de audio del anfitrión: recibe PCM de 16 bits y lo reproduce con SDL3 a su ritmo real.
//   NX_SIN_AUDIO=1     no abre el dispositivo (las pruebas igual llevan el ritmo)
//   NX_AUDIO_WAV=f.wav además guarda todo lo reproducido en un WAV
#define _GNU_SOURCE
#include "nxaudio.h"

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioStream *stream;
static int opened, rate = 48000, channels = 2;
static FILE *wav;
static uint32_t wav_bytes;

static void wav_header(void) {
    uint32_t byte_rate = (uint32_t)(rate * channels * 2);
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    uint32_t v = 36 + wav_bytes;
    memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16;
    memcpy(h + 16, &v, 4);
    uint16_t s = 1;
    memcpy(h + 20, &s, 2); // PCM
    s = (uint16_t)channels;
    memcpy(h + 22, &s, 2);
    v = (uint32_t)rate;
    memcpy(h + 24, &v, 4);
    memcpy(h + 28, &byte_rate, 4);
    s = (uint16_t)(channels * 2);
    memcpy(h + 32, &s, 2);
    s = 16;
    memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &wav_bytes, 4);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fseek(wav, 0, SEEK_END);
    fflush(wav); // el proceso termina con _exit: el archivo tiene que quedar bien en cada momento
}

void nx_audio_open(int sample_rate, int channel_count) {
    pthread_mutex_lock(&lock);
    if (!opened) {
        opened = 1;
        rate = sample_rate;
        channels = channel_count;
        const char *path = getenv("NX_AUDIO_WAV");
        if (path && (wav = fopen(path, "wb"))) wav_header();
        if (!getenv("NX_SIN_AUDIO") && SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            SDL_AudioSpec spec = {SDL_AUDIO_S16LE, channels, rate};
            stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
            if (stream) SDL_ResumeAudioStreamDevice(stream);
            else fprintf(stderr, "[nxrt] sin dispositivo de audio: %s\n", SDL_GetError());
        }
    }
    pthread_mutex_unlock(&lock);
}

void nx_audio_write(const int16_t *samples, size_t frames) {
    size_t bytes = frames * (size_t)channels * 2;
    pthread_mutex_lock(&lock);
    if (stream) SDL_PutAudioStreamData(stream, samples, (int)bytes);
    if (wav) {
        fwrite(samples, 1, bytes, wav);
        wav_bytes += (uint32_t)bytes;
        wav_header();
    }
    pthread_mutex_unlock(&lock);
}

int nx_audio_rate(void) { return rate; }
int nx_audio_channels(void) { return channels; }
