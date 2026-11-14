// NVN en el host (HLE). Tabla de funciones: nvnBootstrapLoader y nvnDeviceGetProcAddress entregan al juego direcciones
// sintéticas (NX_HLE_BASE + 16 k); nx_buscar las resuelve con nx_hle_lookup y el juego las llama como a cualquier
// función recompilada. nvnp_<nombre> implementa una función; las que faltan salen con su nombre (débiles).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "nxnvn.h"

#define NVN_PROC(n) void nvnp_##n(Ctx *c);
#include "nvn_procs.inc"
#undef NVN_PROC

static void hle_unknown_proc(Ctx *c) {
    nx_enter(c);
    nx_host_enter();
    ret_x(c, 0);
    nx_host_leave();
}

static void hle_unknown_init(Ctx *c) {
    nx_enter(c);
    nx_host_enter();
    ret_x(c, 1);
    nx_host_leave();
}

// El nvnp_nvnDeviceGetProcAddress de abajo reemplaza al de relleno que se genera para la misma fila.
#define nvnp_nvnDeviceGetProcAddress nvnp_relleno_nvnDeviceGetProcAddress
#define NVN_PROC(n)                                                                                             \
    __attribute__((weak)) void nvnp_##n(Ctx *c) {                                                               \
        (void)c;                                                                                                \
        fflush(stdout);                                                                                         \
        fprintf(stderr, "[RED] NVN: " #n " todavía no está implementada en el host\n");                         \
        exit(1);                                                                                                \
    }
#include "nvn_procs.inc"
#undef NVN_PROC
#undef nvnp_nvnDeviceGetProcAddress
void nvnp_nvnDeviceGetProcAddress(Ctx *c);

static NxFn hle_fns[2048];
static int nhle;
static pthread_mutex_t hle_lock = PTHREAD_MUTEX_INITIALIZER;

uint64_t nx_hle_register(NxFn fn) {
    pthread_mutex_lock(&hle_lock);
    if (nhle == (int)(sizeof hle_fns / sizeof *hle_fns)) {
        fprintf(stderr, "[RED] nxnvn: tabla de funciones del host llena\n");
        exit(1);
    }
    hle_fns[nhle] = fn;
    uint64_t a = NX_HLE_BASE + 16ull * nhle++;
    pthread_mutex_unlock(&hle_lock);
    return a;
}

NxFn nx_hle_lookup(uint64_t a) {
    if (a < NX_HLE_BASE || a >= NX_HLE_BASE + 16ull * (uint64_t)(sizeof hle_fns / sizeof *hle_fns) || (a & 15)) return 0;
    return hle_fns[(a - NX_HLE_BASE) / 16];
}

typedef struct { const char *name; uint64_t addr; } Proc;

#define NVN_PROC(n) +1
enum { PROC_COUNT = 0
#include "nvn_procs.inc"
};
#undef NVN_PROC

static Proc procs[PROC_COUNT];
static int nprocs;
static uint64_t unknown_proc_addr, unknown_init_addr;

static int by_name(const void *a, const void *b) { return strcmp(((const Proc *)a)->name, ((const Proc *)b)->name); }

// El juego también llega a estas funciones con un salto de cola (B/BR): nx_enter consume el retorno pendiente,
// o la siguiente función recompilada lo tomaría como propio y volvería a una dirección equivocada.
#define NVN_PROC(n)                                                                                             \
    static void hle_##n(Ctx *c) {                                                                               \
        nx_enter(c);                                                                                            \
        nx_host_enter(); /* dentro del runtime: sin interrupciones; un hilo mejor puede tomar el núcleo */       \
        nvnp_##n(c);                                                                                            \
        nx_host_leave();                                                                                        \
    }
#include "nvn_procs.inc"
#undef NVN_PROC

static void procs_register(void) {
    if (nprocs) return;
#define NVN_PROC(n) procs[nprocs++] = (Proc){#n, nx_hle_register(hle_##n)};
#include "nvn_procs.inc"
#undef NVN_PROC
    qsort(procs, (size_t)nprocs, sizeof *procs, by_name);
    // Hookshot and NXnative return callable stubs for versioned NVN names the
    // host does not implement. Returning NULL makes Switch 2 games branch to
    // address zero before they can decide whether the optional proc is used.
    unknown_proc_addr = nx_hle_register(hle_unknown_proc);
    unknown_init_addr = nx_hle_register(hle_unknown_init);
}

uint64_t nvn_proc_address(const char *name) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    static unsigned long long requests;
    pthread_once(&once, procs_register);
    if (!name) return 0;
    const Proc *p = bsearch(&(Proc){name, 0}, procs, (size_t)nprocs, sizeof *procs, by_name);
    if (getenv("NX_TRAZA_NVN")) {
        unsigned long long n = __atomic_add_fetch(&requests, 1, __ATOMIC_RELAXED);
        fprintf(stderr, "[nvn] proc #%llu \"%s\" -> %#llx%s\n", n, name,
                (unsigned long long)(p ? p->addr : (strstr(name, "Initialize") ? unknown_init_addr : unknown_proc_addr)),
                p ? "" : " (stub desconocido)");
    }
    return p ? p->addr : (strstr(name, "Initialize") ? unknown_init_addr : unknown_proc_addr);
}

// PFNNVNGENERICFUNC nvnDeviceGetProcAddress(const NVNdevice *device, const char *name)
void nvnp_nvnDeviceGetProcAddress(Ctx *c) { ret_x(c, nvn_proc_address((const char *)c->x[1])); }

// PFNNVNGENERICFUNC nvnBootstrapLoader(const char *name): sustituye al del juego (recomp/sustituciones.txt)
void nxhle_nvnBootstrapLoader(Ctx *c) {
    const char *name = (const char *)c->x[0];
    nx_host_enter();
    if (getenv("NX_TRAZA_NVN")) fprintf(stderr, "[nvn] nvnBootstrapLoader(\"%s\")\n", name ? name : "(null)");
    ret_x(c, nvn_proc_address(name));
    nx_host_leave();
}
