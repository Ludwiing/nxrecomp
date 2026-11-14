// Command and control memory exhaustion of NVN command buffers (runtime/nvn/nvn_api_cmd.c), driven through the same
// nvnp_* entry points the game calls. The game's memory callback runs through rt_call like the real one (registers
// saved and restored around it) and calls AddCommandMemory / AddControlMemory through the API with the registers it
// sets, as guest code does. Covered:
// - command memory that runs out while recording asks the callback (OUT_OF_COMMAND_MEMORY, min size >= the record),
//   every time it runs out, and GetCommandMemoryUsed counts on in the new memory instead of staying at the old size;
// - control memory that cannot hold one more recording asks the callback (OUT_OF_CONTROL_MEMORY) at EndRecording, the
//   handle lives in the new memory and GetControlMemoryUsed counts on;
// - a game that measures a recording as (command used + control used) after minus before, and drops recordings under
//   9 bytes as empty, keeps a one-command recording made while both memories are full;
// - records made around the callback keep their arguments and none is dropped;
// - disabled callbacks and callbacks that add nothing: no callback / one callback per need, counters at the size;
// - the existing calls: BeginRecording without command memory, EndRecording without control memory.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "nvn_api.h"
#include "platform/platform.h"

// ---------- the runtime services nvn_api_cmd.c uses (single-threaded test)
unsigned rt_trace;
void nvn_called(uint8_t *seen, const char *name) { (void)seen, (void)name; }
const char *cfg_get(const char *name) { (void)name; return NULL; }
uint64_t gfx_replayed_submits(void) { return 0; }
void plat_mutex_lock(PlatMutex *m) { (void)m; }
void plat_mutex_unlock(PlatMutex *m) { (void)m; }
void plat_mutex_lock_shared(PlatMutex *m) { (void)m; }
void plat_mutex_unlock_shared(PlatMutex *m) { (void)m; }
uint64_t plat_now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}
void plat_sleep_ms(uint32_t ms) { (void)ms; }
void plat_commit_info(double *p, double *u, double *l) { *p = *u = *l = 0; }
static int nlogs;
void rt_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("[log] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    nlogs++;
}
void rt_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("[fatal] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}
// rt_call as in runtime/nvn/nvn_host.c: arguments in x0.., the host's registers back afterwards. The "guest" function is
// a host function here.
static int in_rt_call;
uint64_t rt_call(Cpu *c, uint64_t fn, int n, ...) {
    uint64_t x[31], sp = c->sp;
    V128 v[32];
    memcpy(x, c->x, sizeof x);
    memcpy(v, c->vr, sizeof v);
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) c->x[i] = va_arg(ap, uint64_t);
    va_end(ap);
    in_rt_call++;
    ((void (*)(Cpu *))(uintptr_t)fn)(c);
    in_rt_call--;
    uint64_t r = c->x[0];
    memcpy(c->x, x, sizeof x);
    memcpy(c->vr, v, sizeof v);
    c->sp = sp;
    return r;
}

// ---------- the entry points under test
#define P(n) void nvnp_##n(Cpu *c);
P(nvnCommandBufferInitialize) P(nvnCommandBufferFinalize) P(nvnCommandBufferSetMemoryCallback)
P(nvnCommandBufferSetMemoryCallbackData) P(nvnCommandBufferSetCommandMemoryCallbackEnabled)
P(nvnCommandBufferSetControlMemoryCallbackEnabled) P(nvnCommandBufferAddCommandMemory)
P(nvnCommandBufferAddControlMemory) P(nvnCommandBufferGetCommandMemorySize) P(nvnCommandBufferGetCommandMemoryUsed)
P(nvnCommandBufferGetCommandMemoryFree) P(nvnCommandBufferGetControlMemorySize) P(nvnCommandBufferGetControlMemoryUsed)
P(nvnCommandBufferBeginRecording) P(nvnCommandBufferEndRecording) P(nvnCommandBufferCopyBufferToBuffer)
P(nvnCommandBufferCallCommands) P(nvnCommandBufferSetViewportSwizzles)
#undef P

