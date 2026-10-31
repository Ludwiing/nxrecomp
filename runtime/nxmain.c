// Punto de entrada del ejecutable: carga los módulos en sus direcciones de la Switch,
// prepara la pila y el TLS, corre los constructores de cada módulo y entra al programa.
#include "nxrt.h"

#include <stdio.h>
#include <stdlib.h>

#define STACK_TOP 0x40000000ull
#define TLS_BASE 0x3FE00000ull         // región de 0x200 bytes a la que apunta TPIDRRO_EL0

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

int main(void) {
    for (int k = 0; k < nx_nmodules; k++) {
        const NxModule *m = &nx_modules[k];
        nx_map_fixed(m->base, align_up(m->size + m->bss, 0x1000), m->name);
        memcpy((void *)(uintptr_t)m->base, m->data, m->size); // el .bss ya viene en ceros
    }
    uint64_t stack = align_up(nx_main_stack, 0x1000);
    nx_map_fixed(STACK_TOP - stack, stack, "la pila");
    nx_map_fixed(TLS_BASE, 0x1000, "el TLS");

    Ctx c = {0};
    c.sp = STACK_TOP;
    c.tpidrro = TLS_BASE;
    c.excl_addr = ~0ull;

    if (nx_main_addr) {
        // Juego con SDK: el rtld de Horizon corre los constructores de cada módulo y después nnMain.
        for (int k = 0; k < nx_ninits; k++) nx_dispatch(&c, nx_inits[k]);
        nx_dispatch(&c, nx_main_addr);
        fflush(stdout);
        return 0;
    }
    // Homebrew: se entra por el inicio del primer módulo, con el pseudo-handle del hilo en x1.
    c.x[1] = 0xFFFF8001;
    nx_dispatch(&c, nx_entry_addr);
    fflush(stdout);
    fprintf(stderr, "[nxrt] el programa volvió de su entrada sin llamar a svcExitProcess\n");
    return 1;
}
