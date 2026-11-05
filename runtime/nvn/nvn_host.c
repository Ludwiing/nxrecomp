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
    // La función del juego corre como código del juego: con su núcleo virtual (si se lo quitaron, lo recupera) y
    // con la preferencia asíncrona activa.
    int en_host = nx_en_host;
    nx_en_host = 1;
    nx_host_leave();
    nx_buscar(fn)(c);
    nx_host_enter();
    nx_en_host = en_host;
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

// ---- backend Vulkan
#include "cfg.h"
#include "sha256.h"

const int rt_game_version = 0;
void (*perf_gpu_stats)(PerfGpu *out);

__attribute__((constructor)) static void leer_configuracion(void) { cfg_init(); }

void rt_sha256(const void *p, size_t n, uint8_t out[32]) { sha256(p, n, out); }

int rt_ab(void) { return 0; }

int rt_knob(const char *name, const char *def) {
    char clave[128];
    snprintf(clave, sizeof clave, "TOTK_%s", name);
    const char *v = cfg_get(clave);
    if (!v) v = def;
    if (!v || !strcmp(v, "0")) return 0;
    return strcmp(v, "ab") ? 1 : 2;
}

uint64_t nx_mem_written(uint64_t addr, uint64_t n, uint64_t *pages, uint64_t max, int reset);   // nxkern.c
uint64_t mem_written_pages(uint64_t addr, uint64_t size, uint64_t *pages, uint64_t max) {
    return nx_mem_written(addr, size, pages, max, 1);
}
uint64_t mem_written_peek(uint64_t addr, uint64_t size, uint64_t *pages, uint64_t max) {
    return nx_mem_written(addr, size, pages, max, 0);
}
uint32_t mem_set_heap_size(uint64_t size, uint64_t *addr) {
    (void)size;
    *addr = 0;
    return 0xD001;   // OutOfMemory: el heap del juego lo maneja el kernel de nxrecomp
}

void res_sizes(unsigned out[2], unsigned scene[2]) { out[0] = out[1] = scene[0] = scene[1] = 0; }

// Mensajes de foco al juego (applet OE de totk-pc-recomp): el servicio am de nxrecomp ya informa el foco.
void oe_push_message(uint32_t message) { (void)message; }
void oe_set_focus(int in_focus) { (void)in_focus; }
// Capturas de pantalla de prueba y RenderDoc: no hay en esta adaptación.
void rt_write_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h) {
    (void)rgba, (void)w, (void)h;
    rt_log("captura %s: rt_write_png no está en esta adaptación", path);
}
void rdoc_init(void) {}
int rdoc_active(void) { return 0; }
void rdoc_after_present(uint64_t frame) { (void)frame; }
