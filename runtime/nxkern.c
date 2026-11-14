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

// GSV's host-side request event is signaled by the guest through SVC. The weak
// hook keeps the kernel independent of applet services in minimal test builds.
void nx2_syncpt_event_signaled(KEvent *event) __attribute__((weak));

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/userfaultfd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
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
#define R_INVALID_CMB 0xE801u       // 116 InvalidCombination (Atmosphère svc_results.hpp; antes 0xE001 = 112, InvalidPriority)
#define R_OUT_OF_MEMORY 0xD001u     // 104: el SDK solo acepta este (o el de límite) al pedir heap de más
#define WAITERS 0x40000000u
#define H_CUR_THREAD 0xFFFF8000u
#define H_CUR_PROCESS 0xFFFF8001u

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sync_cv;          // se avisa con cualquier señal: WaitSynchronization lo escucha
static __thread KThread *cur;
static uint64_t wait_seq;
static int tls_hooks_ready;

void nx_tls_hooks_ready(void) { __atomic_store_n(&tls_hooks_ready, 1, __ATOMIC_RELEASE); }

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
// Límite de hilos del proceso (LimitableResource ThreadCountMax), el que dice GetResourceLimitLimitValue. pm le da 96 a
// una aplicación y 256 con el aumento (Atmosphère pm_spec.cpp, citado por NXnative threads.md); aquí se mantiene 256,
// el valor que el runtime ya informaba.
#define LIMITE_HILOS 256

// Hilos que ocupan el límite: creados y no terminados. En Horizon la reserva se libera al destruir el KThread (terminado
// y sin handles abiertos); aquí al terminar, así que un hilo terminado con su handle aún abierto ya no cuenta.
static uint64_t hilos_vivos(void) {
    uint64_t n = 0;
    pthread_mutex_lock(&G);
    for (int i = 0; i < MAX_THREADS; i++) n += threads[i] && threads[i]->state != TS_EXITED;
    pthread_mutex_unlock(&G);
    return n;
}

// ---------- núcleos virtuales ----------
// En Horizon un hilo corre solo en un núcleo de su máscara de afinidad, y dos hilos no comparten un núcleo a la vez: el
// que lo tiene sigue hasta que se bloquea, cede o llega uno de mejor prioridad. Los motores guardan datos por núcleo
// (GetCurrentProcessorNumber) sin mutex y cuentan con eso: TotK reparte un anillo de memoria de control de NVN por
// núcleo, y dos hilos del mismo núcleo corriendo a la vez se daban el mismo trozo. Aquí cada hilo del juego toma un
// núcleo virtual antes de ejecutar código del juego y lo suelta en las SVC que bloquean (nx_svc) y en las esperas del
// host (nx_core_away). El modelo es el de totk-pc-recomp (docs/architecture.md, "Threads and cores").
// Válvula: un bucle de espera activa no llama a ninguna SVC, así que el que espera un núcleo no puede quitárselo; tras
// un plazo corre igual (sin núcleo) en lugar de colgar el juego. NX_NUCLEOS_LIBRES=1 desactiva todo el modelo.
#define NUM_CORES 4
#define SENAL_TRAS_NS 250000ull         // el que espera pide por señal el núcleo de uno peor (preferencia asíncrona)
#define VALVULA_MEJOR_NS 20000000ull    // último recurso si el peor sigue sin ceder (dentro del runtime, en un spin lock)
#define VALVULA_IGUAL_NS 5000000000ull  // igual o peor prioridad: en Horizon también esperaría; solo contra un cuelgue
static pthread_mutex_t CL = PTHREAD_MUTEX_INITIALIZER;
volatile int nx_core_pending;   // hilos avisados de que deben ceder su núcleo: nx_dispatch solo mira esto
__thread int nx_en_host;        // >0: el hilo está dentro del runtime (SVC, HLE): la señal no lo interrumpe ahí
static pthread_cond_t core_cv = PTHREAD_COND_INITIALIZER;
static KThread *holder[NUM_CORES];
static int core_waiters;

static int vcores(void) {
    static int on = -1;
    if (on < 0) on = !getenv("NX_NUCLEOS_LIBRES");
    return on;
}

// El mejor hilo que espera un núcleo k (prioridad más baja en número; a igual prioridad, el que espera hace más).
static KThread *core_best_waiter(int k) {
    KThread *best = NULL;
    for (int i = 0; i < MAX_THREADS; i++) {
        KThread *t = threads[i];
        if (t && t->core_waiting && (t->mask >> k & 1) &&
            (!best || t->priority < best->priority ||
             (t->priority == best->priority && t->core_wait_since < best->core_wait_since)))
            best = t;
    }
    return best;
}

static void core_take(KThread *t) {
    if (!t || !vcores() || t->vcore >= 0) return;
    pthread_mutex_lock(&CL);
    t->core_waiting = 1;
    t->core_wait_since = nx_now_ns();
    core_waiters++;
    for (;;) {
        // Un núcleo libre de la máscara: primero el ideal, luego el último que tuvo, luego cualquiera.
        int cand[2 + NUM_CORES], n = 0, k = -1;
        cand[n++] = t->core;
        cand[n++] = t->last_vcore;
        for (int j = 0; j < NUM_CORES; j++) cand[n++] = j;
        for (int j = 0; j < n && k < 0; j++)
            if (cand[j] >= 0 && cand[j] < NUM_CORES && (t->mask >> cand[j] & 1) && !holder[cand[j]] &&
                core_best_waiter(cand[j]) == t)
                k = cand[j];
        if (k >= 0) {
            holder[k] = t;
            t->vcore = t->last_vcore = k;
            break;
        }
        // Avisar al peor que tenga un núcleo de la máscara si este hilo es mejor: cede en su próxima SVC.
        KThread *peor = NULL;
        for (int j = 0; j < NUM_CORES; j++)
            if ((t->mask >> j & 1) && holder[j] && holder[j]->priority > t->priority &&
                (!peor || holder[j]->priority > peor->priority))
                peor = holder[j];
        // El peor está dentro del runtime (un envío a la GPU, un pipeline que se compila): no ejecuta código del juego,
        // así que su núcleo pasa al mejor ya, como con la interrupción de Horizon; al volver al juego lo pide otra vez.
        if (peor && peor->en_host_k && peor->vcore >= 0) {
            holder[peor->vcore] = NULL;
            peor->vcore = -1;
            peor->robado = 1;
            if (peor->preempt) {
                peor->preempt = 0;
                __atomic_sub_fetch(&nx_core_pending, 1, __ATOMIC_RELEASE);
            }
            continue;
        }
        if (peor && !peor->preempt) {
            peor->preempt = 1;
            __atomic_add_fetch(&nx_core_pending, 1, __ATOMIC_RELEASE);
        }
        uint64_t espera = nx_now_ns() - t->core_wait_since;
        // Preferencia asíncrona: el peor puede pasar milisegundos en código del juego sin SVC ni llamada indirecta;
        // la señal lo hace ceder ahí (preempt_signal), como la interrupción de Horizon.
        if (peor && espera > SENAL_TRAS_NS && peor->pthread) pthread_kill(peor->pthread, SIGURG);
        if (espera > (peor ? VALVULA_MEJOR_NS : VALVULA_IGUAL_NS)) {
            static int avisos;
            if (getenv("NX_TRAZA_NUCLEOS") || avisos++ < 3) {
                fprintf(stderr, "[nxkern] hilo %llu (prioridad %d, máscara %#llx) corre sin núcleo tras %llu ms; núcleos:",
                        (unsigned long long)t->id, t->priority, (unsigned long long)t->mask,
                        (unsigned long long)(espera / 1000000));
                for (int j = 0; j < NUM_CORES; j++)
                    if (holder[j])
                        fprintf(stderr, " %d=hilo %llu (prioridad %d, pc %#llx)", j, (unsigned long long)holder[j]->id,
                                holder[j]->priority, (unsigned long long)holder[j]->ctx->x[30]);
                fprintf(stderr, "\n");
            }
            break;   // válvula: sigue sin núcleo (vcore -1); vuelve a pedirlo en su próxima SVC que bloquee
        }
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 1000000;
        if (ts.tv_nsec >= 1000000000) ts.tv_sec++, ts.tv_nsec -= 1000000000;
        pthread_cond_timedwait(&core_cv, &CL, &ts);
    }
    t->core_waiting = 0;
    core_waiters--;
    pthread_mutex_unlock(&CL);
}

