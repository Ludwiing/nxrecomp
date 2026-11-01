// El kernel de Horizon, implementado en el host: las SVC que usan el SDK de Nintendo y libnx.
// El código del juego (nn::os, libnx) corre traducido tal cual; solo lo de abajo se reemplaza.
// La semántica sigue el comportamiento público del kernel de Atmosphère (mesosphère):
//   hilos        0x08 CreateThread 0x09 StartThread 0x0A ExitThread 0x0B SleepThread
//                0x0C/0x0D prioridad 0x0E/0x0F núcleos 0x10 procesador actual 0x25 GetThreadId
//   mutex        0x1A ArbitrateLock 0x1B ArbitrateUnlock (palabra = handle del dueño | 0x40000000 si hay espera)
//   condiciones  0x1C WaitProcessWideKeyAtomic 0x1D SignalProcessWideKey
//   direcciones  0x34 WaitForAddress 0x35 SignalToAddress
//   eventos      0x45 CreateEvent 0x11 SignalEvent 0x12 ClearEvent 0x17 ResetSignal
//   espera       0x18 WaitSynchronization 0x19 CancelSynchronization 0x16 CloseHandle
//   memoria      0x01 SetHeapSize 0x04/0x05 Map/UnmapMemory 0x06 QueryMemory 0x2C/0x2D memoria física
//   información  0x29 GetInfo 0x1E GetSystemTick
// Cada hilo del juego es un pthread. Todas las esperas cuelgan de un solo candado global G:
// primero que sea correcto; el rendimiento se mide después.
#define _GNU_SOURCE
#include "nxkern.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define R_OK_ 0u
#define R_INVALID_SIZE 0xCA01u      // 101
#define R_OUT_OF_RES 0xCE01u        // 103
#define R_INVALID_ADDR 0xCC01u      // 102
#define R_INVALID_HANDLE 0xE401u    // 114
#define R_TIMEOUT 0xEA01u           // 117
#define R_CANCELLED 0xEC01u         // 118
#define R_INVALID_ENUM 0xF001u      // 120
#define R_INVALID_STATE 0xFA01u     // 125
#define R_INVALID_CMB 0xE001u       // 112 InvalidCombination
#define WAITERS 0x40000000u
#define H_CUR_THREAD 0xFFFF8000u
#define H_CUR_PROCESS 0xFFFF8001u

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sync_cv;          // se avisa con cualquier señal: WaitSynchronization lo escucha
static __thread KThread *cur;
static uint64_t wait_seq;

static struct timespec deadline(int64_t timeout_ns) {
    uint64_t t = nx_now_ns() + (uint64_t)timeout_ns;
    return (struct timespec){(time_t)(t / 1000000000ull), (long)(t % 1000000000ull)};
}

// ---------- handles ----------
#define MAX_HANDLES 4096
typedef struct {
    int type; // 0 libre
    void *obj;
    int writable;
} HandleEntry;
static HandleEntry handles[MAX_HANDLES];

uint32_t nx_handle_new(int type, void *obj, int writable) {
    for (int k = 1; k < MAX_HANDLES; k++)
        if (!handles[k].type) {
            handles[k] = (HandleEntry){type, obj, writable};
            return 0x1000u + (uint32_t)k;
        }
    return 0;
}
void *nx_handle_get(uint32_t h, int type) {
    if (type == KO_THREAD && h == H_CUR_THREAD) return cur;
    if (h < 0x1001u || h >= 0x1000u + MAX_HANDLES) return NULL;
    HandleEntry *e = &handles[h - 0x1000u];
    return e->type == type ? e->obj : NULL;
}
static HandleEntry *handle_entry(uint32_t h) {
    if (h < 0x1001u || h >= 0x1000u + MAX_HANDLES) return NULL;
    HandleEntry *e = &handles[h - 0x1000u];
    return e->type ? e : NULL;
}

// ---------- hilos ----------
#define MAX_THREADS 512
static KThread *threads[MAX_THREADS];
static uint64_t next_thread_id = 0x50;
static uint64_t tls_next = NX_TLS_BASE;

