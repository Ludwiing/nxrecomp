// Dibujo de la GPU sobre OpenGL 4.6 (nxgl.c), sin ventana (EGL).
#pragma once
#include <stdint.h>

typedef struct {
    const uint32_t *regs;            // registros del motor 3D
    const uint64_t (*cb_addr)[18];   // constbufs enlazados por etapa (0 vértice ... 4 fragmento)
    const uint32_t (*cb_size)[18];
    uint32_t topology, first, count, instance, ninstances; // instancias [instance, instance + ninstances)
    int indexed;
} NxDraw;

int nx_gl_draw(const NxDraw *d);
int nx_gl_dispatch(const uint32_t *compute_regs, const uint32_t *qmd); // lanzamiento de cómputo  // 0 si no hay OpenGL (NX_SIN_GPU o sin EGL)
int nx_gl_clear(const uint32_t *regs, uint32_t v); // ClearBuffers en la GPU; 0 si la tiene que hacer la CPU
// Contadores de los reportes (tipo = bits 23-27 del método ReportSemaphore): el valor acumulado
// desde el último reinicio; 0 si no se cuenta. nx_gl_contador_reiniciar recibe el valor de CounterReset.
uint64_t nx_gl_contador(uint32_t tipo);
void nx_gl_contador_reiniciar(uint32_t v);
void nx_gl_flush(void);           // escribe en la memoria del juego lo que la GPU dibujó
void nx_gl_release(void);         // flush y suelta el contexto (al terminar un envío)
// La CPU va a escribir esa memoria (ClearBuffers por CPU, cargas en línea): lo pendiente se escribe
// antes y la copia en la GPU se recarga en el próximo uso.
void nx_gl_cpu_write(uint64_t gpu, uint64_t size);
// Al empezar un envío: el programa pudo cambiar la memoria de cualquier render target.
void nx_gl_invalidate_all(void);
