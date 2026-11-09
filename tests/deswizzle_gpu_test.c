// Prueba del deswizzle en la GPU (runtime/shaders/deswizzle.comp, el mismo SPIR-V que usa vk_deswizzle.c): para
// geometrías al azar (filas que terminan en un GOB parcial, block height 0..5, depth 0..1, varias capas una detrás de
// otra como en la subida de vk_texture.c) compara byte a byte lo que escribe el shader con tex_bl_copy y con
// nvn_bl_offset del procesador. Corre Switch 1 y Switch 2, cada uno en su proceso (NX_PLATAFORMA se lee una vez).
// Devuelve 77 si no hay Vulkan.
//
//   cc -O1 -Iruntime -Iruntime/nvn -Iruntime/nvn/third_party/vma tests/deswizzle_gpu_test.c runtime/nvn/tex_deswizzle.c \
//      -lvulkan -o build/deswizzle_gpu_test && build/deswizzle_gpu_test
//   DESWIZZLE_CASOS=<n> cambia el número de geometrías por modo (200).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <vulkan/vulkan.h>

#include "deswizzle_spv.h"
#define VK_REC_IMPL   // las llamadas vkCmd* van directo al driver, no al grabador del runtime (vk_rec.h)
#include "tex.h"

// tex_deswizzle.c también usa estas funciones de nvn_tex.c (solo para nvn_deswizzle_geom, que aquí no se usa)
int nvn_fmt_bpb(int f, int *bw, int *bh) { (void)f; *bw = *bh = 1; return 0; }
void nvn_tex_layout(const nvn_texdesc *d, nvn_texlayout *l) { (void)d; memset(l, 0, sizeof *l); }
uint64_t nvn_tex_level_offset(const nvn_texdesc *d, unsigned l) { (void)d; (void)l; return 0; }
void nvn_tex_level_tiles(const nvn_texdesc *d, unsigned l, unsigned *th, unsigned *td) { (void)d; (void)l; *th = *td = 0; }

#define OK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", #x, r_); exit(1); } } while (0)

static VkDevice dev;
static VkPhysicalDevice phys;

static uint32_t tipo_memoria(uint32_t bits, VkMemoryPropertyFlags f) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & f) == f) return i;
    fprintf(stderr, "sin memoria adecuada\n");
    exit(1);
}

typedef struct { VkBuffer buf; VkDeviceMemory mem; uint8_t *p; } Buf;

