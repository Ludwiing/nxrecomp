// Deswizzle de texturas en la GPU: el reacomodo block-linear -> lineal de nvn_deswizzle (tex_deswizzle.c) hecho por
// un shader de cómputo (runtime/shaders/deswizzle.comp), con el GOB de Switch 1 o el de Switch 2 (nvn_gob_switch2).
//
// TOTK_DESWIZZLE = auto (por defecto): la GPU cuando el pipeline se pudo crear, si no el procesador; en auto las partes
// de una subida de menos de AUTO_MIN bytes siguen en el procesador (crear el buffer de trabajo y despachar cuesta más
// que copiar unos pocos KB). gpu: la GPU para toda parte que el shader cubra. cpu: nvn_deswizzle, como antes.
//
// Se graba en el lote de subida (vk_memory.c) como el decodificador ASTC (vk_astc.c): vk_texture.c pone en un buffer
// de staging los bytes block-linear de cada nivel y capa (bloques enteros, nvn_bl_level_size), vk_deswizzle_record
// los copia a un buffer de trabajo del dispositivo, los despachos escriben las filas lineales detrás y la copia a la
// imagen (o el decodificador ASTC) las lee de ahí. El buffer de trabajo se libera cuando el lote termina
// (vk_defer_buffer). Lo que el shader no cubre (texturas lineales, multisample, formatos de 3 componentes que se
// expanden, ASTC decodificado en el procesador, TOTK_ASTC=check) lo sigue haciendo nvn_deswizzle; ver upload en
// vk_texture.c.
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "deswizzle_spv.h"
#include "host.h"
#include "tex.h"
#include "vk.h"
#include "platform/platform.h"

#define AUTO_MIN (16u << 10)
// Tope de un llamado (entrada + salida): más que esto sigue en el procesador antes que pedir un buffer de trabajo tan
// grande. Las partes de vk_texture.c son de hasta 32 MiB salvo ASTC decodificado en la GPU (todas las regiones juntas).
#define MAX_BYTES (512ull << 20)

static PlatMutex g_lock = PLAT_MUTEX_INIT;
static struct {
    VkDevice dev;   // el dispositivo de los objetos de abajo
    int mode;       // VK_DESWIZZLE_*; -1 antes de decidir
    VkDescriptorSetLayout dsl;
    VkPipelineLayout layout;
    VkPipeline pipe;
    PFN_vkCmdPushDescriptorSetKHR push;
} G = {.mode = -1};
static uint64_t n_calls, n_regions, n_bytes;

static int create_pipeline(VkCtx *c) {
    G.push = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(c->device, "vkCmdPushDescriptorSetKHR");
    if (!G.push) return 0;
    VkDescriptorSetLayoutBinding b[2];
    for (int i = 0; i < 2; i++)
        b[i] = (VkDescriptorSetLayoutBinding){.binding = (uint32_t)i, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                           .bindingCount = 2, .pBindings = b};
    if (vkCreateDescriptorSetLayout(c->device, &dci, NULL, &G.dsl) != VK_SUCCESS) return 0;
    VkPushConstantRange pr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(NvnDeswizzlePush)};
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &G.dsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pr};
    if (vkCreatePipelineLayout(c->device, &li, NULL, &G.layout) != VK_SUCCESS) return 0;
    VkShaderModuleCreateInfo mi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sizeof deswizzle_spv,
                                   .pCode = deswizzle_spv};
    VkShaderModule m;
    if (vkCreateShaderModule(c->device, &mi, NULL, &m) != VK_SUCCESS) return 0;
    VkComputePipelineCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = m, .pName = "main"},
                                      .layout = G.layout};
    VkResult r = vkCreateComputePipelines(c->device, VK_NULL_HANDLE, 1, &ci, NULL, &G.pipe);
    vkDestroyShaderModule(c->device, m, NULL);
    if (r != VK_SUCCESS) G.pipe = VK_NULL_HANDLE;
    return r == VK_SUCCESS;
}

