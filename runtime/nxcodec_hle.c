// Linux VP9 HLE based on the public nn::codec call flow implemented by
// nxnative (MIT); this is a separate implementation using NX2Linux's runtime.
#include "nxcodec_hle.h"

#include "nxcodec.h"
#include "nxguest.h"
#include "nxhle_event.h"
#include "nxkern.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CV_MAGIC 0x56436d44u
#define CV_SYNC 0x53436d44u
#define CV_MAX_DEC 16
#define CV_MAX_BUF 32
#define CV_MAX_AVAILABLE 8
#define CV_MAX_PENDING 32
#define CV_EVENT_OFF 0x100u
#define CV_R(desc) (0x6fu | ((uint32_t)(desc) << 9))
#define CV_BUSY CV_R(3603)
#define CV_NONE CV_R(3604)
#define CV_BAD CV_R(2004)

typedef struct { uint16_t w, h; uint32_t format, mode, magic, pad[2]; } CvPreference;
typedef struct { uint32_t magic, decoder, index, generation; } CvSync;
typedef struct { uint32_t magic, armed; CvSync want; } CvSyncPtState;
typedef struct {
    uint32_t magic, count;
    NxCodecImage **images;
    pthread_mutex_t mutex;
} CvPool;
typedef struct {
    int registered, filled, eos;
    uint32_t generation, filled_generation;
    CvPool *pool;
    uint32_t pool_index;
    uint16_t width, height;
    int64_t timestamp;
} CvBuffer;
typedef struct {
    uint32_t magic, id;
    pthread_mutex_t mutex;
    NxVpxDecoder *decoder;
    uint16_t width, height;
    uint32_t event_handle;
    int started;
    uint64_t submitted;
    CvBuffer buffers[CV_MAX_BUF];
    uint32_t available[CV_MAX_AVAILABLE], available_head, available_count;
    uint32_t released[CV_MAX_BUF], released_head, released_count;
    struct { NxCodecImage *image; int eos; int64_t timestamp; } pending[CV_MAX_PENDING];
    uint32_t pending_head, pending_count;
} CvDecoder;

static pthread_mutex_t g_cv_mutex = PTHREAD_MUTEX_INITIALIZER;
static CvDecoder *g_cv[CV_MAX_DEC];
typedef struct { uint64_t address; uint32_t write_handle; } CvSyncPtEntry;
static pthread_mutex_t g_syncpt_mutex = PTHREAD_MUTEX_INITIALIZER;
static CvSyncPtEntry g_syncpt[64];
static atomic_uint g_codec_trace_count;

static void codec_trace(const char *format, ...) {
    if (!getenv("NX_CODEC_TRACE")) return;
    unsigned n = atomic_fetch_add_explicit(&g_codec_trace_count, 1, memory_order_relaxed);
    if (n >= 100000) return;
    fprintf(stderr, "[codec-trace %u] ", n);
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static int sync_reached(const CvSync *s) {
    if (!s || s->magic != CV_SYNC || !s->generation) return 1;
    pthread_mutex_lock(&g_cv_mutex);
    CvDecoder *d = s->decoder && s->decoder <= CV_MAX_DEC ? g_cv[s->decoder - 1] : NULL;
    int ready = 1;
    if (d && s->index < CV_MAX_BUF) {
        pthread_mutex_lock(&d->mutex);
        CvBuffer *b = &d->buffers[s->index];
        ready = b->filled && b->filled_generation >= s->generation;
        pthread_mutex_unlock(&d->mutex);
    }
    pthread_mutex_unlock(&g_cv_mutex);
    return ready;
}

// The Switch 2 SDK's SyncPointEventType embeds a SystemEventType at +0 and
// its sync-point state at +0x40. Keep host handles in this side table; only
// the SDK-defined magic/armed/sync handle fields are stored in guest memory.
static uint32_t syncpt_handle(uint64_t address) {
    uint32_t h = 0;
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++)
        if (g_syncpt[i].address == address) { h = g_syncpt[i].write_handle; break; }
    pthread_mutex_unlock(&g_syncpt_mutex);
    return h;
}

static void syncpt_disarm(uint64_t address, int clear) {
    uint32_t h = 0;
    CvSyncPtState *state = address ? (CvSyncPtState *)(uintptr_t)(address + 0x40) : NULL;
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++) {
        if (g_syncpt[i].address == address) {
            h = g_syncpt[i].write_handle;
            break;
        }
    }
    if (state) state->armed = 0;
    pthread_mutex_unlock(&g_syncpt_mutex);
    if (clear && h) nx_event_clear_handle(h);
}

