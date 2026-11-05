// Adaptación de platform/platform.h de totk-pc-recomp (lo que usa su capa NVN) sobre POSIX.
#pragma once
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>

// Mutex con lectores compartidos: un rwlock de pthread.
typedef pthread_rwlock_t PlatMutex;
#define PLAT_MUTEX_INIT PTHREAD_RWLOCK_INITIALIZER
static inline void plat_mutex_lock(PlatMutex *m) { pthread_rwlock_wrlock(m); }
static inline void plat_mutex_unlock(PlatMutex *m) { pthread_rwlock_unlock(m); }
static inline void plat_mutex_lock_shared(PlatMutex *m) { pthread_rwlock_rdlock(m); }
static inline void plat_mutex_unlock_shared(PlatMutex *m) { pthread_rwlock_unlock(m); }

static inline void plat_sleep_ms(unsigned ms) {
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000};
    nanosleep(&t, NULL);
}
static inline uint64_t plat_now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
static inline void plat_yield(void) { sched_yield(); }

// Memoria comprometida del proceso (solo informativa en la capa NVN): sin datos en esta adaptación.
static inline void plat_commit_info(double *proc, double *used, double *limit) { *proc = *used = *limit = 0; }
