// Programas de NVN2 (Switch 2): lo que el juego le da a nvnProgramSetShaders por cada etapa. Escrito para nxrecomp
// (MIT) a partir del formato:
//
//   sección de control, en la memoria del juego, 0xd8 bytes:
//     +0x00  u32 magia 0x98761234
//     +0x38  u32 largo del código (bytes, en el área de código)
//     +0x40  u32 offset de las constantes c1 dentro del área de código
//     +0x44  u32 tamaño de las constantes c1
//     +0x56  u16 etapa
//     +0x58  u16 x3 tamaño del grupo de trabajo (cómputo)
//   área de código, en memoria de la GPU: una etapa gráfica empieza con una cabecera de 128 bytes (SPH); luego las
//   instrucciones de Ampere (SM86) y, en c1_off, las constantes c1.
//
// Los anchos de etapa y grupo de trabajo (u16) y la numeración de las etapas son supuestos razonables (los de NVN 1:
// vertex 0, fragment 1, geometry 2, tess control 3, tess evaluation 4, compute 5) que falta confirmar con un juego real
// de Switch 2; nvn2_control_leer los deja anotados como no confirmados.
#pragma once
#include <stdint.h>
#include <string.h>

#define NVN2_CONTROL_MAGIA 0x98761234u
#define NVN2_CONTROL_TAM 0xd8u
#define NVN2_SPH_TAM 128u

typedef enum { NVN2_VERTEX, NVN2_FRAGMENT, NVN2_GEOMETRY, NVN2_TESS_CONTROL, NVN2_TESS_EVAL, NVN2_COMPUTE } Nvn2Etapa;

typedef struct {
    uint32_t largo_codigo;    // bytes de instrucciones (sin la cabecera)
    uint32_t c1_off, c1_tam;  // constantes c1 en el área de código
    uint16_t etapa;           // Nvn2Etapa (sin confirmar)
    uint16_t grupo[3];        // tamaño del grupo de trabajo (solo cómputo)
    int grafica;              // 1: el área de código empieza con la cabecera de 128 bytes
} Nvn2Control;

static inline uint32_t nvn2_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t nvn2_u16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }

// Lee una sección de control de `n` bytes. 0 si no lo es (magia, tamaño o campos imposibles para un área de
// `area_tam` bytes; 0 = no se conoce el área y no se comprueba).
static inline int nvn2_control_leer(const void *datos, uint32_t n, uint32_t area_tam, Nvn2Control *out) {
    const uint8_t *p = (const uint8_t *)datos;
    if (n < NVN2_CONTROL_TAM || nvn2_u32(p) != NVN2_CONTROL_MAGIA) return 0;
    Nvn2Control c = {.largo_codigo = nvn2_u32(p + 0x38), .c1_off = nvn2_u32(p + 0x40), .c1_tam = nvn2_u32(p + 0x44),
                     .etapa = nvn2_u16(p + 0x56)};
    for (int i = 0; i < 3; i++) c.grupo[i] = nvn2_u16(p + 0x58 + 2 * i);
    if (c.etapa > NVN2_COMPUTE || !c.largo_codigo || (c.largo_codigo & 15)) return 0;   // Ampere: 16 bytes por instrucción
    c.grafica = c.etapa != NVN2_COMPUTE;
    if (area_tam) {
        uint64_t fin = (uint64_t)(c.grafica ? NVN2_SPH_TAM : 0) + c.largo_codigo;
        if (fin > area_tam || (c.c1_tam && (uint64_t)c.c1_off + c.c1_tam > area_tam)) return 0;
    }
    if (!c.grafica && (!c.grupo[0] || !c.grupo[1] || !c.grupo[2])) return 0;
    *out = c;
    return 1;
}

// Dónde empiezan las instrucciones dentro del área de código.
static inline uint32_t nvn2_codigo_inicio(const Nvn2Control *c) { return c->grafica ? NVN2_SPH_TAM : 0; }
