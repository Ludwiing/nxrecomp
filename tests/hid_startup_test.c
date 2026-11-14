#include "../runtime/nxsvc_hid.c"
#include <assert.h>
#include <stdio.h>

ServiceObject *nx_object(const char *name, uint32_t (*handle)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = malloc(sizeof *o);
    *o = (ServiceObject){name, handle, state};
    return o;
}
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { (void)r; (void)h; assert(0); }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { r->out_size = (r->out_size + 7) & ~7u; memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
uint64_t nx_in64(NxRequest *r, uint32_t off) {
    uint64_t v = 0;
    if (off + 8 <= r->in_size) memcpy(&v, r->in + off, 8);
    return v;
}
uint32_t nx_shmem_new(uint64_t size, KShmem **out) { (void)size; (void)out; assert(0); return 0; }
void nx_input_attach(uint8_t *p) { (void)p; assert(0); }
uint32_t nx_event_new(KEvent **out) { (void)out; assert(0); return 0; }
void nx_event_signal(KEvent *event) { (void)event; assert(0); }

int main(void) {
    NxRequest r = {.cmd = 203};
    assert(nx_hid(NULL, &r) == 0 && r.nout_objects == 1);
    ServiceObject *list = r.out_objects[0];
    assert(list && !strcmp(list->name, "IActiveVibrationDeviceList"));
    r = (NxRequest){.cmd = 0};
    assert(list->handle(list, &r) == 0);
    r = (NxRequest){.cmd = 1};
    assert(list->handle(list, &r) == NX_R_UNKNOWN_CMD);
    free(list);
    // nn::hid::InitializeTouchScreen(1920, 1080) del SDK de Switch 2: comando 1004, sin salida.
    uint32_t res[2] = {1920, 1080};
    r = (NxRequest){.cmd = 1004, .in = (uint8_t *)res, .in_size = sizeof res};
    assert(nx_hid(NULL, &r) == 0 && r.out_size == 0 && r.nout_objects == 0);
    // FlushEnqueuedVibrationValues (Switch 2, comando 214): acepta el lote, sin salida.
    r = (NxRequest){.cmd = 214};
    assert(nx_hid(NULL, &r) == 0 && r.out_size == 0 && r.nout_objects == 0);
    // InitializeJcms (Switch 2, comando 3110): Nintendo Switch Sports Resort aborta al arrancar si no se acepta.
    r = (NxRequest){.cmd = 3110};
    assert(nx_hid(NULL, &r) == 0 && r.out_size == 0 && r.nout_objects == 0);
    // GetNpadJoyHoldType is command 121 and returns a u64 enum (0 = vertical/default).
    r = (NxRequest){.cmd = 121};
    assert(nx_hid(NULL, &r) == 0 && r.out_size == 8);
    uint64_t hold_type;
    memcpy(&hold_type, r.out, sizeof hold_type);
    assert(hold_type == 0);
    // Command 127 is StopLrAssignmentMode and has no output payload.
    r = (NxRequest){.cmd = 127};
    assert(nx_hid(NULL, &r) == 0 && r.out_size == 0);
    puts("HID vibration, touch, Jcms, GetNpadJoyHoldType/StopLrAssignmentMode: PASS");
}
