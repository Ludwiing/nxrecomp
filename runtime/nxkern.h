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

enum { KO_THREAD = 1, KO_EVENT, KO_SESSION };
enum { TS_CREATED = 1, TS_RUNNING, TS_EXITED };
enum { W_NONE, W_MUTEX, W_CONDVAR, W_ADDRESS, W_SYNC };

typedef struct KThread {
    Ctx *ctx;
    pthread_t pthread;
    pthread_cond_t cv;
    uint64_t id, entry, tls;
    uint32_t handle;
    int priority, core, state;
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

KThread *nx_kernel_main_thread(void);
KThread *nx_current_thread(void);
__attribute__((noreturn)) void nx_exit_thread(void);
void nx_memory_init(void);
void nx_memory_add_stack(uint64_t addr, uint64_t size);
uint32_t nx_handle_new(int type, void *obj, int writable);
void *nx_handle_get(uint32_t h, int type);
void nx_kernel_lock(void);
void nx_kernel_unlock(void);

// IPC (nxipc.c)
uint32_t nx_connect_named_port(const char *name, uint64_t *handle);
uint32_t nx_send_sync_request(Ctx *c, uint32_t handle);
void nx_session_close(KSession *s);
