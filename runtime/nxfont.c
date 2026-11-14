// HLE del buffer de trabajo de ScalableFontEngine.
// Algunas fuentes de sustitución del anfitrión necesitan más caché de glifos que la fuente original.
#include "nxrt.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define FONT_WORK_MIN (512u * 1024u)
#define FONT_ENGINE_MAX 32

typedef struct FontWork {
    uint64_t engine;
    void *game_buffer;
    void *host_buffer;
} FontWork;

static FontWork engines[FONT_ENGINE_MAX];
static pthread_mutex_t engines_lock = PTHREAD_MUTEX_INITIALIZER;

static int find_engine(uint64_t engine) {
    for (int i = 0; i < FONT_ENGINE_MAX; i++)
        if (engines[i].engine == engine) return i;
    return -1;
}

static void discard_engine(uint64_t engine) {
    void *host_buffer = NULL;
    pthread_mutex_lock(&engines_lock);
    int slot = find_engine(engine);
    if (slot >= 0) {
        host_buffer = engines[slot].host_buffer;
        engines[slot] = (FontWork){0};
    }
    pthread_mutex_unlock(&engines_lock);
    free(host_buffer);
}

// The callback is the unwrapped, statically recompiled SDK body. Only the temporary work buffer
// is enlarged; the game's own buffer remains the value returned by GetPointerToWorkBuffer.
void nx_font_engine_initialize(Ctx *c, NxFn original) {
    const uint64_t engine = c->x[0];
    void *game_buffer = (void *)(uintptr_t)c->x[1];
    const uint32_t requested = (uint32_t)c->x[2];
    if (!original || requested >= FONT_WORK_MIN) {
        if (original) original(c);
        discard_engine(engine);
        return;
    }

    void *host_buffer = NULL;
    pthread_mutex_lock(&engines_lock);
    int slot = find_engine(engine);
    if (slot >= 0) {
        engines[slot].game_buffer = game_buffer;
        host_buffer = engines[slot].host_buffer;
    } else {
        for (int i = 0; i < FONT_ENGINE_MAX; i++) {
            if (!engines[i].engine) { slot = i; break; }
        }
        if (slot >= 0) {
            host_buffer = malloc(FONT_WORK_MIN);
            if (host_buffer)
                engines[slot] = (FontWork){engine, game_buffer, host_buffer};
        }
    }
    pthread_mutex_unlock(&engines_lock);

    if (!host_buffer) {
        fprintf(stderr, "[nxrt] font: no se pudo ampliar el buffer del motor %p (%u bytes)\n",
                (void *)(uintptr_t)engine, requested);
        original(c);
        return;
    }

    c->x[1] = (uintptr_t)host_buffer;
    c->x[2] = FONT_WORK_MIN;
    fprintf(stderr, "[nxrt] font: motor %p usa buffer anfitrión de %u bytes (el juego pidió %u)\n",
            (void *)(uintptr_t)engine, FONT_WORK_MIN, requested);
    original(c);
}

void nx_font_engine_get_work_buffer(Ctx *c, NxFn original) {
    const uint64_t engine = c->x[0];
    if (original) original(c);
    pthread_mutex_lock(&engines_lock);
    int slot = find_engine(engine);
    if (slot >= 0) c->x[0] = (uintptr_t)engines[slot].game_buffer;
    pthread_mutex_unlock(&engines_lock);
}

void nx_font_engine_finalize(Ctx *c, NxFn original) {
    const uint64_t engine = c->x[0];
    if (original) original(c);
    discard_engine(engine);
}

// Keep the result of the SDK font calls visible while diagnosing titles that ignore a failed load.
// The return value is left in x0 exactly as the original function produced it.
static int font_trace_slot(void) {
    const char *trace = getenv("NX_TRAZA_FONT");
    if (!trace || !*trace || !strcmp(trace, "0")) return 0;
    static unsigned calls;
    return __sync_fetch_and_add(&calls, 1) < 64;
}

void nx_font_engine_load_font(Ctx *c, NxFn original) {
    uint64_t args[6];
    for (unsigned i = 0; i < 6; i++) args[i] = c->x[i];
    if (original) original(c);
    uint64_t result = c->x[0];
    if (font_trace_slot())
        fprintf(stderr, "[nxrt] font: LoadFont(this=%p, a1=%p, a2=%p, a3=%#llx, a4=%#llx, a5=%#llx) -> %#llx\n",
                (void *)(uintptr_t)args[0], (void *)(uintptr_t)args[1], (void *)(uintptr_t)args[2],
                (unsigned long long)args[3], (unsigned long long)args[4], (unsigned long long)args[5],
                (unsigned long long)result);
}

void nx_font_engine_set_font(Ctx *c, NxFn original) {
    uint64_t engine = c->x[0], font = c->x[1];
    if (original) original(c);
    uint64_t result = c->x[0];
    if (font_trace_slot())
        fprintf(stderr, "[nxrt] font: SetFont(this=%p, font=%p) -> %#llx\n", (void *)(uintptr_t)engine,
                (void *)(uintptr_t)font, (unsigned long long)result);
}