static KThread *thread_new(void) {
    KThread *t = calloc(1, sizeof *t);
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(&t->cv, &a);
    t->ctx = calloc(1, sizeof(Ctx));
    t->ctx->excl_addr = ~0ull;
    t->id = next_thread_id++;
    t->tls = tls_next;
    tls_next += 0x200;
    memset((void *)(uintptr_t)t->tls, 0, 0x200);
    t->ctx->tpidrro = t->tls;
    for (int k = 0; k < MAX_THREADS; k++)
        if (!threads[k]) {
            threads[k] = t;
            break;
        }
    return t;
}

KThread *nx_kernel_main_thread(void) {
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(&sync_cv, &a);
    nx_map_fixed(NX_TLS_BASE, NX_TLS_SIZE, "el TLS");
    nx_memory_init();
    pthread_mutex_lock(&G);
    KThread *t = thread_new();
    t->priority = 44;
    t->state = TS_RUNNING;
    t->handle = nx_handle_new(KO_THREAD, t, 1);
    cur = t;
    pthread_mutex_unlock(&G);
    return t;
}
KThread *nx_current_thread(void) { return cur; }

static void *thread_main(void *arg) {
    KThread *t = arg;
    cur = t;
    nx_dispatch(t->ctx, t->entry);
    nx_exit_thread();
    return NULL;
}

void nx_exit_thread(void) {
    pthread_mutex_lock(&G);
    cur->state = TS_EXITED;
    pthread_cond_broadcast(&sync_cv);
    pthread_mutex_unlock(&G);
    pthread_exit(NULL);
}

// Espera del hilo actual a que alguien lo despierte (o al plazo). Con G tomado.
static uint32_t sleep_until_woken(int64_t timeout_ns) {
    cur->woken = 0;
    cur->result = R_OK_;
    cur->seq = ++wait_seq;
    if (timeout_ns == 0) {
        cur->waiting = 0;
        return R_TIMEOUT;
    }
    struct timespec dl = deadline(timeout_ns);
    while (!cur->woken) {
        // Esperando una condición, el plazo vale; ya pasado a esperar el mutex de vuelta, no.
        int timed = timeout_ns > 0 && !(cur->waiting == W_MUTEX && cur->cv_timeout_ignored);
        if (!timed) {
            pthread_cond_wait(&cur->cv, &G);
        } else if (pthread_cond_timedwait(&cur->cv, &G, &dl) == ETIMEDOUT && !cur->woken &&
                   !(cur->waiting == W_MUTEX && cur->cv_timeout_ignored)) {
            cur->waiting = 0;
            return R_TIMEOUT;
        }
    }
    return cur->result;
}
static void wake(KThread *t, uint32_t result) {
    t->waiting = 0;
    t->woken = 1;
    t->result = result;
    pthread_cond_signal(&t->cv);
}

// El que espera con más prioridad (número menor); a igual prioridad, el que llegó primero.
static KThread *best_waiter(int kind, uint64_t addr, KThread *owner) {
    KThread *best = NULL;
    for (int k = 0; k < MAX_THREADS; k++) {
        KThread *t = threads[k];
        if (!t || t->waiting != kind || t->wait_addr != addr) continue;
        if (owner && t->lock_owner != owner) continue;
        if (!best || t->priority < best->priority || (t->priority == best->priority && t->seq < best->seq)) best = t;
    }
    return best;
}
static int count_waiters(int kind, uint64_t addr, KThread *owner) {
    int n = 0;
    for (int k = 0; k < MAX_THREADS; k++) {
        KThread *t = threads[k];
        if (t && t->waiting == kind && t->wait_addr == addr && (!owner || t->lock_owner == owner)) n++;
    }
    return n;
}

static uint32_t *u32p(uint64_t a) { return (uint32_t *)(uintptr_t)a; }