static Buf bufer(VkDeviceSize n) {
    Buf b;
    VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = n,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    OK(vkCreateBuffer(dev, &ci, NULL, &b.buf));
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(dev, b.buf, &mr);
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
                               .memoryTypeIndex = tipo_memoria(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    OK(vkAllocateMemory(dev, &ai, NULL, &b.mem));
    OK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
    void *p;
    OK(vkMapMemory(dev, b.mem, 0, n, 0, &p));
    b.p = p;
    return b;
}

static void soltar(Buf *b) {
    vkUnmapMemory(dev, b->mem);
    vkDestroyBuffer(dev, b->buf, NULL);
    vkFreeMemory(dev, b->mem, NULL);
}

static uint64_t semilla = 0x9e3779b97f4a7c15ull;
static uint32_t azar(uint32_t n) {   // 0..n-1
    semilla ^= semilla << 13, semilla ^= semilla >> 7, semilla ^= semilla << 17;
    return (uint32_t)(semilla >> 11) % n;
}

static int fallas;
static void ver(const char *n, int c) { printf("%s: %s\n", c ? "OK" : "MAL", n); fallas += !c; }

int main(int argc, char **argv) {
    if (argc < 2) {   // corre los dos modos
        char cmd[512];
        int r1, r2;
        snprintf(cmd, sizeof cmd, "NX_PLATAFORMA=switch1 %s 1", argv[0]);
        r1 = system(cmd);
        if (WIFEXITED(r1) && WEXITSTATUS(r1) == 77) return 77;
        snprintf(cmd, sizeof cmd, "NX_PLATAFORMA=switch2 %s 2", argv[0]);
        r2 = system(cmd);
        return r1 || r2 ? 1 : 0;
    }
    int s2 = argv[1][0] == '2';
    semilla += (uint64_t)s2 * 0x1234567ull;
    const char *nc = getenv("DESWIZZLE_CASOS");
    int casos = nc && atoi(nc) > 0 ? atoi(nc) : 200;

    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_2};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    uint32_t nd = 8;
    VkPhysicalDevice pds[8];
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS || vkEnumeratePhysicalDevices(inst, &nd, pds) < 0 || !nd) {
        fprintf(stderr, "deswizzle_gpu_test: sin Vulkan, no se prueba\n");
        return 77;
    }
    phys = pds[0];
    for (uint32_t i = 0; i < nd; i++) {   // la GPU dedicada si hay
        VkPhysicalDeviceProperties pp;
        vkGetPhysicalDeviceProperties(pds[i], &pp);
        if (pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) phys = pds[i];
    }
    VkPhysicalDeviceProperties pp;
    vkGetPhysicalDeviceProperties(phys, &pp);
    printf("-- Switch %d, GPU: %s\n", s2 ? 2 : 1, pp.deviceName);
    ver("el modo sigue a NX_PLATAFORMA", nvn_gob_switch2() == s2);
    uint32_t nq = 16, fam = 0;
    VkQueueFamilyProperties qf[16];
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
    for (uint32_t i = 0; i < nq; i++)
        if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { fam = i; break; }
    float prio = 1;
    VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = fam,
                                   .queueCount = 1, .pQueuePriorities = &prio};
    const char *ext[] = {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                              .pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = ext};
    OK(vkCreateDevice(phys, &dci, NULL, &dev));
    VkQueue q;
    vkGetDeviceQueue(dev, fam, 0, &q);
    PFN_vkCmdPushDescriptorSetKHR push = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(dev, "vkCmdPushDescriptorSetKHR");
    if (!push) {
        fprintf(stderr, "sin VK_KHR_push_descriptor\n");
        return 1;
    }

    // el mismo pipeline que vk_deswizzle.c: dos storage buffers por push descriptor y las constantes de tex.h
    VkDescriptorSetLayoutBinding b[2];
    for (int i = 0; i < 2; i++)
        b[i] = (VkDescriptorSetLayoutBinding){.binding = (uint32_t)i, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo dsci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                            .bindingCount = 2, .pBindings = b};
    VkDescriptorSetLayout dsl;
    OK(vkCreateDescriptorSetLayout(dev, &dsci, NULL, &dsl));
    VkPushConstantRange pr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(NvnDeswizzlePush)};
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &dsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pr};
    VkPipelineLayout layout;
    OK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
    VkShaderModuleCreateInfo mi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sizeof deswizzle_spv,
                                   .pCode = deswizzle_spv};
    VkShaderModule m;
    OK(vkCreateShaderModule(dev, &mi, NULL, &m));
    VkComputePipelineCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                       .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                 .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = m, .pName = "main"},
                                       .layout = layout};
    VkPipeline pipe;
    OK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, NULL, &pipe));
    vkDestroyShaderModule(dev, m, NULL);
    VkCommandPoolCreateInfo poi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = fam};
    VkCommandPool pool;
    OK(vkCreateCommandPool(dev, &poi, NULL, &pool));
    VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cb;
    OK(vkAllocateCommandBuffers(dev, &cai, &cb));

    static const uint32_t bpbs[] = {1, 2, 4, 8, 16};
    int malos = 0, malos_off = 0, parciales = 0, por_th[6] = {0}, por_td[2] = {0};
    uint64_t bytes = 0;
    for (int caso = 0; caso < casos; caso++) {
        uint32_t bpb = bpbs[azar(5)], w = 1 + azar(caso % 4 ? 300 : 40);   // anchos chicos también: una sola columna
        uint32_t row_bytes = w * bpb, rows = 1 + azar(caso % 3 ? 260 : 20);
        unsigned th = azar(6), td = azar(2);
        uint32_t slices = td ? 1 + azar(9) : 1 + azar(2), layers = 1 + azar(3);
        uint32_t pitch = (row_bytes + 15) & ~15u;
        if (azar(4) == 0) pitch += 16 * azar(4);   // a veces una fila de salida más larga
        parciales += row_bytes % 64 != 0, por_th[th]++, por_td[td]++;
        // las capas una detrás de otra, como las sube vk_texture.c (cada una en un múltiplo de 16)
        uint64_t ls = nvn_bl_level_size(row_bytes, rows, slices, th, td), ins = ls * layers;
        uint64_t outl = (uint64_t)pitch * rows * slices, outs = outl * layers;
        uint64_t in_off0 = 16 * azar(4), out_off0 = 16 * azar(4);   // y no siempre al principio del buffer
        Buf in = bufer(in_off0 + ins), out = bufer(out_off0 + outs);
        // los datos en memoria normal: la del dispositivo mapeada puede ser lenta de leer desde el procesador
        uint8_t *src = malloc(in_off0 + ins), *dst = malloc(out_off0 + outs);
        for (uint64_t i = 0; i < in_off0 + ins; i++) src[i] = (uint8_t)(azar(256));
        memcpy(in.p, src, in_off0 + ins);
        memset(out.p, 0xAB, out_off0 + outs);

        VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                       .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        OK(vkBeginCommandBuffer(cb, &bi));
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        VkDescriptorBufferInfo dbi[2] = {{in.buf, 0, VK_WHOLE_SIZE}, {out.buf, 0, VK_WHOLE_SIZE}};
        VkWriteDescriptorSet wr[2];
        for (int i = 0; i < 2; i++)
            wr[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = (uint32_t)i,
                                           .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           .pBufferInfo = &dbi[i]};
        push(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 2, wr);
        for (uint32_t ly = 0; ly < layers; ly++) {
            uint32_t gx, gy;
            NvnDeswizzlePush pc = nvn_deswizzle_push(in_off0 + ly * ls, out_off0 + ly * outl, pitch, row_bytes, rows, th,
                                                     td, nvn_gob_switch2(), &gx, &gy);
            vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc);
            vkCmdDispatch(cb, gx, gy, slices);
        }
        VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                              .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
                             NULL);
        OK(vkEndCommandBuffer(cb));
        VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
        OK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
        OK(vkQueueWaitIdle(q));
        memcpy(dst, out.p, out_off0 + outs);

        // el procesador: tex_bl_copy (por filas) y nvn_bl_offset (byte por byte), capa por capa
        uint8_t *ref = malloc(outl);
        int bien = 1, bien_off = 1;
        for (uint32_t ly = 0; ly < layers && bien && bien_off; ly++) {
            uint8_t *bl = src + in_off0 + ly * ls, *gpu = dst + out_off0 + ly * outl;
            memset(ref, 0, outl);
            tex_bl_copy(bl, ref, pitch, row_bytes, rows, slices, th, td, 1);
            for (uint32_t z = 0; z < slices && bien; z++)
                for (uint32_t y = 0; y < rows && bien; y++) {
                    size_t f = ((size_t)z * rows + y) * pitch;
                    if (memcmp(gpu + f, ref + f, row_bytes)) {
                        bien = 0;
                        printf("  caso %d capa %u: fila %u corte %u distinta de tex_bl_copy (rb %u rows %u slices %u th %u td %u "
                               "pitch %u)\n", caso, ly, y, z, row_bytes, rows, slices, th, td, pitch);
                    }
                    for (uint32_t x = 0; x < row_bytes && bien_off; x++)
                        if (gpu[f + x] != bl[nvn_bl_offset(x, y, z, row_bytes, rows, th, td)]) {
                            bien_off = 0;
                            printf("  caso %d capa %u: byte (%u, %u, %u) distinto de nvn_bl_offset\n", caso, ly, x, y, z);
                        }
                }
            bytes += (uint64_t)row_bytes * rows * slices;
        }
        malos += !bien, malos_off += !bien_off;
        free(ref), free(src), free(dst);
        soltar(&in), soltar(&out);
    }
    printf("  %d geometrías (%d con GOB parcial al final de la fila; th 0..5: %d %d %d %d %d %d; td 0/1: %d %d), %.1f MB "
           "comparados\n", casos, parciales, por_th[0], por_th[1], por_th[2], por_th[3], por_th[4], por_th[5], por_td[0],
           por_td[1], bytes / 1e6);
    ver("el shader da lo mismo que tex_bl_copy en todas las geometrías", !malos);
    ver("el shader da lo mismo que nvn_bl_offset byte por byte", !malos_off);

    vkDestroyCommandPool(dev, pool, NULL);
    vkDestroyPipeline(dev, pipe, NULL);
    vkDestroyPipelineLayout(dev, layout, NULL);
    vkDestroyDescriptorSetLayout(dev, dsl, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    return fallas ? 1 : 0;
}