static Cpu cpu;
static uint64_t call(void (*f)(Cpu *), int n, ...) {
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) cpu.x[i] = va_arg(ap, uint64_t);
    va_end(ap);
    f(&cpu);
    return cpu.x[0];
}
#define CALL(fn, ...) call(nvnp_nvnCommandBuffer##fn, (int)(sizeof((uint64_t[]){__VA_ARGS__}) / 8), __VA_ARGS__)
#define U(p) ((uint64_t)(uintptr_t)(p))

static int fails;
#define CHECK(cond, ...)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fails++;                                                                                                   \
            printf("MAL %s:%d: ", __func__, __LINE__);                                                                 \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

// ---------- the game: its command buffer, memory and callback
static _Alignas(16) uint8_t cmdbuf[sizeof(NVNcommandBuffer)];
static uint8_t pool_mem[1 << 20];
static pool_t pool = {.magic = NVN_POOL_MAGIC, .mem = pool_mem, .size = sizeof pool_mem};
static uint8_t ctl_mem[1 << 16];

typedef struct {
    int calls[2];                // per event
    uint64_t last_min[2];
    uint64_t cmd_grant, ctl_grant;   // bytes the callback adds (0: adds nothing)
    uint64_t cmd_next, ctl_next;     // where the next block starts
    int bad_args, nested;
} game_t;
static game_t game;

// void callback(NVNcommandBuffer *cmd, NVNcommandBufferMemoryEvent event, size_t minSize, void *data), as guest code:
// it calls the API with its own register values.
static void game_callback(Cpu *c) {
    uint64_t o = c->x[0], ev = c->x[1], min = c->x[2], data = c->x[3];
    if (o != U(cmdbuf) || data != U(&game) || ev > 1) { game.bad_args++; return; }
    if (in_rt_call != 1) game.nested++;
    game.calls[ev]++;
    game.last_min[ev] = min;
    if (ev == NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY && game.cmd_grant) {
        uint64_t n = game.cmd_grant < min ? min : game.cmd_grant;
        c->x[0] = o, c->x[1] = U(&pool), c->x[2] = game.cmd_next, c->x[3] = n;
        nvnp_nvnCommandBufferAddCommandMemory(c);
        game.cmd_next += n;
    }
    if (ev == NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_CONTROL_MEMORY && game.ctl_grant) {
        uint64_t n = game.ctl_grant < min ? min : game.ctl_grant;
        c->x[0] = o, c->x[1] = U(ctl_mem + game.ctl_next), c->x[2] = n;
        nvnp_nvnCommandBufferAddControlMemory(c);
        game.ctl_next += n;
    }
    for (int i = 0; i < 18; i++) c->x[i] = 0xdead0000u + (uint64_t)i;   // a guest function clobbers its scratch registers
}

static uint64_t cmd_used(void) { return CALL(GetCommandMemoryUsed, U(cmdbuf)); }
static uint64_t ctl_used(void) { return CALL(GetControlMemoryUsed, U(cmdbuf)); }

static void setup(uint64_t cmd_size, uint64_t ctl_size) {
    memset(&game, 0, sizeof game);
    memset(cmdbuf, 0, sizeof cmdbuf);
    CALL(Initialize, U(cmdbuf), 0);
    CALL(SetMemoryCallback, U(cmdbuf), U(game_callback));
    CALL(SetMemoryCallbackData, U(cmdbuf), U(&game));
    CALL(SetCommandMemoryCallbackEnabled, U(cmdbuf), 1);
    CALL(SetControlMemoryCallbackEnabled, U(cmdbuf), 1);
    if (cmd_size) CALL(AddCommandMemory, U(cmdbuf), U(&pool), 0, cmd_size);
    if (ctl_size) CALL(AddControlMemory, U(cmdbuf), U(ctl_mem), ctl_size);
    game.cmd_next = cmd_size;
    game.ctl_next = ctl_size;
}

static void copy_b2b(uint64_t k) { CALL(CopyBufferToBuffer, U(cmdbuf), 0x1000 + k, 0x2000 + k, 0x10 + k, 0); }