// ---------- mutex ----------
// Suelta el mutex en addr que tiene `owner`: se lo pasa al que espera con más prioridad.
static void release_mutex(KThread *owner, uint64_t addr) {
    KThread *next = best_waiter(W_MUTEX, addr, owner);
    if (!next) {
        __atomic_store_n(u32p(addr), 0, __ATOMIC_SEQ_CST);
        return;
    }
    int others = count_waiters(W_MUTEX, addr, owner) - 1;
    for (int k = 0; k < MAX_THREADS; k++) // los demás ahora esperan al nuevo dueño
        if (threads[k] && threads[k] != next && threads[k]->waiting == W_MUTEX && threads[k]->wait_addr == addr &&
            threads[k]->lock_owner == owner)
            threads[k]->lock_owner = next;
    __atomic_store_n(u32p(addr), next->tag | (others > 0 ? WAITERS : 0), __ATOMIC_SEQ_CST);
    wake(next, R_OK_);
}

static uint32_t arbitrate_lock(uint32_t owner_h, uint64_t addr, uint32_t tag) {
    if (addr & 3) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    uint32_t v = __atomic_load_n(u32p(addr), __ATOMIC_SEQ_CST);
    if (v != (owner_h | WAITERS)) { // ya cambió: el código de usuario vuelve a intentar
        pthread_mutex_unlock(&G);
        return R_OK_;
    }
    KThread *owner = nx_handle_get(owner_h, KO_THREAD);
    if (!owner) {
        pthread_mutex_unlock(&G);
        return R_INVALID_HANDLE;
    }
    cur->waiting = W_MUTEX;
    cur->wait_addr = addr;
    cur->tag = tag;
    cur->lock_owner = owner;
    cur->cv_timeout_ignored = 0;
    uint32_t r = sleep_until_woken(-1);
    pthread_mutex_unlock(&G);
    return r;
}

static uint32_t arbitrate_unlock(uint64_t addr) {
    if (addr & 3) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    release_mutex(cur, addr);
    pthread_mutex_unlock(&G);
    return R_OK_;
}

// ---------- variables de condición ----------
static uint32_t wait_key(uint64_t mutex_addr, uint64_t key, uint32_t tag, int64_t timeout) {
    pthread_mutex_lock(&G);
    release_mutex(cur, mutex_addr);
    cur->waiting = W_CONDVAR;
    cur->wait_addr = key;
    cur->mutex_addr = mutex_addr;
    cur->tag = tag;
    cur->cv_timeout_ignored = 0;
    __atomic_store_n(u32p(key), 1, __ATOMIC_SEQ_CST);
    uint32_t r = sleep_until_woken(timeout);
    if (r == R_TIMEOUT && count_waiters(W_CONDVAR, key, NULL) == 0) __atomic_store_n(u32p(key), 0, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&G);
    return r;
}

