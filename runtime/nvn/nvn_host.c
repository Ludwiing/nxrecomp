// Adaptación: las funciones de host.h (runtime/nvn/host.h) que la capa NVN de totk-pc-recomp llama y que no caben
// en un encabezado.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "host.h"

unsigned rt_trace;

__attribute__((constructor)) static void leer_trazas(void) {
    if (getenv("NX_TRAZA_NVN")) rt_trace |= RT_TRACE_GFX;
}

void rt_log(const char *fmt, ...) {
    char linea[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(linea, sizeof linea, fmt, ap);
    va_end(ap);
    fprintf(stderr, "[nvn] %s\n", linea);
}

void rt_fatal(const char *fmt, ...) {
    char linea[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(linea, sizeof linea, fmt, ap);
    va_end(ap);
    fflush(stdout);
    fprintf(stderr, "[RED] %s\n", linea);
    exit(1);
}

// La función del juego vuelve con un RET a este LR: nx_enter lo toma como su retorno y la llamada C termina ahí.
#define RETORNO_AL_HOST 0x00FD00000000ull

uint64_t rt_call(Cpu *c, uint64_t fn, int n, ...) {
    if (n > 8) rt_fatal("rt_call: %d argumentos (máximo 8)", n);
    uint64_t x[31], sp = c->sp;
    V128 v[32];
    memcpy(x, c->x, sizeof x);
    memcpy(v, c->vr, sizeof v);
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) c->x[i] = va_arg(ap, uint64_t);
    va_end(ap);
    c->x[30] = RETORNO_AL_HOST;
    c->tail_pending = 0;
    nx_buscar(fn)(c);
    uint64_t r = c->x[0];
    memcpy(c->x, x, sizeof x);   // el host sigue con los registros que tenía (sus argumentos incluidos)
    memcpy(c->vr, v, sizeof v);
    c->sp = sp;
    return r;
}

uint64_t rt_original(Cpu *c) {
    (void)c;
    rt_fatal("rt_original: los observadores de funciones del juego no se registran en nxrecomp");
}

// Sin el backend Vulkan enlazado, gfx_vulkan_backend queda en cero y se usa el nulo.
__attribute__((weak)) const GfxBackend gfx_vulkan_backend;

const RtConfig *rt_config(void) {
    static RtConfig cfg;
    if (!cfg.gpu) {
        const char *g = getenv("NX_GPU");
        cfg.gpu = gfx_vulkan_backend.name && !(g && !strcmp(g, "null")) ? "vulkan" : "null";
    }
    return &cfg;
}

void rt_frame_presented(const tex_t *t, uint64_t n) { (void)t, (void)n; }
