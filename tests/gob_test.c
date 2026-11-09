// runtime/nvn/tex_deswizzle.c: el acomodo block-linear de Switch 1 y el de Switch 2 (GOB con los bits 5 y 6
// cambiados). Cada modo corre en su propio proceso (NX_PLATAFORMA se lee una vez).
//   cc -O1 -Iruntime -Iruntime/nvn tests/gob_test.c runtime/nvn/tex_deswizzle.c -o build/gob_test && build/gob_test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tex.h"

// tex_deswizzle.c también usa estas funciones de nvn_tex.c (solo para nvn_deswizzle_geom, que aquí no se prueba)
int nvn_fmt_bpb(int f, int *bw, int *bh) { (void)f; *bw = *bh = 1; return 0; }
void nvn_tex_layout(const nvn_texdesc *d, nvn_texlayout *l) { (void)d; memset(l, 0, sizeof *l); }
uint64_t nvn_tex_level_offset(const nvn_texdesc *d, unsigned l) { (void)d; (void)l; return 0; }
void nvn_tex_level_tiles(const nvn_texdesc *d, unsigned l, unsigned *th, unsigned *td) { (void)d; (void)l; *th = *td = 0; }

static int fallas;
static void ver(const char *n, int c) { printf("%s: %s\n", c ? "OK" : "MAL", n); fallas += !c; }

int main(int argc, char **argv) {
    if (argc < 2) {   // corre los dos modos
        char cmd[512];
        int r = 0;
        snprintf(cmd, sizeof cmd, "NX_PLATAFORMA=switch1 %s 1", argv[0]);
        r |= system(cmd);
        snprintf(cmd, sizeof cmd, "NX_PLATAFORMA=switch2 %s 2", argv[0]);
        r |= system(cmd);
        return r ? 1 : 0;
    }
    int s2 = argv[1][0] == '2';
    printf("-- Switch %d\n", s2 ? 2 : 1);
    ver("el modo sigue a NX_PLATAFORMA", nvn_gob_switch2() == s2);
    // offsets dentro del primer GOB: x = 16 (segmento 1) y la fila 2 cambian de lugar entre consolas
    ver("x=16, y=0", nvn_bl_offset(16, 0, 0, 256, 64, 1, 0) == (s2 ? 64u : 32u));
    ver("x=0, y=2", nvn_bl_offset(0, 2, 0, 256, 64, 1, 0) == (s2 ? 32u : 64u));
    ver("x=32, y=1 (los bits 4 y 8 no cambian)", nvn_bl_offset(32, 1, 0, 256, 64, 1, 0) == 256u + 16u);
    // la copia por filas (tex_bl_copy) coincide byte a byte con nvn_bl_offset, y la ida y vuelta da lo mismo
    const uint32_t rb = 200, rows = 40, sl = 3;   // fila que termina en un GOB parcial; 3 cortes de profundidad
    const unsigned th = 2, td = 1;
    size_t tam = (size_t)nvn_bl_offset(rb - 1, rows - 1, sl - 1, rb, rows, th, td) + 4096;
    uint8_t *lin = malloc((size_t)rb * rows * sl), *bl = calloc(1, tam), *vuelta = malloc((size_t)rb * rows * sl);
    for (size_t i = 0; i < (size_t)rb * rows * sl; i++) lin[i] = (uint8_t)(i * 2654435761u >> 13);
    tex_bl_copy(bl, lin, rb, rb, rows, sl, th, td, 0);
    int igual = 1;
    for (uint32_t z = 0; z < sl; z++)
        for (uint32_t y = 0; y < rows; y++)
            for (uint32_t x = 0; x < rb; x++)
                igual &= bl[nvn_bl_offset(x, y, z, rb, rows, th, td)] == lin[((size_t)z * rows + y) * rb + x];
    ver("la copia por filas coincide con el cálculo byte por byte", igual);
    tex_bl_copy(bl, vuelta, rb, rb, rows, sl, th, td, 1);
    ver("ida y vuelta (swizzle y deswizzle) da la imagen original", !memcmp(lin, vuelta, (size_t)rb * rows * sl));
    ver("intercambiar los bits 5 y 6 dos veces es la identidad", nvn_gob_swap56(nvn_gob_swap56(0x1e7)) == 0x1e7);
    return fallas ? 1 : 0;
}