// Un hilo despertado de una condición vuelve a tomar su mutex, o se pone a esperarlo.
static void reacquire(KThread *t) {
    uint64_t addr = t->mutex_addr;
    for (;;) {
        uint32_t v = __atomic_load_n(u32p(addr), __ATOMIC_SEQ_CST);
        if (v == 0) {
            if (__atomic_compare_exchange_n(u32p(addr), &v, t->tag, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                wake(t, R_OK_);
                return;
            }
            continue;
        }
        if (!(v & WAITERS) &&
            !__atomic_compare_exchange_n(u32p(addr), &v, v | WAITERS, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            continue;
        KThread *owner = nx_handle_get(v & ~WAITERS, KO_THREAD);
        if (!owner) {
            wake(t, R_INVALID_HANDLE);
            return;
        }
        t->waiting = W_MUTEX;
        t->wait_addr = addr;
        t->lock_owner = owner;
        t->cv_timeout_ignored = 1;
        return;
    }
}

static uint32_t signal_key(uint64_t key, int32_t count) {
    pthread_mutex_lock(&G);
    int n = 0;
    while (count <= 0 || n < count) {
        KThread *t = best_waiter(W_CONDVAR, key, NULL);
        if (!t) break;
        n++;
        reacquire(t);
    }
    if (count_waiters(W_CONDVAR, key, NULL) == 0) __atomic_store_n(u32p(key), 0, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&G);
    return R_OK_;
}

// ---------- árbitro de direcciones ----------
static uint32_t wait_for_address(uint64_t addr, uint32_t type, int32_t value, int64_t timeout) {
    if (addr & 3) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    int32_t v = (int32_t)__atomic_load_n(u32p(addr), __ATOMIC_SEQ_CST);
    int wait;
    if (type == 0) wait = v < value;                    // WaitIfLessThan
    else if (type == 1) {                               // DecrementAndWaitIfLessThan
        wait = v < value;
        if (wait) __atomic_store_n(u32p(addr), (uint32_t)(v - 1), __ATOMIC_SEQ_CST);
    } else if (type == 2) wait = v == value;            // WaitIfEqual
    else {
        pthread_mutex_unlock(&G);
        return R_INVALID_ENUM;
    }
    if (!wait) {
        pthread_mutex_unlock(&G);
        return R_INVALID_STATE;
    }
    cur->waiting = W_ADDRESS;
    cur->wait_addr = addr;
    uint32_t r = sleep_until_woken(timeout);
    pthread_mutex_unlock(&G);
    return r;
}

static uint32_t signal_to_address(uint64_t addr, uint32_t type, int32_t value, int32_t count) {
    if (addr & 3) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    if (type == 1 || type == 2) {
        int32_t nv;
        if (type == 1) nv = value + 1;                  // SignalAndIncrementIfEqual
        else {                                          // SignalAndModifyByWaitingCountIfEqual
            int w = count_waiters(W_ADDRESS, addr, NULL);
            if (count <= 0) nv = w > 0 ? value - 1 : value + 1;
            else if (w == 0) nv = value + 1;
            else if (w <= count) nv = value - 1;
            else nv = value;
        }
        uint32_t e = (uint32_t)value;
        if (!__atomic_compare_exchange_n(u32p(addr), &e, (uint32_t)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            pthread_mutex_unlock(&G);
            return R_INVALID_STATE;
        }
    } else if (type != 0) {
        pthread_mutex_unlock(&G);
        return R_INVALID_ENUM;
    }
    for (int n = 0; count <= 0 || n < count; n++) {
        KThread *t = best_waiter(W_ADDRESS, addr, NULL);
        if (!t) break;
        wake(t, R_OK_);
    }
    pthread_mutex_unlock(&G);
    return R_OK_;
}

// ---------- objetos sincronizables ----------
static int signaled(HandleEntry *e) {
    if (e->type == KO_THREAD) return ((KThread *)e->obj)->state == TS_EXITED;
    if (e->type == KO_EVENT) return ((KEvent *)e->obj)->signaled;
    if (e->type == KO_SESSION) return 0;
    return 0;
}

static uint32_t wait_sync(uint64_t hptr, int32_t n, int64_t timeout, uint64_t *index) {
    if (n < 0 || n > 0x40) return R_OUT_OF_RES;
    const uint32_t *hs = (const uint32_t *)(uintptr_t)hptr;
    pthread_mutex_lock(&G);
    struct timespec dl = timeout > 0 ? deadline(timeout) : (struct timespec){0};
    for (;;) {
        if (cur->cancel) {
            cur->cancel = 0;
            pthread_mutex_unlock(&G);
            return R_CANCELLED;
        }
        for (int k = 0; k < n; k++) {
            HandleEntry *e = hs[k] == H_CUR_THREAD ? NULL : handle_entry(hs[k]);
            if (!e) {
                pthread_mutex_unlock(&G);
                return R_INVALID_HANDLE;
            }
            if (signaled(e)) {
                *index = (uint64_t)k;
                pthread_mutex_unlock(&G);
                return R_OK_;
            }
        }
        if (timeout == 0) break;
        cur->waiting = W_SYNC;
        int rc = timeout > 0 ? pthread_cond_timedwait(&sync_cv, &G, &dl) : pthread_cond_wait(&sync_cv, &G);
        cur->waiting = 0;
        if (rc == ETIMEDOUT) break;
    }
    pthread_mutex_unlock(&G);
    return R_TIMEOUT;
}

// ---------- memoria ----------
// Disposición de un proceso de 39 bits (como Horizon): código en 0x8000000, el heap, la
// región alias (memoria física del SDK) y la región de pilas, cada una en su lugar fijo.
typedef struct {
    uint64_t addr, size;
    uint32_t type, perm;
} Region;
#define MAX_REGIONS 1024
static Region regions[MAX_REGIONS];
static int nregions;
static uint64_t heap_size;

static void region_add(uint64_t addr, uint64_t size, uint32_t type, uint32_t perm) {
    if (nregions < MAX_REGIONS) regions[nregions++] = (Region){addr, size, type, perm};
}
static void region_remove(uint64_t addr) {
    for (int k = 0; k < nregions; k++)
        if (regions[k].addr == addr) {
            regions[k] = regions[--nregions];
            return;
        }
}
static void reserve(uint64_t addr, uint64_t size, const char *what) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0);
    if (p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "[nxrt] no pude reservar %s en 0x%llx\n", what, (unsigned long long)addr);
        exit(1);
    }
}

void nx_memory_init(void) {
    reserve(NX_HEAP_BASE, NX_HEAP_MAX, "el heap");
    reserve(NX_ALIAS_BASE, NX_ALIAS_SIZE, "la región alias");
    reserve(NX_STACK_BASE, NX_STACK_SIZE, "la región de pilas");
    for (int k = 0; k < nx_nmodules; k++) {
        const NxModule *m = &nx_modules[k];
        region_add(m->base, (m->size + m->bss + 0xFFF) & ~0xFFFull, 0x03, 0x5); // código estático
    }
    region_add(NX_TLS_BASE, NX_TLS_SIZE, 0x0C, 0x3);
}

void nx_memory_add_stack(uint64_t addr, uint64_t size) { region_add(addr, size, 0x0B, 0x3); }

static uint32_t set_heap_size(uint64_t size, uint64_t *out) {
    if (size & 0x1FFFFF || size > NX_HEAP_MAX) return R_INVALID_SIZE; // múltiplo de 2 MiB
    pthread_mutex_lock(&G);
    if (size > heap_size)
        mprotect((void *)(uintptr_t)(NX_HEAP_BASE + heap_size), size - heap_size, PROT_READ | PROT_WRITE);
    else if (size < heap_size) {
        void *p = (void *)(uintptr_t)(NX_HEAP_BASE + size);
        madvise(p, heap_size - size, MADV_DONTNEED);
        mprotect(p, heap_size - size, PROT_NONE);
    }
    region_remove(NX_HEAP_BASE);
    if (size) region_add(NX_HEAP_BASE, size, 0x05, 0x3);
    heap_size = size;
    pthread_mutex_unlock(&G);
    *out = NX_HEAP_BASE;
    return R_OK_;
}

// MapMemory: el SDK mueve la pila de un hilo desde el heap a la región de pilas.
// En la consola las dos direcciones comparten las páginas; aquí se copian al mapear y al desmapear.
static uint32_t map_memory(uint64_t dst, uint64_t src, uint64_t size) {
    if ((dst | src | size) & 0xFFF || !size) return R_INVALID_SIZE;
    if (dst < NX_STACK_BASE || dst + size > NX_STACK_BASE + NX_STACK_SIZE) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    mprotect((void *)(uintptr_t)dst, size, PROT_READ | PROT_WRITE);
    memcpy((void *)(uintptr_t)dst, (void *)(uintptr_t)src, size);
    region_add(dst, size, 0x0B, 0x3);
    pthread_mutex_unlock(&G);
    return R_OK_;
}
static uint32_t unmap_memory(uint64_t dst, uint64_t src, uint64_t size) {
    if ((dst | src | size) & 0xFFF || !size) return R_INVALID_SIZE;
    pthread_mutex_lock(&G);
    memcpy((void *)(uintptr_t)src, (void *)(uintptr_t)dst, size);
    madvise((void *)(uintptr_t)dst, size, MADV_DONTNEED);
    mprotect((void *)(uintptr_t)dst, size, PROT_NONE);
    region_remove(dst);
    pthread_mutex_unlock(&G);
    return R_OK_;
}
static uint32_t map_physical(uint64_t addr, uint64_t size, int map) {
    if ((addr | size) & 0xFFF || !size) return R_INVALID_SIZE;
    if (addr < NX_ALIAS_BASE || addr + size > NX_ALIAS_BASE + NX_ALIAS_SIZE) return R_INVALID_ADDR;
    pthread_mutex_lock(&G);
    if (map) {
        mprotect((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE);
        region_add(addr, size, 0x05, 0x3);
    } else {
        madvise((void *)(uintptr_t)addr, size, MADV_DONTNEED);
        mprotect((void *)(uintptr_t)addr, size, PROT_NONE);
        region_remove(addr);
    }
    pthread_mutex_unlock(&G);
    return R_OK_;
}

static void query_memory(uint64_t out, uint64_t addr) {
    pthread_mutex_lock(&G);
    uint64_t lo = 0, hi = 1ull << 39;
    uint32_t type = 0, perm = 0;
    for (int k = 0; k < nregions; k++) {
        Region *r = &regions[k];
        if (addr >= r->addr && addr < r->addr + r->size) {
            lo = r->addr, hi = r->addr + r->size, type = r->type, perm = r->perm;
            break;
        }
        if (r->addr > addr && r->addr < hi) hi = r->addr;
        if (r->addr + r->size <= addr && r->addr + r->size > lo) lo = r->addr + r->size;
    }
    if (addr >= 1ull << 39) lo = 1ull << 39, hi = 0, type = 0x10; // fuera del espacio de direcciones
    pthread_mutex_unlock(&G);
    uint64_t *o = (uint64_t *)(uintptr_t)out;
    o[0] = lo;
    o[1] = hi - lo;
    uint32_t *o32 = (uint32_t *)(o + 2);
    o32[0] = type, o32[1] = 0, o32[2] = perm, o32[3] = 0, o32[4] = 0, o32[5] = 0;
}

static uint32_t get_info(uint32_t id, uint32_t h, uint64_t sub, uint64_t *out) {
    switch (id) {
    case 0: *out = 0xF; return R_OK_;                        // núcleos permitidos
    case 1: *out = 0x3FFFFFFFFFFFFFFFull; return R_OK_;      // prioridades permitidas
    case 2: *out = NX_ALIAS_BASE; return R_OK_;
    case 3: *out = NX_ALIAS_SIZE; return R_OK_;
    case 4: *out = NX_HEAP_BASE; return R_OK_;
    case 5: *out = NX_HEAP_MAX; return R_OK_;
    case 6: case 21: *out = 0xCD500000ull; return R_OK_;     // memoria total de una aplicación (3285 MiB)
    case 7: case 22: *out = heap_size; return R_OK_;
    case 8: *out = 0; return R_OK_;                          // sin depurador
    case 9: *out = 0; return R_OK_;                          // límite de recursos (handle)
    case 10: *out = 0; return R_OK_;                         // ticks ociosos
    case 11: *out = nx_ticks() * 0x9E3779B97F4A7C15ull ^ (sub * 0xBF58476D1CE4E5B9ull); return R_OK_; // entropía
    case 12: *out = 0x8000000ull; return R_OK_;              // región ASLR
    case 13: *out = (1ull << 39) - 0x8000000ull; return R_OK_;
    case 14: *out = NX_STACK_BASE; return R_OK_;
    case 15: *out = NX_STACK_SIZE; return R_OK_;
    case 16: case 17: *out = 0; return R_OK_;                // recurso del sistema
    case 18: *out = 0x0100000000001000ull; return R_OK_;     // id de programa
    case 20: *out = 0; return R_OK_;                         // contexto de excepción
    case 23: *out = 1; return R_OK_;                         // es una aplicación
    case 24: *out = 256; return R_OK_;                       // hilos libres
    case 0xF0000002u: *out = nx_now_ns() * 12 / 625; return R_OK_; // ticks del hilo
    }
    (void)h;
    fprintf(stderr, "[RED] GetInfo(%u, 0x%x, %llu) no implementado\n", id, h, (unsigned long long)sub);
    return R_INVALID_ENUM;
}

// ---------- la tabla de SVC ----------
void nx_svc(Ctx *c, uint32_t imm) {
    uint64_t *x = c->x, out = 0;
    switch (imm) {
    case 0x01: x[0] = set_heap_size(x[1], &out); x[1] = out; return;
    case 0x02: case 0x03: x[0] = R_OK_; return;          // permisos y atributos de memoria
    case 0x04: x[0] = map_memory(x[0], x[1], x[2]); return;
    case 0x05: x[0] = unmap_memory(x[0], x[1], x[2]); return;
    case 0x06: query_memory(x[0], x[2]); x[0] = R_OK_; x[1] = 0; return;
    case 0x07: fflush(stdout); _exit(0);
    case 0x08: { // CreateThread(entry, arg, stack_top, priority, core)
        if ((uint32_t)x[4] > 63) { x[0] = 0xE201u; return; } // InvalidPriority
        pthread_mutex_lock(&G);
        KThread *t = thread_new();
        t->entry = x[1];
        t->ctx->x[0] = x[2];
        t->ctx->sp = x[3];
        t->priority = (int)(uint32_t)x[4];
        t->core = (int32_t)x[5] < 0 ? 0 : (int)x[5];
        t->state = TS_CREATED;
        t->handle = nx_handle_new(KO_THREAD, t, 1);
        pthread_mutex_unlock(&G);
        x[0] = t->handle ? R_OK_ : R_OUT_OF_RES;
        x[1] = t->handle;
        return;
    }
    case 0x09: { // StartThread
        pthread_mutex_lock(&G);
        KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD);
        if (!t || t->state != TS_CREATED) {
            pthread_mutex_unlock(&G);
            x[0] = t ? R_INVALID_STATE : R_INVALID_HANDLE;
            return;
        }
        t->state = TS_RUNNING;
        pthread_mutex_unlock(&G);
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 16 << 20); // pila del host: las llamadas del juego se anidan en ella
        pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
        pthread_create(&t->pthread, &a, thread_main, t);
        x[0] = R_OK_;
        return;
    }
    case 0x0A: nx_exit_thread();
    case 0x0B: { // SleepThread(ns): 0, -1 y -2 ceden el procesador
        int64_t ns = (int64_t)x[0];
        if (ns <= 0) sched_yield();
        else {
            struct timespec ts = {ns / 1000000000, ns % 1000000000};
            while (nanosleep(&ts, &ts) && errno == EINTR) {}
        }
        return;
    }
    case 0x0C: { KThread *t = nx_handle_get((uint32_t)x[1], KO_THREAD); x[0] = t ? R_OK_ : R_INVALID_HANDLE; x[1] = t ? (uint64_t)t->priority : 0; return; }
    case 0x0D: { KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD); if (t) t->priority = (int)x[1]; x[0] = t ? R_OK_ : R_INVALID_HANDLE; return; }
    case 0x0E: { KThread *t = nx_handle_get((uint32_t)x[2], KO_THREAD); x[0] = t ? R_OK_ : R_INVALID_HANDLE; x[1] = t ? (uint64_t)t->core : 0; x[2] = 0xF; return; }
    case 0x0F: { KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD); if (t && (int32_t)x[1] >= 0) t->core = (int)x[1]; x[0] = t ? R_OK_ : R_INVALID_HANDLE; return; }
    case 0x10: { int cpu = sched_getcpu(); x[0] = (uint64_t)(cpu < 0 ? 0 : cpu % 4); return; }
    case 0x11: case 0x12: case 0x17: { // SignalEvent, ClearEvent, ResetSignal
        pthread_mutex_lock(&G);
        HandleEntry *e = handle_entry((uint32_t)x[0]);
        if (!e || e->type != KO_EVENT || (imm == 0x11 && !e->writable)) {
            x[0] = R_INVALID_HANDLE;
        } else {
            KEvent *ev = e->obj;
            if (imm == 0x11) {
                ev->signaled = 1;
                pthread_cond_broadcast(&sync_cv);
                x[0] = R_OK_;
            } else if (imm == 0x17 && !ev->signaled) {
                x[0] = R_INVALID_STATE;
            } else {
                ev->signaled = 0;
                x[0] = R_OK_;
            }
        }
        pthread_mutex_unlock(&G);
        return;
    }
    case 0x16: { // CloseHandle
        pthread_mutex_lock(&G);
        HandleEntry *e = handle_entry((uint32_t)x[0]);
        if (e) {
            if (e->type == KO_SESSION) nx_session_close(e->obj);
            *e = (HandleEntry){0};
        }
        x[0] = e || (uint32_t)x[0] == H_CUR_THREAD || (uint32_t)x[0] == H_CUR_PROCESS ? R_OK_ : R_INVALID_HANDLE;
        pthread_mutex_unlock(&G);
        return;
    }
    case 0x18: x[0] = wait_sync(x[1], (int32_t)x[2], (int64_t)x[3], &out); x[1] = out; return;
    case 0x19: { // CancelSynchronization
        pthread_mutex_lock(&G);
        KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD);
        if (t) {
            t->cancel = 1;
            pthread_cond_broadcast(&sync_cv);
        }
        pthread_mutex_unlock(&G);
        x[0] = t ? R_OK_ : R_INVALID_HANDLE;
        return;
    }
    case 0x1A: x[0] = arbitrate_lock((uint32_t)x[0], x[1], (uint32_t)x[2]); return;
    case 0x1B: x[0] = arbitrate_unlock(x[0]); return;
    case 0x1C: x[0] = wait_key(x[0], x[1], (uint32_t)x[2], (int64_t)x[3]); return;
    case 0x1D: x[0] = signal_key(x[0], (int32_t)x[1]); return;
    case 0x1E: x[0] = nx_ticks(); return;
    case 0x1F: x[0] = nx_connect_named_port((const char *)(uintptr_t)x[1], &out); x[1] = out; return;
    case 0x21: x[0] = nx_send_sync_request(c, (uint32_t)x[0]); return;
    case 0x25: { KThread *t = nx_handle_get((uint32_t)x[1], KO_THREAD); x[0] = t ? R_OK_ : R_INVALID_HANDLE; x[1] = t ? t->id : 0; return; }
    case 0x26:
        fflush(stdout);
        fprintf(stderr, "[nxrt] svcBreak(0x%llx, 0x%llx, 0x%llx)\n", (unsigned long long)x[0], (unsigned long long)x[1],
                (unsigned long long)x[2]);
        _exit(1);
    case 0x27:
        fwrite((const void *)(uintptr_t)x[0], 1, x[1], stdout);
        fflush(stdout);
        x[0] = R_OK_;
        return;
    case 0x28: x[0] = R_OK_; return;                     // ReturnFromException
    case 0x29: x[0] = get_info((uint32_t)x[1], (uint32_t)x[2], x[3], &out); x[1] = out; return;
    case 0x2C: x[0] = map_physical(x[0], x[1], 1); return;
    case 0x2D: x[0] = map_physical(x[0], x[1], 0); return;
    case 0x34: x[0] = wait_for_address(x[0], (uint32_t)x[1], (int32_t)x[2], (int64_t)x[3]); return;
    case 0x35: x[0] = signal_to_address(x[0], (uint32_t)x[1], (int32_t)x[2], (int32_t)x[3]); return;
    case 0x45: { // CreateEvent → escritura, lectura
        pthread_mutex_lock(&G);
        KEvent *ev = calloc(1, sizeof *ev);
        x[1] = nx_handle_new(KO_EVENT, ev, 1);
        x[2] = nx_handle_new(KO_EVENT, ev, 0);
        pthread_mutex_unlock(&G);
        x[0] = R_OK_;
        return;
    }
    }
    fflush(stdout);
    fprintf(stderr, "[RED] svc 0x%x no implementado\n", imm);
    exit(1);
}

// Lo que toca la tabla de handles desde fuera (servicios) lo hace con G tomado.
void nx_kernel_lock(void) { pthread_mutex_lock(&G); }
void nx_kernel_unlock(void) { pthread_mutex_unlock(&G); }
