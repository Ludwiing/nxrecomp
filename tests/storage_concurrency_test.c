#include "../runtime/nxsvc_fs.c"
#include <assert.h>
#include <pthread.h>

static uint8_t image[4096 * 512];
uint64_t nx_in64(NxRequest *r, uint32_t off) { uint64_t v; memcpy(&v, r->in + off, 8); return v; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out, &v, 8); r->out_size = 8; }
void nx_vigia_tocar(uint64_t addr, uint64_t size) { (void)addr; (void)size; }

typedef struct { ServiceObject *service; unsigned seed; } Reader;
static void *read_blocks(void *arg) {
    Reader *reader = arg;
    unsigned state = reader->seed;
    for (int k = 0; k < 4000; k++) {
        state = state * 1664525u + 1013904223u;
        uint64_t input[2] = {(state % 4096u) * 512ull, 512};
        uint8_t output[512];
        NxRequest r = {.cmd = 0, .in = (uint8_t *)input, .in_size = sizeof input,
                       .nb = 1, .b = {{(uintptr_t)output, sizeof output}}};
        assert(storage_handle(reader->service, &r) == 0);
        assert(!memcmp(output, image + input[0], sizeof output));
    }
    return NULL;
}
static void check(FILE *file) {
    Storage storage = {.f = file, .size = sizeof image};
    ServiceObject service = {.state = &storage};
    pthread_t threads[8];
    Reader readers[8];
    for (unsigned k = 0; k < 8; k++) {
        readers[k] = (Reader){&service, k + 1};
        assert(!pthread_create(&threads[k], NULL, read_blocks, &readers[k]));
    }
    for (unsigned k = 0; k < 8; k++) assert(!pthread_join(threads[k], NULL));
    uint64_t input[2] = {sizeof image - 1, 2};
    uint8_t output[2] = {0xaa, 0xbb};
    NxRequest r = {.in = (uint8_t *)input, .nb = 1, .b = {{(uintptr_t)output, 2}}};
    assert(storage_handle(&service, &r) == R_FS_OUT_OF_RANGE);
    assert(output[0] == 0xaa && output[1] == 0xbb);
}
int main(void) {
    for (size_t k = 0; k < sizeof image; k++) image[k] = (uint8_t)(k / 512 + k / 131072 + k % 251);
    FILE *file = tmpfile(); assert(file);
    assert(fwrite(image, 1, sizeof image, file) == sizeof image);
    assert(!fflush(file));
    check(file);
    assert(ftello(file) == sizeof image); // pread leaves the shared cursor alone
    fclose(file);
    file = fmemopen(image, sizeof image, "rb"); assert(file);
    check(file);
    fclose(file);
    puts("Concurrent RomFS reads: 64000 exact blocks across file and embedded storage: PASS");
}
