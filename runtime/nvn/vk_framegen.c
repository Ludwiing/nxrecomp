// Generación de cuadros en la presentación (TOTK_FRAMEGEN=on): cuando el juego va lento (un cuadro cada 25 a 100 ms,
// p. ej. 30 fps), entre dos cuadros reales se muestra uno intermedio armado en la GPU, y el movimiento se ve a 60.
// Escrito para nxrecomp (MIT). La idea es la de la generación por flujo óptico de los ports nativos: luma a un cuarto,
// búsqueda por bloques, mediana de los vectores y mezcla de los dos cuadros desplazados (runtime/shaders/fg_*.comp).
//
// Por cuadro del juego (vk_backend.c present_main): la presentación intermedia graba vk_framegen_store (guarda el
// recorte del cuadro nuevo y su luma en la historia) y vk_framegen_interp (vectores contra el cuadro anterior y el
// intermedio en t = 0,5); la presentación real del mismo cuadro ya encuentra la historia guardada y no repite nada. Todo
// va en el command buffer de la presentación, en la cola de gráficos, sin envíos aparte.
//
// Imágenes (todas RGBA16F en GENERAL): historia de color [2] y de luma [2] (cuarto de resolución), vectores [2] (uno por
// bloque de 16x16 píxeles; el segundo es la mediana) y la salida. Son de todas las ranuras: cada barrera ordena después
// de lo anterior en la misma cola. Un cambio de tamaño del recorte las rehace y vacía la historia.
#include "vk.h"

#include <string.h>

#include "cfg.h"
#include "fg_interp_spv.h"
#include "fg_luma_spv.h"
#include "fg_median_spv.h"
#include "fg_motion_spv.h"
#include "fg_store_spv.h"
#include "host.h"

#define SLOTS 8
#define RADIO 8          // búsqueda: ±8 píxeles de luma = ±32 reales por cuadro
#define CASTIGO 0.02f    // costo extra por píxel de luma de largo del vector
#define LADO 4           // bloque de la búsqueda, en luma: 4x4 = 16x16 píxeles reales

enum { P_STORE, P_LUMA, P_MOTION, P_MEDIAN, P_INTERP, P_N };
enum { I_HIST0, I_HIST1, I_LUMA0, I_LUMA1, I_MV0, I_MV1, I_OUT, I_N };

static struct {
    int tried, ok, logged;
    PFN_vkCmdPushDescriptorSetKHR push;
    VkDescriptorSetLayout dsl;
    VkPipelineLayout layout;
    VkPipeline pipe[P_N];
    VkSampler lineal;
    VkImage img[I_N];
    VmaAllocation alloc[I_N];
    VkImageView view[I_N];
    int w, h, lw, lh, bw, bh;     // recorte, luma, bloques
    int k;                        // la historia más nueva es hist[k]
    int validos;                  // cuadros guardados desde el último vaciado (0, 1 o 2)
    uint64_t guardado;            // el cuadro del juego que ya está en hist[k]
    VkImageView src_view[SLOTS];
} G;

int vk_framegen_on(void) { return !strcmp(cfg_value("TOTK_FRAMEGEN"), "on"); }

static VkPipeline make_pipeline(VkCtx *c, const uint32_t *code, size_t size) {
    VkShaderModuleCreateInfo mi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = code};
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(c->device, &mi, NULL, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = m, .pName = "main"},
                                      .layout = G.layout};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(c->device, VK_NULL_HANDLE, 1, &ci, NULL, &p) != VK_SUCCESS) p = VK_NULL_HANDLE;
    vkDestroyShaderModule(c->device, m, NULL);
    return p;
}

static int init(VkCtx *c) {
    G.tried = 1;
    G.push = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(c->device, "vkCmdPushDescriptorSetKHR");
    if (!G.push) {
        rt_log("vk: generación de cuadros apagada: falta VK_KHR_push_descriptor");
        return 0;
    }
    VkDescriptorSetLayoutBinding b[4];
    for (int i = 0; i < 4; i++)
        b[i] = (VkDescriptorSetLayoutBinding){.binding = (uint32_t)i,
                                              .descriptorType = i < 3 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                                                      : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                           .bindingCount = 4, .pBindings = b};
    VkPushConstantRange pr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 32};
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &G.dsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pr};
    VkSamplerCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR,
                              .minFilter = VK_FILTER_LINEAR, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                              .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .maxLod = 0};
    if (vkCreateDescriptorSetLayout(c->device, &dci, NULL, &G.dsl) != VK_SUCCESS ||
        vkCreatePipelineLayout(c->device, &li, NULL, &G.layout) != VK_SUCCESS ||
        vkCreateSampler(c->device, &si, NULL, &G.lineal) != VK_SUCCESS ||
        !(G.pipe[P_STORE] = make_pipeline(c, fg_store_spv, sizeof fg_store_spv)) ||
        !(G.pipe[P_LUMA] = make_pipeline(c, fg_luma_spv, sizeof fg_luma_spv)) ||
        !(G.pipe[P_MOTION] = make_pipeline(c, fg_motion_spv, sizeof fg_motion_spv)) ||
        !(G.pipe[P_MEDIAN] = make_pipeline(c, fg_median_spv, sizeof fg_median_spv)) ||
        !(G.pipe[P_INTERP] = make_pipeline(c, fg_interp_spv, sizeof fg_interp_spv))) {
        rt_log("vk: generación de cuadros apagada: no se pudieron crear sus pipelines");
        return 0;
    }
    rt_log("vk: generación de cuadros lista (flujo óptico por bloques)");
    return G.ok = 1;
}

