// El kernel de Horizon en el host (nxkern.c) y los servicios por IPC (nxipc.c).
#pragma once
#include <pthread.h>

#include "nxrt.h"

// Disposición de memoria de un proceso de 39 bits, como en Horizon.
#define NX_HEAP_BASE 0x1000000000ull
#define NX_HEAP_MAX 0x180000000ull      // 6 GiB
#define NX_ALIAS_BASE 0x2000000000ull
#define NX_ALIAS_SIZE 0x1000000000ull   // 64 GiB
#define NX_STACK_BASE 0x4000000000ull
#define NX_STACK_SIZE 0x80000000ull     // 2 GiB
#define NX_TLS_BASE 0x3F000000ull       // 0x200 bytes por hilo; los primeros 0x100 son el buffer de IPC
#define NX_TLS_SIZE 0x100000ull

enum { KO_THREAD = 1, KO_EVENT, KO_SESSION, KO_SHMEM, KO_TMEM };
enum { TS_CREATED = 1, TS_RUNNING, TS_EXITED };
enum { W_NONE, W_MUTEX, W_CONDVAR, W_ADDRESS, W_SYNC };

typedef struct KThread {
    Ctx *ctx;
    pthread_t pthread;
    pthread_cond_t cv;
    uint64_t id, entry, tls;
    uint32_t handle;
    int priority, core, state;
    int pausa, pausado; // SetThreadActivity: pedido de pausa y confirmación del hilo
    // espera
    int waiting, woken, cancel, cv_timeout_ignored;
    uint32_t result, tag;
    uint64_t wait_addr, mutex_addr, seq;
    struct KThread *lock_owner;
} KThread;

typedef struct KEvent {
    int signaled;
} KEvent;

typedef struct KSession KSession;

typedef struct KShmem {    // memoria compartida: un memfd que el runtime y el juego mapean
    int fd;
    uint64_t size;
    uint8_t *host;
} KShmem;

typedef struct KTmem {     // memoria de transferencia: el juego le presta una zona a un servicio
    uint64_t addr, size;
} KTmem;

KThread *nx_kernel_main_thread(void);
KThread *nx_current_thread(void);
void nx_kernel_adopt(KThread *t);
__attribute__((noreturn)) void nx_exit_thread(void);
void nx_memory_init(void);
void nx_memory_add_stack(uint64_t addr, uint64_t size);
uint32_t nx_handle_new(int type, void *obj, int writable);
void *nx_handle_get(uint32_t h, int type);
void nx_kernel_lock(void);
void nx_kernel_unlock(void);

// Para los servicios: eventos que señala el sistema y memoria compartida.
uint32_t nx_event_new(KEvent **out);              // devuelve el handle de lectura
void nx_event_signal(KEvent *ev);
uint32_t nx_shmem_new(uint64_t size, KShmem **out); // devuelve un handle para copiar al juego

// IPC (nxipc.c)
uint32_t nx_connect_named_port(const char *name, uint64_t *handle);
uint32_t nx_send_sync_request(Ctx *c, uint32_t handle);
void nx_session_close(KSession *s);
