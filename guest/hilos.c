// Prueba del kernel: hilos, mutex y variables de condición con el protocolo de Horizon
// (el mismo algoritmo que libnx: LDAXR/STLXR en la palabra del mutex y svcArbitrateLock
// cuando hay que esperar), semáforos con el árbitro de direcciones, eventos, esperas con
// plazo, heap, MapMemory, QueryMemory y GetInfo. Los resultados son fijos: se comparan con
// guest/esperado/hilos.txt.
#include "mini.h"

#define WAIT_MASK 0x40000000u

// ---------- SVC ----------
#define SVC(n, ...) __asm__ volatile("svc #" #n : __VA_ARGS__ : "memory")

static u32 svc_create_thread(u32 *h, void (*entry)(void *), void *arg, void *stack_top, int prio, int core) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1") = (u64)entry;
    register u64 x2 __asm__("x2") = (u64)arg;
    register u64 x3 __asm__("x3") = (u64)stack_top;
    register u64 x4 __asm__("x4") = (u64)prio;
    register u64 x5 __asm__("x5") = (u64)core;
    SVC(0x08, "=r"(x0), "+r"(x1) : "r"(x2), "r"(x3), "r"(x4), "r"(x5));
    *h = (u32)x1;
    return (u32)x0;
}
static u32 svc_start_thread(u32 h) {
    register u64 x0 __asm__("x0") = h;
    SVC(0x09, "+r"(x0) :);
    return (u32)x0;
}
__attribute__((noreturn)) static void svc_exit_thread(void) {
    __asm__ volatile("svc #0x0a");
    __builtin_unreachable();
}
static void svc_sleep(s64 ns) {
    register u64 x0 __asm__("x0") = (u64)ns;
    SVC(0x0b, "+r"(x0) :);
}
static u32 svc_arbitrate_lock(u32 owner, u32 *addr, u32 tag) {
    register u64 x0 __asm__("x0") = owner;
    register u64 x1 __asm__("x1") = (u64)addr;
    register u64 x2 __asm__("x2") = tag;
    SVC(0x1a, "+r"(x0) : "r"(x1), "r"(x2));
    return (u32)x0;
}
static u32 svc_arbitrate_unlock(u32 *addr) {
    register u64 x0 __asm__("x0") = (u64)addr;
    SVC(0x1b, "+r"(x0) :);
    return (u32)x0;
}
static u32 svc_wait_key(u32 *mutex, u32 *key, u32 tag, s64 timeout) {
    register u64 x0 __asm__("x0") = (u64)mutex;
    register u64 x1 __asm__("x1") = (u64)key;
    register u64 x2 __asm__("x2") = tag;
    register u64 x3 __asm__("x3") = (u64)timeout;
    SVC(0x1c, "+r"(x0) : "r"(x1), "r"(x2), "r"(x3));
    return (u32)x0;
}
static void svc_signal_key(u32 *key, s32 count) {
    register u64 x0 __asm__("x0") = (u64)key;
    register u64 x1 __asm__("x1") = (u64)(u32)count;
    SVC(0x1d, "+r"(x0) : "r"(x1));
}
static u32 svc_wait_address(s32 *addr, u32 type, s32 value, s64 timeout) {
    register u64 x0 __asm__("x0") = (u64)addr;
    register u64 x1 __asm__("x1") = type;
    register u64 x2 __asm__("x2") = (u64)(u32)value;
    register u64 x3 __asm__("x3") = (u64)timeout;
    SVC(0x34, "+r"(x0) : "r"(x1), "r"(x2), "r"(x3));
    return (u32)x0;
}
static u32 svc_signal_address(s32 *addr, u32 type, s32 value, s32 count) {
    register u64 x0 __asm__("x0") = (u64)addr;
    register u64 x1 __asm__("x1") = type;
    register u64 x2 __asm__("x2") = (u64)(u32)value;
    register u64 x3 __asm__("x3") = (u64)(u32)count;
    SVC(0x35, "+r"(x0) : "r"(x1), "r"(x2), "r"(x3));
    return (u32)x0;
}
static u32 svc_create_event(u32 *wr, u32 *rd) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1");
    register u64 x2 __asm__("x2");
    SVC(0x45, "=r"(x0), "=r"(x1), "=r"(x2) :);
    *wr = (u32)x1;
    *rd = (u32)x2;
    return (u32)x0;
}
static u32 svc_handle_op(int which, u32 h) { // 0x11 SignalEvent, 0x12 ClearEvent, 0x16 CloseHandle
    register u64 x0 __asm__("x0") = h;
    if (which == 0x11) SVC(0x11, "+r"(x0) :);
    else if (which == 0x12) SVC(0x12, "+r"(x0) :);
    else SVC(0x16, "+r"(x0) :);
    return (u32)x0;
}
static u32 svc_wait_sync(s32 *index, const u32 *handles, s32 n, s64 timeout) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1") = (u64)handles;
    register u64 x2 __asm__("x2") = (u64)n;
    register u64 x3 __asm__("x3") = (u64)timeout;
    SVC(0x18, "=r"(x0), "+r"(x1) : "r"(x2), "r"(x3));
    *index = (s32)x1;
    return (u32)x0;
}
static u32 svc_set_heap_size(void **out, u64 size) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1") = size;
    SVC(0x01, "=r"(x0), "+r"(x1) :);
    *out = (void *)x1;
    return (u32)x0;
}
static u32 svc_map_memory(void *dst, void *src, u64 size, int unmap) {
    register u64 x0 __asm__("x0") = (u64)dst;
    register u64 x1 __asm__("x1") = (u64)src;
    register u64 x2 __asm__("x2") = size;
    if (unmap) SVC(0x05, "+r"(x0) : "r"(x1), "r"(x2));
    else SVC(0x04, "+r"(x0) : "r"(x1), "r"(x2));
    return (u32)x0;
}
typedef struct {
    u64 addr, size;
    u32 type, attr, perm, ipc, dev, pad;
} MemInfo;
static u32 svc_query_memory(MemInfo *mi, u64 addr) {
    register u64 x0 __asm__("x0") = (u64)mi;
    register u64 x1 __asm__("x1");
    register u64 x2 __asm__("x2") = addr;
    SVC(0x06, "+r"(x0), "=r"(x1) : "r"(x2));
    return (u32)x0;
}
static u32 svc_get_info(u64 *out, u32 id, u32 handle, u64 sub) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1") = id;
    register u64 x2 __asm__("x2") = handle;
    register u64 x3 __asm__("x3") = sub;
    SVC(0x29, "=r"(x0), "+r"(x1) : "r"(x2), "r"(x3));
    *out = x1;
    return (u32)x0;
}

