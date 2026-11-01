// vi (pantallas y capas), el binder con la cola de buffers (IGraphicBufferProducer de Android)
// y nvdrv con /dev/nvmap: el camino por el que un programa pone cuadros en la pantalla.
//   1. nvdrv /dev/nvmap: el programa registra la memoria de sus cuadros (CREATE + ALLOC + GET_ID).
//   2. vi: abre la pantalla y una capa; la capa devuelve el id del binder de su cola.
//   3. binder: CONNECT, SET_PREALLOCATED_BUFFER (un GraphicBuffer por ranura con el id de
//      nvmap, el tamaño, el formato y la disposición), y en cada cuadro DEQUEUE_BUFFER /
//      QUEUE_BUFFER. Al encolar, el cuadro va a la pantalla (nxdisplay.c).
// Estructuras como en libnx (display/parcel.c, buffer_producer.c, nvidia/graphic_buffer.h).
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxdisplay.h"
#include "nxgpu.h"
#include "nxkern.h"
#include "nxsvc.h"

#define OK 0

// ---------- nvmap ----------
typedef struct {
    uint32_t size, align, kind, id;
    uint64_t addr;
} NvMapObj;
static NvMapObj nvmaps[256];
static int nnvmaps;
static pthread_mutex_t nv_lock = PTHREAD_MUTEX_INITIALIZER;

static NvMapObj *nvmap_by_handle(uint32_t h) { return h >= 1 && (int)h <= nnvmaps ? &nvmaps[h - 1] : NULL; }
static NvMapObj *nvmap_by_id(uint32_t id) {
    for (int k = 0; k < nnvmaps; k++)
        if (nvmaps[k].id == id) return &nvmaps[k];
    return NULL;
}

// ---------- la cola de buffers ----------
#define SLOTS 8
typedef struct {
    int configured, dequeued;
    uint32_t width, height, format, stride, nvmap_id;
    uint32_t layout, pitch, offset, block_height_log2;
} Slot;
static Slot slots[SLOTS];
static int next_slot;
static KEvent *buffer_event;
static uint32_t buffer_event_h;

