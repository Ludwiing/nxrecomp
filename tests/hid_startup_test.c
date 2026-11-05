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
    puts("HID vibration session creation and activation: PASS");
}
