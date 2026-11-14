#include "nxcodec.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>

#define NX_VPX_ERROR_MAX 256
#define NX_VPX_MAX_IMAGE_BYTES (256u * 1024u * 1024u)

struct NxVpxDecoder {
    vpx_codec_ctx_t codec;
    int initialized;
    char error[NX_VPX_ERROR_MAX];
};

static void set_error(NxVpxDecoder *d, const char *what, const vpx_codec_ctx_t *ctx) {
    if (!d) return;
    const char *detail = ctx ? vpx_codec_error_detail(ctx) : NULL;
    snprintf(d->error, sizeof d->error, "%s%s%s", what,
             detail && *detail ? ": " : "", detail && *detail ? detail : "");
}

static int plane_size(uint32_t width, uint32_t height, size_t *out) {
    if (height && width > SIZE_MAX / height) return 0;
    *out = (size_t)width * height;
    return 1;
}

static NxCodecImage *copy_image(const vpx_image_t *src, int64_t timestamp) {
    if (!src || !src->d_w || !src->d_h) return NULL;
    if (src->d_w > 16384 || src->d_h > 16384 || src->fmt != VPX_IMG_FMT_I420 ||
        !src->planes[VPX_PLANE_Y] || !src->planes[VPX_PLANE_U] || !src->planes[VPX_PLANE_V] ||
        src->stride[VPX_PLANE_Y] < (int)src->d_w ||
        src->stride[VPX_PLANE_U] < (int)((src->d_w + 1u) / 2u) ||
        src->stride[VPX_PLANE_V] < (int)((src->d_w + 1u) / 2u))
        return NULL;
    uint32_t cw = src->d_w / 2u + src->d_w % 2u;
    uint32_t ch = src->d_h / 2u + src->d_h % 2u;
    size_t y_size, c_size;
    if (!plane_size(src->d_w, src->d_h, &y_size) ||
        !plane_size(cw, ch, &c_size) || c_size > (SIZE_MAX - y_size) / 2)
        return NULL;
    size_t total = y_size + 2 * c_size;
    if (total > NX_VPX_MAX_IMAGE_BYTES) return NULL;
    NxCodecImage *out = calloc(1, sizeof *out);
    if (!out) return NULL;
    out->storage = malloc(total);
    if (!out->storage) {
        free(out);
        return NULL;
    }
    out->width = src->d_w;
    out->height = src->d_h;
    out->y_stride = src->d_w;
    out->u_stride = cw;
    out->v_stride = cw;
    out->y = out->storage;
    out->u = out->y + y_size;
    out->v = out->u + c_size;
    out->color_space = (uint32_t)src->cs;
    out->color_range = (uint32_t)src->range;
    out->timestamp = timestamp;

    for (uint32_t row = 0; row < src->d_h; row++)
        memcpy(out->y + (size_t)row * out->y_stride,
               src->planes[VPX_PLANE_Y] + (size_t)row * src->stride[VPX_PLANE_Y], src->d_w);
    for (uint32_t row = 0; row < ch; row++) {
        memcpy(out->u + (size_t)row * out->u_stride,
               src->planes[VPX_PLANE_U] + (size_t)row * src->stride[VPX_PLANE_U], cw);
        memcpy(out->v + (size_t)row * out->v_stride,
               src->planes[VPX_PLANE_V] + (size_t)row * src->stride[VPX_PLANE_V], cw);
    }
    return out;
}

NxVpxDecoder *nx_vp9_decoder_create(uint32_t width, uint32_t height, unsigned threads) {
    // Deliberate HLE resource limit (not a claim about the VP9 bitstream spec).
    if (!width || !height || width > 16384 || height > 16384 || threads > 64) return NULL;
    NxVpxDecoder *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    vpx_codec_dec_cfg_t cfg = {
        .threads = threads ? threads : 1,
        .w = width,
        .h = height,
    };
    vpx_codec_err_t rc = vpx_codec_dec_init(&d->codec, vpx_codec_vp9_dx(), &cfg, 0);
    if (rc != VPX_CODEC_OK) {
        set_error(d, vpx_codec_err_to_string(rc), &d->codec);
        free(d);
        return NULL;
    }
    d->initialized = 1;
    return d;
}

int nx_vp9_decoder_decode(NxVpxDecoder *d, const void *packet, size_t packet_size,
                          int64_t timestamp, NxCodecImage **images, size_t image_capacity) {
    if (!d || !d->initialized || (!packet && packet_size) || (!images && image_capacity))
        return -1;
    if (packet_size > UINT_MAX) {
        set_error(d, "VP9 packet is too large", NULL);
        return -1;
    }
    for (size_t i = 0; i < image_capacity; i++) images[i] = NULL;
    d->error[0] = 0;
    // An empty packet is an end-of-stream flush. libvpx only accepts a flush
    // as (NULL, 0); a non-NULL pointer with size 0 is VPX_CODEC_INVALID_PARAM,
    // and guests routinely pass their (valid) buffer pointer with size 0.
    // Frames still held by libvpx after the flush are returned below.
    if (!packet_size) packet = NULL;
    vpx_codec_err_t rc = vpx_codec_decode(&d->codec, packet, (unsigned)packet_size, NULL, 0);
    if (rc != VPX_CODEC_OK) {
        set_error(d, vpx_codec_error(&d->codec), &d->codec);
        return -1;
    }

    size_t count = 0;
    vpx_codec_iter_t iter = NULL;
    const vpx_image_t *frame;
    while ((frame = vpx_codec_get_frame(&d->codec, &iter))) {
        if (count == image_capacity) {
            set_error(d, "decoded more frames than the caller's capacity", NULL);
            for (size_t i = 0; i < count; i++) {
                nx_codec_image_free(images[i]);
                images[i] = NULL;
            }
            return -1;
        }
        images[count] = copy_image(frame, timestamp);
        if (!images[count]) {
            set_error(d, "could not copy decoded VP9 frame", NULL);
            for (size_t i = 0; i < count; i++) {
                nx_codec_image_free(images[i]);
                images[i] = NULL;
            }
            return -1;
        }
        count++;
    }
    if (count > INT_MAX) {
        set_error(d, "decoded frame count exceeds API range", NULL);
        for (size_t i = 0; i < count; i++) {
            nx_codec_image_free(images[i]);
            images[i] = NULL;
        }
        return -1;
    }
    return (int)count;
}

const char *nx_vp9_decoder_error(const NxVpxDecoder *d) {
    return d ? d->error : "invalid VP9 decoder";
}

void nx_codec_image_free(NxCodecImage *image) {
    if (!image) return;
    free(image->storage);
    free(image);
}

void nx_vp9_decoder_destroy(NxVpxDecoder *d) {
    if (!d) return;
    if (d->initialized) vpx_codec_destroy(&d->codec);
    free(d);
}