static void free_images(VkCtx *c) {
    for (int i = 0; i < I_N; i++) {
        if (G.view[i]) vkDestroyImageView(c->device, G.view[i], NULL);
        if (G.img[i]) vmaDestroyImage(c->vma, G.img[i], G.alloc[i]);
        G.view[i] = VK_NULL_HANDLE;
        G.img[i] = VK_NULL_HANDLE;
    }
    G.w = G.h = 0;
    G.validos = 0;
}

static int ensure_images(VkCtx *c, int w, int h) {
    if (G.img[0] && G.w == w && G.h == h) return 1;
    vk_queue_lock();   // las presentaciones anteriores pueden seguir leyendo las imágenes viejas
    vkDeviceWaitIdle(c->device);
    vk_queue_unlock();
    free_images(c);
    int lw = (w + 3) / 4, lh = (h + 3) / 4, bw = (lw + LADO - 1) / LADO, bh = (lh + LADO - 1) / LADO;
    const int tam[I_N][2] = {{w, h}, {w, h}, {lw, lh}, {lw, lh}, {bw, bh}, {bw, bh}, {w, h}};
    for (int i = 0; i < I_N; i++) {
        VkImageCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
                                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                .extent = {(uint32_t)tam[i][0], (uint32_t)tam[i][1], 1}, .mipLevels = 1,
                                .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
                                .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        if (vmaCreateImage(c->vma, &ci, &ai, &G.img[i], &G.alloc[i], NULL) != VK_SUCCESS) {
            G.img[i] = VK_NULL_HANDLE;
            free_images(c);
            return 0;
        }
        VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = G.img[i],
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        if (vkCreateImageView(c->device, &vi, NULL, &G.view[i]) != VK_SUCCESS) {
            G.view[i] = VK_NULL_HANDLE;
            free_images(c);
            return 0;
        }
    }
    G.w = w, G.h = h, G.lw = lw, G.lh = lh, G.bw = bw, G.bh = bh;
    return 1;
}

static void barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkPipelineStageFlags2 ss, VkAccessFlags2 sa,
                    VkPipelineStageFlags2 ds, VkAccessFlags2 da) {
    VkImageMemoryBarrier2 b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .srcStageMask = ss, .srcAccessMask = sa,
                               .dstStageMask = ds, .dstAccessMask = da, .oldLayout = from,
                               .newLayout = VK_IMAGE_LAYOUT_GENERAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = img,
                               .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkDependencyInfo d = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1,
                          .pImageMemoryBarriers = &b};
    vkCmdPipelineBarrier2(cb, &d);
}

#define CS VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
#define W VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
#define R VK_ACCESS_2_SHADER_SAMPLED_READ_BIT

// Una imagen que el paso siguiente reescribe entera: se descarta lo anterior y se espera a quien la leyó.
static void para_escribir(VkCommandBuffer cb, int i) {
    barrier(cb, G.img[i], VK_IMAGE_LAYOUT_UNDEFINED, CS | VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, CS, W);
}
static void escrita(VkCommandBuffer cb, int i) { barrier(cb, G.img[i], VK_IMAGE_LAYOUT_GENERAL, CS, W, CS, R); }

// in[3]: vistas para las ligaduras 0..2 (VK_NULL_HANDLE = sin usar), out: la ligadura 3.
static void paso(VkCommandBuffer cb, int p, const VkImageView in[3], VkImageView out, const float pc[8], int gx, int gy) {
    VkDescriptorImageInfo ii[3], oi = {VK_NULL_HANDLE, out, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet w[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        if (!in[i]) continue;
        ii[i] = (VkDescriptorImageInfo){G.lineal, in[i], VK_IMAGE_LAYOUT_GENERAL};
        w[n++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = (uint32_t)i,
                                        .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .pImageInfo = &ii[i]};
    }
    w[n++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 3, .descriptorCount = 1,
                                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &oi};
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, G.pipe[p]);
    rec_push(G.push, cb, VK_PIPELINE_BIND_POINT_COMPUTE, G.layout, 0, (uint32_t)n, w);
    vkCmdPushConstants(cb, G.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 32, pc);
    vkCmdDispatch(cb, (uint32_t)gx, (uint32_t)gy, 1);
}

void vk_framegen_reset(void) { G.validos = 0; }

