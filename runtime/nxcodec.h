// Backend host para los codecs de video usados por el HLE de nn::codec.
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct NxVpxDecoder NxVpxDecoder;

typedef struct NxCodecImage {
    uint32_t width, height;
    size_t y_stride, u_stride, v_stride;
    uint8_t *y, *u, *v;
    uint32_t color_space, color_range;
    int64_t timestamp;
    uint8_t *storage;
} NxCodecImage;

// Each decoder is single-owner: create/decode/destroy must be serialized by its caller.
NxVpxDecoder *nx_vp9_decoder_create(uint32_t width, uint32_t height, unsigned threads);
// Returns a non-negative frame count, or -1 on invalid input/allocation/decoder error.
// The caller owns every returned image and frees it with nx_codec_image_free.
// packet_size == 0 (with any packet pointer) flushes the decoder and returns
// whatever frames it still held; it is not an error.
int nx_vp9_decoder_decode(NxVpxDecoder *decoder, const void *packet, size_t packet_size,
                          int64_t timestamp, NxCodecImage **images, size_t image_capacity);
const char *nx_vp9_decoder_error(const NxVpxDecoder *decoder);
void nx_vp9_decoder_destroy(NxVpxDecoder *decoder);
void nx_codec_image_free(NxCodecImage *image);