static void syncpt_fire(CvDecoder *d, uint32_t index, uint32_t generation) {
    uint32_t handles[64], n = 0;
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++) {
        CvSyncPtEntry *entry = &g_syncpt[i];
        if (!entry->address) continue;
        CvSyncPtState *state = (CvSyncPtState *)(uintptr_t)(entry->address + 0x40);
        CvSync *want = &state->want;
        if (state->magic == CV_SYNC && state->armed && want->decoder == d->id &&
            want->index == index && generation >= want->generation) {
            state->armed = 0;
            if (n < sizeof handles / sizeof handles[0]) handles[n++] = entry->write_handle;
        }
    }
    pthread_mutex_unlock(&g_syncpt_mutex);
    for (uint32_t i = 0; i < n; i++) {
        if (!nx_event_signal_handle(handles[i]))
            fprintf(stderr, "[RED] nn::syncpt: no se pudo señalar evento de video\n");
    }
}

static uint64_t stack_arg(const Ctx *c, unsigned index) {
    uint64_t value = 0;
    memcpy(&value, (const void *)(uintptr_t)(c->sp + (uint64_t)index * 8), sizeof value);
    return value;
}

static void enter(Ctx *c) { (void)c; nx_host_enter(); }
static void leave(Ctx *c, uint64_t result) { c->x[0] = result; nx_host_leave(); }
static CvDecoder *decoder_of(const void *type) {
    CvDecoder *d = type ? *(CvDecoder *const *)type : NULL;
    return d && d->magic == CV_MAGIC ? d : NULL;
}
static CvPool *pool_of(const void *type) {
    CvPool *p = type ? *(CvPool *const *)type : NULL;
    return p && p->magic == CV_MAGIC ? p : NULL;
}

static int qpush(uint32_t *q, uint32_t *head, uint32_t *count, uint32_t cap, uint32_t value) {
    if (*count >= cap) return 0;
    q[(*head + *count) % cap] = value;
    (*count)++;
    return 1;
}

static void qremove(uint32_t *q, uint32_t *head, uint32_t *count, uint32_t cap, uint32_t value) {
    uint32_t kept[CV_MAX_BUF], n = 0;
    for (uint32_t i = 0; i < *count; i++) {
        uint32_t v = q[(*head + i) % cap];
        if (v != value) kept[n++] = v;
    }
    for (uint32_t i = 0; i < n; i++) q[i] = kept[i];
    *head = 0; *count = n;
}

static void fill_released(CvDecoder *d) {
    while (d->pending_count && d->released_count) {
        uint32_t i = d->released[d->released_head];
        d->released_head = (d->released_head + 1) % CV_MAX_BUF;
        d->released_count--;
        CvBuffer *b = &d->buffers[i];
        if (!b->registered) continue;
        uint32_t k = d->pending_head;
        NxCodecImage *image = d->pending[k].image;
        int eos = d->pending[k].eos;
        int64_t ts = d->pending[k].timestamp;
        d->pending[k].image = NULL;
        d->pending_head = (d->pending_head + 1) % CV_MAX_PENDING;
        d->pending_count--;
        if (b->pool && b->pool_index < b->pool->count) {
            pthread_mutex_lock(&b->pool->mutex);
            nx_codec_image_free(b->pool->images[b->pool_index]);
            b->pool->images[b->pool_index] = image;
            pthread_mutex_unlock(&b->pool->mutex);
        } else {
            nx_codec_image_free(image);
        }
        b->width = image ? (uint16_t)image->width : 0;
        b->height = image ? (uint16_t)image->height : 0;
        b->timestamp = ts;
        b->eos = eos;
        b->filled = 1;
        b->filled_generation = b->generation;
        syncpt_fire(d, i, b->filled_generation);
    }
}

static void fail_event(Ctx *c, const char *where, uint32_t result) {
    fprintf(stderr, "[RED] nn::codec VP9: %s no pudo crear SystemEventType (resultado SDK %#x)\n",
            where, result);
    c->x[0] = result ? result : CV_BAD;
}

void nxcodec_video_init(Ctx *c) { enter(c); leave(c, 0); }
void nxcodec_video_finalize(Ctx *c) { enter(c); leave(c, 0); }

void nxcodec_pool_preference(Ctx *c) {
    enter(c);
    uint64_t out = c->x[8];
    if (out) {
        CvPreference p = {(uint16_t)c->x[0], (uint16_t)c->x[1], (uint32_t)c->x[2],
                          (uint32_t)c->x[3], CV_MAGIC, {0, 0}};
        memcpy((void *)(uintptr_t)out, &p, sizeof p);
    }
    nx_host_leave();
}

