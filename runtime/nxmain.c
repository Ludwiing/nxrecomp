// Punto de entrada del ejecutable: carga la imagen del módulo en su dirección de la
// Switch, prepara la pila y el TLS y salta a la entrada recompilada.
#include "nxrt.h"

#include <stdio.h>

#define STACK_SIZE 0x100000ull         // 1 MiB, como el hilo principal de un homebrew
#define STACK_TOP 0x40000000ull
#define TLS_BASE 0x3FE00000ull         // región de 0x200 bytes a la que apunta TPIDRRO_EL0

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

int main(void) {
    const NxImage *im = &nx_image;
    uint64_t img_end = align_up(im->bss_start + im->bss_size, 0x1000);
    nx_map_fixed(im->base, img_end - im->base, "el módulo");
    memcpy((void *)(uintptr_t)im->base, im->data, im->size); // el .bss ya viene en ceros
    nx_map_fixed(STACK_TOP - STACK_SIZE, STACK_SIZE, "la pila");
    nx_map_fixed(TLS_BASE, 0x1000, "el TLS");

    Ctx c = {0};
    c.sp = STACK_TOP;
    c.tpidrro = TLS_BASE;
    c.excl_addr = ~0ull;
    c.x[1] = 0xFFFF8001; // pseudo-handle del hilo actual, como lo entrega el cargador

    nx_entry(&c);
    fflush(stdout);
    fprintf(stderr, "[nxrt] el programa volvió de su entrada sin llamar a svcExitProcess\n");
    return 1;
}