// The COPY_B2B records of the recording behind handle h, in order; -1 when a record has other arguments.
static int count_copies(uint64_t h) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)h;
    int n = 0;
    for (size_t at = 0;;) {
        op_hdr hd;
        memcpy(&hd, p + at, sizeof hd);
        if (hd.op == OP_COPY_B2B) {
            p_copy_b2b r;
            memcpy(&r, p + at + 8, sizeof r);
            if (r.src != 0x1000u + (uint64_t)n || r.dst != 0x2000u + (uint64_t)n || r.size != 0x10u + (uint64_t)n) return -1;
            n++;
        }
        if (hd.op == OP_END) return n;
        at += hd.size;
    }
}

// Command memory runs out twice in one recording: two callbacks, every command kept with its arguments, Used counts in
// the newest memory.
static void test_command_exhaustion(void) {
    setup(64, 4096);
    game.cmd_grant = 1024;
    CALL(BeginRecording, U(cmdbuf));
    CHECK(game.calls[0] == 0, "callback at BeginRecording with memory: %d", game.calls[0]);
    int n = 0;
    while (game.calls[0] == 0 && n < 100) copy_b2b((uint64_t)n++);
    CHECK(game.calls[0] == 1, "no OUT_OF_COMMAND_MEMORY callback after %d records in 64 bytes", n);
    CHECK(game.last_min[0] >= 32, "min size %llu below the record that did not fit", (unsigned long long)game.last_min[0]);
    CHECK(CALL(GetCommandMemorySize, U(cmdbuf)) == 1024, "size after the callback %llu",
          (unsigned long long)CALL(GetCommandMemorySize, U(cmdbuf)));
    uint64_t u1 = cmd_used();
    CHECK(u1 > 0 && u1 <= 64, "used after the callback %llu (the record that did not fit, in the new memory)",
          (unsigned long long)u1);
    copy_b2b((uint64_t)n++);
    CHECK(cmd_used() > u1, "used frozen after the callback: %llu -> %llu", (unsigned long long)u1,
          (unsigned long long)cmd_used());
    while (game.calls[0] == 1 && n < 400) copy_b2b((uint64_t)n++);
    CHECK(game.calls[0] == 2, "the callback was not asked again when the new memory ran out (%d records)", n);
    uint64_t h = CALL(EndRecording, U(cmdbuf));
    CHECK(h != 0, "null handle");
    int kept = h ? count_copies(h) : 0;
    CHECK(kept == n, "%d of %d records in the recording (-1: wrong arguments)", kept, n);
    CHECK(!game.bad_args && !game.nested, "callback arguments %d, nested %d", game.bad_args, game.nested);
    CALL(Finalize, U(cmdbuf));
}

// Control memory for two recordings: the third EndRecording asks for more, the handle is live, Used counts on.
static void test_control_exhaustion(void) {
    setup(1 << 16, 128);
    game.ctl_grant = 256;
    uint64_t h[4];
    for (int i = 0; i < 4; i++) {
        CALL(BeginRecording, U(cmdbuf));
        copy_b2b(0);
        h[i] = CALL(EndRecording, U(cmdbuf));
        CHECK(h[i] != 0, "recording %d: null handle", i);
    }
    CHECK(game.calls[1] == 1, "OUT_OF_CONTROL_MEMORY callbacks: %d (want 1)", game.calls[1]);
    CHECK(game.last_min[1] >= 64, "control min size %llu", (unsigned long long)game.last_min[1]);
    CHECK(CALL(GetControlMemorySize, U(cmdbuf)) == 256, "control size %llu",
          (unsigned long long)CALL(GetControlMemorySize, U(cmdbuf)));
    CHECK(ctl_used() == 128, "control used %llu after two recordings in the new memory", (unsigned long long)ctl_used());
    CALL(BeginRecording, U(cmdbuf));
    CALL(CallCommands, U(cmdbuf), 1, U(&h[3]));   // the handle made in the new memory is live
    uint64_t h5 = CALL(EndRecording, U(cmdbuf));
    CHECK(h5 && count_copies(h5) == 1, "called recording not inlined");
    CALL(Finalize, U(cmdbuf));
}