static void core_give(KThread *t) {
    if (!t || !vcores()) return;
    pthread_mutex_lock(&CL);
    if (t->vcore >= 0 && holder[t->vcore] == t) holder[t->vcore] = NULL;
    t->vcore = -1;
    if (t->preempt) {
        t->preempt = 0;
        __atomic_sub_fetch(&nx_core_pending, 1, __ATOMIC_RELEASE);
    }
    if (core_waiters) pthread_cond_broadcast(&core_cv);
    pthread_mutex_unlock(&CL);
}

// Entrar y salir del runtime (SVC, funciones HLE como NVN). Al salir del más externo, el hilo vuelve a código del juego:
// si un hilo mejor le quitó el núcleo mientras tanto (core_take), lo pide de nuevo. La salida mira bajo CL, para no
// cruzarse con quien se lo está quitando.
void nx_host_enter(void) {
    if (nx_en_host++ == 0 && cur) cur->en_host_k = 1;
}

void nx_host_leave(void) {
    KThread *t = cur;
    if (--nx_en_host || !t) return;
    pthread_mutex_lock(&CL);
    t->en_host_k = 0;
    int robado = t->robado;
    t->robado = 0;
    pthread_mutex_unlock(&CL);
    if (robado) core_take(t);
}

// La señal de preferencia asíncrona, en el hilo que tiene el núcleo: cede solo si está en código del juego (no dentro
// del runtime, que puede tener tomados sus mutex) y fuera de un spin lock del SDK.
static void preempt_signal(int sig) {
    (void)sig;
    KThread *t = cur;
    if (!t || nx_en_host || !t->preempt || t->vcore < 0 || *(volatile uint16_t *)(uintptr_t)(t->tls + 0x100)) return;
    int e = errno;
    nx_en_host++;
    core_give(t);
    core_take(t);
    nx_en_host--;
    errno = e;
}

static void core_signal_install(void) {
    struct sigaction sa = {0};
    sa.sa_handler = preempt_signal;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGURG, &sa, NULL);
}

static void core_signal_init(void) {
    static pthread_once_t una = PTHREAD_ONCE_INIT;
    pthread_once(&una, core_signal_install);
}

// Punto de cesión (cada SVC y cada llamada indirecta del juego, vía nx_dispatch): si un hilo mejor espera el núcleo,
// se lo da. No dentro de un spin lock del SDK (ThreadLocalRegion+0x100, disable_count): ahí cede en la SVC 0x36.
void nx_core_check(void) {
    KThread *t = cur;
    if (!t || !t->preempt || t->vcore < 0) return;
    if (*(volatile uint16_t *)(uintptr_t)(t->tls + 0x100)) return;
    core_give(t);
    core_take(t);
}

// Esperas largas del host en nombre del hilo actual (la capa NVN: envíos, presentación, esperas de la GPU): el núcleo
// queda para los demás, como en la Switch, donde ese hilo estaría bloqueado en IPC.
int nx_core_away(void) {
    if (!cur || cur->vcore < 0) return 0;
    core_give(cur);
    return 1;
}
void nx_core_back(int away) {
    if (away) core_take(cur);
}
static uint64_t next_thread_id = 0x50;
static uint64_t tls_next = NX_TLS_BASE;
static uint8_t tls_slot_used[NX_TLS_SLOTS];

void nx_tls_allocate(Ctx *c) {
    uint32_t *out = (uint32_t *)(uintptr_t)c->x[0];
    pthread_mutex_lock(&G);
    for (uint32_t i = 0; i < NX_TLS_SLOTS; i++) {
        if (!tls_slot_used[i]) {
            tls_slot_used[i] = 1;
            pthread_mutex_unlock(&G);
            if (out) *out = i;
            c->x[0] = 0;
            return;
        }
    }
    pthread_mutex_unlock(&G);
    c->x[0] = 0x2c01;
}

void nx_tls_free(Ctx *c) {
    uint32_t slot = (uint32_t)c->x[0];
    if (slot < NX_TLS_SLOTS) {
        pthread_mutex_lock(&G);
        tls_slot_used[slot] = 0;
        pthread_mutex_unlock(&G);
    }
}

void nx_tls_get(Ctx *c) {
    uint32_t slot = (uint32_t)c->x[0];
    KThread *t = nx_current_thread();
    c->x[0] = t && slot < NX_TLS_SLOTS ? t->tls_slots[slot] : 0;
}

void nx_tls_set(Ctx *c) {
    uint32_t slot = (uint32_t)c->x[0];
    KThread *t = nx_current_thread();
    if (t && slot < NX_TLS_SLOTS) {
        t->tls_slots[slot] = c->x[1];
        if (getenv("NX_TRAZA_TLS") && slot == 3) {
            uint64_t field = 0;
            if (c->x[1]) memcpy(&field, (const void *)(uintptr_t)(c->x[1] + 0x88), sizeof field);
            fprintf(stderr, "[tls] set tid=%llu slot=3 value=0x%llx field+88=0x%llx lr=0x%llx\n",
                    (unsigned long long)t->id, (unsigned long long)c->x[1], (unsigned long long)field, (unsigned long long)c->x[30]);
        }
    }
}

void nx_tls_used(Ctx *c) {
    uint32_t n = 0;
    pthread_mutex_lock(&G);
    for (uint32_t i = 0; i < NX_TLS_SLOTS; i++) n += tls_slot_used[i] != 0;
    pthread_mutex_unlock(&G);
    c->x[0] = n;
}

static KThread *thread_new(void) {
    KThread *t = calloc(1, sizeof *t);
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(&t->cv, &a);
    t->ctx = calloc(1, sizeof(Ctx));
    t->ctx->excl_addr = ~0ull;
    t->vcore = t->last_vcore = -1;
    t->id = next_thread_id++;
    uint64_t tls_region = tls_next;
    t->tls = tls_region + NX_THREAD_TLR_OFFSET;
    t->tp = tls_region + NX_THREAD_TP_OFFSET;
    tls_next += NX_THREAD_TLS_STRIDE;
    memset((void *)(uintptr_t)tls_region, 0, NX_THREAD_TLS_STRIDE);
    t->ctx->tpidrro = t->tls;
    // Hookshot reserva un bloque TLS de 4 KiB por hilo y sitúa TP en el centro.
    // El SDK usa TPIDR_EL0 para TLS estático y construye su DTV desde este bloque.
    t->ctx->tpidr = t->tp;
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
    t->priority = nx_main_priority ? nx_main_priority : 44; // prioridad y núcleo del hilo principal: del NPDM
    t->core = nx_main_core >= 0 && nx_main_core < 4 ? nx_main_core : 0;
    t->mask = 1ull << t->core;
    t->state = TS_RUNNING;
    t->handle = nx_handle_new(KO_THREAD, t, 1);
    *(uint32_t *)(uintptr_t)(t->tls + 0x110) = (uint32_t)t->id; // id del kernel (GetThreadId), no el handle: nxnative threads.md, SDK 22
    // Horizon's SDK looks up the current nn::os::ThreadType through TLR+0x1f8.
    // Hookshot creates this object for its main thread because the guest SDK does
    // not. Keep it in the runtime-owned TLS mapping, outside the game's heap.
    t->thread_type = t->tls - NX_THREAD_TLR_OFFSET + 0x400;
    st32(t->thread_type + 0x1b0, t->handle);
    st64(t->tls + 0x1f8, t->thread_type);
    cur = t;
    pthread_mutex_unlock(&G);
    return t;
}
KThread *nx_current_thread(void) { return cur; }

