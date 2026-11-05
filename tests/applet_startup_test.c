// Isolated startup-policy tests; no display, game or saved data required.
#include "../runtime/nxsvc_am.c"
#include <assert.h>

void nx_kernel_lock(void) {}
void nx_kernel_unlock(void) {}
uint32_t nx_event_new(KEvent **event) {
    *event = calloc(1, sizeof **event);
    return 42;
}
void nx_event_signal(KEvent *event) { event->signaled = 1; }
int nx_en_base(void) { return 1; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { r->out_copy[r->nout_copy++] = h; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
void nx_out8(NxRequest *r, uint8_t v) { r->out[r->out_size++] = v; }

int main(void) {
    NxRequest r = {.cmd = 0};
    assert(common_state(NULL, &r) == 0 && r.nout_copy == 1);
    assert(msg_event->signaled);
    r = (NxRequest){.cmd = 1};
    assert(common_state(NULL, &r) == 0 && r.out_size == 4);
    uint32_t message; memcpy(&message, r.out, 4);
    assert(message == 15 && !msg_event->signaled);
    r = (NxRequest){.cmd = 1};
    assert(common_state(NULL, &r) == R_AM_NO_MESSAGE);
    r = (NxRequest){.cmd = 900};
    assert(common_state(NULL, &r) == 0);
    r = (NxRequest){.cmd = 44};
    assert(self_controller(NULL, &r) == 0 && r.out_size == 16);
    uint64_t layers[2]; memcpy(layers, r.out, 16);
    assert(layers[0] == 1 && layers[1] == 2);
    free(msg_event);
    puts("Startup focus notification, drained event and separable display layers: PASS");
}