int vk_framegen_store(VkCommandBuffer cb, unsigned slot, VkImage src, VkFormat fmt, int sw, int sh, const int rect[4],
                      uint64_t frame) {
    if (!vk_framegen_on() || !fmt) return 0;
    VkCtx *c = vk_ctx();
    if (!G.tried) init(c);
    if (!G.ok || !ensure_images(c, rect[2], rect[3])) return 0;
    if (G.validos && G.guardado == frame) return 1;   // la presentación intermedia de este cuadro ya lo guardó
    slot %= SLOTS;
    if (G.src_view[slot]) vkDestroyImageView(c->device, G.src_view[slot], NULL);
    VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = src,
                                .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmt,
                                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_ONE},
                                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    if (vkCreateImageView(c->device, &vi, NULL, &G.src_view[slot]) != VK_SUCCESS) {
        G.src_view[slot] = VK_NULL_HANDLE;
        return 0;
    }
    if (!G.logged) {
        G.logged = 1;
        rt_log("vk: generación de cuadros sobre %dx%d (vectores %dx%d)", G.w, G.h, G.bw, G.bh);
    }
    int k = G.validos ? 1 - G.k : G.k;
    // lo que el juego escribió en la fuente, visible para la lectura del cómputo
    barrier(cb, src, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, CS, R);
    para_escribir(cb, I_HIST0 + k);
    const VkImageView s1[3] = {G.src_view[slot]};
    float pc[8] = {(float)rect[0], (float)rect[1], (float)G.w, (float)G.h, (float)sw, (float)sh, 0, 0};
    paso(cb, P_STORE, s1, G.view[I_HIST0 + k], pc, (G.w + 15) / 16, (G.h + 15) / 16);
    // los envíos siguientes que escriben la fuente esperan esta lectura
    barrier(cb, src, VK_IMAGE_LAYOUT_GENERAL, CS, 0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
    escrita(cb, I_HIST0 + k);
    para_escribir(cb, I_LUMA0 + k);
    const VkImageView s2[3] = {G.view[I_HIST0 + k]};
    float pl[8] = {(float)G.lw, (float)G.lh};
    paso(cb, P_LUMA, s2, G.view[I_LUMA0 + k], pl, (G.lw + 15) / 16, (G.lh + 15) / 16);
    escrita(cb, I_LUMA0 + k);
    G.k = k;
    G.guardado = frame;
    if (G.validos < 2) G.validos++;
    return 1;
}

VkImage vk_framegen_interp(VkCommandBuffer cb, float t) {
    if (!G.ok || G.validos < 2) return VK_NULL_HANDLE;
    int k = G.k, a = 1 - k;
    para_escribir(cb, I_MV0);
    const VkImageView m[3] = {G.view[I_LUMA0 + k], G.view[I_LUMA0 + a]};
    float pm[8] = {(float)G.bw, (float)G.bh, (float)G.lw, (float)G.lh, (float)RADIO, CASTIGO, (float)LADO};
    paso(cb, P_MOTION, m, G.view[I_MV0], pm, (G.bw + 7) / 8, (G.bh + 7) / 8);
    escrita(cb, I_MV0);
    para_escribir(cb, I_MV1);
    const VkImageView md[3] = {G.view[I_MV0]};
    float pd[8] = {(float)G.bw, (float)G.bh};
    paso(cb, P_MEDIAN, md, G.view[I_MV1], pd, (G.bw + 7) / 8, (G.bh + 7) / 8);
    escrita(cb, I_MV1);
    para_escribir(cb, I_OUT);
    const VkImageView in[3] = {G.view[I_HIST0 + a], G.view[I_HIST0 + k], G.view[I_MV1]};
    float pi[8] = {(float)G.w, (float)G.h, t, 4.0f, (float)LADO};
    paso(cb, P_INTERP, in, G.view[I_OUT], pi, (G.w + 15) / 16, (G.h + 15) / 16);
    // la salida la leen el ajuste de color o FSR (cómputo) o la copia final (transferencia)
    barrier(cb, G.img[I_OUT], VK_IMAGE_LAYOUT_GENERAL, CS, W, CS | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            R | VK_ACCESS_2_TRANSFER_READ_BIT);
    return G.img[I_OUT];
}

void vk_framegen_release(void) {
    VkCtx *c = vk_ctx();
    if (!c || !G.tried) return;
    free_images(c);
    for (int i = 0; i < SLOTS; i++)
        if (G.src_view[i]) vkDestroyImageView(c->device, G.src_view[i], NULL);
    for (int i = 0; i < P_N; i++)
        if (G.pipe[i]) vkDestroyPipeline(c->device, G.pipe[i], NULL);
    if (G.lineal) vkDestroySampler(c->device, G.lineal, NULL);
    if (G.layout) vkDestroyPipelineLayout(c->device, G.layout, NULL);
    if (G.dsl) vkDestroyDescriptorSetLayout(c->device, G.dsl, NULL);
    memset(&G, 0, sizeof G);
}