// El hilo principal del juego corre en un pthread propio (el principal del proceso es la ventana).
void nx_kernel_adopt(KThread *t) {
    cur = t;
    t->pthread = pthread_self();
    nx_en_host = 1;
    core_signal_init();
    core_take(t);
    nx_en_host = 0;
    nx_set_crash_ctx(t->ctx);
    static __thread char altstack[64 * 1024];
    stack_t ss = {.ss_sp = altstack, .ss_size = sizeof altstack};
    sigaltstack(&ss, NULL);
}

// Hook de nnmusl al iniciar (k = 0) o terminar (k = 1) un hilo creado por nx_thread_create. La entrada de la
// GOT la halla el recompilador en la imagen (recomp/program.py thread_hook_gots); no hay direcciones de un juego
// aquí. Sin entrada (C generado viejo o sin la firma de la entrada de hilo del SDK) no se llama ningún hook.
static uint64_t thread_hook(int k) {
    uint64_t got = &nx_thread_hook_got[0] ? nx_thread_hook_got[k] : 0;
    uint64_t slot = got ? ld64(got) : 0;
    uint64_t hook = slot ? ld64(slot) : 0;
    if (getenv("NX_TRACE_THREAD_CREATE"))
        fprintf(stderr, "[thread] hook %d: got=%#llx slot=%#llx function=%#llx\n", k, (unsigned long long)got,
                (unsigned long long)slot, (unsigned long long)hook);
    // En algunas imágenes SDK 22 el slot débil apunta a un thunk de BSS lleno de ceros: no es código traducido.
    if (hook && !ld32(hook) && !ld32(hook + 4)) hook = 0;
    return hook;
}

static void *thread_main(void *arg) {
    KThread *t = arg;
    cur = t;
    t->pthread = pthread_self();
    nx_en_host = 1;
    core_signal_init();
    core_take(t);
    nx_set_crash_ctx(t->ctx);
    static __thread char altstack[64 * 1024];
    stack_t ss = {.ss_sp = altstack, .ss_size = sizeof altstack};
    sigaltstack(&ss, NULL);
    if (nx_main_addr)
        while (!__atomic_load_n(&tls_hooks_ready, __ATOMIC_ACQUIRE)) sched_yield();
    if (t->thread_type) {
        if (getenv("NX_TRACE_THREAD_CREATE"))
        {
            uint64_t args[4] = {0};
            int readable = nx_leer_seguro(t->thread_arg, args, sizeof args);
            fprintf(stderr, "[thread] HLE dispatch tid=%llu tt=%#llx entry=%#llx arg=%#llx arg_words=%s",
                    (unsigned long long)t->id, (unsigned long long)t->thread_type,
                    (unsigned long long)t->entry, (unsigned long long)t->thread_arg,
                    readable ? "" : "unreadable");
            if (readable)
                for (unsigned i = 0; i < 4; i++) fprintf(stderr, "%s%#llx", i ? "," : " ",
                                                            (unsigned long long)args[i]);
            fputc('\n', stderr);
        }
        // Técnica descrita por Hookshot (GPL-3.0; solo la idea, implementación propia): sin MapAliasStack,
        // run nnmusl's per-thread TLS hooks, then enter the requested function.
        st32(t->thread_type + 0x1b0, t->handle);
        st64(t->tls + 0x1f8, t->thread_type);
        st32(t->tls + 0x110, (uint32_t)t->id); // id del kernel, como GetThreadId
        uint64_t hook = thread_hook(0);
        if (hook && getenv("NX_TRACE_THREAD_CREATE"))
            fprintf(stderr, "[thread] start-hook=%#llx words=%08x %08x %08x %08x\n",
                    (unsigned long long)hook, ld32(hook), ld32(hook + 4), ld32(hook + 8), ld32(hook + 12));
        if (hook) {
            t->ctx->x[0] = t->thread_type;
            t->ctx->x[30] = 0;
            nx_dispatch(t->ctx, hook);
        }
        uint64_t tp = ld64(t->tls + 0x1f0);
        if (tp) t->ctx->tpidr = tp;
        t->ctx->sp = t->stack_top;
        t->ctx->x[0] = t->thread_arg;
        t->ctx->x[30] = 0;
        nx_en_host = 0;
        nx_dispatch(t->ctx, t->entry);
        nx_en_host = 1;
        hook = thread_hook(1);
        if (hook) {
            t->ctx->x[0] = t->thread_type;
            t->ctx->x[30] = 0;
            nx_en_host = 0;
            nx_dispatch(t->ctx, hook);
            nx_en_host = 1;
        }
    } else {
        // SVC-created SDK trampolines still publish their ThreadType handle here.
        if (t->entry && ld32(t->entry + 0x10) == 0xD53BD061u &&
        ld32(t->entry + 0x14) == 0x9107E021u && ld32(t->entry + 0x18) == 0xF9000020u && t->ctx->x[0]) {
            st32(t->ctx->x[0] + 0x1b0, t->handle);
            st32(t->tls + 0x110, (uint32_t)t->id); // id del kernel, como GetThreadId
            if (getenv("NX_TRACE_TLS"))
                fprintf(stderr, "[tls] thread handle tid=%llu handle=%#x ThreadType=%#llx\n",
                        (unsigned long long)t->id, t->handle, (unsigned long long)t->ctx->x[0]);
        }
        nx_en_host = 0;   // desde aquí, código del juego
        nx_dispatch(t->ctx, t->entry);
        nx_en_host = 1;
    }
    nx_exit_thread();
    return NULL;
}