// The measure Mario Tennis Fever (Switch 2) takes of each recording (ro module 0xaf633c4): command plus control used,
// after EndRecording minus before BeginRecording, in 32 bits; under 9 bytes the handle is dropped as an empty
// recording. With both memories full a one-command recording must still measure 9 bytes or more.
static void test_full_memories_keep_recording(void) {
    setup(256, 256);
    game.cmd_grant = 4096;
    game.ctl_grant = 4096;
    int dropped = 0;
    for (int i = 0; i < 40; i++) {
        uint32_t cmd0 = (uint32_t)cmd_used(), ctl0 = (uint32_t)ctl_used();
        CALL(BeginRecording, U(cmdbuf));
        copy_b2b(0);
        uint64_t h = CALL(EndRecording, U(cmdbuf));
        uint32_t d = ((uint32_t)cmd_used() - cmd0) + ((uint32_t)ctl_used() - ctl0);
        if (d < 9) {
            if (!dropped) printf("  recording %d measures %u bytes (command %llu/%llu, control %llu/%llu)\n", i, d,
                                 (unsigned long long)cmd_used(), (unsigned long long)CALL(GetCommandMemorySize, U(cmdbuf)),
                                 (unsigned long long)ctl_used(), (unsigned long long)CALL(GetControlMemorySize, U(cmdbuf)));
            dropped++;
        }
        CHECK(h != 0, "null handle");
    }
    CHECK(dropped == 0, "%d of 40 one-command recordings measure under 9 bytes (frozen counters)", dropped);
    CHECK(game.calls[0] >= 1 && game.calls[1] >= 1, "callbacks: command %d, control %d", game.calls[0], game.calls[1]);
    CALL(Finalize, U(cmdbuf));
}

// Callbacks disabled: none is called, nothing is dropped, the counters stop at the size (the previous behaviour).
static void test_disabled(void) {
    setup(64, 64);
    game.cmd_grant = game.ctl_grant = 4096;
    CALL(SetCommandMemoryCallbackEnabled, U(cmdbuf), 0);
    CALL(SetControlMemoryCallbackEnabled, U(cmdbuf), 0);
    int n = 0;
    for (int r = 0; r < 3; r++) {
        CALL(BeginRecording, U(cmdbuf));
        for (int i = 0; i < 5; i++) copy_b2b((uint64_t)i), n++;
        uint64_t h = CALL(EndRecording, U(cmdbuf));
        CHECK(h && count_copies(h) == 5, "recording %d lost records", r);
    }
    CHECK(game.calls[0] == 0 && game.calls[1] == 0, "disabled callbacks called: %d %d", game.calls[0], game.calls[1]);
    CHECK(cmd_used() == 64 && ctl_used() == 64, "used %llu / %llu (want the sizes)", (unsigned long long)cmd_used(),
          (unsigned long long)ctl_used());
    CALL(Finalize, U(cmdbuf));
}

// A callback that adds nothing: asked once per record that does not fit (not in a loop), the record is kept, the
// counter stays at the size, and the runtime says so.
static void test_callback_adds_nothing(void) {
    setup(64, 4096);
    int logs0 = nlogs;
    CALL(BeginRecording, U(cmdbuf));
    for (int i = 0; i < 4; i++) copy_b2b((uint64_t)i);   // 4 x 32 bytes: the 3rd and 4th do not fit
    uint64_t h = CALL(EndRecording, U(cmdbuf));
    CHECK(game.calls[0] == 2, "callbacks %d (want one per record that did not fit: 2)", game.calls[0]);
    CHECK(h && count_copies(h) == 4, "records lost");
    CHECK(cmd_used() == 64, "used %llu", (unsigned long long)cmd_used());
    CHECK(nlogs > logs0, "no report of the callback that added no memory");
    CALL(Finalize, U(cmdbuf));
}

// The calls that existed: BeginRecording without command memory and EndRecording without control memory ask once each.
static void test_begin_end_without_memory(void) {
    setup(0, 0);
    game.cmd_grant = game.ctl_grant = 4096;
    CALL(BeginRecording, U(cmdbuf));
    CHECK(game.calls[0] == 1, "BeginRecording without command memory: %d callbacks", game.calls[0]);
    copy_b2b(0);
    uint64_t h = CALL(EndRecording, U(cmdbuf));
    CHECK(game.calls[1] == 1, "EndRecording without control memory: %d callbacks", game.calls[1]);
    CHECK(game.calls[0] == 1, "command callbacks after one record: %d", game.calls[0]);
    CHECK(h && count_copies(h) == 1, "record lost");
    CHECK(cmd_used() == 32 && ctl_used() == 64, "used %llu / %llu", (unsigned long long)cmd_used(),
          (unsigned long long)ctl_used());
    CALL(Finalize, U(cmdbuf));
}