void nxcodec_pool_size(Ctx *c) {
    enter(c);
    uint64_t *size = (uint64_t *)(uintptr_t)c->x[0];
    uint64_t *align = (uint64_t *)(uintptr_t)c->x[1];
    const CvPreference *p = (const CvPreference *)(uintptr_t)c->x[2];
    uint32_t count = (uint32_t)c->x[3];
    if (!size || !align || !p || p->magic != CV_MAGIC || !p->w || !p->h || p->w > 16384 || p->h > 16384) {
        leave(c, CV_BAD); return;
    }
    uint64_t stride = ((uint64_t)p->w * 4 + 63) & ~UINT64_C(63);
    uint64_t bytes = (stride * p->h + 0x3fff) & ~UINT64_C(0x3fff);
    uint64_t n = count ? count : 1;
    if (bytes > UINT64_MAX / n) { leave(c, CV_BAD); return; }
    *size = bytes * n;
    *align = 0x4000;
    leave(c, 0);
}

void nxcodec_pool_init(Ctx *c) {
    enter(c);
    void *type = (void *)(uintptr_t)c->x[0];
    const CvPreference *pref = (const CvPreference *)(uintptr_t)c->x[3];
    uint32_t count = (uint32_t)c->x[4];
    if (!type || !pref || pref->magic != CV_MAGIC || !count || count > 256) { leave(c, CV_BAD); return; }
    CvPool *p = calloc(1, sizeof *p);
    if (!p) { leave(c, CV_R(103)); return; }
    p->images = calloc(count, sizeof *p->images);
    if (!p->images) { free(p); leave(c, CV_R(103)); return; }
    p->magic = CV_MAGIC; p->count = count; pthread_mutex_init(&p->mutex, NULL);
    *(CvPool **)type = p;
    leave(c, 0);
}

void nxcodec_pool_finalize(Ctx *c) {
    enter(c);
    void *type = (void *)(uintptr_t)c->x[0];
    CvPool *p = pool_of(type);
    if (!p) { leave(c, 0); return; }
    pthread_mutex_lock(&g_cv_mutex);
    for (int n = 0; n < CV_MAX_DEC; n++) {
        CvDecoder *d = g_cv[n];
        if (!d) continue;
        pthread_mutex_lock(&d->mutex);
        for (int i = 0; i < CV_MAX_BUF; i++) if (d->buffers[i].pool == p) d->buffers[i].pool = NULL;
        pthread_mutex_unlock(&d->mutex);
    }
    pthread_mutex_unlock(&g_cv_mutex);
    for (uint32_t i = 0; i < p->count; i++) nx_codec_image_free(p->images[i]);
    free(p->images); p->magic = 0; pthread_mutex_destroy(&p->mutex); free(p); *(CvPool **)type = NULL;
    leave(c, 0);
}

void nxcodec_vp9_work_size(Ctx *c) {
    enter(c);
    uint64_t *size = (uint64_t *)(uintptr_t)c->x[0];
    uint64_t *align = (uint64_t *)(uintptr_t)c->x[1];
    if (!size || !align) { leave(c, CV_BAD); return; }
    *size = 0x1000; *align = 0x1000;
    leave(c, 0);
}

static void decoder_init(Ctx *c, void *type, uint64_t w, uint64_t h, uint64_t input, uint64_t output) {
    if (!type || !w || !h || w > 16384 || h > 16384 || decoder_of(type)) { leave(c, CV_BAD); return; }
    CvDecoder *d = calloc(1, sizeof *d);
    if (!d) { leave(c, CV_R(103)); return; }
    d->decoder = nx_vp9_decoder_create((uint32_t)w, (uint32_t)h, 2);
    if (!d->decoder) { free(d); leave(c, CV_R(103)); return; }
    pthread_mutex_init(&d->mutex, NULL);
    d->magic = CV_MAGIC; d->width = (uint16_t)w; d->height = (uint16_t)h;

    uint64_t evaddr = (uint64_t)(uintptr_t)type + CV_EVENT_OFF;
    uint32_t sdk_rc = 0;
    if (!nx_hle_event_create(c, evaddr, 0, 1, &d->event_handle, &sdk_rc)) {
        nx_vp9_decoder_destroy(d->decoder); pthread_mutex_destroy(&d->mutex); free(d);
        fail_event(c, "InitializeVp9Decoder", sdk_rc); nx_host_leave(); return;
    }
    pthread_mutex_lock(&g_cv_mutex);
    for (int i = 0; i < CV_MAX_DEC; i++) if (!g_cv[i]) { g_cv[i] = d; d->id = (uint32_t)i + 1; break; }
    pthread_mutex_unlock(&g_cv_mutex);
    if (!d->id) {
        nx_vp9_decoder_destroy(d->decoder); pthread_mutex_destroy(&d->mutex); free(d);
        leave(c, CV_R(103)); return;
    }
    *(CvDecoder **)type = d;
    if (!nx_event_signal_handle(d->event_handle)) {
        pthread_mutex_lock(&g_cv_mutex); g_cv[d->id - 1] = NULL; pthread_mutex_unlock(&g_cv_mutex);
        *(CvDecoder **)type = NULL; nx_vp9_decoder_destroy(d->decoder);
        pthread_mutex_destroy(&d->mutex); free(d); fail_event(c, "CreateSystemEvent handle", 0);
        nx_host_leave(); return;
    }
    fprintf(stderr, "[codec] VP9 decoder %u: %ux%u, input format %llu, output format %llu (libvpx host HLE)\n",
            d->id, d->width, d->height, (unsigned long long)input, (unsigned long long)output);
    leave(c, 0);
}

