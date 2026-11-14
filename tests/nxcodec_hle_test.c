#include "nxcodec_hle.h"
#include "nxkern.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vpx/vp8cx.h>
#include <vpx/vpx_encoder.h>

static uint8_t *encode_frame(unsigned w, unsigned h, size_t *packet_size) {
    vpx_codec_enc_cfg_t cfg;
    if (vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &cfg, 0) != VPX_CODEC_OK) return NULL;
    cfg.g_w = w; cfg.g_h = h; cfg.g_timebase.num = 1; cfg.g_timebase.den = 30;
    cfg.g_lag_in_frames = 0; cfg.rc_target_bitrate = 100;
    cfg.rc_min_quantizer = cfg.rc_max_quantizer = 0;
    vpx_codec_ctx_t enc;
    if (vpx_codec_enc_init(&enc, vpx_codec_vp9_cx(), &cfg, 0) != VPX_CODEC_OK) return NULL;
    vpx_image_t *im = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, w, h, 1);
    if (!im) { vpx_codec_destroy(&enc); return NULL; }
    for (unsigned y = 0; y < h; y++) memset(im->planes[VPX_PLANE_Y] + y * im->stride[VPX_PLANE_Y], 96, w);
    for (unsigned y = 0; y < h / 2; y++) {
        memset(im->planes[VPX_PLANE_U] + y * im->stride[VPX_PLANE_U], 112, w / 2);
        memset(im->planes[VPX_PLANE_V] + y * im->stride[VPX_PLANE_V], 144, w / 2);
    }
    uint8_t *owned = NULL;
    if (vpx_codec_encode(&enc, im, 123, 1, 0, VPX_DL_REALTIME) == VPX_CODEC_OK) {
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t *out;
        while ((out = vpx_codec_get_cx_data(&enc, &iter))) if (out->kind == VPX_CODEC_CX_FRAME_PKT) {
            owned = malloc(out->data.frame.sz);
            if (owned) { *packet_size = out->data.frame.sz; memcpy(owned, out->data.frame.buf, *packet_size); }
            break;
        }
    }
    vpx_img_free(im); vpx_codec_destroy(&enc);
    return owned;
}

static int host_depth;
static uint32_t next_handle = 0x1002;
static uint64_t event_addr;
static uint32_t event_signaled;
static uint64_t destroy_addr;
static uint64_t holder_addr, holder_event_addr;
static int last_event_inter_process;
static struct { uint64_t address; uint32_t handle, signaled; } events[16];

void nx_host_enter(void) { host_depth++; }
void nx_host_leave(void) { assert(host_depth > 0); host_depth--; }
int nx_hle_event_create(Ctx *ctx, uint64_t ev, uint32_t clear, int inter,
                        uint32_t *wh, uint32_t *sdk_rc) {
    assert(ctx && clear == 0 && (inter == 0 || inter == 1));
    event_addr = ev; last_event_inter_process = inter; *wh = next_handle++; *sdk_rc = 0;
    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++) if (!events[i].address) {
        events[i] = (typeof(events[0])){ev, *wh, 0}; return 1;
    }
    return 0;
}
int nx_event_signal_handle(uint32_t h) {
    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++) if (events[i].handle == h) {
        events[i].signaled = 1; event_signaled = 1; return 1;
    }
    return 0;
}
int nx_event_clear_handle(uint32_t h) {
    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++) if (events[i].handle == h) {
        events[i].signaled = 0; event_signaled = 0; return 1;
    }
    return 0;
}
uint64_t nx_symbol(const char *name) {
    if (!strcmp(name, "_ZN2nn2os18DestroySystemEventEPNS0_15SystemEventTypeE")) return 0x9000000;
    if (!strcmp(name, "_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE")) return 0x9000001;
    if (!strcmp(name, "_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE")) return 0x9000002;
    return 0;
}
int nx_guest_call(Ctx *ctx, uint64_t addr, const uint64_t *args, size_t n, uint64_t *result) {
    (void)ctx;
    if (addr == 0x9000000 && n == 1) { destroy_addr = args[0]; if (result) *result = 0; return 1; }
    if (addr == 0x9000001 && n == 2) { holder_addr = args[0]; holder_event_addr = args[1]; if (result) *result = 0; return 1; }
    if (addr == 0x9000002 && n == 1 && result) {
        for (size_t i = 0; i < sizeof events / sizeof events[0]; i++) if (events[i].address == args[0]) {
            *result = events[i].signaled; return 1;
        }
        *result = 0; return 1;
    }
    return 0;
}