void nx_exit_thread(void) {
    core_give(cur);
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

// Older callers pass a kernel thread handle to ArbitrateLock. Recent Switch 2 SDK mutexes pass
// the thread ID stored in TLS instead; accept both representations.
static KThread *thread_by_handle_or_id(uint32_t id) {
    KThread *t = nx_handle_get(id, KO_THREAD);
    if (t) return t;
    for (int k = 0; k < MAX_THREADS; k++)
        if (threads[k] && (uint32_t)threads[k]->id == id) return threads[k];
    return NULL;
}

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
    KThread *owner = thread_by_handle_or_id(owner_h);
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
static uint32_t wait_for_address(uint64_t addr, uint32_t type, int64_t value64, int64_t timeout) {
    if (addr & (type == 3 ? 7 : 3)) return R_INVALID_ADDR;
    int32_t value = (int32_t)value64;
    pthread_mutex_lock(&G);
    int32_t v = (int32_t)__atomic_load_n(u32p(addr), __ATOMIC_SEQ_CST);
    int wait;
    if (type == 3) wait = (int64_t)__atomic_load_n((uint64_t *)(uintptr_t)addr, __ATOMIC_SEQ_CST) == value64; // WaitIfEqual64 (SDK 19)
    else if (type == 0) wait = v < value;                    // WaitIfLessThan
    else if (type == 1) {                               // DecrementAndWaitIfLessThan
        uint32_t expected = (uint32_t)v;
        for (;;) {
            wait = (int32_t)expected < value;
            if (!wait || __atomic_compare_exchange_n(u32p(addr), &expected, expected - 1u, 0,
                                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) break;
        }
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
            if (count <= 0) nv = w > 0 ? value - 2 : value + 1; // como mesosphere: -2 con esperas
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

// Trazas de SVC que un juego puede repetir millones de veces (un TryWait en bucle): la misma línea seguida en el
// mismo hilo se escribe las 8 primeras veces y luego en cada potencia de 2, con su número de repetición; al cambiar
// la línea se dice cuántas veces se repitió la anterior. Una línea distinta siempre sale, así nada nuevo se oculta.
typedef struct { uint64_t firma, veces; } TrazaRepetida;
static uint64_t traza_firma(const uint64_t *v, int n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int k = 0; k < n; k++) h = (h ^ v[k]) * 0x100000001b3ull;
    return h;
}
// Devuelve 0 si la línea se omite; si no, el número de repetición (1 = primera vez).
static uint64_t nx_traza_repetida(TrazaRepetida *t, uint64_t firma, const char *prefijo) {
    if (t->veces && t->firma == firma) {
        uint64_t v = ++t->veces;
        return v <= 8 || !(v & (v - 1)) ? v : 0;
    }
    if (t->veces > 8)
        fprintf(stderr, "%s la línea anterior se repitió %llu veces seguidas\n", prefijo, (unsigned long long)t->veces);
    t->firma = firma;
    t->veces = 1;
    return 1;
}

static uint32_t wait_sync(uint64_t hptr, int32_t n, int64_t timeout, uint64_t *index) {
    if (n < 0 || n > 0x40) return R_OUT_OF_RES;
    const uint32_t *hs = (const uint32_t *)(uintptr_t)hptr;
    pthread_mutex_lock(&G);
    if (getenv("NX_TRACE_WAITS")) {
        static __thread TrazaRepetida rep;
        uint64_t v[0x43] = {cur->id, (uint64_t)timeout, (uint64_t)n};
        for (int k = 0; k < n; k++) v[3 + k] = hs[k];
        uint64_t vez = nx_traza_repetida(&rep, traza_firma(v, 3 + n), "[wait-sync]");
        if (vez) {
            fprintf(stderr, "[wait-sync] thread=%llu timeout=%lld handles=", (unsigned long long)cur->id, (long long)timeout);
            for (int k = 0; k < n; k++) fprintf(stderr, " %x", hs[k]);
            if (vez > 1) fprintf(stderr, " (repetición %llu)", (unsigned long long)vez);
            fprintf(stderr, "\n");
        }
    }
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
static int is_switch2_runtime(void);

// NXNative's Switch 2 profiles reserve a 9 GiB application pool. OOT asks
// SetHeapSize for 0x18ce00000, above the old 6 GiB limit.
#define NX_SWITCH2_HEAP_MAX 0x240000000ull
static uint64_t heap_limit(void) {
    return is_switch2_runtime() ? NX_SWITCH2_HEAP_MAX : NX_HEAP_MAX;
}

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
// ---- vigilancia de escrituras: qué páginas escribió la CPU desde la última consulta. El backend Vulkan (runtime/nvn)
// copia a la GPU solo esas, en lugar de cada pool entero en cada envío. userfaultfd en protección contra escritura
// asíncrona (Linux 6.7): el kernel quita la protección de una página en su primera escritura sin avisarnos, y
// PAGEMAP_SCAN lista las que la perdieron y las protege otra vez (como platform/linux.c de totk-pc-recomp).
static int ww_fd = -1, pm_fd = -1;

static void ww_open(void) {
    int fd = (int)syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    struct uffdio_api api = {.api = UFFD_API, .features = UFFD_FEATURE_WP_ASYNC | UFFD_FEATURE_WP_UNPOPULATED};
    if (fd < 0 || ioctl(fd, UFFDIO_API, &api)) {
        fprintf(stderr, "[nxrt] sin userfaultfd asíncrono (%s): la GPU copiará la memoria entera\n", strerror(errno));
        if (fd >= 0) close(fd);
        return;
    }
    pm_fd = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (pm_fd < 0) close(fd);
    else ww_fd = fd;
}

static void reserve(uint64_t addr, uint64_t size, const char *what, int watch) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0);
    if (p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "[nxrt] no pude reservar %s en 0x%llx\n", what, (unsigned long long)addr);
        exit(1);
    }
    if (!watch || getenv("NX_SIN_VIGILANCIA")) return;   // NX_SIN_VIGILANCIA=1: la GPU copia la memoria entera
    if (ww_fd < 0 && pm_fd < 0) ww_open();
    if (ww_fd < 0) return;
    // Las páginas enormes contarían 2 MiB como escritos al tocar 4 KiB.
    madvise(p, size, MADV_NOHUGEPAGE);
    struct uffdio_register reg = {.range = {addr, size}, .mode = UFFDIO_REGISTER_MODE_WP};
    if (ioctl(ww_fd, UFFDIO_REGISTER, &reg))
        fprintf(stderr, "[nxrt] no pude vigilar las escrituras en %s (%s)\n", what, strerror(errno));
}

// Las páginas de [addr, addr+n) escritas desde la última consulta (hasta max, en orden); reset: vuelven a protegerse.
// UINT64_MAX si ese rango no se vigila (fuera del heap y de la región alias, o sin userfaultfd).
uint64_t nx_mem_written(uint64_t addr, uint64_t n, uint64_t *pages, uint64_t max, int reset) {
    if (!max || !n) return 0;
    int dentro = (addr >= NX_HEAP_BASE && addr + n <= NX_HEAP_BASE + heap_limit()) ||
                 (addr >= NX_ALIAS_BASE && addr + n <= NX_ALIAS_BASE + NX_ALIAS_SIZE);
    if (ww_fd < 0 || !dentro) return UINT64_MAX;
    struct page_region r[256];
    uint64_t got = 0;
    for (uint64_t at = addr & ~0xFFFull, end = (addr + n + 0xFFF) & ~0xFFFull; at < end && got < max;) {
        // Escrita: sin la protección y con página propia (presente o en swap), que no sea la página cero compartida.
        struct pm_scan_arg arg = {.size = sizeof arg,
                                  .flags = reset ? PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC : 0,
                                  .start = at, .end = end, .vec = (uint64_t)(uintptr_t)r, .vec_len = 256,
                                  .max_pages = max - got,
                                  .category_inverted = PAGE_IS_PFNZERO,
                                  .category_mask = PAGE_IS_WRITTEN | PAGE_IS_PFNZERO,
                                  .category_anyof_mask = PAGE_IS_PRESENT | PAGE_IS_SWAPPED,
                                  .return_mask = PAGE_IS_WRITTEN};
        long k = ioctl(pm_fd, PAGEMAP_SCAN, &arg);
        if (k < 0) return UINT64_MAX;
        for (long i = 0; i < k; i++)
            for (uint64_t pg = r[i].start; pg < r[i].end && got < max; pg += 0x1000) pages[got++] = pg;
        if (arg.walk_end <= at) break;
        at = arg.walk_end;
    }
    return got;
}

// La región de pilas está reservada en el host, pero para Horizon lo que no se usa ahí sigue
// libre: libnx puede elegir esas direcciones (virtmemFindAslr no la evita) para memoria compartida.
static int in_stack_region(uint64_t addr, uint64_t size) {
    return addr >= NX_STACK_BASE && addr + size <= NX_STACK_BASE + NX_STACK_SIZE;
}
static int range_free(uint64_t addr, uint64_t size) {
    for (int k = 0; k < nregions; k++)
        if (regions[k].addr < addr + size && addr < regions[k].addr + regions[k].size) return 0;
    return 1;
}

void nx_memory_init(void) {
    reserve(NX_HEAP_BASE, heap_limit(), "el heap", 1);
    reserve(NX_ALIAS_BASE, NX_ALIAS_SIZE, "la región alias", 1);
    reserve(NX_STACK_BASE, NX_STACK_SIZE, "la región de pilas", 0);
    for (int k = 0; k < nx_nmodules; k++) {
        const NxModule *m = &nx_modules[k];
        // Cada segmento aparte, como en la consola: el desenrollador de excepciones del SDK busca el
        // módulo recorriendo Code RX, Code R y CodeData RW (nn::ro::detail::GetExceptionInfo).
        uint64_t fin = (m->size + m->bss + 0xFFF) & ~0xFFFull, ro = m->ro & ~0xFFFull, rw = m->rw & ~0xFFFull;
        if (ro && rw > ro && fin > rw) {
            region_add(m->base, ro, 0x03, 0x5);
            region_add(m->base + ro, rw - ro, 0x03, 0x1);
            region_add(m->base + rw, fin - rw, 0x04, 0x3);
        } else {
            region_add(m->base, fin, 0x03, 0x5);
        }
    }
    region_add(NX_TLS_BASE, NX_TLS_SIZE, 0x0C, 0x3);
}

void nx_memory_add_stack(uint64_t addr, uint64_t size) { region_add(addr, size, 0x0B, 0x3); }

static uint32_t set_heap_size(uint64_t size, uint64_t *out) {
    if (size & 0x1FFFFF) return R_INVALID_SIZE; // múltiplo de 2 MiB
    if (size > heap_limit()) return R_OUT_OF_MEMORY; // DKC Tropical Freeze busca el máximo probando
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

// Tiempo de CPU de un hilo en ticks de 19,2 MHz.
static uint64_t ticks_de_hilo(KThread *t) {
    clockid_t reloj;
    struct timespec ts;
    if (!t || pthread_getcpuclockid(t->pthread ? t->pthread : pthread_self(), &reloj) || clock_gettime(reloj, &ts)) return 0;
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    return ns / 625 * 12 + ns % 625 * 12 / 625;
}

static uint64_t program_id(void) {
    const char *e = getenv("NX_TITLE_ID"); // lo pone NX2Linux
    if (e && *e) return strtoull(e, NULL, 16);
    return nx_program_id ? nx_program_id : 0x0100000000001000ull;
}

static int is_switch2_runtime(void) {
    const char *platform = getenv("NX_PLATAFORMA"); // lo pone NX2Linux según el NPDM/ExeFS
    return platform && !strcmp(platform, "switch2");
}

static uint32_t get_info(uint32_t id, uint32_t h, uint64_t sub, uint64_t *out) {
    static uint8_t contexto_excepcion[0x1000]; // el SDK escribe ahí (+0x1C0) al instalar su manejador
    switch (id) {
    case 0: *out = 0xF; return R_OK_;                        // núcleos permitidos
    case 1: *out = 0x3FFFFFFFFFFFFFFFull; return R_OK_;      // prioridades permitidas
    // Switch 2's VAMM allocator occupies the upper half of the reserved alias
    // range; the game's physical-memory alias allocator owns the lower half.
    // This mirrors nxnative's documented GetInfo 29/30 layout while keeping
    // the existing full reservation and MapPhysicalMemory backing.
    case 2: *out = NX_ALIAS_BASE; return R_OK_;
    case 3: *out = is_switch2_runtime() ? NX_ALIAS_SIZE / 2 : NX_ALIAS_SIZE; return R_OK_;
    case 4: *out = NX_HEAP_BASE; return R_OK_;
    case 5: *out = heap_limit(); return R_OK_;
    case 6: case 21: *out = is_switch2_runtime() ? NX_SWITCH2_HEAP_MAX : 0xCD500000ull; return R_OK_;
    case 7: case 22: *out = heap_size; return R_OK_;
    case 8: *out = 0; return R_OK_;                          // sin depurador
    case 9: *out = 0; return R_OK_;                          // límite de recursos (handle)
    case 10: *out = 0; return R_OK_;                         // ticks ociosos
    case 11: *out = nx_ticks() * 0x9E3779B97F4A7C15ull ^ (sub * 0xBF58476D1CE4E5B9ull); return R_OK_; // entropía
    case 12: *out = 0x8000000ull; return R_OK_;              // región ASLR
    case 13: *out = (1ull << 39) - 0x8000000ull; return R_OK_;
    case 14: *out = NX_STACK_BASE; return R_OK_;
    case 15: *out = NX_STACK_SIZE; return R_OK_;
    // Recurso del sistema (del NPDM): con 0, Unreal 4.26+/UE5 y Unity 6 eligen otro asignador o salen.
    case 16: *out = nx_system_resource; return R_OK_;
    case 17: *out = nx_system_resource / 4; return R_OK_;
    case 18: *out = program_id(); return R_OK_;              // id de programa
    case 20: *out = (uint64_t)(uintptr_t)contexto_excepcion; return R_OK_; // contexto de excepción del usuario
    case 25: { // ticks de CPU de un hilo (handle 0xFFFF8000 = el actual)
        KThread *t = h == 0xFFFF8000u ? cur : nx_handle_get(h, KO_THREAD);
        *out = ticks_de_hilo(t);
        return R_OK_;
    }
    case 26: *out = 1; return R_OK_;                         // IsSvcPermitted: todas
    case 23: *out = 1; return R_OK_;                         // es una aplicación
    case 28: *out = 0; return R_OK_;                         // tamaño extra de la región alias
    // Switch 2 consulta el rango VAMM al iniciar nnos. La mitad alta de la
    // reserva alias se dedica a ese allocator; la mitad baja sigue siendo la
    // región alias normal (GetInfo 2/3). No hay memoria remota del host.
    // No exponer estos InfoType de Switch 2 a títulos Switch 1.
    case 29:
        if (is_switch2_runtime()) { *out = NX_ALIAS_BASE + NX_ALIAS_SIZE / 2; return R_OK_; }
        break;
    case 30:
        if (is_switch2_runtime()) { *out = NX_ALIAS_SIZE / 2; return R_OK_; }
        break;
    case 31: case 32:
        if (is_switch2_runtime()) { *out = 0; return R_OK_; }
        break;
    case 33:
        if (is_switch2_runtime()) { *out = 0x1000; return R_OK_; } // tamaño de página de 4 KiB
        break;
    case 24: {   // FreeThreadCount: límite menos actual (mesosphere kern_svc_info.cpp); subtipo distinto de 0 es inválido
        if (sub) return R_INVALID_CMB;
        uint64_t vivos = hilos_vivos();
        *out = vivos < LIMITE_HILOS ? LIMITE_HILOS - vivos : 0;
        return R_OK_;
    }
    case 0xF0000002u: *out = nx_now_ns() * 12 / 625; return R_OK_; // ticks del hilo
    }
    (void)h;
    fprintf(stderr, "[RED] GetInfo(%u, 0x%x, %llu) no implementado\n", id, h, (unsigned long long)sub);
    return R_INVALID_ENUM;
}

// ---------- pausar hilos (recolectores de basura de Unity IL2CPP, Mono, .NET) ----------
// SetThreadActivity(1) detiene el hilo donde esté: una señal lo deja esperando dentro de su
// manejador hasta que lo reanudan. GetThreadContext3 lee sus registros, que viven en su Ctx.
static void pausa_senal(int s) {
    (void)s;
    KThread *t = cur;
    if (!t) return;
    sigset_t m;
    sigfillset(&m);
    sigdelset(&m, SIGUSR2);
    __atomic_store_n(&t->pausado, 1, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&t->pausa, __ATOMIC_SEQ_CST)) sigsuspend(&m);
    __atomic_store_n(&t->pausado, 0, __ATOMIC_SEQ_CST);
}

static uint32_t set_thread_activity(uint32_t h, uint32_t pausar) {
    static int instalado;
    if (!instalado++) {
        struct sigaction sa = {0};
        sa.sa_handler = pausa_senal;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, NULL);
    }
    KThread *t = nx_handle_get(h, KO_THREAD);
    if (!t) return R_INVALID_HANDLE;
    if (t == cur) return R_INVALID_HANDLE; // un hilo no se pausa a sí mismo
    if (t->state != TS_RUNNING) return R_INVALID_STATE;
    __atomic_store_n(&t->pausa, pausar ? 1 : 0, __ATOMIC_SEQ_CST);
    pthread_kill(t->pthread, SIGUSR2);
    if (pausar) // esperar a que de verdad se detenga
        for (int i = 0; i < 2000 && !__atomic_load_n(&t->pausado, __ATOMIC_SEQ_CST); i++) usleep(50);
    return R_OK_;
}

