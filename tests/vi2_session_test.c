// Deterministic VI2 protocol test: advance the vsync source explicitly.
#include "../runtime/nxsvc_vi.c"
#include <assert.h>
static KEvent *handles[32];
static uint32_t count;
uint32_t nx_event_new(KEvent **out) {
    assert(count + 1 < 32);
    *out = calloc(1, sizeof **out);
    handles[++count] = *out;
    return count;
}
void *nx_handle_get(uint32_t h, int type) {
    return type == KO_EVENT && h <= count ? handles[h] : NULL;
}
void nx_event_signal(KEvent *e) { e->signaled = 1; }
uint32_t nx_in32(NxRequest *r, uint32_t off) { uint32_t v; memcpy(&v, r->in + off, 4); return v; }
uint64_t nx_in64(NxRequest *r, uint32_t off) { uint64_t v; memcpy(&v, r->in + off, 8); return v; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { r->out_copy[r->nout_copy++] = h; }
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n) { memcpy(r->out + r->out_size, p, n); r->out_size += n; } // como nxipc.c
static void manual_clock(void) {}
int main(void) {
    pthread_once(&vsync_once, manual_clock);
    ServiceObject a = {.handle = nx2_vi2}, b = {.handle = nx2_vi2};
    uint32_t slot = 0;
    NxRequest acquire = {.cmd = 31, .in = (uint8_t *)&slot, .in_size = 4};
    assert(nx2_vi2(&a, &acquire) == 0 && acquire.nout_copy == 1);
    uint32_t h = acquire.out_copy[0];
    acquire = (NxRequest){.cmd = 31, .in = (uint8_t *)&slot, .in_size = 4};
    assert(nx2_vi2(&b, &acquire) == 0 && acquire.out_copy[0] != h);
    KEvent *event = handles[h];
    vi2_tick(); assert(!event->signaled);
    struct { uint64_t display; uint32_t slot; } bind = {0x00746c7561666564ull, 0};
    NxRequest request = {.cmd = 261, .in = (uint8_t *)&bind, .in_size = 12};
    assert(nx2_vi2(&a, &request) == 0);
    vi2_tick(); assert(event->signaled && !handles[acquire.out_copy[0]]->signaled);
    request = (NxRequest){.cmd = 32, .in = (uint8_t *)&slot, .in_size = 4,
                          .nin_handles = 1, .in_handles = {h}};
    assert(nx2_vi2(&a, &request) == 0 && request.out_copy[0] == h);
    request.in_handles[0] = acquire.out_copy[0]; request.nout_copy = 0;
    assert(nx2_vi2(&a, &request) != 0 && request.nout_copy == 0);
    request = (NxRequest){.cmd = 262, .in = (uint8_t *)&bind, .in_size = 12};
    assert(nx2_vi2(&a, &request) == 0);
    event->signaled = 0; vi2_tick(); assert(!event->signaled);
    slot = 8;
    request = (NxRequest){.cmd = 31, .in = (uint8_t *)&slot, .in_size = 4};
    assert(nx2_vi2(&a, &request) != 0 && !request.nout_copy);
    request = (NxRequest){.cmd = 261};
    assert(nx2_vi2(&a, &request) != 0);
    for (unsigned cmd = 1; cmd <= 5; cmd += 2) {
        request = (NxRequest){.cmd = cmd}; uint32_t value;
        assert(nx2_vi2(&a, &request) == 0 && request.out_size == 4);
        memcpy(&value, request.out, 4); assert(value == (cmd == 5 ? 7 : 1));
    }
    uint32_t timestamp_slots = 7;
    request = (NxRequest){.cmd = 51, .in = (uint8_t *)&timestamp_slots, .in_size = 4};
    assert(nx2_vi2(&a, &request) == 0 && request.out_size == 0);
    request.in_size = 3;
    assert(nx2_vi2(&a, &request) != 0);
    uint64_t create_layer_args[3] = {0x00746c7561666564ull, 0x00746c7561666564ull, 0x12345678};
    request = (NxRequest){.cmd = 111, .in = (uint8_t *)create_layer_args, .in_size = sizeof create_layer_args};
    assert(nx2_vi2(&a, &request) == 0 && request.out_size == 0);
    request.in_size--;
    assert(nx2_vi2(&a, &request) != 0);
    request = (NxRequest){.cmd = 301};
    assert(nx2_vi2(&a, &request) == 0 && request.out_size == 0);
    uint64_t display_args[2] = {0x00746c7561666564ull, 1};
    request = (NxRequest){.cmd = 101, .in = (uint8_t *)display_args, .in_size = 12};
    assert(nx2_vi2(&a, &request) == 0 && !request.out_size);
    struct { uint64_t token; uint32_t options[2]; } layer_args = {0x12345678, {2, 3}};
    request = (NxRequest){.cmd = 201, .in = (uint8_t *)&layer_args, .in_size = 16};
    assert(nx2_vi2(&a, &request) == 0 && !request.out_size);
    assert(nx2_vi2(&a, &request) != 0); // Duplicate client token.
    uint64_t bind_layer[2] = {layer_args.token, 0x00746c7561666564ull};
    request = (NxRequest){.cmd = 251, .in = (uint8_t *)bind_layer, .in_size = sizeof bind_layer};
    assert(nx2_vi2(&b, &request) != 0);
    assert(nx2_vi2(&a, &request) == 0 && request.out_size == 0);
    bind_layer[1] = 1;
    assert(nx2_vi2(&a, &request) != 0);
    request = (NxRequest){.cmd = 212, .in = (uint8_t *)&layer_args.token, .in_size = 8};
    assert(nx2_vi2(&b, &request) != 0 && !request.out_size);
    assert(nx2_vi2(&a, &request) == 0 && request.out_size == 8);
    uint64_t available; memcpy(&available, request.out, 8); assert(available == 0x7fff);
    request = (NxRequest){.cmd = 202, .in = (uint8_t *)&layer_args.token, .in_size = 8};
    assert(nx2_vi2(&a, &request) == 0 && nx2_vi2(&a, &request) != 0);
    request = (NxRequest){.cmd = 201}; assert(nx2_vi2(&a, &request) != 0);
    // QueryBufferMaxResolution (106): u64 pantalla → {u32 ancho, u32 alto}; sin los 8 bytes, error de tamaño.
    uint64_t pantalla = 0;
    NxRequest qmax = {.cmd = 106, .in = (uint8_t *)&pantalla, .in_size = 8};
    assert(nx2_vi2(&a, &qmax) == 0 && qmax.out_size == 8);
    uint32_t maxres[2];
    memcpy(maxres, qmax.out, 8);
    assert(maxres[0] == 1920 && maxres[1] == 1080);
    NxRequest qcorta = {.cmd = 106, .in = (uint8_t *)&pantalla, .in_size = 4};
    assert(nx2_vi2(&a, &qcorta) == 0xE401u && qcorta.out_size == 0);
    puts("VI2 events and layers: isolation, binding, duplication, lifecycle and validation, buffer max resolution: PASS");
}
