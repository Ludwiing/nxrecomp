// pl:u de Switch 2: las fuentes integradas (nn::pl::GetBuiltInFontData), con el formato que lee el SDK del juego.
// Sacado del proxy CMIF del SDK traducido de Mario Tennis Fever (sdk+0x52264c para 2010, sdk+0x52251c para 2000;
// las dos entradas de IPlatformServiceManager en vtable +0x60 y +0x58) y de quién lo usa:
//   2000 (sin entrada) → datos u32 tamaño de la memoria compartida + un handle copiado de esa memoria. El SDK la
//        mapea una vez y suma a su base el desplazamiento de cada fuente.
//   2010 (entrada: u8 tipo, u8 bandera) → 16 bytes: +0 u8 cargada; +4 u32 con el tipo en el byte 0 y la bandera en
//        el bit 8; +8 u32 desplazamiento en la memoria compartida; +12 u32 tamaño. Si "cargada" es 0, el SDK no
//        mira lo demás y GetBuiltInFontData devuelve false (GetSharedFontLoadState: todavía no cargada).
// Los tipos 0-5 son las seis fuentes compartidas de siempre: GetSharedFontLoadState(t) = GetBuiltInFontData(t) & 1.
// El desplazamiento apunta al TTF, detrás de la cabecera BFTTF de 8 bytes, y el tamaño es el del TTF (como en el
// pl:u de Switch 1, comandos 2 y 3). Comprobado con el juego bajo GDB: pide la fuente 0 por GetSharedFontAddress y
// pasa esa dirección tal cual a nn::fontll::ScalableFontEngine::LoadFont, sin ScalableFontEngineHelper::Decode (que
// usa solo con sus BFTTF propias). Con la dirección de la cabecera, LoadFont fallaba y el caché de glifos quedaba
// vacío (lectura de 0x30). La cabecera se exige igual: confirma que en esa memoria hay una fuente.
#pragma once
#include <stdint.h>
#include <string.h>

#define NX_PL_FUENTES 6
#define NX_PL_MARCA_BFTTF 0x18029A7Fu

// Respuesta de 2010 en out (16 bytes). off/tam: dónde empieza el TTF de cada fuente (detrás de su cabecera) y cuánto
// mide; tam 0 = no hay. Devuelve 1 si la fuente está cargada. mem/mem_tam: la memoria compartida, para comprobar
// que la cabecera está y que todo cabe; si no, la fuente no se da por cargada.
static inline int nx_pl_fuente_integrada(const uint32_t off[NX_PL_FUENTES], const uint32_t tam[NX_PL_FUENTES],
                                         const uint8_t *mem, uint64_t mem_tam, const uint8_t *in, uint32_t in_tam,
                                         uint8_t out[16]) {
    memset(out, 0, 16);
    if (in_tam < 2) return 0;
    uint8_t tipo = in[0], bandera = in[1] & 1;
    if (tipo >= NX_PL_FUENTES || !tam[tipo] || off[tipo] < 8) return 0;
    uint32_t marca;
    if ((uint64_t)off[tipo] + tam[tipo] > mem_tam) return 0;
    memcpy(&marca, mem + off[tipo] - 8, 4);
    if (marca != NX_PL_MARCA_BFTTF) return 0;
    uint32_t info = tipo | (uint32_t)bandera << 8;
    out[0] = 1;
    memcpy(out + 4, &info, 4);
    memcpy(out + 8, &off[tipo], 4);
    memcpy(out + 12, &tam[tipo], 4);
    return 1;
}
