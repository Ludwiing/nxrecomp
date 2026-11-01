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
} NxShaderKey;

// Lo que el traductor encontró en el shader.
typedef struct {
    uint32_t cbufs;       // constbufs que lee (bit = ranura)
    uint32_t inputs;      // atributos genéricos que lee
    uint32_t outputs;     // atributos genéricos que escribe
    uint32_t omap;        // fragmento: componentes de color que escribe (4 bits por render target)
    uint32_t code_size;   // bytes del programa (cabecera incluida) hasta la última instrucción usada
    int errors;           // instrucciones que no supo traducir
} NxShaderInfo;

// `code` apunta a la cabecera (SPH, 0x50 bytes) seguida de las instrucciones; `avail` es cuánto se
// puede leer. Devuelve el GLSL (malloc) o NULL.
char *nx_shader_glsl(const uint8_t *code, size_t avail, int stage, const NxShaderKey *key, NxShaderInfo *info);
