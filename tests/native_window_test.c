#include "../runtime/nxsvc_vi.c"
#include <assert.h>

NxBuffer *nx_out_buffer(NxRequest *r, int k) {
    return r->nb > k && r->b[k].size ? &r->b[k] : r->nc > k ? &r->c[k] : NULL;
}
void nx_vigia_tocar(uint64_t addr, uint64_t size) { (void)addr; (void)size; }
int main(void) {
    uint8_t buffer[256] = {0};
    NxRequest r = {.nb = 1, .b = {{(uintptr_t)buffer, sizeof buffer}}};
    uint64_t size;
    native_window(&r, &size);
    uint32_t *h = (uint32_t *)buffer;
    assert(size == 60 && h[0] == 40 && h[1] == 16 && h[2] == 4 && h[3] == 56);
    assert(u32at(buffer + h[3]) == 0); // flattened binder starts at payload offset 0
    assert(u32at(buffer + h[1]) == 2 && u32at(buffer + h[1] + 8) == 1);
    assert(!memcmp(buffer + h[1] + 24, "dispdrv", 8));
    memset(buffer, 0, sizeof buffer);
    r = (NxRequest){.nc = 1, .c = {{(uintptr_t)buffer, sizeof buffer}}};
    native_window(&r, &size);
    assert(u32at(buffer + 8) == 4);
    uint8_t ioctl[0xB0] = {0}, characteristics[0xA0] = {0};
    assert(ctrl_gpu_ioctl(5, ioctl) == 0);
    r = (NxRequest){.nb = 2, .b = {{(uintptr_t)buffer, 16}, {(uintptr_t)characteristics, sizeof characteristics}}};
    assert(ctrl_gpu_inline_out(&r, 5, ioctl) == 0);
    assert(u32at(characteristics) == 0x120);
    assert(u32at(characteristics + 4) == 0xB);
    assert(!memcmp(characteristics + 0x90, "gm20b", 5));
    r.b[1].size = 4;
    assert(ctrl_gpu_inline_out(&r, 5, ioctl) == 4);
    // NVN requests a buffer again after registering it; return its full flattening.
    uint8_t request[16 + 0x90] = {0};
    uint32_t header[4] = {0, 1, 0x90, 0};
    memcpy(request, header, 16);
    request[16] = 0x42;
    Reply reply = {0};
    transact(14, request, sizeof request, &reply);
    assert(reply.size == 4 && u32at(reply.data) == 0);
    reply = (Reply){0};
    uint32_t slot = 0;
    transact(1, (const uint8_t *)&slot, 4, &reply);
    assert(u32at(reply.data) == 1 && u32at(reply.data + 4) == 0x90);
    assert(!memcmp(reply.data + 12, request + 16, 0x90));
    assert(u32at(reply.data + reply.size - 4) == 0);
    // A real game's startup exceeds 256 concurrently registered GPU buffers.
    // Existing handles and IDs must keep resolving after the metadata grows.
    NvMapObj *first = NULL;
    for (uint32_t k = 1; k <= 4096; ++k) {
        uint8_t io[32] = {0};
        io32(io, 0, k * 4096);
        assert(nvmap_ioctl(1, io) == 0);
        uint32_t handle = u32at(io + 4);
        assert(handle == k);
        if (k == 1) first = nvmap_by_handle(handle);
        memset(io, 0, sizeof io);
        io32(io, 0, handle);
        io32(io, 12, 4096);
        io64(io, 24, 0x100000000ull + k * 4096ull);
        assert(nvmap_ioctl(4, io) == 0);
        io32(io, 4, handle);
        assert(nvmap_ioctl(0xE, io) == 0);
        uint32_t id = u32at(io);
        assert(id == handle); // NVN shares one pool between several buffer slots
        assert(nvmap_ioctl(3, io) == 0 && u32at(io + 4) == handle);
        assert(nvmap_by_id(id) == nvmap_by_handle(handle));
    }
    assert(first == nvmap_by_handle(1));
    assert(first->size == 4096 && first->addr == 0x100001000ull);
    assert(nvmap_by_handle(4096)->size == 4096u * 4096);
    puts("Native window payload, binder object offsets and pointer buffer: PASS");
    puts("4096 concurrent nvmap buffers, stable handles and IDs: PASS");
}
