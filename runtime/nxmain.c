// Punto de entrada del ejecutable: carga los módulos en sus direcciones de la Switch,
// arranca el kernel (hilo principal, TLS, heap, pila) y entra al programa.
#include "nxkern.h"
#include "nxdisplay.h"
#include "nxsvc.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>


static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void call(Ctx *c, const char *name, uint64_t x0, uint64_t x1) {
    uint64_t a = nx_symbol(name);
    if (!a) { // opcional: no todo programa con SDK exporta cada paso; con NX_TRACE_BOOT queda a la vista
        if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] %s ausente, se omite\n", name);
        return;
    }
    if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] %s 0x%llx\n", name, (unsigned long long)a);
    c->x[0] = x0;
    c->x[1] = x1;
    nx_dispatch(c, a);
}

// Horizon stores process arguments in guest memory before nninitStartup. Keep
// this tiny block in the low, otherwise unused part of the main stack so the
// SDK can read it later through ordinary guest pointers.
static void setup_host_args(Ctx *c) {
    const char *set_argc = "_ZN2nn2os11SetHostArgcEi";
    const char *set_argv = "_ZN2nn2os11SetHostArgvEPPc";
    if (!nx_symbol(set_argc) || !nx_symbol(set_argv) || nx_main_stack < 0x2000) return;
    uint64_t argv_addr = NX_STACK_BASE + 0x1000;
    uint64_t arg0_addr = argv_addr + 0x100;
    uint64_t argv[2] = {arg0_addr, 0};
    memcpy((void *)(uintptr_t)argv_addr, argv, sizeof argv);
    memcpy((void *)(uintptr_t)arg0_addr, "game", 5);
    call(c, set_argc, 1, 0);
    call(c, set_argv, argv_addr, 0);
}

static KThread *main_thread;

static void sdk_loader_ready(Ctx *c) {
    Ctx saved = *c;
    KThread *t = main_thread;
    uint64_t sdk_thread = c->tpidrro ? ld64(c->tpidrro + 0x1f8) : 0;
    if (sdk_thread) {
        st32(sdk_thread + 0x1b0, t->handle);
        if (getenv("NX_TRACE_BOOT"))
            fprintf(stderr, "[boot] adopted SDK main ThreadType %#llx -> handle %#x\n",
                    (unsigned long long)sdk_thread, t->handle);
    }
    *c = saved;
}

static void sdk_module_inits(Ctx *c) {
    Ctx saved = *c;
    for (int k = 0; k < nx_ninits; k++) {
        if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] constructor 0x%llx\n", (unsigned long long)nx_inits[k]);
        nx_dispatch(c, nx_inits[k]);
        // Constructors may initialize the current thread's TP and FP state.
        // Preserve those architectural changes while restoring Start's caller
        // registers before the next callback.
        saved.tpidr = c->tpidr;
        saved.tpidrro = c->tpidrro;
        saved.fpcr = c->fpcr;
        saved.fpsr = c->fpsr;
        *c = saved;
    }
    nx_tls_hooks_ready();
    *c = saved;
}

static void sdk_module_finis(Ctx *c) { (void)c; }

// La búsqueda global que rtld deja a nn::ro (g_pLookupGlobalAutoFunctionPointer): nombre en x0, dirección o 0 en x0.
// Busca en los símbolos globales de los módulos del arranque, ordenados por nombre (recomp/nxrecomp.py).
static int comparar_simbolo(const void *nombre, const void *s) {
    return strcmp((const char *)nombre, ((const NxSymbol *)s)->name);
}
uint64_t nx_ro_buscar_global(const char *nombre) {
    if (!&nx_nsimbolos_globales || !nombre) return 0;
    const NxSymbol *s = bsearch(nombre, nx_simbolos_globales, (size_t)nx_nsimbolos_globales, sizeof *s, comparar_simbolo);
    return s ? s->addr : 0;
}
static void ro_buscar_global(Ctx *c) {
    const char *nombre = (const char *)(uintptr_t)c->x[0];
    c->x[0] = nx_ro_buscar_global(nombre);
    if (!c->x[0] && nombre && getenv("NX_TRACE_BOOT"))
        fprintf(stderr, "[boot] nn::ro: %s no está en los módulos del arranque\n", nombre);
}

int nx_guest_host_call(Ctx *c, uint64_t addr) {
    if (addr == (uint64_t)(uintptr_t)ro_buscar_global) {
        ro_buscar_global(c);
        return 1;
    }
    if (addr == (uint64_t)(uintptr_t)sdk_loader_ready) {
        sdk_loader_ready(c);
        return 1;
    }
    if (addr == (uint64_t)(uintptr_t)sdk_module_inits) {
        sdk_module_inits(c);
        return 1;
    }
    if (addr == (uint64_t)(uintptr_t)sdk_module_finis) {
        sdk_module_finis(c);
        return 1;
    }
    return 0;
}

// El hilo del juego: arranca el programa como lo haría Horizon.
static void *guest_main(void *arg) {
    (void)arg;
    KThread *t = main_thread;
    nx_kernel_adopt(t);
    Ctx *c = t->ctx;
    if (nx_main_addr) {
        uint64_t start = nx_symbol("_ZN2nn4init5StartEmmPFvvES2_S2_");
        if (!start) {
            // Sin nn::init::Start: el orden de nxnative (dm/host/main.c), símbolo por símbolo y luego
            // constructores y nnMain. Antes este caso terminaba con _exit(3) (sustituir_test lo detectó).
            if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] sin nn::init::Start: arranque por símbolos\n");
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
        if (getenv("NX_TRACE_BOOT")) fprintf(stderr, "[boot] nn::init::Start 0x%llx\n", (unsigned long long)start);
        c->x[0] = t->handle;
        c->x[1] = 0;
        c->x[2] = (uint64_t)(uintptr_t)sdk_loader_ready;
        c->x[3] = (uint64_t)(uintptr_t)sdk_module_inits;
        c->x[4] = (uint64_t)(uintptr_t)sdk_module_finis;
        c->x[30] = 0;
        nx_dispatch(c, start);
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
    // Lo que rtld hace al terminar de enlazar y el recompilador no ejecuta: publicar su estado de nn::ro.
    if (&nx_nrtld_ro_vars && nx_nrtld_ro_vars) nx_rtld_publicar_ro(nx_rtld_ro_vars, nx_nrtld_ro_vars);
    // Módulos de nn::ro traducidos (nxrecomp.py --modulos-ro): ldr:ro los entrega y nn::ro los enlaza con la búsqueda.
    if (&nx_nmodulos_ro && nx_nmodulos_ro > 0) {
        static NxRoModulo ro[64];
        int n = 0;
        for (int k = 0; k < nx_nmodulos_ro && n < 64; k++) {
            const NxModule *m = &nx_modules[nx_modulos_ro[k].modulo];
            ro[n] = (NxRoModulo){m->name, m->base, m->size, m->bss, {0}};
            memcpy(ro[n++].sha256, nx_modulos_ro[k].sha256, 32);
        }
        nx_ro_modulos_traducidos(ro, n);
        if (&nx_nrtld_ro_vars && nx_nrtld_ro_vars)
            nx_rtld_publicar_busqueda(nx_rtld_ro_vars, nx_nrtld_ro_vars, (uint64_t)(uintptr_t)ro_buscar_global);
        fprintf(stderr, "[ro] %d módulo%s de nn::ro traducido%s, %d símbolos globales para enlazarlos\n", n,
                n == 1 ? "" : "s", n == 1 ? "" : "s", nx_nsimbolos_globales);
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
