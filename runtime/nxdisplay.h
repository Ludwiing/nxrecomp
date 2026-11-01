// La pantalla del runtime (nxdisplay.c): recibe cuadros de la cola de vi y los muestra.
#pragma once
#include <stdint.h>

enum { // formatos de píxel de Android, los que usan vi y libnx
    NX_FMT_RGBA8888 = 1,
    NX_FMT_RGBX8888 = 2,
    NX_FMT_RGB565 = 4,
    NX_FMT_BGRA8888 = 5,
    NX_FMT_RGBA4444 = 7,
};

typedef struct {
    const uint8_t *data;
    uint32_t width, height, pitch, format;
    uint32_t layout;            // 1 lineal, 3 block linear
    uint32_t block_height_log2;
} NxFrame;

void nx_display_present(const NxFrame *f); // lo llama el hilo del juego; marca el ritmo de 60 Hz
void nx_display_run(void);                 // el hilo principal: la ventana (no vuelve)