void nxcodec_vp9_init(Ctx *c) {
    enter(c);
    uint64_t out_format = stack_arg(c, 0);
    decoder_init(c, (void *)(uintptr_t)c->x[0], c->x[4], c->x[5], c->x[7], out_format);
}

void nxcodec_vp9_init_no_work(Ctx *c) {
    enter(c);
    decoder_init(c, (void *)(uintptr_t)c->x[0], c->x[2], c->x[3], c->x[5], c->x[6]);
}

void nxcodec_vp9_finalize(Ctx *c) {
    enter(c);
    void *type = (void *)(uintptr_t)c->x[0]; CvDecoder *d = decoder_of(type);
    if (!d) { leave(c, 0); return; }
    pthread_mutex_lock(&g_cv_mutex);
    g_cv[d->id - 1] = NULL;
    pthread_mutex_unlock(&g_cv_mutex);
    uint64_t destroy = nx_symbol("_ZN2nn2os18DestroySystemEventEPNS0_15SystemEventTypeE");
    uint64_t sdk_rc = 0;
    uint64_t args[] = {(uint64_t)(uintptr_t)type + CV_EVENT_OFF};
    int called = destroy && nx_guest_call(c, destroy, args, 1, &sdk_rc);
    for (uint32_t k = 0; k < d->pending_count; k++) {
        uint32_t i = (d->pending_head + k) % CV_MAX_PENDING;
        nx_codec_image_free(d->pending[i].image);
    }
    nx_vp9_decoder_destroy(d->decoder);
    d->magic = 0; pthread_mutex_destroy(&d->mutex); free(d); *(CvDecoder **)type = NULL;
    leave(c, called ? (uint32_t)sdk_rc : CV_BAD);
}

void nxcodec_vp9_register(Ctx *c) {
    enter(c);
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[0]);
    CvPool *p = pool_of((void *)(uintptr_t)c->x[2]);
    uint32_t i = (uint32_t)c->x[1], pi = (uint32_t)c->x[3];
    if (!d || !p || i >= CV_MAX_BUF || pi >= p->count) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    CvBuffer *b = &d->buffers[i];
    if (b->registered) {
        pthread_mutex_unlock(&d->mutex); leave(c, CV_BAD); return;
    }
    b->registered = 1; b->pool = p; b->pool_index = pi;
    b->generation = b->filled_generation = 0;
    if (!qpush(d->available, &d->available_head, &d->available_count, CV_MAX_AVAILABLE, i))
        fprintf(stderr, "[RED] nn::codec VP9: cola de buffers disponibles llena al registrar índice %u\n", i);
    codec_trace("register decoder=%u index=%u pool=%u available=%u", d->id, i, pi, d->available_count);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}

void nxcodec_vp9_unregister(Ctx *c) {
    enter(c);
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[0]); uint32_t i = (uint32_t)c->x[1];
    if (!d || i >= CV_MAX_BUF) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    d->buffers[i].registered = 0; d->buffers[i].filled = 0; d->buffers[i].pool = NULL;
    d->buffers[i].generation++;
    qremove(d->available, &d->available_head, &d->available_count, CV_MAX_AVAILABLE, i);
    qremove(d->released, &d->released_head, &d->released_count, CV_MAX_BUF, i);
    codec_trace("unregister decoder=%u index=%u", d->id, i);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}