// ---------- como libnx: el handle de cada hilo en su TLS ----------
static u8 *tls(void) {
    u64 v;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(v));
    return (u8 *)v;
}
static u32 self_handle(void) { return *(u32 *)(tls() + 0x1E4); }

static void mutex_lock(u32 *m) {
    u32 self = self_handle();
    for (;;) {
        u32 v = 0;
        if (__atomic_compare_exchange_n(m, &v, self, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
        if (!(v & WAIT_MASK)) {
            u32 e = v;
            if (!__atomic_compare_exchange_n(m, &e, v | WAIT_MASK, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) continue;
        }
        svc_arbitrate_lock(v & ~WAIT_MASK, m, self);
        if ((__atomic_load_n(m, __ATOMIC_ACQUIRE) & ~WAIT_MASK) == self) return;
    }
}
static void mutex_unlock(u32 *m) {
    u32 self = self_handle();
    if (!__atomic_compare_exchange_n(m, &self, 0, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) svc_arbitrate_unlock(m);
}
static u32 condvar_wait(u32 *cv, u32 *m, s64 timeout) {
    u32 r = svc_wait_key(m, cv, self_handle(), timeout);
    if (r == 0xEA01) mutex_lock(m); // con plazo vencido el kernel no devuelve el mutex
    return r;
}

static void sem_wait(s32 *s) {
    for (;;) {
        s32 v = __atomic_load_n(s, __ATOMIC_ACQUIRE);
        if (v > 0) {
            if (__atomic_compare_exchange_n(s, &v, v - 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
            continue;
        }
        svc_wait_address(s, 0, 1, -1); // espera mientras *s < 1
    }
}
static void sem_post(s32 *s) {
    __atomic_fetch_add(s, 1, __ATOMIC_RELEASE);
    svc_signal_address(s, 0, 0, 1);
}

// ---------- hilos ----------
typedef struct {
    u32 handle;
    void (*fn)(void *);
    void *arg;
} Start;

static void thread_entry(void *p) {
    Start *s = p;
    *(u32 *)(tls() + 0x1E4) = s->handle;
    s->fn(s->arg);
    svc_exit_thread();
}

static u8 *heap, *heap_top;
static Start starts[16];
static int nstarts;

static u32 spawn(void (*fn)(void *), void *arg, void *stack_top) {
    Start *s = &starts[nstarts++];
    s->fn = fn;
    s->arg = arg;
    if (!stack_top) {
        heap_top -= 0x10000; // 64 KiB de pila del heap, como libnx
        stack_top = heap_top + 0x10000;
    }
    svc_create_thread(&s->handle, thread_entry, s, stack_top, 44, -2);
    svc_start_thread(s->handle);
    return s->handle;
}
static void join(u32 h) {
    s32 idx;
    svc_wait_sync(&idx, &h, 1, -1);
    svc_handle_op(0x16, h);
}

// 1. contador protegido por mutex
static u32 mtx;
static u64 counter;
static void count_with_mutex(void *arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++) {
        mutex_lock(&mtx);
        counter++;
        mutex_unlock(&mtx);
    }
}

// 2. contador atómico (LDXR/STXR)
static u64 atomic_counter;
static void count_atomic(void *arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++) __atomic_fetch_add(&atomic_counter, 1, __ATOMIC_RELAXED);
}

// 3. productor y consumidores con variables de condición
#define QCAP 4
static u32 qmtx, q_not_empty, q_not_full;
static u32 queue[QCAP];
static int qhead, qcount, done;
static u64 consumed_sum, consumed_n;
static void consumer(void *arg) {
    (void)arg;
    for (;;) {
        mutex_lock(&qmtx);
        while (!qcount && !done) condvar_wait(&q_not_empty, &qmtx, -1);
        if (!qcount && done) {
            mutex_unlock(&qmtx);
            return;
        }
        u32 v = queue[qhead];
        qhead = (qhead + 1) % QCAP;
        qcount--;
        consumed_sum += v;
        consumed_n++;
        svc_signal_key(&q_not_full, 1);
        mutex_unlock(&qmtx);
    }
}

// 4. ping-pong con semáforos del árbitro de direcciones
static s32 sem_a, sem_b;
static char trace[64];
static int trace_n;
static void ping(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        sem_wait(&sem_a);
        if (trace_n < 20) trace[trace_n++] = 'A';
        sem_post(&sem_b);
    }
}
static void pong(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        sem_wait(&sem_b);
        if (trace_n < 20) trace[trace_n++] = 'B';
        sem_post(&sem_a);
    }
}

// 5. eventos
static u32 ev_read;
static int event_seen;
static void wait_event(void *arg) {
    (void)arg;
    s32 idx;
    svc_wait_sync(&idx, &ev_read, 1, -1);
    event_seen = 1;
}

// 6. una pila puesta con MapMemory en la región de pilas, como hace el SDK
static int deep(int n) { return n ? deep(n - 1) + 1 : 0; }
static int mapped_result;
static void on_mapped_stack(void *arg) { mapped_result = deep((int)(u64)arg); }

int nx_main(u64 config, u32 main_thread) {
    (void)config;
    *(u32 *)(tls() + 0x1E4) = main_thread;
    pr("Kernel de Horizon en el host: hilos, mutex, condiciones, eventos y memoria.\n");

    void *h;
    u32 r = svc_set_heap_size(&h, 0x2000000);
    heap = h;
    heap_top = heap + 0x2000000;
    u64 hsum = 0;
    for (u32 i = 0; i < 0x2000000 / 2; i += 4096) heap[i] = (u8)(i >> 12), hsum += heap[i];
    pr("heap de 32 MiB: resultado 0x%x, suma de prueba %U\n", r, hsum);

    MemInfo mi;
    svc_query_memory(&mi, (u64)&nx_main);
    pr("QueryMemory(código): tipo 0x%x, permisos %u\n", mi.type, mi.perm);
    svc_query_memory(&mi, (u64)heap + 12345);
    pr("QueryMemory(heap): tipo 0x%x, permisos %u, empieza en el heap: %s, tamaño 32 MiB: %s\n", mi.type, mi.perm,
       mi.addr == (u64)heap ? "sí" : "no", mi.size == 0x2000000 ? "sí" : "no");
    u64 info;
    svc_get_info(&info, 4, 0xFFFF8001, 0);
    pr("GetInfo(región del heap) coincide con SetHeapSize: %s\n", info == (u64)heap ? "sí" : "no");

    u32 t[4];
    for (int i = 0; i < 4; i++) t[i] = spawn(count_with_mutex, 0, 0);
    for (int i = 0; i < 4; i++) join(t[i]);
    pr("4 hilos × 100000 con mutex: %U\n", counter);

    for (int i = 0; i < 4; i++) t[i] = spawn(count_atomic, 0, 0);
    for (int i = 0; i < 4; i++) join(t[i]);
    pr("4 hilos × 100000 atómicos (LDXR/STXR): %U\n", atomic_counter);

    for (int i = 0; i < 3; i++) t[i] = spawn(consumer, 0, 0);
    for (u32 v = 1; v <= 5000; v++) {
        mutex_lock(&qmtx);
        while (qcount == QCAP) condvar_wait(&q_not_full, &qmtx, -1);
        queue[(qhead + qcount) % QCAP] = v;
        qcount++;
        svc_signal_key(&q_not_empty, 1);
        mutex_unlock(&qmtx);
    }
    mutex_lock(&qmtx);
    done = 1;
    svc_signal_key(&q_not_empty, -1);
    mutex_unlock(&qmtx);
    for (int i = 0; i < 3; i++) join(t[i]);
    pr("productor y 3 consumidores: %U valores, suma %U\n", consumed_n, consumed_sum);

    sem_a = 1;
    t[0] = spawn(ping, 0, 0);
    t[1] = spawn(pong, 0, 0);
    join(t[0]);
    join(t[1]);
    pr("ping-pong con WaitForAddress: %s\n", trace);

    u32 ev_write;
    svc_create_event(&ev_write, &ev_read);
    t[0] = spawn(wait_event, 0, 0);
    svc_sleep(2000000);
    pr("evento antes de la señal: %s\n", event_seen ? "visto" : "esperando");
    svc_handle_op(0x11, ev_write);
    join(t[0]);
    pr("evento después de la señal: %s\n", event_seen ? "visto" : "esperando");
    svc_handle_op(0x12, ev_write);
    s32 idx;
    pr("espera con plazo de 1 ms: 0x%x\n", svc_wait_sync(&idx, &ev_read, 1, 1000000));
    u32 cvm = 0, cv = 0;
    mutex_lock(&cvm);
    u32 rc = condvar_wait(&cv, &cvm, 1000000);
    pr("condición con plazo: 0x%x, y el mutex vuelve: %s\n", rc,
       (cvm & ~WAIT_MASK) == self_handle() ? "sí" : "no");
    mutex_unlock(&cvm);

    u64 stack_region;
    svc_get_info(&stack_region, 14, 0xFFFF8001, 0);
    u8 *src = heap + 0x100000, *dst = (u8 *)stack_region + 0x200000;
    r = svc_map_memory(dst, src, 0x40000, 0);
    t[0] = spawn(on_mapped_stack, (void *)2000, dst + 0x40000);
    join(t[0]);
    pr("pila mapeada con MapMemory: 0x%x, recursión de %d niveles\n", r, mapped_result);
    pr("UnmapMemory: 0x%x\n", svc_map_memory(dst, src, 0x40000, 1));
    pr("Fin.\n");
    flush();
    return 0;
}
