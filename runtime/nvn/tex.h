// CPU-side texture code: block-linear <-> linear copies, ASTC decode, NVN format -> VkFormat. Vulkan memory and
// the texture objects are in vk_memory.c and vk_texture.c.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "nvn_obj.h"
#include "nvn_tex.h"
#include "vk.h"

// ---- tex_deswizzle.c (MacNativeGames nvn_deswizzle.c, MIT) ----
int nvn_gob_switch2(void);              // 1 con NX_PLATAFORMA=switch2: GOB de Switch 2 (bits 5 y 6 cambiados)
uint32_t nvn_gob_swap56(uint32_t o);    // intercambia los bits 5 y 6 de un offset dentro del GOB
// Block-linear <-> linear for one level and layer of a texture. The linear side is laid out by block rows: dst_pitch
// bytes per row (>= row bytes of the level), compressed formats one block row per row, 3D textures slice k at
// dst + k * dst_pitch * block rows. LINEAR / LINEAR_RENDER_TARGET storage is copied by pitch. Multisampled and BUFFER
// textures return -1; success returns 0.
int nvn_deswizzle(const nvn_texdesc *d, unsigned level, unsigned layer, const void *src_storage, void *dst_linear,
                  size_t dst_pitch);
int nvn_swizzle(const nvn_texdesc *d, unsigned level, unsigned layer, const void *src_linear, size_t src_pitch,
                void *dst_storage);
// One box of a level and layer, [x, x + wb) bytes by [y, y + h) block rows of slice z (3D textures), between the storage
// and caller rows of pitch bytes, touching nothing else of the level: ReadTexels / WriteTexels of a region cost the
// region, not the level. Same textures as nvn_deswizzle; -1 for others and for a box outside the level.
int nvn_texels_box(const nvn_texdesc *d, unsigned level, unsigned layer, uint32_t x, uint32_t y, uint32_t z, uint32_t wb,
                   uint32_t h, void *storage, void *rows, size_t pitch, int to_linear);
// Byte offset of (x byte, y block row, z slice) inside one level of block height 2^th and block depth 2^td GOBs.
uint64_t nvn_bl_offset(uint32_t x, uint32_t y, uint32_t z, uint32_t row_bytes, uint32_t rows, unsigned th, unsigned td);
// Geometry of a level as the two functions above use it; -1 for textures they reject.
int nvn_deswizzle_geom(const nvn_texdesc *d, unsigned level, unsigned layer, uint32_t *row_bytes, uint32_t *rows,
                       uint32_t *slices, uint64_t *base, unsigned *th, unsigned *td, uint32_t *bpb);
// The copy loop on its own, for callers that know the level geometry (bl points at the level's first byte).
void tex_bl_copy(uint8_t *bl, uint8_t *lin, size_t pitch, uint32_t row_bytes, uint32_t rows, uint32_t slices,
                 unsigned th, unsigned td, int to_linear);
// Bytes que ocupa un nivel block-linear (bloques enteros): lo que el deswizzle de la GPU sube por nivel y capa.
static inline uint64_t nvn_bl_level_size(uint32_t row_bytes, uint32_t rows, uint32_t slices, unsigned th, unsigned td) {
    uint64_t bh = 1ull << th, bd = 1ull << td;
    return (row_bytes + 63) / 64 * ((rows + 8 * bh - 1) / (8 * bh)) * ((slices + bd - 1) / bd) * (512ull << th << td);
}
// Constantes de runtime/shaders/deswizzle.comp (vk_deswizzle.c y tests/deswizzle_gpu_test.c), en unidades de 16 bytes.
typedef struct {
    uint32_t in_base, out_base, cols, rows, pitch16, gobs_x, blocks_y, th, td, switch2, gx;
} NvnDeswizzlePush;
// Llena las constantes de un nivel (entrada en in_off, salida en out_off con filas de pitch bytes; los tres múltiplos
// de 16 y pitch >= row_bytes redondeado a 16) y da los grupos del despacho: (*gx, *gy, slices).
static inline NvnDeswizzlePush nvn_deswizzle_push(uint64_t in_off, uint64_t out_off, uint32_t pitch, uint32_t row_bytes,
                                                  uint32_t rows, unsigned th, unsigned td, int switch2, uint32_t *gx,
                                                  uint32_t *gy) {
    uint32_t cols = (row_bytes + 15) / 16;
    uint64_t groups = ((uint64_t)cols * rows + 63) / 64;
    *gx = groups < 65535 ? (uint32_t)groups : 65535;
    if (!*gx) *gx = 1;
    *gy = (uint32_t)((groups + *gx - 1) / *gx);
    if (!*gy) *gy = 1;
    return (NvnDeswizzlePush){.in_base = (uint32_t)(in_off / 16), .out_base = (uint32_t)(out_off / 16), .cols = cols,
                              .rows = rows, .pitch16 = pitch / 16, .gobs_x = (row_bytes + 63) / 64,
                              .blocks_y = (rows + (8u << th) - 1) / (8u << th), .th = th, .td = td,
                              .switch2 = (uint32_t)(switch2 != 0), .gx = *gx};
}