void nxcodec_vp9_start(Ctx *c) {
    enter(c); CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[0]);
    if (!d) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex); d->started = 1;
    if (d->pending_count < 4 && !nx_event_signal_handle(d->event_handle)) {
        pthread_mutex_unlock(&d->mutex); leave(c, CV_BAD); return;
    }
    codec_trace("start decoder=%u available=%u pending=%u released=%u", d->id,
                d->available_count, d->pending_count, d->released_count);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}
void nxcodec_vp9_stop(Ctx *c) {
    enter(c); CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[0]);
    if (!d) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    for (uint32_t k = 0; k < d->pending_count; k++) nx_codec_image_free(d->pending[(d->pending_head + k) % CV_MAX_PENDING].image);
    d->pending_head = d->pending_count = d->released_head = d->released_count = 0;
    d->started = 0;
    NxVpxDecoder *fresh = nx_vp9_decoder_create(d->width, d->height, 2);
    if (!fresh) { pthread_mutex_unlock(&d->mutex); leave(c, CV_R(103)); return; }
    NxVpxDecoder *old = d->decoder; d->decoder = fresh;
    pthread_mutex_unlock(&d->mutex);
    nx_vp9_decoder_destroy(old);
    leave(c, 0);
}

void nxcodec_vp9_event(Ctx *c) {
    enter(c);
    uint64_t *out = (uint64_t *)(uintptr_t)c->x[0]; CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[1]);
    if (!out || !d) { leave(c, CV_BAD); return; }
    *out = (uint64_t)(uintptr_t)((uint8_t *)((void *)(uintptr_t)c->x[1]) + CV_EVENT_OFF);
    leave(c, 0);
}

void nxcodec_vp9_submit(Ctx *c) {
    enter(c);
    uint64_t *submit_id = (uint64_t *)(uintptr_t)c->x[0];
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[1]);
    const void *data = (const void *)(uintptr_t)c->x[2]; size_t size = (size_t)c->x[3];
    int64_t timestamp = (int64_t)c->x[4]; int eos = c->x[5] != 0;
    if (!d || !submit_id || (!data && size)) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    if (d->pending_count > CV_MAX_PENDING - 16) {
        nx_event_clear_handle(d->event_handle); pthread_mutex_unlock(&d->mutex); leave(c, CV_BUSY); return;
    }
    NxCodecImage *images[16] = {0};
    int n = nx_vp9_decoder_decode(d->decoder, data, size, timestamp, images, 16);
    if (n < 0) {
        codec_trace("decode-error decoder=%u size=%zu eos=%d error=%s", d->id, size, eos,
                    nx_vp9_decoder_error(d->decoder));
        for (int i = 0; i < 16; i++) nx_codec_image_free(images[i]);
        pthread_mutex_unlock(&d->mutex); leave(c, CV_BAD); return;
    }
    for (int i = 0; i < n; i++) {
        uint32_t k = (d->pending_head + d->pending_count++) % CV_MAX_PENDING;
        d->pending[k].image = images[i]; d->pending[k].timestamp = timestamp; d->pending[k].eos = eos && i == n - 1;
    }
    if (!n && eos) {
        uint32_t k = (d->pending_head + d->pending_count++) % CV_MAX_PENDING;
        d->pending[k].image = NULL; d->pending[k].timestamp = timestamp; d->pending[k].eos = 1;
    }
    *submit_id = ++d->submitted;
    fill_released(d);
    codec_trace("submit decoder=%u id=%llu size=%zu eos=%d decoded=%d pending=%u available=%u released=%u",
                d->id, (unsigned long long)*submit_id, size, eos, n,
                d->pending_count, d->available_count, d->released_count);
    if (d->pending_count >= 4) nx_event_clear_handle(d->event_handle);
    else nx_event_signal_handle(d->event_handle);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}

void nxcodec_vp9_submit_encrypted(Ctx *c) {
    enter(c);
    fprintf(stderr, "[RED] nn::codec VP9: flujo cifrado no está implementado en el HLE Linux\n");
    leave(c, CV_BAD);
}

void nxcodec_vp9_acquire(Ctx *c) {
    enter(c);
    uint32_t *out_index = (uint32_t *)(uintptr_t)c->x[0];
    CvSync *out_sync = (CvSync *)(uintptr_t)c->x[1];
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[2]);
    if (!out_index || !out_sync || !d) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    if (!d->available_count) {
        codec_trace("acquire-empty decoder=%u pending=%u released=%u", d->id,
                    d->pending_count, d->released_count);
        pthread_mutex_unlock(&d->mutex); leave(c, CV_NONE); return;
    }
    uint32_t i = d->available[d->available_head];
    d->available_head = (d->available_head + 1) % CV_MAX_AVAILABLE; d->available_count--;
    *out_index = i; *out_sync = (CvSync){CV_SYNC, d->id, i, d->buffers[i].generation};
    codec_trace("acquire decoder=%u index=%u generation=%u filled=%d eos=%d available=%u",
                d->id, i, d->buffers[i].generation, d->buffers[i].filled,
                d->buffers[i].eos, d->available_count);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}