// CallCommands inlines a recording larger than the command memory left: one callback, Used counts the inlined bytes in
// the new memory, the records are all there.
static void test_call_commands_exhaustion(void) {
    setup(1 << 16, 4096);
    game.cmd_grant = 4096;
    CALL(BeginRecording, U(cmdbuf));
    for (int i = 0; i < 10; i++) copy_b2b((uint64_t)i);
    uint64_t h = CALL(EndRecording, U(cmdbuf));
    CALL(AddCommandMemory, U(cmdbuf), U(&pool), 1 << 17, 64);
    CALL(BeginRecording, U(cmdbuf));
    CALL(CallCommands, U(cmdbuf), 1, U(&h));
    CHECK(game.calls[0] == 1, "callbacks for the inlined recording: %d", game.calls[0]);
    CHECK(game.last_min[0] >= 320, "min size %llu below the inlined 320 bytes", (unsigned long long)game.last_min[0]);
    CHECK(cmd_used() == 320, "used %llu after inlining 320 bytes into new memory", (unsigned long long)cmd_used());
    uint64_t h2 = CALL(EndRecording, U(cmdbuf));
    CHECK(h2 && count_copies(h2) == 10, "inlined records lost");
    CALL(Finalize, U(cmdbuf));
}

// A null command handle stays fatal (nvn_api_cmd.c inline_recording): the change hides no invalid handle.
static void test_null_handle_still_fatal(void) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        setup(1 << 16, 4096);
        uint64_t zero = 0;
        CALL(BeginRecording, U(cmdbuf));
        CALL(CallCommands, U(cmdbuf), 1, U(&zero));
        _exit(0);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 2, "CallCommands with a null handle did not end in rt_fatal (status %#x)", st);
}

static void test_viewport_swizzle_recording(void) {
    setup(1 << 16, 4096);
    CALL(BeginRecording, U(cmdbuf));
    const NVNviewportSwizzle sw[4] = {
        NVN_VIEWPORT_SWIZZLE_NEGATIVE_Y, NVN_VIEWPORT_SWIZZLE_POSITIVE_X,
        NVN_VIEWPORT_SWIZZLE_POSITIVE_Z, NVN_VIEWPORT_SWIZZLE_NEGATIVE_W};
    CALL(SetViewportSwizzles, U(cmdbuf), 0, 1, U(sw));
    uint64_t h = CALL(EndRecording, U(cmdbuf));
    const op_hdr *op = nvn_op_first(h);
    while (op->op != OP_VIEWPORT_SWIZZLES && op->op != OP_END) op = nvn_op_next(op);
    CHECK(op->op == OP_VIEWPORT_SWIZZLES, "viewport swizzle wasn't recorded");
    if (op->op == OP_VIEWPORT_SWIZZLES) {
        const p_viewport_swizzles *r = nvn_op_payload(op);
        CHECK(r->first == 0 && r->n == 1, "wrong viewport swizzle range %d,%d", r->first, r->n);
        for (int i = 0; i < 4; i++)
            CHECK(r->v[0][i] == (uint8_t)sw[i], "viewport swizzle component %d changed: got %u expected %d",
                  i, r->v[0][i], (int)sw[i]);
    }
    CALL(Finalize, U(cmdbuf));
}

int main(void) {
    test_command_exhaustion();
    test_control_exhaustion();
    test_full_memories_keep_recording();
    test_disabled();
    test_callback_adds_nothing();
    test_begin_end_without_memory();
    test_call_commands_exhaustion();
    test_null_handle_still_fatal();
    test_viewport_swizzle_recording();
    if (fails) {
        printf("MAL: %d comprobaciones de memoria de command buffers NVN\n", fails);
        return 1;
    }
    printf("OK: agotamiento de memoria de comandos y de control NVN (callbacks, contadores, grabaciones completas)\n");
    return 0;
}