static void destroy_objects(VkCtx *c) {   // g_lock tomado
    if (c && G.dev == c->device) {
        if (G.pipe) vkDestroyPipeline(c->device, G.pipe, NULL);
        if (G.layout) vkDestroyPipelineLayout(c->device, G.layout, NULL);
        if (G.dsl) vkDestroyDescriptorSetLayout(c->device, G.dsl, NULL);
    }
    G.pipe = VK_NULL_HANDLE, G.layout = VK_NULL_HANDLE, G.dsl = VK_NULL_HANDLE, G.push = NULL;
}

int vk_deswizzle_mode(void) {
    VkCtx *c = vk_ctx();
    if (!c) return VK_DESWIZZLE_CPU;
    plat_mutex_lock_shared(&g_lock);
    int m = G.dev == c->device ? G.mode : -1;
    plat_mutex_unlock_shared(&g_lock);
    if (m >= 0) return m;
    plat_mutex_lock(&g_lock);
    if (G.dev != c->device) {   // otro dispositivo (las sondas lo recrean): sus objetos se fueron con él
        G.pipe = VK_NULL_HANDLE, G.layout = VK_NULL_HANDLE, G.dsl = VK_NULL_HANDLE, G.push = NULL;
        G.dev = c->device, G.mode = -1;
    }
    if (G.mode < 0) {
        const char *e = cfg_value("TOTK_DESWIZZLE");
        int want = !strcmp(e, "auto") ? VK_DESWIZZLE_AUTO : !strcmp(e, "gpu") ? VK_DESWIZZLE_GPU
                 : !strcmp(e, "cpu") ? VK_DESWIZZLE_CPU : -1;
        if (want < 0) rt_log("vk: TOTK_DESWIZZLE=%s no es auto, gpu ni cpu: auto", e), want = VK_DESWIZZLE_AUTO;
        m = want;
        if (m != VK_DESWIZZLE_CPU) {
            uint64_t t0 = plat_now_ns();
            if (create_pipeline(c)) {
                rt_log("vk: deswizzle de texturas en la GPU (%s, GOB de Switch %d; pipeline en %.1f ms)",
                       m == VK_DESWIZZLE_GPU ? "gpu" : "auto", nvn_gob_switch2() ? 2 : 1,
                       (double)(plat_now_ns() - t0) / 1e6);
            } else {
                destroy_objects(c);
                rt_log("vk: no se pudo crear el pipeline de deswizzle en %s: las texturas se reacomodan en el procesador",
                       c->props.deviceName);
                m = VK_DESWIZZLE_CPU;
            }
        } else {
            rt_log("vk: TOTK_DESWIZZLE=cpu: las texturas se reacomodan en el procesador");
        }
        G.mode = m;
    }
    m = G.mode;
    plat_mutex_unlock(&g_lock);
    return m;
}

int vk_deswizzle_wants(VkDeviceSize linear_bytes) {
    int m = vk_deswizzle_mode();
    return m == VK_DESWIZZLE_GPU || (m == VK_DESWIZZLE_AUTO && linear_bytes >= AUTO_MIN);
}

static void mem_barrier(VkCommandBuffer cb, VkPipelineStageFlags2 src, VkAccessFlags2 sa, VkPipelineStageFlags2 dst,
                        VkAccessFlags2 da) {
    VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .srcStageMask = src, .srcAccessMask = sa,
                           .dstStageMask = dst, .dstAccessMask = da};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &mb};
    vkCmdPipelineBarrier2(cb, &di);
}

int vk_deswizzle_fits(const VkDeswizzleRegion *rg, int n, VkDeviceSize in_size, VkDeviceSize out_size) {
    VkCtx *c = vk_ctx();
    if (!c || in_size + out_size > MAX_BYTES) return 0;
    const VkPhysicalDeviceLimits *l = &c->props.limits;
    if (in_size > l->maxStorageBufferRange || out_size > l->maxStorageBufferRange) return 0;
    for (int i = 0; i < n; i++) {
        const VkDeswizzleRegion *r = &rg[i];
        uint32_t gx, gy;
        if ((r->src | r->dst | r->pitch) & 15 || r->pitch < ((r->row_bytes + 15) & ~15u) || r->th > 5 || r->td > 5)
            return 0;
        nvn_deswizzle_push(r->src, r->dst, r->pitch, r->row_bytes, r->rows, r->th, r->td, 0, &gx, &gy);
        if (gx > l->maxComputeWorkGroupCount[0] || gy > l->maxComputeWorkGroupCount[1] ||
            r->slices > l->maxComputeWorkGroupCount[2])
            return 0;
    }
    return 1;
}