static uint32_t u32at(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

typedef struct { // un Parcel que se escribe como respuesta
    uint8_t data[0x400];
    uint32_t size;
} Reply;
static void put32(Reply *r, uint32_t v) {
    memcpy(r->data + r->size, &v, 4);
    r->size += 4;
}
static void put_flat(Reply *r, const void *obj, uint32_t n) { // objeto aplanado: largo, fds, datos
    put32(r, n);
    put32(r, 0);
    memcpy(r->data + r->size, obj, n);
    r->size += (n + 3) & ~3u;
}

static void present(int s, const uint8_t *qbi) {
    Slot *sl = &slots[s];
    NvMapObj *m = nvmap_by_id(sl->nvmap_id);
    if (!m || !m->addr) {
        fprintf(stderr, "[RED] QUEUE_BUFFER: la ranura %d no tiene memoria de nvmap (id %u)\n", s, sl->nvmap_id);
        return;
    }
    NxFrame f = {(const uint8_t *)(uintptr_t)(m->addr + sl->offset), sl->width, sl->height,
                 sl->pitch ? sl->pitch : sl->stride * 4, sl->format, sl->layout, sl->block_height_log2,
                 {0, 0, 0, 0}, 0, 1};
    if (qbi) { // QueueBufferInput: marca de tiempo (8), auto (4), recorte (16), escalado, transformación,
               // transformación fija, async, intervalo de swap, cercas
        for (int k = 0; k < 4; k++) f.crop[k] = (int32_t)u32at(qbi + 12 + k * 4);
        f.transform = u32at(qbi + 32);
        f.interval = u32at(qbi + 44);
    }
    nx_display_present(&f);
}

// Transacción de IGraphicBufferProducer. `in` es el payload (después del token de interfaz).
static void transact(uint32_t code, const uint8_t *in, uint32_t in_size, Reply *out) {
    switch (code) {
    case 10: { // CONNECT → QueueBufferOutput
        uint32_t o[4] = {1280, 720, 0, 0};
        for (int k = 0; k < 4; k++) put32(out, o[k]);
        put32(out, 0);
        return;
    }
    case 14: { // SET_PREALLOCATED_BUFFER(ranura, hay buffer, GraphicBuffer aplanado)
        int32_t s = (int32_t)u32at(in);
        if (s >= 0 && s < SLOTS && u32at(in + 4)) {
            const uint8_t *g = in + 16; // después de ranura, hay buffer, largo y fds
            Slot *sl = &slots[s];
            memset(sl, 0, sizeof *sl);
            sl->configured = 1;
            sl->width = u32at(g + 4);
            sl->height = u32at(g + 8);
            sl->stride = u32at(g + 12);
            sl->format = u32at(g + 16);
            // NvGraphicBuffer desde el campo 0xC: el campo F queda en 40 + F - 0xC
            sl->nvmap_id = u32at(g + 0x10 + 28);
            const uint8_t *plane = g + 0x40 + 28;
            sl->layout = u32at(plane + 0x10);
            sl->pitch = u32at(plane + 0x14);
            sl->offset = u32at(plane + 0x1C);
            sl->block_height_log2 = u32at(plane + 0x24);
        }
        put32(out, 0);
        return;
    }
    case 3: { // DEQUEUE_BUFFER → ranura y una cerca vacía (no hay GPU que esperar)
        int s = -1;
        for (int k = 0; k < SLOTS; k++) {
            int c = (next_slot + k) % SLOTS;
            if (slots[c].configured && !slots[c].dequeued) {
                s = c;
                break;
            }
        }
        if (s < 0) s = next_slot % SLOTS;
        next_slot = s + 1;
        slots[s].dequeued = 1;
        put32(out, (uint32_t)s);
        put32(out, 1);
        uint8_t fence[0x24] = {0};
        put_flat(out, fence, sizeof fence);
        put32(out, 0);
        return;
    }
    case 1: put32(out, 0); put32(out, 0); return; // REQUEST_BUFFER: sin buffer (libnx usa los preasignados)
    case 7: { // QUEUE_BUFFER(ranura, entrada) → el cuadro a la pantalla
        int32_t s = (int32_t)u32at(in);
        if (s >= 0 && s < SLOTS) {
            present(s, in_size >= 12 + 48 ? in + 12 : NULL); // después de la ranura, largo y fds
            slots[s].dequeued = 0;
        }
        uint32_t o[4] = {1280, 720, 0, 0};
        for (int k = 0; k < 4; k++) put32(out, o[k]);
        put32(out, 0);
        return;
    }
    case 8: { // CANCEL_BUFFER
        int32_t s = (int32_t)u32at(in);
        if (s >= 0 && s < SLOTS) slots[s].dequeued = 0;
        put32(out, 0);
        return;
    }
    case 9: { // QUERY
        uint32_t what = u32at(in);
        put32(out, what == 0 ? 1280 : what == 1 ? 720 : what == 2 ? 1 : 0);
        put32(out, 0);
        return;
    }
    case 2: case 11: put32(out, 0); return; // SET_BUFFER_COUNT, DISCONNECT
    }
    fprintf(stderr, "[RED] IGraphicBufferProducer: transacción %u no implementada\n", code);
    put32(out, (uint32_t)-38); // ENOSYS
}

static uint32_t binder_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 3: { // TransactParcel(id, código, banderas): parcel de entrada → parcel de respuesta
        NxBuffer *in = nx_in_buffer(r, 0), *outb = nx_out_buffer(r, 0);
        if (!in || !outb) return 0x7F4u;
        const uint8_t *p = (const uint8_t *)(uintptr_t)in->addr;
        uint32_t psize = u32at(p), poff = u32at(p + 4);
        const uint8_t *payload = p + poff;
        // token de interfaz: política (4 bytes), largo, cadena UTF-16 con su cero, alineada a 4
        uint32_t len = u32at(payload + 4);
        uint32_t skip = 8 + ((len + 1) * 2 + 3) / 4 * 4;
        Reply rep = {.size = 0};
        pthread_mutex_lock(&nv_lock);
        transact(nx_in32(r, 4), payload + skip, psize > skip ? psize - skip : 0, &rep);
        pthread_mutex_unlock(&nv_lock);
        uint8_t *o = (uint8_t *)(uintptr_t)outb->addr;
        uint32_t hdr[4] = {rep.size, 0x10, 0, 0x10 + rep.size};
        if (outb->size < 0x10 + rep.size) return 0x7F4u;
        memcpy(o, hdr, 0x10);
        memcpy(o + 0x10, rep.data, rep.size);
        return OK;
    }
    case 1: return OK; // AdjustRefcount
    case 2: // GetNativeHandle: el evento de "hay buffer libre", siempre señalado
        if (!buffer_event_h) {
            buffer_event_h = nx_event_new(&buffer_event);
            nx_event_signal(buffer_event);
        }
        nx_out_copy_handle(r, buffer_event_h);
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- vsync ----------
static KEvent *vsync_event;
static uint32_t vsync_event_h;
static void *vsync_thread(void *arg) {
    (void)arg;
    for (;;) {
        struct timespec ts = {0, 16666667};
        nanosleep(&ts, NULL);
        nx_event_signal(vsync_event);
    }
    return NULL;
}

// ---------- vi ----------
static void native_window(NxRequest *r, uint64_t *size) {
    // Parcel con el binder de la cola: magia 2, pid 1, id 1, relleno y "dispdrv"
    NxBuffer *b = r->nb ? &r->b[0] : NULL;
    uint8_t data[0x28] = {0};
    uint32_t w[3] = {2, 1, 1};
    memcpy(data, w, 12);
    memcpy(data + 0x18, "dispdrv", 8);
    uint32_t hdr[4] = {sizeof data, 0x10, 0, 0x10 + sizeof data};
    if (b && b->size >= 0x10 + sizeof data) {
        memcpy((void *)(uintptr_t)b->addr, hdr, 0x10);
        memcpy((void *)(uintptr_t)(b->addr + 0x10), data, sizeof data);
    }
    *size = 0x10 + sizeof data;
}

static uint32_t system_display(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1200: nx_out64(r, 0); return OK;    // GetZOrderCountMin
    case 1202: nx_out64(r, 255); return OK;  // GetZOrderCountMax
    case 1203: nx_out64(r, 1920); nx_out64(r, 1080); return OK; // GetDisplayLogicalResolution
    case 1204: case 2201: case 2203: case 2205: case 2207: case 2209: case 2312: return OK;
    case 3200: { // GetDisplayMode
        uint32_t m[4] = {1920, 1080, 0, 0};
        float hz = 60.0f;
        memcpy(&m[2], &hz, 4);
        nx_out_bytes(r, m, sizeof m);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t manager_display(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 2010: nx_out64(r, 1); return OK; // CreateManagedLayer
    case 2011: case 4201: case 4205: case 6000: case 6002: case 7000: return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t app_display(ServiceObject *self, NxRequest *r) {
    (void)self;
    uint64_t size;
    switch (r->cmd) {
    case 100: case 103: nx_out_object(r, nx_object("IHOSBinderDriver", binder_handle, NULL)); return OK;
    case 101: nx_out_object(r, nx_object("ISystemDisplayService", system_display, NULL)); return OK;
    case 102: nx_out_object(r, nx_object("IManagerDisplayService", manager_display, NULL)); return OK;
    case 1000: { // ListDisplays
        NxBuffer *b = r->nb ? &r->b[0] : NULL;
        if (b && b->size >= 0x60) {
            uint8_t *d = (uint8_t *)(uintptr_t)b->addr;
            memset(d, 0, 0x60);
            strcpy((char *)d, "Default");
            d[0x40] = 1;
            uint64_t v[3] = {1, 1920, 1080};
            memcpy(d + 0x48, v, sizeof v);
        }
        nx_out64(r, 1);
        return OK;
    }
    case 1010: case 1011: nx_out64(r, 1); return OK; // OpenDisplay / OpenDefaultDisplay
    case 1020: case 1101: case 2021: case 2031: case 2101: return OK;
    case 1102: nx_out64(r, 1920); nx_out64(r, 1080); return OK; // GetDisplayResolution
    case 2020: native_window(r, &size); nx_out64(r, size); return OK; // OpenLayer
    case 2030: native_window(r, &size); nx_out64(r, 1); nx_out64(r, size); return OK; // CreateStrayLayer
    case 2102: nx_out64(r, nx_in32(r, 0)); return OK; // ConvertScalingMode
    case 5202: case 5203: // GetDisplayVsyncEvent
        if (!vsync_event_h) {
            vsync_event_h = nx_event_new(&vsync_event);
            pthread_t t;
            pthread_create(&t, NULL, vsync_thread, NULL);
        }
        nx_out_copy_handle(r, vsync_event_h);
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_vi(ServiceObject *self, NxRequest *r) {
    (void)self;
    if (r->cmd <= 2) { // GetDisplayService (vi:u 0, vi:s 1, vi:m 2)
        nx_out_object(r, nx_object("IApplicationDisplayService", app_display, NULL));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- nvdrv ----------
enum { DEV_NONE, DEV_NVMAP, DEV_CTRL, DEV_CTRL_GPU, DEV_AS_GPU, DEV_GPU, DEV_OTHER };
static struct {
    int dev;
    char name[32];
} fds[64];

static uint32_t nvmap_ioctl(uint32_t nr, uint8_t *io) {
    switch (nr) {
    case 0x01: { // CREATE(tamaño) → handle
        if (nnvmaps >= 256) return 0xA; // NvError_InsufficientMemory
        NvMapObj *m = &nvmaps[nnvmaps++];
        memset(m, 0, sizeof *m);
        m->size = u32at(io);
        m->id = 0x100 + (uint32_t)nnvmaps;
        uint32_t h = (uint32_t)nnvmaps;
        memcpy(io + 4, &h, 4);
        return 0;
    }
    case 0x03: { // FROM_ID(id) → handle
        for (int k = 0; k < nnvmaps; k++)
            if (nvmaps[k].id == u32at(io)) {
                uint32_t h = (uint32_t)k + 1;
                memcpy(io + 4, &h, 4);
                return 0;
            }
        return 4; // NvError_BadParameter
    }
    case 0x04: { // ALLOC(handle, heapmask, banderas, alineación, kind, dirección)
        NvMapObj *m = nvmap_by_handle(u32at(io));
        if (!m) return 4;
        m->align = u32at(io + 12);
        m->kind = io[16];
        memcpy(&m->addr, io + 24, 8);
        return 0;
    }
    case 0x05: { // FREE
        NvMapObj *m = nvmap_by_handle(u32at(io));
        if (!m) return 4;
        memcpy(io + 8, &m->addr, 8);
        memcpy(io + 16, &m->size, 4);
        memset(io + 20, 0, 4);
        return 0;
    }
    case 0x09: { // PARAM(handle, qué) → valor
        NvMapObj *m = nvmap_by_handle(u32at(io));
        if (!m) return 4;
        uint32_t p = u32at(io + 4), v = p == 1 ? m->size : p == 2 ? m->align : p == 3 ? (uint32_t)m->addr : p == 5 ? m->kind : 0;
        memcpy(io + 8, &v, 4);
        return 0;
    }
    case 0x0E: { // GET_ID(handle) → id
        NvMapObj *m = nvmap_by_handle(u32at(io + 4));
        if (!m) return 4;
        memcpy(io, &m->id, 4);
        return 0;
    }
    }
    return 1; // NvError_NotImplemented
}

// ---------- dispositivos de la GPU ----------
static void io32(uint8_t *io, uint32_t off, uint32_t v) { memcpy(io + off, &v, 4); }
static void io64(uint8_t *io, uint32_t off, uint64_t v) { memcpy(io + off, &v, 8); }
static uint64_t get64(const uint8_t *io, uint32_t off) { uint64_t v; memcpy(&v, io + off, 8); return v; }

static uint32_t ctrl_ioctl(uint32_t nr, uint8_t *io) { // /dev/nvhost-ctrl: syncpoints
    uint32_t id = u32at(io);
    switch (nr) {
    case 0x14: io32(io, 4, nx_syncpt_read(id)); return 0;                // SyncptRead
    case 0x15: nx_syncpt_incr(id); return 0;                              // SyncptIncr
    case 0x16: return nx_syncpt_wait(id, u32at(io + 4), (int32_t)u32at(io + 8) * 1000) ? 0 : 5; // SyncptWait
    case 0x1D: case 0x1E: { // EventWait / EventWaitAsync: con todo en orden, el valor ya llegó
        int ok = nx_syncpt_wait(id, u32at(io + 4), nr == 0x1E ? 0 : (int32_t)u32at(io + 8));
        io32(io, 12, nx_syncpt_read(id));
        return ok ? 0 : 5; // NvError_Timeout
    }
    case 0x1C: case 0x1F: case 0x20: return 0; // EventSignal / EventRegister / EventUnregister
    }
    return 1;
}

static uint32_t ctrl_gpu_ioctl(uint32_t nr, uint8_t *io) { // /dev/nvhost-ctrl-gpu: cómo es la GPU
    switch (nr) {
    case 0x01: io32(io, 0, 0x7A000); return 0; // ZCullGetCtxSize
    case 0x02: { // ZCullGetInfo
        uint32_t z[10] = {0x20, 0x20, 0x400, 0x800, 0x20, 0x20, 0xC0, 0x20, 0x40, 0x10};
        memcpy(io, z, sizeof z);
        return 0;
    }
    case 0x03: case 0x04: return 0; // ZbcSetTable / ZbcQueryTable
    case 0x05: { // GetCharacteristics: los valores reales del Tegra X1 (GM20B)
        uint8_t *g = io + 16;
        memset(g, 0, 0xA0);
        io64(io, 0, 0xA0);
        uint32_t a[] = {0x120, 0xB, 0xA1, 1};
        memcpy(g, a, 16);
        io64(g, 0x10, 0x40000);
        uint32_t b[] = {2, 0x20, 0x20000, 0x20000, 0x1B, 0x30000, 1, 0x503, 0x503, 0x80, 0x28, 0};
        memcpy(g + 0x20, b, sizeof b);
        io64(g, 0x50, 0x55);
        uint32_t c[] = {0x902D, 0xB197, 0xB1C0, 0xB06F, 0xA140, 0xB0B5, 1, 0, 2, 1, 0, 1, 0x21D70, 0};
        memcpy(g + 0x58, c, sizeof c);
        io64(g, 0x90, 0x6230326D67ull); // "gm20b"
        return 0;
    }
    case 0x06: io[16] = 3; return 0;                                   // GetTpcMasks: 2 TPC
    case 0x14: io32(io, 0, 7); io32(io, 4, 0); return 0;             // ZbcGetActiveSlotMask
    case 0x1C: io64(io, 0, nx_now_ns()); return 0;                    // GetGpuTime
    }
    return 1;
}

static uint32_t as_ioctl(uint32_t nr, uint8_t *io) { // /dev/nvhost-as-gpu: el espacio de direcciones
    switch (nr) {
    case 0x01: case 0x03: case 0x09: return 0; // BindChannel / FreeSpace / InitializeEx
    case 0x02: { // AllocSpace(páginas, tamaño de página, banderas, offset o alineación)
        uint64_t size = (uint64_t)u32at(io) * u32at(io + 4);
        if (!(u32at(io + 8) & 1)) io64(io, 16, nx_gpu_alloc_va(size, get64(io, 16)));
        return 0;
    }
    case 0x05: nx_gpu_unmap(get64(io, 0)); return 0; // UnmapBuffer
    case 0x06: { // MapBufferEx(banderas, kind, handle, página, offset en el buffer, tamaño, dirección)
        if (u32at(io) & 0x100) return 0; // "modificar": cambia el kind (compresión) de un mapeo que ya existe
        NvMapObj *m = nvmap_by_handle(u32at(io + 8));
        if (!m) return 4;
        uint64_t off = get64(io, 16), size = get64(io, 24);
        if (!size) size = m->size - off;
        uint64_t va = (u32at(io) & 1) ? get64(io, 32) : nx_gpu_alloc_va(size, 0x20000);
        nx_gpu_map(va, size, m->addr + off);
        io64(io, 32, va);
        return 0;
    }
    case 0x08: { // GetVARegions: páginas chicas y grandes
        io32(io, 8, 0x30);
        uint64_t r0[3] = {0x04000000, 0x1000, 0x3FBFFF}, r1[3] = {0x400000000ull, 0x20000, 0x1BFFF};
        memcpy(io + 16, r0, 24);
        memcpy(io + 40, r1, 24);
        return 0;
    }
    }
    return 1;
}

static uint32_t chan_syncpt = 18; // el syncpoint del canal de la GPU

static uint32_t gpu_ioctl(uint32_t type, uint32_t nr, uint8_t *io, const uint8_t *extra, uint64_t extra_size) {
    if (type == 0x47) return 0; // SetUserData
    switch (nr) {
    case 0x01: case 0x03: case 0x0B: case 0x0C: case 0x0D: return 0; // nvmap fd, timeout, zcull, notificador, prioridad
    case 0x09: io64(io, 8, 1); return 0;                             // AllocObjCtx
    case 0x16: case 0x17: memset(io, 0, 16); return 0;                 // errores: ninguno
    case 0x1A: io32(io, 12, chan_syncpt); io32(io, 16, nx_syncpt_read(chan_syncpt)); return 0; // AllocGpfifoEx2
    case 0x08: case 0x1B: { // SubmitGpfifo (entradas en el mismo buffer) / KickoffPb (en el segundo)
        uint32_t n = u32at(io + 8);
        const uint8_t *e = nr == 0x08 ? io + 24 : extra;
        if (!e || (nr == 0x1B && extra_size < (uint64_t)n * 8)) return 4;
        nx_gpu_submit((const uint64_t *)e, n);
        io32(io, 16, chan_syncpt);
        io32(io, 20, nx_syncpt_read(chan_syncpt));
        return 0;
    }
    }
    return 1;
}

uint32_t nx_nvdrv(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // Open(ruta) → fd, error
        char path[64] = {0};
        NxBuffer *b = r->na ? &r->a[0] : r->nx ? &r->x[0] : NULL;
        if (b) memcpy(path, (const void *)(uintptr_t)b->addr, b->size < 63 ? b->size : 63);
        int dev = !strcmp(path, "/dev/nvmap") ? DEV_NVMAP : !strcmp(path, "/dev/nvhost-ctrl") ? DEV_CTRL
                : !strcmp(path, "/dev/nvhost-ctrl-gpu") ? DEV_CTRL_GPU : !strcmp(path, "/dev/nvhost-as-gpu") ? DEV_AS_GPU
                : !strcmp(path, "/dev/nvhost-gpu") ? DEV_GPU : DEV_OTHER;
        pthread_mutex_lock(&nv_lock);
        uint32_t fd = 0;
        for (uint32_t k = 1; k < 64; k++)
            if (!fds[k].dev) {
                fds[k].dev = dev;
                snprintf(fds[k].name, sizeof fds[k].name, "%s", path);
                fd = k;
                break;
            }
        pthread_mutex_unlock(&nv_lock);
        if (dev == DEV_OTHER) fprintf(stderr, "[RED] nvdrv: dispositivo %s todavía no implementado\n", path);
        nx_out32(r, fd);
        nx_out32(r, fd ? 0 : 0xA);
        return OK;
    }
    case 1: case 11: case 12: { // Ioctl(fd, pedido): entrada → buffer de salida
        uint32_t fd = nx_in32(r, 0), req = nx_in32(r, 4);
        uint32_t size = req >> 16 & 0x3FFF, type = req >> 8 & 0xFF, nr = req & 0xFF;
        NxBuffer *in = nx_in_buffer(r, 0), *out = nx_out_buffer(r, r->cmd == 12 ? 0 : 0);
        static __thread uint8_t io[0x4000];
        memset(io, 0, sizeof io);
        if (size > sizeof io) size = sizeof io;
        if (in && in->size) memcpy(io, (const void *)(uintptr_t)in->addr, in->size < size ? in->size : size);
        uint32_t err = 1;
        pthread_mutex_lock(&nv_lock);
        int dev = fd < 64 ? fds[fd].dev : DEV_NONE;
        if (dev == DEV_NVMAP && type == 0x01) err = nvmap_ioctl(nr, io);
        pthread_mutex_unlock(&nv_lock);
        if (dev == DEV_CTRL) err = ctrl_ioctl(nr, io);
        else if (dev == DEV_CTRL_GPU) err = ctrl_gpu_ioctl(nr, io);
        else if (dev == DEV_AS_GPU) err = as_ioctl(nr, io);
        else if (dev == DEV_GPU) {
            NxBuffer *x = r->cmd == 11 ? nx_in_buffer(r, 1) : NULL;
            err = gpu_ioctl(type, nr, io, x ? (const uint8_t *)(uintptr_t)x->addr : NULL, x ? x->size : 0);
        }
        if (err == 1)
            fprintf(stderr, "[RED] nvdrv: ioctl 0x%08x en %s no implementado\n", req, fd < 64 ? fds[fd].name : "?");
        static int trace = -1;
        if (trace < 0) trace = getenv("NX_TRAZA_NV") != NULL;
        if (trace) {
            fprintf(stderr, "[nv] %s ioctl 0x%08x → %u:", fd < 64 ? fds[fd].name : "?", req, err);
            for (uint32_t k = 0; k < size && k < 32; k += 4) fprintf(stderr, " %08x", u32at(io + k));
            fprintf(stderr, "\n");
        }
        if (out && out->size) memcpy((void *)(uintptr_t)out->addr, io, out->size < size ? out->size : size);
        nx_out32(r, err);
        return OK;
    }
    case 2: { // Close(fd)
        uint32_t fd = nx_in32(r, 0);
        if (fd < 64) fds[fd].dev = DEV_NONE;
        nx_out32(r, 0);
        return OK;
    }
    case 3: nx_out32(r, 0); return OK; // Initialize(memoria de transferencia)
    case 4: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); nx_out32(r, 0); return OK; } // QueryEvent
    case 8: case 13: nx_out32(r, 0); return OK; // SetClientPID, SetGraphicsFirmwareMemoryMarginEnabled
    }
    return NX_R_UNKNOWN_CMD;
}
