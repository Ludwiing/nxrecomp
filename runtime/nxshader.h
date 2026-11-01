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
    uint8_t rt_int[8];    // fragmento: render targets enteros (1 con signo, 2 sin signo)
    uint16_t tex_int_handle[8]; // texturas enteras: handle y tipo (1 con signo, 2 sin signo)
    uint8_t tex_int_kind[8];
    uint8_t img_binding;  // primera unidad de imagen de esta etapa
    uint16_t img_handle[8]; // imágenes: handle y formato real (NX_IMG_*), para las operaciones con formato
    uint8_t img_fmt[8];
    uint16_t local[3];    // cómputo: hilos por grupo (de la QMD)
    uint32_t shared_bytes, local_bytes; // cómputo: memoria compartida y local (las gráficas la leen de su cabecera)
} NxShaderKey;

#define NX_MAX_SSBO 16
#define NX_MAX_IMG 8

// Formatos de imagen de OpenGL (calificadores de layout). Los primeros son los "crudos" de SULD.D/SUST.D.
enum { NX_IMG_R8UI = 1, NX_IMG_R8I, NX_IMG_R16UI, NX_IMG_R16I, NX_IMG_R32UI, NX_IMG_RG32UI, NX_IMG_RGBA32UI,
       NX_IMG_RGBA8, NX_IMG_RGBA8_SNORM, NX_IMG_RGBA16F, NX_IMG_RGBA32F, NX_IMG_R32F, NX_IMG_RG16F, NX_IMG_R11G11B10F,
       NX_IMG_RGB10A2, NX_IMG_RGBA8UI, NX_IMG_RGBA16UI, NX_IMG_R32I, NX_IMG_RG32F, NX_IMG_R16F, NX_IMG_RG8, NX_IMG_R8,
       NX_IMG_COUNT };

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
        uint8_t type, entera; // entera: 1 con signo, 2 sin signo (de la clave)
    } tex[NX_MAX_TEX];
    int nssbo;            // memoria global: cada base distinta (leída de un constbuf) es un storage buffer
    struct {
        uint8_t slot, written;
        uint16_t offset;  // en bytes: ahí están la dirección (64 bits) y, si el driver la pone, el tamaño
    } ssbo[NX_MAX_SSBO];
    int nimg;             // imágenes: handle, tipo (NX_TEX_*), formato (NX_IMG_*) y si se escriben
    struct {
        uint16_t handle;
        uint8_t type, fmt, written;
    } img[NX_MAX_IMG];
    int uses_instance;    // lee el índice de instancia (uniform nx_inst0 en la location 0)
    int errors;           // instrucciones que no supo traducir
} NxShaderInfo;

// `code` apunta a la cabecera (SPH, 0x50 bytes) seguida de las instrucciones; `avail` es cuánto se
// puede leer. Devuelve el GLSL (malloc) o NULL.
char *nx_shader_glsl(const uint8_t *code, size_t avail, int stage, const NxShaderKey *key, NxShaderInfo *info);