int vk_deswizzle_record(VkCommandBuffer cb, VkBuffer src, VkDeviceSize src_size, const VkDeswizzleRegion *rg, int n,
                        VkDeviceSize out_size, VkBuffer *out, VkDeviceSize *out_off) {
    static volatile int32_t n_fail;
    VkCtx *c = vk_ctx();
    if (!c || !n || vk_deswizzle_mode() == VK_DESWIZZLE_CPU || !vk_deswizzle_fits(rg, n, src_size, out_size)) return 0;
    plat_mutex_lock_shared(&g_lock);
    VkPipeline pipe = G.dev == c->device ? G.pipe : VK_NULL_HANDLE;
    VkPipelineLayout layout = G.layout;
    PFN_vkCmdPushDescriptorSetKHR push = G.push;
    plat_mutex_unlock_shared(&g_lock);
    if (!pipe) return 0;
    // buffer de trabajo: los bytes block-linear, y detrás (alineadas para el binding) las filas lineales
    VkDeviceSize al = c->props.limits.minStorageBufferOffsetAlignment;
    if (al < 16) al = 16;
    VkDeviceSize cap_in = (src_size + al - 1) / al * al, cap_out = (out_size + 15) & ~15ull;
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = cap_in + cap_out,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo aci = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkBuffer sb;
    VmaAllocation sa;
    if (vmaCreateBuffer(c->vma, &bi, &aci, &sb, &sa, NULL) != VK_SUCCESS) {
        if (plat_inc32(&n_fail) <= 16)
            rt_log("vk: deswizzle: el buffer de trabajo de %llu bytes falló: esta subida se reacomoda en el procesador",
                   (unsigned long long)(cap_in + cap_out));
        return 0;
    }
    VkBufferCopy cp = {0, 0, src_size};
    vkCmdCopyBuffer(cb, src, sb, 1, &cp);
    mem_barrier(cb, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    VkDescriptorBufferInfo dbi[2] = {{sb, 0, cap_in}, {sb, cap_in, cap_out}};
    VkWriteDescriptorSet w[2];
    for (int i = 0; i < 2; i++)
        w[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = (uint32_t)i,
                                      .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      .pBufferInfo = &dbi[i]};
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    rec_push(push, cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 2, w);
    int s2 = nvn_gob_switch2();
    for (int i = 0; i < n; i++) {
        const VkDeswizzleRegion *r = &rg[i];
        uint32_t gx, gy;
        NvnDeswizzlePush pc = nvn_deswizzle_push(r->src, r->dst, r->pitch, r->row_bytes, r->rows, r->th, r->td, s2, &gx, &gy);
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc);
        vkCmdDispatch(cb, gx, gy, r->slices);
        n_bytes += (uint64_t)r->row_bytes * r->rows * r->slices;
    }
    mem_barrier(cb, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    if (!n_calls || (n_regions + (uint64_t)n) / 4096 != n_regions / 4096)
        rt_log("vk: deswizzle en la GPU: %llu subidas, %llu niveles y capas, %.1f MB reacomodados hasta ahora",
               (unsigned long long)(n_calls + 1), (unsigned long long)(n_regions + (uint64_t)n),
               (double)n_bytes / 1e6);
    n_calls++, n_regions += (uint64_t)n;
    vk_defer_buffer(sb, sa);   // vive hasta que el lote abierto, que lo usa, termine
    *out = sb;
    *out_off = cap_in;
    return 1;
}

void vk_deswizzle_release(void) {
    VkCtx *c = vk_ctx();
    plat_mutex_lock(&g_lock);
    destroy_objects(c);
    G.dev = VK_NULL_HANDLE;
    G.mode = -1;
    plat_mutex_unlock(&g_lock);
}
