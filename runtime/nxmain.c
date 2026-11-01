// Punto de entrada del ejecutable: carga los módulos en sus direcciones de la Switch,
// arranca el kernel (hilo principal, TLS, heap, pila) y entra al programa.
#include "nxkern.h"
#include "nxdisplay.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void call(Ctx *c, const char *name, uint64_t x0, uint64_t x1) {
    uint64_t a = nx_symbol(name);
    if (!a) return;
    c->x[0] = x0;
    c->x[1] = x1;
    nx_dispatch(c, a);
}

static KThread *main_thread;

// El hilo del juego: arranca el programa como lo haría Horizon.
static void *guest_main(void *arg) {
    (void)arg;
    KThread *t = main_thread;
    nx_kernel_adopt(t);
    Ctx *c = t->ctx;
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
        _exit(0);
    }
    // Homebrew: x0 = configuración del cargador (ninguna), x1 = handle del hilo principal.
    // Si la entrada vuelve, es que el programa terminó y "volvió al cargador".
    c->x[0] = 0;
    c->x[1] = t->handle;
    nx_dispatch(c, nx_entry_addr);
    fflush(stdout);
    _exit(0);
}

int main(void) {
    for (int k = 0; k < nx_nmodules; k++) {
        const NxModule *m = &nx_modules[k];
        nx_map_fixed(m->base, align_up(m->size + m->bss, 0x1000), m->name);
        memcpy((void *)(uintptr_t)m->base, m->data, m->size); // el .bss ya viene en ceros
    }
    main_thread = nx_kernel_main_thread();
    nx_install_crash_handler();
    uint64_t stack = align_up(nx_main_stack, 0x1000);
    if (mprotect((void *)(uintptr_t)NX_STACK_BASE, stack, PROT_READ | PROT_WRITE)) return 1;
    nx_memory_add_stack(NX_STACK_BASE, stack);
    main_thread->ctx->sp = NX_STACK_BASE + stack;

    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 64 << 20); // pila del anfitrión para las llamadas anidadas del juego
    pthread_t g;
    pthread_create(&g, &a, guest_main, NULL);
    nx_display_run(); // el hilo principal es de la ventana; el proceso termina cuando el juego sale
    return 0;
}
