#include "../runtime/nxgpu.c"
#include <assert.h>

int nx_gl_draw(const NxDraw *d) { (void)d; return 1; }
int nx_gl_dispatch(const uint32_t *r, const uint32_t *q) { (void)r;(void)q;return 1; }
int nx_gl_clear(const uint32_t *r,uint32_t v) { (void)r;(void)v;return 1; }
uint64_t nx_gl_contador(uint32_t t) { (void)t;return 0; }
void nx_gl_contador_reiniciar(uint32_t v) { (void)v; }
void nx_gl_flush(void) {}
void nx_gl_release(void) {}
void nx_gl_cpu_write(uint64_t a,uint64_t n) { (void)a;(void)n; }
void nx_gl_invalidate_all(void) {}
void nx_vigia_resolver(uint64_t a,uint64_t n) { (void)a;(void)n; }
void nx_vigia_tocar(uint64_t a,uint64_t n) { (void)a;(void)n; }
uint64_t nx_now_ns(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }

static uint64_t *mutable_entries;
static uint32_t *mutable_words;
static uint32_t *gpu_written_words;
static uint64_t replacement;
void nx_nvdrv_syncpt_changed(uint32_t id) {
    if (id == 30 && mutable_entries) mutable_entries[1] = replacement;
    if (id == 33 && mutable_words) mutable_words[3] = (35u << 8) | 1;
    if (id == 36 && gpu_written_words) gpu_written_words[1] = (37u << 8) | 1;
}
static uint64_t entry(uint64_t address,uint32_t words) { return address | ((uint64_t)words << 42); }
int main(void) {
    uint32_t a[1024]={0x2001001d,(30u<<8)|1};
    uint32_t b[1024]={0x2001001d,(31u<<8)|1};
    uint32_t other[1024]={0x2001001d,(32u<<8)|1};
    nx_gpu_map(0x100000,4096,(uintptr_t)a);nx_gpu_map(0x110000,4096,(uintptr_t)b);nx_gpu_map(0x120000,4096,(uintptr_t)other);
    uint64_t entries[]={entry(0x100000,2),entry(0x110000,2)};
    mutable_entries=entries;replacement=entry(0x120000,2);
    nx_gpu_submit(1,entries,2);
    assert(nx_syncpt_read(30)==1);
    if (nx_syncpt_read(31)!=1 || nx_syncpt_read(32)!=0) { fputs("FAIL: submit fetched descriptors after publishing completion\n",stderr);return 1; }
    mutable_entries=NULL;
    uint32_t words[1024]={0x2001001d,(33u<<8)|1,0x2001001d,(34u<<8)|1};
    nx_gpu_map(0x130000,4096,(uintptr_t)words);mutable_words=words;
    uint64_t e=entry(0x130000,4);nx_gpu_submit(2,&e,1);
    if (nx_syncpt_read(34)!=1 || nx_syncpt_read(35)!=0) { fputs("FAIL: submit reread prefetched words after publishing completion\n",stderr);return 1; }
    uint32_t producer[1024]={0x2001001d,(36u<<8)|1};
    uint32_t consumer[1024]={0x2001001d,0};
    nx_gpu_map(0x140000,4096,(uintptr_t)producer);nx_gpu_map(0x150000,4096,(uintptr_t)consumer);
    gpu_written_words=consumer;
    uint64_t indirect[]={entry(0x140000,2),entry(0x150000,2)};
    nx_gpu_submit(3,indirect,2);
    assert(nx_syncpt_read(37)==1); // Earlier entries may generate later arguments.
    puts("GPU submission inputs remain stable across completion callbacks: PASS");
}