// ThreadContext de 0x320 bytes: x0-x28, fp, lr, sp, pc, pstate, v0-v31, fpcr, fpsr, tpidr.
static uint32_t get_thread_context(uint32_t h, uint64_t dst) {
    KThread *t = nx_handle_get(h, KO_THREAD);
    if (!t) return R_INVALID_HANDLE;
    if (t == cur) return R_INVALID_HANDLE;
    uint8_t *o = (uint8_t *)(uintptr_t)dst;
    const Ctx *c = t->ctx;
    memset(o, 0, 0x320);
    memcpy(o, c->x, 31 * 8);           // x0-x28, fp (x29), lr (x30)
    memcpy(o + 0xF8, &c->sp, 8);
    memcpy(o + 0x100, &c->pc, 8);
    uint32_t psr = (uint32_t)c->n << 31 | (uint32_t)c->z << 30 | (uint32_t)c->c << 29 | (uint32_t)c->v << 28;
    memcpy(o + 0x108, &psr, 4);
    memcpy(o + 0x110, c->vr, 32 * 16);
    uint32_t fpcr = (uint32_t)c->fpcr, fpsr = (uint32_t)c->fpsr;
    memcpy(o + 0x310, &fpcr, 4);
    memcpy(o + 0x314, &fpsr, 4);
    memcpy(o + 0x318, &c->tpidr, 8);
    return R_OK_;
}

