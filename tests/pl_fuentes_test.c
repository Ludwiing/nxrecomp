// pl:u de Switch 2, comando 2010 (runtime/nx_pl.h): bytes exactos de la respuesta, desplazamientos, tamaños y tipos,
// y que el camino del juego (base de la memoria compartida + desplazamiento, directo a LoadFont) llega al TTF, con la
// cabecera BFTTF delante. Nunca «cargada» sin datos que lo respalden.
#include "nx_pl.h"

#include <assert.h>
#include <stdio.h>

static uint8_t mem[0x8000];
static uint32_t off[NX_PL_FUENTES], tam[NX_PL_FUENTES];

// Lo que hace el juego con la respuesta (visto bajo GDB): GetSharedFontAddress = base + desplazamiento, y esa
// dirección va tal cual a ScalableFontEngine::LoadFont, que espera el TTF.
static const uint8_t *camino_sdk(const uint8_t r[16], uint32_t *ttf_tam) {
    uint32_t desp, total, marca;
    memcpy(&desp, r + 8, 4);
    memcpy(&total, r + 12, 4);
    if (desp < 8) return NULL;
    memcpy(&marca, mem + desp - 8, 4);             // la cabecera BFTTF queda justo delante
    if (marca != NX_PL_MARCA_BFTTF) return NULL;
    *ttf_tam = total;
    return mem + desp;
}

int main(void) {
    // Seis fuentes como las deja cargar_fuentes: cabecera de 8 bytes (marca, tamaño cifrado) y el TTF, alineadas a
    // 0x1000; el tipo 4 sin fuente.
    uint32_t pos = 0;
    for (int t = 0; t < NX_PL_FUENTES; t++) {
        if (t == 4) continue;
        uint32_t n = 0x100 + 0x10 * (uint32_t)t, marca = NX_PL_MARCA_BFTTF, cifrado = __builtin_bswap32(n ^ 0x49621806u);
        memcpy(mem + pos, &marca, 4);
        memcpy(mem + pos + 4, &cifrado, 4);
        for (uint32_t i = 0; i < n; i++) mem[pos + 8 + i] = (uint8_t)(t * 31 + i);
        off[t] = pos + 8, tam[t] = n;
        pos = (pos + 8 + n + 0xFFF) & ~0xFFFu;
    }

    uint8_t r[16];
    for (int t = 0; t < NX_PL_FUENTES; t++) {
        for (int bandera = 0; bandera < 2; bandera++) {
            uint8_t in[12] = {(uint8_t)t, (uint8_t)bandera};  // la petición real: 12 bytes, tipo y bandera en +0 y +1
            int c = nx_pl_fuente_integrada(off, tam, mem, sizeof mem, in, sizeof in, r);
            if (t == 4) {
                // Sin fuente: «no cargada» y nada más (el SDK no lee el resto).
                assert(c == 0 && r[0] == 0);
                for (int i = 0; i < 16; i++) assert(r[i] == 0);
                continue;
            }
            assert(c == 1 && r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);
            uint32_t info, desp, total;
            memcpy(&info, r + 4, 4), memcpy(&desp, r + 8, 4), memcpy(&total, r + 12, 4);
            assert(info == ((uint32_t)t | (uint32_t)bandera << 8));
            assert(desp == off[t] && total == tam[t]);
            uint32_t n;
            const uint8_t *ttf = camino_sdk(r, &n);
            assert(ttf == mem + off[t] && n == tam[t] && ttf[1] == (uint8_t)(t * 31 + 1));
        }
    }
    // Tipo fuera de rango, petición corta, cabecera sin la marca y fuente que no cabe: «no cargada».
    uint8_t in[2] = {6, 0};
    assert(nx_pl_fuente_integrada(off, tam, mem, sizeof mem, in, 2, r) == 0 && r[0] == 0);
    in[0] = 0;
    assert(nx_pl_fuente_integrada(off, tam, mem, sizeof mem, in, 1, r) == 0 && r[0] == 0);
    mem[off[1] - 8] ^= 1;
    in[0] = 1;
    assert(nx_pl_fuente_integrada(off, tam, mem, sizeof mem, in, 2, r) == 0 && r[0] == 0);
    mem[off[1] - 8] ^= 1;
    assert(nx_pl_fuente_integrada(off, tam, mem, off[1] + tam[1] - 1, in, 2, r) == 0);
    // Un desplazamiento menor que la cabecera: aunque haya una marca justo antes de la memoria compartida (aquí, 4
    // bytes antes de la base que se pasa), no es una fuente de esa memoria.
    static uint8_t otra[0x100];
    uint32_t marca = NX_PL_MARCA_BFTTF, off_malo[NX_PL_FUENTES] = {4}, tam_malo[NX_PL_FUENTES] = {8};
    memcpy(otra + 0x10 - 4, &marca, 4);
    in[0] = 0;
    assert(nx_pl_fuente_integrada(off_malo, tam_malo, otra + 0x10, 0x80, in, 2, r) == 0 && r[0] == 0);
    in[0] = 1;
    assert(nx_pl_fuente_integrada(off, tam, mem, off[1] + tam[1], in, 2, r) == 1);
    puts("pl:u Switch 2 GetBuiltInFontData (2010): response bytes, BFTTF offsets, sizes and types: PASS");
    return 0;
}
