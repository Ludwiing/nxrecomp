// Punto de entrada del ejecutable: carga los módulos en sus direcciones de la Switch,
// arranca el kernel (hilo principal, TLS, heap, pila) y entra al programa.
#include "nxkern.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void call(Ctx *c, const char *name, uint64_t x0, uint64_t x1) {
    uint64_t a = nx_symbol(name);
    if (!a) return;
    c->x[0] = x0;
    c->x[1] = x1;
    nx_dispatch(c, a);
}

int main(void) {
    for (int k = 0; k < nx_nmodules; k++) {
        const NxModule *m = &nx_modules[k];
        nx_map_fixed(m->base, align_up(m->size + m->bss, 0x1000), m->name);
        memcpy((void *)(uintptr_t)m->base, m->data, m->size); // el .bss ya viene en ceros
    }
    KThread *t = nx_kernel_main_thread();
    Ctx *c = t->ctx;
    uint64_t stack = align_up(nx_main_stack, 0x1000);
    if (mprotect((void *)(uintptr_t)NX_STACK_BASE, stack, PROT_READ | PROT_WRITE)) return 1;
    nx_memory_add_stack(NX_STACK_BASE, stack);
    c->sp = NX_STACK_BASE + stack;

    if (nx_main_addr) {
        // Juego con el SDK de Nintendo. Si están los símbolos de nn::init::Start, se sigue su
        // orden (el mismo que usa nxnative, dm/host/main.c); si no, constructores y nnMain.
        // OJO: el orden del SDK todavía no se probó con un juego real.
        call(c, "_init_libc0", 0, 0);
        call(c, "nnosInitialize", t->handle, 0);
        call(c, "_init_libc1", 0, 0);
        call(c, "nninitInitializeSdkModule", 0, 0);
        call(c, "nninitStartup", 0, 0);
        call(c, "_init_libc2", 0, 0);
        for (int k = 0; k < nx_ninits; k++) nx_dispatch(c, nx_inits[k]);
        nx_dispatch(c, nx_main_addr);
        fflush(stdout);
        return 0;
    }
    // Homebrew: x0 = configuración del cargador (ninguna), x1 = handle del hilo principal.
    c->x[0] = 0;
    c->x[1] = t->handle;
    nx_dispatch(c, nx_entry_addr);
    fflush(stdout);
    fprintf(stderr, "[nxrt] el programa volvió de su entrada sin llamar a svcExitProcess\n");
    return 1;
}
