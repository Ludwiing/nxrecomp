// Salida de audio del anfitrión (nxaudio.c): PCM de 16 bits intercalado.
#pragma once
#include <stddef.h>
#include <stdint.h>

void nx_audio_open(int sample_rate, int channels);
void nx_audio_write(const int16_t *samples, size_t frames); // frames = muestras por canal
int nx_audio_rate(void);
int nx_audio_channels(void);