// ---------- la tabla de SVC ----------
static void svc(Ctx *c, uint32_t imm);

// Las SVC que pueden bloquear sueltan el núcleo virtual mientras esperan (en Horizon el hilo deja de correr); en las
// demás, un hilo avisado de que lo pide uno de mejor prioridad lo cede (y lo vuelve a pedir).
void nx_svc(Ctx *c, uint32_t imm) {
    KThread *t = cur;
    nx_host_enter();
    int bloquea = imm == 0x0B || imm == 0x18 || imm == 0x1A || imm == 0x1C || imm == 0x34 || imm == 0x21 ||
                  imm == 0x22 || imm == 0x23 || imm == 0x43;
    if (t && bloquea) {
        core_give(t);
    } else if (t && t->preempt) {
        if (imm == 0x36) {   // SynchronizePreemptionState: el SDK acaba de salir de un spin lock con el aviso puesto
            core_give(t);
            core_take(t);
        } else {
            nx_core_check();
        }
    }
    svc(c, imm);
    if (t && bloquea) core_take(t);
    nx_host_leave();
}

static void svc(Ctx *c, uint32_t imm) {
    uint64_t *x = c->x, out = 0;
    static int trace = -1;
    if (trace < 0) trace = getenv("NX_TRAZA_SVC") != NULL;
    if (trace) {
        static __thread TrazaRepetida rep;
        uint64_t v[7] = {cur ? cur->id : 0, imm, x[0], x[1], x[2], c->x[30], c->sp};
        uint64_t vez = nx_traza_repetida(&rep, traza_firma(v, 7), "[svc]");
        if (vez == 1)
            fprintf(stderr, "[svc] hilo %llu: 0x%02x x0=%llx x1=%llx x2=%llx lr=%llx sp=%llx fp=%llx\n", (unsigned long long)(cur ? cur->id : 0), imm,
                    (unsigned long long)x[0], (unsigned long long)x[1], (unsigned long long)x[2],
                    (unsigned long long)c->x[30], (unsigned long long)c->sp, (unsigned long long)c->x[29]);
        else if (vez)
            fprintf(stderr, "[svc] hilo %llu: 0x%02x x0=%llx x1=%llx x2=%llx lr=%llx sp=%llx fp=%llx (repetición %llu)\n",
                    (unsigned long long)(cur ? cur->id : 0), imm, (unsigned long long)x[0], (unsigned long long)x[1],
                    (unsigned long long)x[2], (unsigned long long)c->x[30], (unsigned long long)c->sp,
                    (unsigned long long)c->x[29], (unsigned long long)vez);
    }
    if (imm == 0x0c && cur && getenv("NX_TRACE_MAIN_TT"))
        fprintf(stderr, "[thread] GetThreadPriority cur=%p tpidr=%#llx tlr=%#llx tt=%#llx tt_handle=%#x tlr_tt=%#llx tp_tt=%#llx\n",
                (void *)cur, (unsigned long long)c->tpidr,
                (unsigned long long)cur->tls, (unsigned long long)cur->thread_type,
                cur->thread_type ? ld32(cur->thread_type + 0x1b0) : 0,
                (unsigned long long)ld64(cur->tls + 0x1f8),
                (unsigned long long)ld64(c->tpidr + 0x1f8));
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
        // Núcleo -2: el del proceso (NPDM). Horizon crea el hilo con afinidad solo a su núcleo ideal.
        t->core = (int32_t)x[5] >= 0 && (int32_t)x[5] < 4 ? (int)x[5] : (nx_main_core >= 0 && nx_main_core < 4 ? nx_main_core : 0);
        t->mask = 1ull << t->core;
        t->state = TS_CREATED;
        t->handle = nx_handle_new(KO_THREAD, t, 1);
        *(uint32_t *)(uintptr_t)(t->tls + 0x110) = (uint32_t)t->id; // id del kernel (GetThreadId), no el handle: nxnative threads.md, SDK 22
        if (getenv("NX_TRACE_THREAD_CREATE"))
            fprintf(stderr, "[thread] create tid=%llu handle=%#x entry=%#llx arg=%#llx prio=%d core=%d mask=%#llx\n",
                    (unsigned long long)t->id, t->handle, (unsigned long long)t->entry,
                    (unsigned long long)t->ctx->x[0], t->priority, t->core, (unsigned long long)t->mask);
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
        int thread_rc = pthread_create(&t->pthread, &a, thread_main, t);
        pthread_attr_destroy(&a);
        if (getenv("NX_TRACE_THREAD_CREATE"))
            fprintf(stderr, "[thread] start tid=%llu handle=%#x rc=%d entry=%#llx stack=%#llx prio=%d core=%d\n",
                    (unsigned long long)t->id, t->handle, thread_rc, (unsigned long long)t->entry,
                    (unsigned long long)t->ctx->sp, t->priority, t->core);
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
    case 0x0C: {
        KThread *t = nx_handle_get((uint32_t)x[1], KO_THREAD);
        // Some Switch 2 SDK startup paths query the running thread before its
        // ThreadType handle has been published. Horizon's current-thread pseudo
        // handle is represented by the active kernel thread in this runtime.
        if (!t && !x[1]) t = cur;
        x[0] = t ? R_OK_ : R_INVALID_HANDLE;
        x[1] = t ? (uint64_t)t->priority : 0;
        return;
    }
    case 0x0D: {
        KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD);
        if (!t && !x[0]) t = cur;
        if (t) t->priority = (int)x[1];
        x[0] = t ? R_OK_ : R_INVALID_HANDLE;
        return;
    }
    case 0x0E: {
        KThread *t = nx_handle_get((uint32_t)x[2], KO_THREAD);
        if (!t && !x[2]) t = cur;
        x[0] = t ? R_OK_ : R_INVALID_HANDLE;
        x[1] = t ? (uint64_t)t->core : 0;
        x[2] = t ? t->mask : 0;
        return;
    }
    case 0x0F: { // SetThreadCoreMask(handle, núcleo ideal, máscara): -2 = el del proceso, -3 = no cambiar el ideal
        KThread *t = nx_handle_get((uint32_t)x[0], KO_THREAD);
        if (!t && !x[0]) t = cur;
        int32_t ideal = (int32_t)x[1];
        if (!t) { x[0] = R_INVALID_HANDLE; return; }
        if (ideal == -2) ideal = nx_main_core >= 0 && nx_main_core < 4 ? nx_main_core : 0;
        if (ideal >= 0 && ideal < 4) t->core = ideal;
        if (x[2] & 0xF) t->mask = x[2] & 0xF;
        if (!(t->mask >> t->core & 1)) t->core = __builtin_ctzll(t->mask);   // como Horizon: el ideal pasa a la máscara
        if (t == cur && t->vcore >= 0 && !(t->mask >> t->vcore & 1)) { core_give(t); core_take(t); } // a un núcleo permitido
        x[0] = R_OK_;
        return;
    }
    case 0x10: { // GetCurrentProcessorNumber: el núcleo del hilo (las colas por núcleo de los motores
                 // esperan que coincida con su máscara), si no, el del host
        int cpu = cur && cur->vcore >= 0 ? cur->vcore                     // el núcleo virtual que tiene
                : cur && cur->last_vcore >= 0 ? cur->last_vcore
                : cur && cur->core >= 0 && cur->core < 4 ? cur->core : sched_getcpu();
        x[0] = (uint64_t)(cpu < 0 ? 0 : cpu % 4);
        return;
    }
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
    case 0x13: { // MapSharedMemory(handle, addr, size, perm)
        pthread_mutex_lock(&G);
        KShmem *s = nx_handle_get((uint32_t)x[0], KO_SHMEM);
        if (!s || x[2] > s->size || (x[1] & 0xFFF)) {
            pthread_mutex_unlock(&G);
            x[0] = s ? R_INVALID_SIZE : R_INVALID_HANDLE;
            return;
        }
        int fixed = in_stack_region(x[1], x[2]) && range_free(x[1], x[2]); // sobre la reserva
        void *p = mmap((void *)(uintptr_t)x[1], x[2], PROT_READ | PROT_WRITE,
                       MAP_SHARED | (fixed ? MAP_FIXED : MAP_FIXED_NOREPLACE), s->fd, 0);
        if (p != (void *)(uintptr_t)x[1]) {
            pthread_mutex_unlock(&G);
            fprintf(stderr, "[nxkern] MapSharedMemory en 0x%llx (0x%llx bytes) falló: %s\n",
                    (unsigned long long)x[1], (unsigned long long)x[2], strerror(errno));
            if (p != MAP_FAILED) munmap(p, x[2]);
            x[0] = R_INVALID_ADDR;
            return;
        }
        region_add(x[1], x[2], 0x06, (uint32_t)x[3]);
        pthread_mutex_unlock(&G);
        x[0] = R_OK_;
        return;
    }
    case 0x14: { // UnmapSharedMemory(handle, addr, size)
        pthread_mutex_lock(&G);
        if (in_stack_region(x[1], x[2])) // devolver el hueco a la reserva
            mmap((void *)(uintptr_t)x[1], x[2], PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);
        else
            munmap((void *)(uintptr_t)x[1], x[2]);
        region_remove(x[1]);
        pthread_mutex_unlock(&G);
        x[0] = R_OK_;
        return;
    }
    case 0x15: { // CreateTransferMemory(addr, size, perm) → handle
        pthread_mutex_lock(&G);
        KTmem *m = calloc(1, sizeof *m);
        m->addr = x[1];
        m->size = x[2];
        x[1] = nx_handle_new(KO_TMEM, m, 1);
        pthread_mutex_unlock(&G);
        x[0] = R_OK_;
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
    case 0x22: { // SendSyncRequestWithUserBuffer(buffer, tamaño, handle): el mensaje va en el buffer, no en el TLS
        uint64_t tls = c->tpidrro;
        c->tpidrro = x[0];
        x[0] = nx_send_sync_request(c, (uint32_t)x[2]);
        c->tpidrro = tls;
        return;
    }
    case 0x24: x[0] = R_OK_; x[1] = 0x51; return;      // GetProcessId: un solo proceso
    case 0x2A: case 0x2B: x[0] = R_OK_; return;        // FlushEntireDataCache, FlushDataCache: x86 es coherente
    case 0x30: case 0x31: { // GetResourceLimitLimitValue / CurrentValue(límite, recurso)
        static const uint64_t limite[] = {0xCD500000ull, LIMITE_HILOS, 700, 200, 900};
        uint32_t k = (uint32_t)x[2];
        x[0] = k < 5 ? R_OK_ : R_INVALID_ENUM;
        x[1] = k >= 5 ? 0 : imm == 0x30 ? limite[k] : k == 0 ? heap_size : k == 1 ? hilos_vivos() : limite[k] / 8;
        return;
    }
    case 0x32: x[0] = set_thread_activity((uint32_t)x[0], (uint32_t)x[1]); return;
    case 0x33: x[0] = get_thread_context((uint32_t)x[1], x[0]); return;
    case 0x36: x[0] = R_OK_; return;                   // SynchronizePreemptionState
    case 0x25: { // Hookshot treats an absent handle as the calling thread for GetThreadId.
        KThread *t = nx_handle_get((uint32_t)x[1], KO_THREAD);
        if (!t && !x[1]) t = cur;
        x[0] = t ? R_OK_ : R_INVALID_HANDLE;
        x[1] = t ? t->id : 0;
        return;
    }
    case 0x26: { // svcBreak(reason, address, size)
        // BreakReason (Atmosphère svc_types_common.hpp): 0 Panic ... 7 CppException; el bit 31 es
        // NotificationOnlyFlag. Sin depurador, el kernel vuelve de una notificación sin cerrar el proceso y cierra en
        // los demás casos (mesosphere kern_svc_exception.cpp Break; GPL-2.0, solo el comportamiento). Break no
        // devuelve Result: los registros quedan como estaban.
        static const char *const razones[] = {"Panic", "Assert", "User", "PreLoadDll", "PostLoadDll", "PreUnloadDll",
                                              "PostUnloadDll", "CppException"};
        uint32_t razon = (uint32_t)x[0], base = razon & 0x7FFFFFFFu;
        const char *nombre = base < 8 ? razones[base] : "desconocida";
        int aviso = (razon & 0x80000000u) != 0;
        char donde[256];
        fflush(stdout);
        fprintf(stderr, "[nxrt] svcBreak(0x%x %s%s, 0x%llx, 0x%llx) hilo %llu lr=0x%llx %s sp=0x%llx fp=0x%llx%s\n",
                razon, nombre, aviso ? ", solo aviso" : "", (unsigned long long)x[1], (unsigned long long)x[2],
                (unsigned long long)(cur ? cur->id : 0), (unsigned long long)c->x[30],
                nx_describir_dir(c->x[30], donde, sizeof donde), (unsigned long long)c->sp, (unsigned long long)c->x[29],
                aviso ? ": sin depurador, el proceso sigue" : "");
        if (aviso) return;
        if (x[2] == 4 && x[1]) { // diagAbortWithResult: el resultado va en el buffer
            uint32_t rc;
            if (nx_leer_seguro(x[1], &rc, sizeof rc))
                fprintf(stderr, "[nxrt] resultado 0x%x (%04u-%04u)\n", rc, 2000 + (rc & 0x1FF), rc >> 9 & 0x1FFF);
            else
                fprintf(stderr, "[nxrt] resultado ilegible en 0x%llx\n", (unsigned long long)x[1]);
        }
        if (!aviso && getenv("NX_TRACE_PANIC_REGS")) {
            fprintf(stderr, "[nxrt] panic guest pc=%#llx tid=%llu x0..x15:",
                    (unsigned long long)c->pc, (unsigned long long)(cur ? cur->id : 0));
            for (unsigned i = 0; i < 16; i++) fprintf(stderr, " x%u=%#llx", i, (unsigned long long)c->x[i]);
            fputs(" x19..x30:", stderr);
            for (unsigned i = 19; i < 31; i++) fprintf(stderr, " x%u=%#llx", i, (unsigned long long)c->x[i]);
            fputc('\n', stderr);
            uint8_t words[0x100];
            if (c->sp && nx_leer_seguro(c->sp, words, sizeof words)) {
                fprintf(stderr, "[nxrt] panic guest stack @%#llx:", (unsigned long long)c->sp);
                for (unsigned i = 0; i < sizeof words; i += 4) {
                    uint32_t word;
                    memcpy(&word, words + i, sizeof word);
                    if (!(i & 0x1f)) fprintf(stderr, "\n  +%03x:", i);
                    fprintf(stderr, " %08x", word);
                }
                fputc('\n', stderr);
            } else {
                fprintf(stderr, "[nxrt] panic guest stack unreadable at %#llx\n", (unsigned long long)c->sp);
            }
        }
        nx_pila_invitado(c);
        fprintf(stderr, "[nxrt] svcBreak %s: el proceso termina con código 1\n", nombre);
        _exit(1);
    }
    case 0x27:
        fwrite((const void *)(uintptr_t)x[0], 1, x[1], stdout);
        fflush(stdout);
        x[0] = R_OK_;
        return;
    case 0x28: x[0] = R_OK_; return;                     // ReturnFromException
    case 0x29: x[0] = get_info((uint32_t)x[1], (uint32_t)x[2], x[3], &out); x[1] = out; return;
    case 0x2C: x[0] = map_physical(x[0], x[1], 1); return;
    case 0x2D: x[0] = map_physical(x[0], x[1], 0); return;
    case 0x34: x[0] = wait_for_address(x[0], (uint32_t)x[1], (int64_t)x[2], (int64_t)x[3]); return;
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

// HLE de nn::os::CreateThread (idea de Hookshot, GPL-3.0; código propio): el kernel crea el hilo con la entrada,
// argument and stack. This avoids the guest SDK's MapAliasStack path, which OoT uses for
// its porter thread pool and which is not available in this Linux runtime.
static void nx_thread_create_common(Ctx *c, int core) {
    uint64_t tt = c->x[0], entry = c->x[1], arg = c->x[2];
    uint64_t stack = c->x[3], stack_size = c->x[4];
    if (getenv("NX_TRACE_THREAD_CREATE"))
        fprintf(stderr, "[thread] HLE create tt=%#llx entry=%#llx arg=%#llx stack=%#llx size=%#llx prio=%llu core=%d\n",
                (unsigned long long)tt, (unsigned long long)entry, (unsigned long long)arg,
                (unsigned long long)stack, (unsigned long long)stack_size,
                (unsigned long long)c->x[5], core);
    if (!tt || !entry || !stack || stack_size > UINT64_MAX - stack) {
        c->x[0] = R_INVALID_ADDR;
        return;
    }
    Ctx call = {0};
    call.x[1] = entry;
    call.x[2] = arg;
    call.x[3] = (stack + stack_size) & ~0xFull;
    call.x[4] = c->x[5];
    call.x[5] = (uint64_t)(int64_t)core;
    nx_svc(&call, 0x08);
    if (call.x[0] != R_OK_) {
        c->x[0] = call.x[0];
        return;
    }
    pthread_mutex_lock(&G);
    KThread *t = nx_handle_get((uint32_t)call.x[1], KO_THREAD);
    if (t) {
        t->thread_type = tt;
        t->thread_arg = arg;
        t->stack_top = call.x[3];
        t->ctx->sp = t->stack_top;
        // Lo que el CreateThread del SDK deja en el ThreadType y lee nn::os::GetCurrentStackInfo (SDK de Switch 2:
        // pila en +0x50, tamaño en +0x58, fibra actual en +0x70; sin fibra usa la pila del hilo). Sin estos campos,
        // Nintendo Switch Sports Resort aborta en GetCurrentStackInfo porque sp no cae en [0, 0).
        st64(tt + 0x50, stack);
        st64(tt + 0x58, stack_size);
        st64(tt + 0x70, 0);
        st32(tt + 0x1b0, t->handle);
        st64(t->tls + 0x1f8, tt);
    }
    pthread_mutex_unlock(&G);
    c->x[0] = t ? R_OK_ : R_OUT_OF_RES;
}

void nx_thread_create(Ctx *c) { nx_thread_create_common(c, -2); }
void nx_thread_create_core(Ctx *c) { nx_thread_create_common(c, (int32_t)c->x[6]); }

void nx_thread_start(Ctx *c) {
    Ctx call = {0};
    call.x[0] = c->x[0] ? ld32(c->x[0] + 0x1b0) : 0;
    nx_svc(&call, 0x09);
    c->x[0] = call.x[0];
}

static KThread *thread_type_handle(uint64_t tt) {
    if (!tt) return NULL;
    return nx_handle_get(ld32(tt + 0x1b0), KO_THREAD);
}

void nx_thread_wait(Ctx *c) {
    pthread_mutex_lock(&G);
    KThread *t = thread_type_handle(c->x[0]);
    while (t && t->state != TS_EXITED) pthread_cond_wait(&sync_cv, &G);
    pthread_mutex_unlock(&G);
    c->x[0] = t ? R_OK_ : R_INVALID_HANDLE;
}

void nx_thread_try_wait(Ctx *c) {
    pthread_mutex_lock(&G);
    KThread *t = thread_type_handle(c->x[0]);
    int exited = t && t->state == TS_EXITED;
    pthread_mutex_unlock(&G);
    c->x[0] = !t ? R_INVALID_HANDLE : exited ? R_OK_ : R_TIMEOUT;
}

void nx_thread_destroy(Ctx *c) { nx_thread_wait(c); }

// Lo que toca la tabla de handles desde fuera (servicios) lo hace con G tomado.
void nx_kernel_lock(void) { pthread_mutex_lock(&G); }
void nx_kernel_unlock(void) { pthread_mutex_unlock(&G); }

uint32_t nx_event_new(KEvent **out) {
    pthread_mutex_lock(&G);
    KEvent *ev = calloc(1, sizeof *ev);
    uint32_t h = nx_handle_new(KO_EVENT, ev, 0);
    pthread_mutex_unlock(&G);
    *out = ev;
    return h;
}

void nx_event_signal(KEvent *ev) {
    pthread_mutex_lock(&G);
    ev->signaled = 1;
    pthread_cond_broadcast(&sync_cv);
    pthread_mutex_unlock(&G);
    if (nx2_syncpt_event_signaled) nx2_syncpt_event_signaled(ev);
}

int nx_event_signal_handle(uint32_t h) {
    pthread_mutex_lock(&G);
    HandleEntry *e = handle_entry(h);
    if (!e || e->type != KO_EVENT || !e->writable) {
        pthread_mutex_unlock(&G);
        return 0;
    }
    KEvent *ev = e->obj;
    ev->signaled = 1;
    pthread_cond_broadcast(&sync_cv);
    pthread_mutex_unlock(&G);
    if (nx2_syncpt_event_signaled) nx2_syncpt_event_signaled(ev);
    return 1;
}

int nx_event_clear_handle(uint32_t h) {
    pthread_mutex_lock(&G);
    HandleEntry *e = handle_entry(h);
    if (!e || e->type != KO_EVENT || !e->writable) {
        pthread_mutex_unlock(&G);
        return 0;
    }
    ((KEvent *)e->obj)->signaled = 0;
    pthread_mutex_unlock(&G);
    return 1;
}

uint32_t nx_shmem_new(uint64_t size, KShmem **out) {
    KShmem *s = calloc(1, sizeof *s);
    s->fd = memfd_create("nx-shmem", 0);
    s->size = size;
    if (s->fd < 0 || ftruncate(s->fd, (off_t)size)) return 0;
    s->host = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
    pthread_mutex_lock(&G);
    uint32_t h = nx_handle_new(KO_SHMEM, s, 1);
    pthread_mutex_unlock(&G);
    *out = s;
    return h;
}

