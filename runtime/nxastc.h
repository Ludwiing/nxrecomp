// Decodificador ASTC (nxastc.c): las GPU de PC no leen el formato que usan casi todos los juegos.
#pragma once
#include <stdint.h>

// Un bloque de 16 bytes de `bw`×`bh` texels a RGBA8 (filas de `stride` bytes).
void nx_astc_bloque(const uint8_t *bloque, int bw, int bh, int srgb, uint8_t *out, int stride);
// Una imagen de `w`×`h` texels (bloques en orden de filas) a RGBA8 con filas de w*4 bytes.
void nx_astc_imagen(const uint8_t *src, int w, int h, int bw, int bh, int srgb, uint8_t *dst);