void nxcodec_vp9_release(Ctx *c) {
    enter(c);
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[0]); uint32_t i = (uint32_t)c->x[1];
    // Horizon's SDK treats the supplied sync handle as advisory here; release
    // is keyed by the registered decoder buffer index.
    if (!d || i >= CV_MAX_BUF) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex);
    CvBuffer *b = &d->buffers[i];
    if (!b->registered) {
        pthread_mutex_unlock(&d->mutex); leave(c, CV_BAD); return;
    }
    b->generation++; b->filled = 0; b->eos = 0;
    if (d->released_count < CV_MAX_BUF)
        qpush(d->released, &d->released_head, &d->released_count, CV_MAX_BUF, i);
    else
        fprintf(stderr, "[RED] nn::codec VP9: cola de imágenes liberadas llena al liberar índice %u\n", i);
    if (d->available_count < CV_MAX_AVAILABLE)
        qpush(d->available, &d->available_head, &d->available_count, CV_MAX_AVAILABLE, i);
    else
        fprintf(stderr, "[RED] nn::codec VP9: cola de buffers disponibles llena al liberar índice %u\n", i);
    fill_released(d);
    codec_trace("release decoder=%u index=%u generation=%u eos=%d filled=%d pending=%u available=%u released=%u",
                d->id, i, b->generation, b->eos, b->filled,
                d->pending_count, d->available_count, d->released_count);
    if (d->pending_count < 4) nx_event_signal_handle(d->event_handle);
    pthread_mutex_unlock(&d->mutex); leave(c, 0);
}

void nxcodec_vp9_properties(Ctx *c) {
    enter(c);
    uint16_t *w = (uint16_t *)(uintptr_t)c->x[0], *h = (uint16_t *)(uintptr_t)c->x[1];
    uint16_t *cw = (uint16_t *)(uintptr_t)c->x[2], *ch = (uint16_t *)(uintptr_t)c->x[3];
    int64_t *ts = (int64_t *)(uintptr_t)c->x[4]; uint8_t *eos = (uint8_t *)(uintptr_t)c->x[5];
    CvDecoder *d = decoder_of((void *)(uintptr_t)c->x[6]); uint32_t i = (uint32_t)c->x[7];
    if (!w || !h || !cw || !ch || !ts || !eos || !d || i >= CV_MAX_BUF) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&d->mutex); CvBuffer b = d->buffers[i]; pthread_mutex_unlock(&d->mutex);
    if (!b.registered || !b.filled) {
        codec_trace("properties-empty decoder=%u index=%u registered=%d filled=%d", d->id, i,
                    b.registered, b.filled);
        leave(c, CV_NONE); return;
    }
    *w = *cw = b.width; *h = *ch = b.height; *ts = b.timestamp; *eos = (uint8_t)b.eos;
    codec_trace("properties decoder=%u index=%u size=%ux%u timestamp=%lld eos=%u generation=%u/%u",
                d->id, i, b.width, b.height, (long long)b.timestamp, b.eos,
                b.filled_generation, b.generation);
    leave(c, 0);
}

