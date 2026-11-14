#include "nxrt.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t called_size;
static void *called_buffer;
static int finalized;

static void original_init(Ctx *c) {
    called_buffer = (void *)(uintptr_t)c->x[1];
    called_size = c->x[2];
    c->x[0] = 0;
}

static void original_get(Ctx *c) {
    c->x[0] = (uintptr_t)called_buffer;
}

static void original_fini(Ctx *c) {
    finalized = 1;
    c->x[0] = 0;
}

void nx_font_engine_initialize(Ctx *, NxFn);
void nx_font_engine_get_work_buffer(Ctx *, NxFn);
void nx_font_engine_finalize(Ctx *, NxFn);

int main(void) {
    uint8_t game_buffer[64 * 1024];
    Ctx c = {0};
    c.x[0] = 0x1234;
    c.x[1] = (uintptr_t)game_buffer;
    c.x[2] = sizeof game_buffer;
    nx_font_engine_initialize(&c, original_init);
    assert(called_size == 512u * 1024u);
    assert(called_buffer && called_buffer != game_buffer);

    c.x[0] = 0x1234;
    nx_font_engine_get_work_buffer(&c, original_get);
    assert(c.x[0] == (uintptr_t)game_buffer);

    c.x[0] = 0x1234;
    nx_font_engine_finalize(&c, original_fini);
    assert(finalized && c.x[0] == 0);

    // Large buffers keep the original SDK path and do not allocate a replacement.
    uint8_t large_buffer[512 * 1024];
    called_buffer = NULL;
    called_size = 0;
    c = (Ctx){0};
    c.x[0] = 0x5678;
    c.x[1] = (uintptr_t)large_buffer;
    c.x[2] = sizeof large_buffer;
    nx_font_engine_initialize(&c, original_init);
    assert(called_buffer == large_buffer && called_size == sizeof large_buffer);
    puts("font engine HLE preserves the game buffer and supplies 512 KiB to small font engines: PASS");
    return 0;
}
