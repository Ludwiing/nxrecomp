// Punto de entrada del ejecutable: carga los módulos en sus direcciones de la Switch,
// arranca el kernel (hilo principal, TLS, heap, pila) y entra al programa.
#include "nxkern.h"
#include "nxdisplay.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void call(Ctx *c, const char *name, uint64_t x0, uint64_t x1) {
    uint64_t a = nx_symbol(name);
    if (!a) return;
    if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] %s 0x%llx\n", name, (unsigned long long)a);
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
        for (int k = 0; k < nx_ninits; k++) {
            if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] constructor 0x%llx\n", (unsigned long long)nx_inits[k]);
            nx_dispatch(c, nx_inits[k]);
        }
        if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] nnMain 0x%llx\n", (unsigned long long)nx_main_addr);
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

static int configurar_renderer(void) {
    const char *renderer = getenv("NX_RENDERER");
    if (!renderer || !*renderer || !strcmp(renderer, "opengl")) return 1;
    if (strcmp(renderer, "vulkan")) {
        fprintf(stderr, "[nxrt] NX_RENDERER desconocido: %s\n", renderer);
        return 0;
    }
    // Zink executes this renderer's OpenGL API over Vulkan. This is a
    // compatibility backend, distinct from a direct NVN-to-Vulkan frontend.
    const char *mesa = "/usr/share/glvnd/egl_vendor.d/50_mesa.json";
    if (access(mesa, R_OK)) {
        fprintf(stderr, "[nxrt] Vulkan (Zink) necesita el proveedor EGL de Mesa: %s\n", mesa);
        return 0;
    }
    setenv("__EGL_VENDOR_LIBRARY_FILENAMES", mesa, 1);
    setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 1);
    setenv("GALLIUM_DRIVER", "zink", 1);
    setenv("SDL_RENDER_DRIVER", "vulkan", 1);
    fprintf(stderr, "[nxrt] renderer solicitado: Vulkan mediante Zink\n");
    return 1;
}

int main(void) {
    if (!configurar_renderer()) return 2;
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