int main(void) {
    enum { W = 64, H = 48 };
    Ctx c = {0};
    uint8_t decoder_type[0x200] = {0};
    uint8_t pool_type_storage[sizeof(void *)] = {0};
    uint8_t pref_storage[32] = {0};
    uint64_t size = 0, align = 0;

    c.x[0] = W; c.x[1] = H; c.x[2] = 0; c.x[3] = 0; c.x[8] = (uintptr_t)pref_storage;
    nxcodec_pool_preference(&c);
    assert(host_depth == 0);
    c.x[0] = (uintptr_t)&size; c.x[1] = (uintptr_t)&align; c.x[2] = (uintptr_t)pref_storage; c.x[3] = 2;
    nxcodec_pool_size(&c);
    assert(c.x[0] == 0 && size == 0x8000 && align == 0x4000);

    uint64_t init_stack[] = {3};
    c = (Ctx){0}; c.sp = (uintptr_t)init_stack;
    c.x[0] = (uintptr_t)decoder_type; c.x[4] = W; c.x[5] = H; c.x[7] = 1;
    nxcodec_vp9_init(&c);
    assert(c.x[0] == 0 && host_depth == 0 && event_signaled);
    assert(event_addr == (uintptr_t)decoder_type + 0x100);

    // GetVp9DecoderSubmissionReadyEvent hands back the decoder's embedded SystemEvent (decoder + 0x100), the event the
    // Switch 2 video player waits on; Start re-arms it for input and Stop resets the decoder. The crash this covers
    // aborted in GetVp9DecoderSubmissionReadyEvent, so these three stay exercised.
    // An uninitialized decoder is zeroed storage: decoder_of reads *type == NULL and returns none (as the guest sees it).
    uint8_t unknown_type[sizeof(void *)] = {0};
    uint64_t ready_event = 0;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ready_event; c.x[1] = (uintptr_t)decoder_type;
    nxcodec_vp9_event(&c);
    assert(c.x[0] == 0 && ready_event == (uintptr_t)decoder_type + 0x100 && host_depth == 0);
    c = (Ctx){0}; c.x[0] = 0; c.x[1] = (uintptr_t)decoder_type; // a null out pointer is rejected
    nxcodec_vp9_event(&c); assert(c.x[0] != 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ready_event; c.x[1] = (uintptr_t)unknown_type; // an unknown decoder is rejected
    nxcodec_vp9_event(&c); assert(c.x[0] != 0);
    event_signaled = 0;
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; nxcodec_vp9_start(&c);
    assert(c.x[0] == 0 && event_signaled && host_depth == 0); // no pending frames: armed for input
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; nxcodec_vp9_stop(&c);
    assert(c.x[0] == 0 && host_depth == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)unknown_type; nxcodec_vp9_start(&c); assert(c.x[0] != 0); // unknown decoder
    c = (Ctx){0}; c.x[0] = (uintptr_t)unknown_type; nxcodec_vp9_stop(&c); assert(c.x[0] != 0);

    c = (Ctx){0}; c.x[0] = (uintptr_t)pool_type_storage; c.x[1] = 0x123000;
    c.x[2] = size; c.x[3] = (uintptr_t)pref_storage; c.x[4] = 2; c.x[5] = 0;
    nxcodec_pool_init(&c); assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; c.x[1] = 0; c.x[2] = (uintptr_t)pool_type_storage; c.x[3] = 0;
    nxcodec_vp9_register(&c); assert(c.x[0] == 0);

    uint32_t index = 99;
    uint32_t sync[4] = {0};
    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c);
    assert(c.x[0] == 0 && index == 0 && sync[0] == 0x53436d44 && sync[3] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; c.x[1] = index;
    c.x[2] = UINT64_MAX; c.x[3] = UINT64_MAX; // release is indexed by decoder + buffer, not sync contents
    nxcodec_vp9_release(&c); assert(c.x[0] == 0);

    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c);
    assert(c.x[0] == 0 && sync[3] == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)sync; nxcodec_try_wait_native_sync(&c);
    assert(c.x[0] == 0); // No decoded frame has filled generation 1.

    uint8_t syncpt_event[0x80] = {0}, multiwait_holder[0x80] = {0};
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_create(&c);
    assert(c.x[0] == 0 && event_addr == (uintptr_t)syncpt_event && last_event_inter_process == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)multiwait_holder; c.x[1] = (uintptr_t)syncpt_event;
    nxcodec_syncpt_init_holder(&c);
    assert(c.x[0] == 0 && holder_addr == (uintptr_t)multiwait_holder && holder_event_addr == (uintptr_t)syncpt_event);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; c.x[1] = (uintptr_t)sync;
    nxcodec_syncpt_request(&c); assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 0); // Requested generation has not been decoded.

    // Complete guest-facing path: packet submit, released-buffer fill, sync and readback.
    size_t packet_size = 0;
    uint8_t *packet = encode_frame(W, H, &packet_size);
    assert(packet && packet_size);
    uint64_t submit_id = 0;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&submit_id; c.x[1] = (uintptr_t)decoder_type;
    c.x[2] = (uintptr_t)packet; c.x[3] = packet_size; c.x[4] = 123; c.x[5] = 0;
    nxcodec_vp9_submit(&c); free(packet);
    assert(c.x[0] == 0 && submit_id == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 1); // Decoder completion signals the armed SyncPointEvent.
    c = (Ctx){0}; c.x[0] = (uintptr_t)sync; nxcodec_try_wait_native_sync(&c);
    assert(c.x[0] == 1);
    uint16_t ow = 0, oh = 0, cw = 0, ch = 0; int64_t timestamp = 0; uint8_t eos = 1;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ow; c.x[1] = (uintptr_t)&oh;
    c.x[2] = (uintptr_t)&cw; c.x[3] = (uintptr_t)&ch; c.x[4] = (uintptr_t)&timestamp;
    c.x[5] = (uintptr_t)&eos; c.x[6] = (uintptr_t)decoder_type; c.x[7] = index;
    nxcodec_vp9_properties(&c);
    assert(c.x[0] == 0 && ow == W && oh == H && cw == W && ch == H && timestamp == 123 && eos == 0);
    uint8_t rgba[W * H * 4]; uint64_t read_stack[] = {sizeof rgba};
    c = (Ctx){0}; c.sp = (uintptr_t)read_stack; c.x[0] = (uintptr_t)pool_type_storage;
    c.x[1] = 0; c.x[4] = W; c.x[5] = H; c.x[6] = (uintptr_t)rgba; c.x[7] = W * 4;
    nxcodec_read_rgba(&c);
    assert(c.x[0] == 0 && rgba[0] >= 110 && rgba[0] <= 128 && rgba[1] >= 82 && rgba[1] <= 102 && rgba[2] >= 50 && rgba[2] <= 72 && rgba[3] == 255);

    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_cancel(&c);
    assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 0); // Cancel clears the manual SystemEvent.
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; c.x[1] = (uintptr_t)sync;
    nxcodec_syncpt_request(&c); assert(c.x[0] == 0); // already-completed point signals immediately
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event; nxcodec_syncpt_destroy(&c);
    assert(c.x[0] == 0 && destroy_addr == (uintptr_t)syncpt_event);

    // End-of-stream regression: the guest submits an empty packet with its
    // (non-NULL) buffer pointer, size 0 and the EOS flag while a decoded frame
    // is still pending (no released buffer to fill). The submit must succeed
    // (CV_BAD here is routed to AbortImpl by the guest), the pending frame must
    // be delivered first and the EOS marker afterwards, in a separate buffer.
    const uint32_t decoder_id = sync[1];
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; c.x[1] = 1; c.x[2] = (uintptr_t)pool_type_storage; c.x[3] = 1;
    nxcodec_vp9_register(&c); assert(c.x[0] == 0);
    packet = encode_frame(W, H, &packet_size);
    assert(packet && packet_size);
    c = (Ctx){0}; c.x[0] = (uintptr_t)&submit_id; c.x[1] = (uintptr_t)decoder_type;
    c.x[2] = (uintptr_t)packet; c.x[3] = packet_size; c.x[4] = 456; c.x[5] = 0;
    nxcodec_vp9_submit(&c); free(packet);
    assert(c.x[0] == 0 && submit_id == 2); // decoded, queued as pending (no released buffer)
    uint8_t eos_byte = 0xAA;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&submit_id; c.x[1] = (uintptr_t)decoder_type;
    c.x[2] = (uintptr_t)&eos_byte; c.x[3] = 0; c.x[4] = 789; c.x[5] = 1;
    nxcodec_vp9_submit(&c);
    assert(c.x[0] == 0 && submit_id == 3 && host_depth == 0);
    int decoder_event_signaled = -1;
    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++)
        if (events[i].address == (uintptr_t)decoder_type + 0x100) decoder_event_signaled = (int)events[i].signaled;
    assert(decoder_event_signaled == 1); // queue below the back-pressure limit: decoder ready for input
    // The buffer the guest still holds is untouched by the queued submits.
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ow; c.x[1] = (uintptr_t)&oh;
    c.x[2] = (uintptr_t)&cw; c.x[3] = (uintptr_t)&ch; c.x[4] = (uintptr_t)&timestamp;
    c.x[5] = (uintptr_t)&eos; c.x[6] = (uintptr_t)decoder_type; c.x[7] = 0;
    nxcodec_vp9_properties(&c);
    assert(c.x[0] == 0 && timestamp == 123 && eos == 0);

    uint8_t syncpt_event2[0x80] = {0};
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_create(&c); assert(c.x[0] == 0);
    uint32_t want_frame[4] = {0x53436d44, decoder_id, 0, 2};
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; c.x[1] = (uintptr_t)want_frame;
    nxcodec_syncpt_request(&c); assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; c.x[1] = 0;
    nxcodec_vp9_release(&c); assert(c.x[0] == 0); // fills buffer 0 with the pending frame
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 1);

    uint32_t sync_b1[4] = {0}, sync_b0[4] = {0};
    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync_b1; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c); assert(c.x[0] == 0 && index == 1 && sync_b1[3] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync_b0; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c); assert(c.x[0] == 0 && index == 0 && sync_b0[3] == 2);
    c = (Ctx){0}; c.x[0] = (uintptr_t)sync_b0; nxcodec_try_wait_native_sync(&c); assert(c.x[0] == 1);
    eos = 1; timestamp = 0; ow = oh = 0;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ow; c.x[1] = (uintptr_t)&oh;
    c.x[2] = (uintptr_t)&cw; c.x[3] = (uintptr_t)&ch; c.x[4] = (uintptr_t)&timestamp;
    c.x[5] = (uintptr_t)&eos; c.x[6] = (uintptr_t)decoder_type; c.x[7] = 0;
    nxcodec_vp9_properties(&c);
    assert(c.x[0] == 0 && ow == W && oh == H && timestamp == 456 && eos == 0); // pending frame not lost

    uint32_t want_eos[4] = {0x53436d44, decoder_id, 1, 1};
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; c.x[1] = (uintptr_t)want_eos;
    nxcodec_syncpt_request(&c); assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; c.x[1] = 1;
    nxcodec_vp9_release(&c); assert(c.x[0] == 0); // fills buffer 1 with the EOS marker
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_try_wait(&c);
    assert(c.x[0] == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync_b1; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c); assert(c.x[0] == 0 && index == 1 && sync_b1[3] == 1);
    c = (Ctx){0}; c.x[0] = (uintptr_t)sync_b1; nxcodec_try_wait_native_sync(&c); assert(c.x[0] == 1);
    eos = 0; timestamp = 0; ow = oh = 0xffff;
    c = (Ctx){0}; c.x[0] = (uintptr_t)&ow; c.x[1] = (uintptr_t)&oh;
    c.x[2] = (uintptr_t)&cw; c.x[3] = (uintptr_t)&ch; c.x[4] = (uintptr_t)&timestamp;
    c.x[5] = (uintptr_t)&eos; c.x[6] = (uintptr_t)decoder_type; c.x[7] = 1;
    nxcodec_vp9_properties(&c);
    assert(c.x[0] == 0 && eos == 1 && timestamp == 789 && ow == 0 && oh == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)&index; c.x[1] = (uintptr_t)sync_b1; c.x[2] = (uintptr_t)decoder_type;
    nxcodec_vp9_acquire(&c); assert(c.x[0] != 0); // nothing else queued after EOS
    c = (Ctx){0}; c.x[0] = (uintptr_t)syncpt_event2; nxcodec_syncpt_destroy(&c); assert(c.x[0] == 0);

    c = (Ctx){0}; c.x[0] = (uintptr_t)pool_type_storage; nxcodec_pool_finalize(&c); assert(c.x[0] == 0);
    c = (Ctx){0}; c.x[0] = (uintptr_t)decoder_type; nxcodec_vp9_finalize(&c);
    assert(c.x[0] == 0 && destroy_addr == (uintptr_t)decoder_type + 0x100 && host_depth == 0);
    puts("OK: API VP9, evento de envío listo, start/stop, paquete real, buffers, sync, lectura RGBA, EOS vacío y destrucción HLE");
    return 0;
}
