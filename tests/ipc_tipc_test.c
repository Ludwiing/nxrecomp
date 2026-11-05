// Host regression: real nxipc.c with isolated kernel/services, no game files.
#include "nxkern.h"
#include "nxsvc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *handles[32];
static unsigned next_handle = 1;
void nx_kernel_lock(void) {}
void nx_kernel_unlock(void) {}
uint32_t nx_handle_new(int type, void *object, int writable) {
    (void)type; (void)writable;
    assert(next_handle < 32);
    handles[next_handle] = object;
    return next_handle++;
}
void *nx_handle_get(uint32_t handle, int type) {
    (void)type;
    return handle < 32 ? handles[handle] : NULL;
}
static uint32_t service(ServiceObject *self, NxRequest *r) {
    (void)self;
    if (r->cmd == 0) {
        assert(r->pid == 123);
        assert(r->in_size == 0);
        return 0;
    }
    if (r->cmd == 1) {
        assert(r->in_size == 8);
        assert(!memcmp(r->in, "lm\0\0\0\0\0\0", 8));
        nx_out_object(r, nx_service_sm());
        return 0;
    }
    if (r->cmd == 7) { nx_out32(r, 0x11223344); return 0; }
    return NX_R_UNKNOWN_CMD;
}
ServiceObject *nx_service_sm(void) {
    static ServiceObject object = {"test", service, NULL};
    return &object;
}
int main(void) {
    uint32_t tls[64] = {0};
    Ctx ctx = {.tpidrro = (uintptr_t)tls};
    uint64_t handle;
    assert(nx_connect_named_port("sm:", &handle) == 0);
    // TIPC RegisterClient: PID descriptor, no raw data, no SFCI.
    tls[0] = 16; tls[1] = 1u << 31; tls[2] = 1; tls[3] = 123;
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(tls[1] == 1 && tls[2] == 0);
    memset(tls, 0, sizeof tls);
    tls[0] = 17; tls[1] = 2; memcpy(tls + 2, "lm", 2);
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(tls[1] == ((1u << 31) | 1));
    assert(tls[2] == 32 && tls[3] != 0 && tls[4] == 0);
    uint32_t child = tls[3];
    memset(tls, 0, sizeof tls);
    tls[0] = 23;
    assert(nx_send_sync_request(&ctx, child) == 0);
    assert(tls[1] == 2 && tls[2] == 0 && tls[3] == 0x11223344);
    memset(tls, 0, sizeof tls);
    tls[0] = 116;
    assert(nx_send_sync_request(&ctx, child) == 0);
    assert(tls[1] == 1 && tls[2] == NX_R_UNKNOWN_CMD);
    // The CMIF route still emits its aligned SFCO header and result.
    memset(tls, 0, sizeof tls);
    tls[0] = 4; tls[1] = 10; memcpy(tls + 4, "SFCI", 4); tls[6] = 7;
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(!memcmp(tls + 4, "SFCO", 4));
    assert(tls[6] == 0 && tls[8] == 0x11223344);
    // ControlWithContext must reach IPC control, not the service handler.
    memset(tls, 0, sizeof tls);
    tls[0] = 7; tls[1] = 8; memcpy(tls + 4, "SFCI", 4);
    tls[5] = 1; tls[6] = 3; // QueryPointerBufferSize
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(tls[6] == 0 && tls[8] == 0x8000);
    memset(tls, 0, sizeof tls);
    tls[0] = 7; tls[1] = 8; memcpy(tls + 4, "SFCI", 4);
    tls[5] = 1; tls[6] = 0; // ConvertCurrentObjectToDomain
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(tls[6] == 0 && tls[8] == 1);
    memset(tls, 0, sizeof tls);
    tls[0] = 6; tls[1] = 12;
    tls[4] = 1 | (16 << 16); tls[5] = 1; // domain object 1
    memcpy(tls + 8, "SFCI", 4); tls[9] = 1; tls[10] = 7;
    assert(nx_send_sync_request(&ctx, handle) == 0);
    assert(!memcmp(tls + 8, "SFCO", 4));
    assert(tls[10] == 0 && tls[12] == 0x11223344);
    memset(tls, 0, sizeof tls);
    tls[0] = 7; tls[1] = 8; memcpy(tls + 4, "SFCI", 4); tls[6] = 2;
    assert(nx_send_sync_request(&ctx, handle) == 0);
    uint32_t clone = tls[3];
    assert(clone && tls[6] == 0);
    memset(tls, 0, sizeof tls);
    tls[0] = 6; tls[1] = 12;
    tls[4] = 1 | (16 << 16); tls[5] = 1;
    memcpy(tls + 8, "SFCI", 4); tls[9] = 1; tls[10] = 7;
    assert(nx_send_sync_request(&ctx, clone) == 0);
    assert(!memcmp(tls + 8, "SFCO", 4));
    assert(tls[10] == 0 && tls[12] == 0x11223344);
    puts("TIPC PID, command, payload, moved handle, error and CMIF regression: PASS");
    return 0;
}