static void write_rgba(Ctx *c, int bgra) {
    enter(c);
    CvPool *p = pool_of((void *)(uintptr_t)c->x[0]); uint32_t i = (uint32_t)c->x[1];
    uint32_t x = (uint16_t)c->x[2], y = (uint16_t)c->x[3], w = (uint16_t)c->x[4], h = (uint16_t)c->x[5];
    uint8_t *dst = (uint8_t *)(uintptr_t)c->x[6]; uint64_t stride = c->x[7], size = stack_arg(c, 0);
    if (!p || i >= p->count || !dst || x || y || stride < (uint64_t)w * 4 || size < stride * h) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&p->mutex); NxCodecImage *im = p->images[i];
    if (!im) { memset(dst, 0, (size_t)size); pthread_mutex_unlock(&p->mutex); leave(c, 0); return; }
    if (w != im->width || h != im->height) { pthread_mutex_unlock(&p->mutex); leave(c, CV_BAD); return; }
    for (uint32_t row = 0; row < h; row++) for (uint32_t col = 0; col < w; col++) {
        int yy = im->y[(size_t)row * im->y_stride + col];
        int uu = (int)im->u[(size_t)(row / 2) * im->u_stride + col / 2] - 128;
        int vv = (int)im->v[(size_t)(row / 2) * im->v_stride + col / 2] - 128;
        int c0 = yy - 16; if (c0 < 0) c0 = 0;
        int r = (298 * c0 + 459 * vv + 128) >> 8;
        int g = (298 * c0 - 55 * uu - 136 * vv + 128) >> 8;
        int b = (298 * c0 + 541 * uu + 128) >> 8;
        uint8_t *px = dst + (size_t)row * stride + col * 4;
        px[bgra ? 0 : 2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
        px[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
        px[bgra ? 2 : 0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
        px[3] = 255;
    }
    pthread_mutex_unlock(&p->mutex); leave(c, 0);
}
void nxcodec_read_rgba(Ctx *c) { write_rgba(c, 0); }
void nxcodec_read_bgra(Ctx *c) { write_rgba(c, 1); }

void nxcodec_read_nv12(Ctx *c) {
    enter(c);
    CvPool *p = pool_of((void *)(uintptr_t)c->x[0]); uint32_t i = (uint32_t)c->x[1];
    uint32_t x = (uint16_t)c->x[2], y = (uint16_t)c->x[3], w = (uint16_t)c->x[4], h = (uint16_t)c->x[5];
    uint8_t *ydst = (uint8_t *)(uintptr_t)c->x[6]; uint64_t ys = c->x[7], yz = stack_arg(c, 0);
    uint8_t *uvdst = (uint8_t *)(uintptr_t)stack_arg(c, 1); uint64_t uvs = stack_arg(c, 2), uvz = stack_arg(c, 3);
    if (!p || i >= p->count || !ydst || !uvdst || x || y || ys < w || uvs < w || yz < ys * h || uvz < uvs * ((h + 1) / 2)) { leave(c, CV_BAD); return; }
    pthread_mutex_lock(&p->mutex); NxCodecImage *im = p->images[i];
    if (!im) { memset(ydst, 16, (size_t)yz); memset(uvdst, 128, (size_t)uvz); pthread_mutex_unlock(&p->mutex); leave(c, 0); return; }
    if (w != im->width || h != im->height) { pthread_mutex_unlock(&p->mutex); leave(c, CV_BAD); return; }
    for (uint32_t row = 0; row < h; row++) memcpy(ydst + (size_t)row * ys, im->y + (size_t)row * im->y_stride, w);
    for (uint32_t row = 0; row < (h + 1) / 2; row++) for (uint32_t col = 0; col < (w + 1) / 2; col++) {
        uint8_t *uv = uvdst + (size_t)row * uvs + col * 2;
        uv[0] = im->u[(size_t)row * im->u_stride + col];
        uv[1] = im->v[(size_t)row * im->v_stride + col];
    }
    pthread_mutex_unlock(&p->mutex); leave(c, 0);
}

void nxcodec_try_wait_native_sync(Ctx *c) {
    enter(c);
    const CvSync *s = (const CvSync *)(uintptr_t)c->x[0];
    if (!s || s->magic != CV_SYNC) { leave(c, 1); return; }
    pthread_mutex_lock(&g_cv_mutex);
    CvDecoder *d = s->decoder && s->decoder <= CV_MAX_DEC ? g_cv[s->decoder - 1] : NULL;
    int ready = !s->generation;
    if (d && s->index < CV_MAX_BUF) {
        pthread_mutex_lock(&d->mutex);
        ready = ready || (d->buffers[s->index].filled && d->buffers[s->index].filled_generation >= s->generation);
        pthread_mutex_unlock(&d->mutex);
    }
    pthread_mutex_unlock(&g_cv_mutex); leave(c, (uint64_t)ready);
}

void nxcodec_syncpt_create(Ctx *c) {
    enter(c);
    uint64_t address = c->x[0];
    if (!address) { leave(c, CV_BAD); return; }
    memset((void *)(uintptr_t)address, 0, 0x58);
    uint32_t handle = 0, sdk_result = 0;
    // SyncPointEventType embeds the SDK SystemEventType at +0. It must be an
    // inter-process event: the SDK's local-event path does not create the
    // readable/writable handle pair used by the codec worker and MultiWait.
    // This matches nn::syncpt's event contract and the decoder-ready event
    // created by decoder_init().
    if (!nx_hle_event_create(c, address, 0, 1, &handle, &sdk_result) || !handle) {
        fprintf(stderr, "[RED] nn::syncpt: no se pudo crear SyncPointEvent (resultado SDK %#x)\n", sdk_result);
        leave(c, sdk_result ? sdk_result : CV_BAD); return;
    }
    CvSyncPtState *state = (CvSyncPtState *)(uintptr_t)(address + 0x40);
    state->magic = CV_SYNC;
    int stored = 0;
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++) {
        if (!g_syncpt[i].address) {
            g_syncpt[i] = (CvSyncPtEntry){address, handle};
            stored = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_syncpt_mutex);
    if (!stored) {
        uint64_t destroy = nx_symbol("_ZN2nn2os18DestroySystemEventEPNS0_15SystemEventTypeE");
        if (destroy) { uint64_t args[] = {address}; nx_guest_call(c, destroy, args, 1, NULL); }
        leave(c, CV_R(103)); return;
    }
    leave(c, 0);
}

void nxcodec_syncpt_init_holder(Ctx *c) {
    enter(c);
    uint64_t init = nx_symbol("_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE");
    if (!init) {
        fprintf(stderr, "[RED] nn::syncpt: falta nn::os::InitializeMultiWaitHolder\n");
        leave(c, CV_BAD); return;
    }
    uint64_t args[] = {c->x[0], c->x[1]};
    if (!nx_guest_call(c, init, args, 2, NULL)) { leave(c, CV_BAD); return; }
    leave(c, 0);
}

void nxcodec_syncpt_request(Ctx *c) {
    enter(c);
    uint64_t address = c->x[0];
    const CvSync *sync = (const CvSync *)(uintptr_t)c->x[1];
    uint32_t handle = syncpt_handle(address);
    if (!address || !sync || !handle || sync->magic != CV_SYNC) { leave(c, CV_BAD); return; }
    syncpt_disarm(address, 1);
    // disarm removes the previous armed entry, so locate its persistent event
    // handle from the event object registry is needed before re-registering.
    uint32_t event_handle = 0;
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++) {
        if (g_syncpt[i].address == address) {
            event_handle = handle;
            break;
        }
    }
    pthread_mutex_unlock(&g_syncpt_mutex);
    if (!event_handle) { leave(c, CV_R(103)); return; }
    CvSyncPtState *state = (CvSyncPtState *)(uintptr_t)(address + 0x40);
    state->want = *sync;
    state->armed = 1;
    if (sync_reached(sync)) {
        pthread_mutex_lock(&g_syncpt_mutex);
        state->armed = 0;
        pthread_mutex_unlock(&g_syncpt_mutex);
        if (!nx_event_signal_handle(event_handle))
            fprintf(stderr, "[RED] nn::syncpt: no se pudo señalar un punto ya completado\n");
    }
    leave(c, 0);
}

void nxcodec_syncpt_cancel(Ctx *c) {
    enter(c);
    uint64_t address = c->x[0];
    if (!address || !syncpt_handle(address)) { leave(c, CV_BAD); return; }
    syncpt_disarm(address, 1);
    leave(c, 0);
}

void nxcodec_syncpt_try_wait(Ctx *c) {
    enter(c);
    uint64_t address = c->x[0];
    if (!address || !syncpt_handle(address)) { leave(c, 0); return; }
    uint64_t try_wait = nx_symbol("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE");
    uint64_t args[] = {address}, result = 0;
    if (!try_wait || !nx_guest_call(c, try_wait, args, 1, &result)) {
        fprintf(stderr, "[RED] nn::syncpt: no se pudo consultar el SystemEvent\n");
        leave(c, 0); return;
    }
    leave(c, result & 1);
}

void nxcodec_syncpt_destroy(Ctx *c) {
    enter(c);
    uint64_t address = c->x[0];
    uint32_t handle = syncpt_handle(address);
    if (!address || !handle) { leave(c, CV_BAD); return; }
    syncpt_disarm(address, 0);
    pthread_mutex_lock(&g_syncpt_mutex);
    for (size_t i = 0; i < sizeof g_syncpt / sizeof g_syncpt[0]; i++)
        if (g_syncpt[i].address == address) { g_syncpt[i] = (CvSyncPtEntry){0}; break; }
    pthread_mutex_unlock(&g_syncpt_mutex);
    uint64_t destroy = nx_symbol("_ZN2nn2os18DestroySystemEventEPNS0_15SystemEventTypeE");
    uint64_t args[] = {address}, result = 0;
    CvSyncPtState *state = (CvSyncPtState *)(uintptr_t)(address + 0x40);
    state->magic = 0;
    if (!destroy || !nx_guest_call(c, destroy, args, 1, &result)) {
        fprintf(stderr, "[RED] nn::syncpt: no se pudo destruir el SystemEvent\n");
        leave(c, CV_BAD); return;
    }
    leave(c, result);
}
