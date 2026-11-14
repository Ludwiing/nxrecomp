#include "nxhle_event.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SDK_CREATE 0x810000
#define SDK_WRITABLE 0x820000
static uint64_t event_addr_seen;
static uint64_t clear_mode_seen;
static uint64_t interprocess_seen;
static int fail_create;

uint64_t nx_symbol(const char *name) {
    if (!strcmp(name, "_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb"))
        return SDK_CREATE;
    if (!strcmp(name, "_ZN2nn2os30GetWritableHandleOfSystemEventEPKNS0_15SystemEventTypeE"))
        return SDK_WRITABLE;
    return 0;
}

void nx_dispatch(Ctx *ctx, uint64_t addr) {
    if (addr == SDK_CREATE) {
        event_addr_seen = ctx->x[0];
        clear_mode_seen = ctx->x[1];
        interprocess_seen = ctx->x[2];
        ctx->x[0] = fail_create ? 0x1234 : 0;
    } else if (addr == SDK_WRITABLE) {
        assert(ctx->x[0] == event_addr_seen);
        ctx->x[0] = 0x1002;
    } else {
        assert(!"unexpected guest SDK target");
    }
}

int main(void) {
    Ctx ctx = {0};
    ctx.x[30] = 0xabcdef;
    ctx.sp = 0x40001000;
    uint32_t handle = 0;
    uint32_t sdk_result = UINT32_MAX;
    assert(nx_hle_event_create(&ctx, 0x40002000, 0, 1, &handle, &sdk_result));
    assert(event_addr_seen == 0x40002000 && clear_mode_seen == 0 && interprocess_seen == 1);
    assert(handle == 0x1002 && sdk_result == 0);
    assert(ctx.x[30] == 0xabcdef && ctx.sp == 0x40001000);

    fail_create = 1;
    handle = 0x7777;
    sdk_result = UINT32_MAX;
    assert(!nx_hle_event_create(&ctx, 0x40003000, 1, 0, &handle, &sdk_result));
    assert(sdk_result == 0x1234 && handle == 0x7777);
    puts("OK: SDK crea SystemEvent y entrega al host el handle de escritura");
    return 0;
}
