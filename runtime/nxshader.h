// Traductor de shaders de la GPU Maxwell (nxshader.c): código de máquina → GLSL 4.60.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Etapas, en el orden de los registros SetProgram del motor 3D.
enum { NX_SH_VERTEX_A, NX_SH_VERTEX, NX_SH_TESS_CTRL, NX_SH_TESS_EVAL, NX_SH_GEOMETRY, NX_SH_FRAGMENT, NX_SH_COMPUTE };

// Lo que el shader necesita saber del estado de la GPU. Forma parte de la clave de la caché.
typedef struct {
    uint8_t attr_int[32]; // atributos de vértice: 0 float, 1 entero con signo, 2 sin signo
    uint8_t cb_binding;   // primer punto de enlace de los constbufs de esta etapa
    uint8_t tex_binding;  // primera unidad de textura de esta etapa
    uint8_t sb_binding;   // primer punto de enlace de los storage buffers de esta etapa
    uint8_t gs_input;     // geometría: primitiva de entrada (0 puntos, 1 líneas, 2 líneas con adyacencia,
                          // 3 triángulos, 4 triángulos con adyacencia)
    uint16_t tess_mode;   // evaluación de teselación: TessellationMode del motor 3D
    uint16_t local[3];    // cómputo: hilos por grupo (de la QMD)
    uint32_t shared_bytes, local_bytes; // cómputo: memoria compartida y local (las gráficas la leen de su cabecera)
} NxShaderKey;

#define NX_MAX_SSBO 16

// Tipos de textura que usa un shader (lo que dice la instrucción, no el descriptor).
enum { NX_TEX_2D, NX_TEX_3D, NX_TEX_CUBE, NX_TEX_2D_ARRAY, NX_TEX_CUBE_ARRAY, NX_TEX_2D_MS, NX_TEX_BUFFER, NX_TEX_SHADOW = 0x10 };
#define NX_MAX_TEX 32

// Lo que el traductor encontró en el shader.
typedef struct {
    uint32_t cbufs;       // constbufs que lee (bit = ranura)
    uint32_t inputs;      // atributos genéricos que lee
    uint32_t outputs;     // atributos genéricos que escribe
    uint32_t omap;        // fragmento: componentes de color que escribe (4 bits por render target)
    uint32_t code_size;   // bytes del programa (cabecera incluida) hasta la última instrucción usada
    int ntex;             // texturas: palabra del handle en el constbuf de texturas y tipo; la i-ésima
    struct {              // va en la unidad key.tex_binding + i
        uint16_t handle;
        uint8_t type;
    } tex[NX_MAX_TEX];
    int nssbo;            // memoria global: cada base distinta (leída de un constbuf) es un storage buffer
    struct {
        uint8_t slot, written;
        uint16_t offset;  // en bytes: ahí están la dirección (64 bits) y, si el driver la pone, el tamaño
    } ssbo[NX_MAX_SSBO];
    int uses_instance;    // lee el índice de instancia (uniform nx_inst0 en la location 0)
    int errors;           // instrucciones que no supo traducir
} NxShaderInfo;

// `code` apunta a la cabecera (SPH, 0x50 bytes) seguida de las instrucciones; `avail` es cuánto se
// puede leer. Devuelve el GLSL (malloc) o NULL.
char *nx_shader_glsl(const uint8_t *code, size_t avail, int stage, const NxShaderKey *key, NxShaderInfo *info);
