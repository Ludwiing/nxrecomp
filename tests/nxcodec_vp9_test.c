#include "../runtime/nxcodec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vpx/vp8cx.h>
#include <vpx/vpx_encoder.h>

static int fail(const char *why) {
    fprintf(stderr, "FAIL: %s\n", why);
    return 1;
}

int main(void) {
    enum { W = 64, H = 48 };
    vpx_codec_enc_cfg_t cfg;
    if (vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &cfg, 0) != VPX_CODEC_OK)
        return fail("default VP9 encoder config");
    cfg.g_w = W;
    cfg.g_h = H;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = 30;
    cfg.g_lag_in_frames = 0;
    cfg.rc_target_bitrate = 100;
    cfg.rc_min_quantizer = 0;
    cfg.rc_max_quantizer = 0;

    vpx_codec_ctx_t encoder;
    if (vpx_codec_enc_init(&encoder, vpx_codec_vp9_cx(), &cfg, 0) != VPX_CODEC_OK)
        return fail("initialize VP9 encoder");
    vpx_image_t *input = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, W, H, 1);
    if (!input) {
        vpx_codec_destroy(&encoder);
        return fail("allocate input frame");
    }
    for (unsigned y = 0; y < H; y++) memset(input->planes[VPX_PLANE_Y] + y * input->stride[VPX_PLANE_Y], 96, W);
    for (unsigned y = 0; y < H / 2; y++) {
        memset(input->planes[VPX_PLANE_U] + y * input->stride[VPX_PLANE_U], 112, W / 2);
        memset(input->planes[VPX_PLANE_V] + y * input->stride[VPX_PLANE_V], 144, W / 2);
    }
    if (vpx_codec_encode(&encoder, input, 7, 1, 0, VPX_DL_REALTIME) != VPX_CODEC_OK) {
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("encode VP9 test frame");
    }
    const uint8_t *packet = NULL;
    size_t packet_size = 0;
    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t *out;
    while ((out = vpx_codec_get_cx_data(&encoder, &iter))) {
        if (out->kind == VPX_CODEC_CX_FRAME_PKT) {
            packet = out->data.frame.buf;
            packet_size = out->data.frame.sz;
            break;
        }
    }
    if (!packet || !packet_size) {
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("encoder produced no frame packet");
    }
    uint8_t *owned_packet = malloc(packet_size);
    if (!owned_packet) {
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("copy encoded packet");
    }
    memcpy(owned_packet, packet, packet_size);
    size_t packet_size1 = packet_size;

    // A second frame exercises reference-frame state across consecutive submissions.
    for (unsigned y = 0; y < H; y++) memset(input->planes[VPX_PLANE_Y] + y * input->stride[VPX_PLANE_Y], 128, W);
    for (unsigned y = 0; y < H / 2; y++) {
        memset(input->planes[VPX_PLANE_U] + y * input->stride[VPX_PLANE_U], 100, W / 2);
        memset(input->planes[VPX_PLANE_V] + y * input->stride[VPX_PLANE_V], 130, W / 2);
    }
    if (vpx_codec_encode(&encoder, input, 8, 1, 0, VPX_DL_REALTIME) != VPX_CODEC_OK) {
        free(owned_packet);
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("encode second VP9 test frame");
    }
    packet = NULL;
    packet_size = 0;
    iter = NULL;
    while ((out = vpx_codec_get_cx_data(&encoder, &iter))) {
        if (out->kind == VPX_CODEC_CX_FRAME_PKT) {
            packet = out->data.frame.buf;
            packet_size = out->data.frame.sz;
            break;
        }
    }
    if (!packet || !packet_size) {
        free(owned_packet);
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("encoder produced no second frame packet");
    }
    uint8_t *owned_packet2 = malloc(packet_size);
    if (!owned_packet2) {
        free(owned_packet);
        vpx_img_free(input);
        vpx_codec_destroy(&encoder);
        return fail("copy second encoded packet");
    }
    memcpy(owned_packet2, packet, packet_size);
    size_t packet_size2 = packet_size;
    vpx_img_free(input);
    vpx_codec_destroy(&encoder);

    NxVpxDecoder *decoder = nx_vp9_decoder_create(W, H, 2);
    if (!decoder) {
        free(owned_packet);
        return fail("create decoder");
    }
    NxCodecImage *images[2] = {0};
    int count = nx_vp9_decoder_decode(decoder, owned_packet, packet_size1, 7, images, 2);
    free(owned_packet);
    if (count != 1) {
        fprintf(stderr, "decoder error: %s\n", nx_vp9_decoder_error(decoder));
        nx_vp9_decoder_destroy(decoder);
        return fail("decode VP9 test frame");
    }
    NxCodecImage *image = images[0];
    if (!image || image->width != W || image->height != H || image->timestamp != 7 ||
        image->y_stride != W || image->u_stride != W / 2 || image->v_stride != W / 2 ||
        !image->y || !image->u || !image->v)
        return fail("decoded frame metadata/planes");
    if (image->y[0] < 90 || image->y[0] > 102 || image->u[0] < 106 || image->u[0] > 118 ||
        image->v[0] < 138 || image->v[0] > 150)
        return fail("decoded I420 sample values outside expected range");
    nx_codec_image_free(image);

    images[0] = NULL;
    count = nx_vp9_decoder_decode(decoder, owned_packet2, packet_size2, 8, images, 2);
    free(owned_packet2);
    if (count != 1 || !images[0] || images[0]->timestamp != 8 ||
        images[0]->y[0] < 122 || images[0]->y[0] > 134) {
        fprintf(stderr, "second-frame decode error: %s\n", nx_vp9_decoder_error(decoder));
        nx_vp9_decoder_destroy(decoder);
        return fail("decode consecutive VP9 frame");
    }
    nx_codec_image_free(images[0]);

    // End-of-stream: guests pass their (non-NULL) buffer with size 0. That is a
    // flush, never an "Invalid parameter" error from libvpx.
    uint8_t eos_byte = 0xAA;
    images[0] = images[1] = NULL;
    count = nx_vp9_decoder_decode(decoder, &eos_byte, 0, 9, images, 2);
    if (count < 0 || nx_vp9_decoder_error(decoder)[0]) {
        fprintf(stderr, "empty EOS packet error: %s\n", nx_vp9_decoder_error(decoder));
        nx_vp9_decoder_destroy(decoder);
        return fail("empty non-NULL EOS packet must flush, not fail");
    }
    for (int i = 0; i < count; i++) nx_codec_image_free(images[i]);
    count = nx_vp9_decoder_decode(decoder, NULL, 0, 9, images, 2);
    if (count < 0) {
        nx_vp9_decoder_destroy(decoder);
        return fail("NULL empty flush");
    }
    for (int i = 0; i < count; i++) nx_codec_image_free(images[i]);
    nx_vp9_decoder_destroy(decoder);

    if (nx_vp9_decoder_create(0, H, 1) != NULL) return fail("reject zero width");
    puts("OK: VP9 encode/decode, owned I420 frame, empty-packet EOS flush and input validation");
    return 0;
}