// ---- tex_astc.c (LAS Recomp runtime/nvn/astc.c, MIT) ----
// ASTC LDR 2D to RGBA8, bit-exact with astcenc's 8-bit decode. blocks: row-major 16-byte blocks covering
// ceil(width/bw) x ceil(height/bh); rgba_out: width*height*4 bytes. srgb only selects the sRGB endpoint expansion.
// Error blocks and HDR content decode to magenta.
void astc_decode_ldr(const uint8_t *blocks, uint32_t width, uint32_t height, uint32_t bw, uint32_t bh, bool srgb,
                     uint8_t *rgba_out);
bool astc_decode_block(const uint8_t block[16], uint32_t bw, uint32_t bh, bool srgb, uint8_t *rgba_out);
// tex_astc_mt.c: the same bytes as astc_decode_ldr, large images split by block rows over a worker pool; blocks.
void astc_decode_ldr_mt(const uint8_t *blocks, uint32_t width, uint32_t height, uint32_t bw, uint32_t bh, bool srgb,
                        uint8_t *rgba_out);

// ---- vk_formats.c (LAS Recomp runtime/nvn/vulkan/textures.c format table, MIT) ----
typedef struct TexVkFormat {
    VkFormat vk;                  // format of the image created on this GPU
    VkFormat ideal;               // format the NVN format maps to by itself; differs from vk after a fallback
    VkFormatFeatureFlags need;    // optimal-tiling features vk must report for sampling and upload
    uint8_t expand_rgb;           // 3-component format: the upload expands each texel to 4 components (alpha / integer 1)
    uint8_t alpha_one;            // "X" format: sample alpha as 1 (view swizzle)
    uint8_t chan[4];              // nonzero chan[0]: vk stands in with another component order; chan[i] is the
                                  // NVN_TEXTURE_SWIZZLE_R..A selector of the image component holding R, G, B, A
    uint8_t astc_bw, astc_bh;     // nonzero: ASTC decoded into vk (RGBA8, same colour space) by vk_astc.c or tex_astc.c;
                                  // zero for native ASTC (vk == ideal, raw blocks as for BC)
    uint8_t astc_srgb;
    uint8_t astc_bc;              // ASTC decoded and then encoded to BC3 (vk BC3, TOTK_ASTC_BC; vk_astc.c)
    uint8_t depth, stencil;       // aspect flags of depth / stencil formats
    const char *note;             // why a fallback or an expansion was chosen; NULL when the mapping is direct
    const char *reason;           // unmapped formats only: why there is no Vulkan format
} TexVkFormat;

// 1 when nvn_format (an NVNformat value) has a Vulkan mapping, 0 otherwise (out->reason says why). The mapping is
// fixed per device: ASTC decodes to RGBA8 unless vk_astc_native (TOTK_ASTC) takes the format as it is, D24S8 is
// D32S8, DEPTH24 is D32. Whether the GPU reports the features is a separate question (tex_vk_format_supported).
int tex_vk_format(int nvn_format, TexVkFormat *out);
// 1 when vk_ctx()->phys reports every bit of f->need for f->vk in optimal tiling. Needs vk_init().
int tex_vk_format_supported(const TexVkFormat *f);
