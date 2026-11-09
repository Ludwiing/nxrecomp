// Prueba de la generación de cuadros (runtime/shaders/fg_*.comp, los mismos SPIR-V que usa vk_framegen.c) en la GPU:
// dos cuadros sintéticos con un cuadrado con textura que avanza DX píxeles sobre un fondo con ruido. El cuadro
// intermedio tiene que poner el cuadrado a mitad de camino y parecerse al cuadro ideal mucho más que una simple mezcla.
//
//   cc -O1 -Iruntime/nvn tests/framegen_test.c -lvulkan -lm -o build/framegen_test && build/framegen_test
//   FG_VECTORES=1 imprime los vectores; FG_PPM=<archivo.ppm> guarda el cuadro generado junto al ideal.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "fg_interp_spv.h"
#include "fg_luma_spv.h"
#include "fg_median_spv.h"
#include "fg_motion_spv.h"
#include "fg_store_spv.h"

#define W 320
#define H 192
#define X0 96   // el cuadrado en el cuadro anterior
#define Y0 64
#define DX 24   // cuánto avanza
#define DY 8
#define LADO 64

#define OK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", #x, r_); exit(1); } } while (0)

static VkDevice dev;
static VkPhysicalDevice phys;
static PFN_vkCmdPushDescriptorSetKHR push;

static float ruido(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 255) / 255.0f;
}

// El cuadro con el cuadrado en (sx, sy): fondo gris con ruido fino, cuadrado rojizo con su propia textura.
static void cuadro(float *rgba, int sx, int sy) {
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float *p = rgba + 4 * (y * W + x);
            float n = ruido(x / 2, y / 2);
            p[0] = p[1] = p[2] = 0.25f + 0.3f * n;
            if (x >= sx && x < sx + LADO && y >= sy && y < sy + LADO) {
                float m = ruido((x - sx) / 3 + 1000, (y - sy) / 3);
                p[0] = 0.7f + 0.3f * m, p[1] = 0.1f + 0.2f * m, p[2] = 0.1f;
            }
            p[3] = 1;
        }
}

static uint32_t tipo_memoria(uint32_t bits, VkMemoryPropertyFlags f) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & f) == f) return i;
    fprintf(stderr, "sin memoria adecuada\n");
    exit(1);
}

typedef struct { VkImage img; VkDeviceMemory mem; VkImageView view; int w, h; } Img;

static Img imagen(int w, int h) {
    Img r = {.w = w, .h = h};
    VkImageCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
                            .format = VK_FORMAT_R32G32B32A32_SFLOAT, .extent = {(uint32_t)w, (uint32_t)h, 1}, .mipLevels = 1,
                            .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT};
    ci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    OK(vkCreateImage(dev, &ci, NULL, &r.img));
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, r.img, &mr);
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
                               .memoryTypeIndex = tipo_memoria(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    OK(vkAllocateMemory(dev, &ai, NULL, &r.mem));
    OK(vkBindImageMemory(dev, r.img, r.mem, 0));
    VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = r.img,
                                .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = ci.format,
                                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    OK(vkCreateImageView(dev, &vi, NULL, &r.view));
    return r;
}

typedef struct { VkBuffer buf; VkDeviceMemory mem; void *p; } Buf;

static Buf bufer(VkDeviceSize n) {
    Buf b;
    VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = n,
                             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    OK(vkCreateBuffer(dev, &ci, NULL, &b.buf));
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(dev, b.buf, &mr);
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
                               .memoryTypeIndex = tipo_memoria(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    OK(vkAllocateMemory(dev, &ai, NULL, &b.mem));
    OK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
    OK(vkMapMemory(dev, b.mem, 0, n, 0, &b.p));
    return b;
}

static uint16_t a_half(float f) {
    unsigned u;
    memcpy(&u, &f, 4);
    unsigned s = (u >> 16) & 0x8000, e = (u >> 23) & 255, m = u & 0x7fffff;
    if (e < 113) return (uint16_t)s;
    return (uint16_t)(s | ((e - 112) << 10) | (m >> 13));
}
static float de_half(uint16_t h) {
    unsigned s = (h & 0x8000u) << 16, e = (h >> 10) & 31, m = h & 1023;
    unsigned u = e ? s | ((e + 112) << 23) | (m << 13) : s;
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static void barrera(VkCommandBuffer cb) {
    VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, NULL, 0,
                         NULL);
}

static VkPipelineLayout layout;
static VkSampler lineal;

static VkPipeline pipeline(const uint32_t *code, size_t n) {
    VkShaderModuleCreateInfo mi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = n, .pCode = code};
    VkShaderModule m;
    OK(vkCreateShaderModule(dev, &mi, NULL, &m));
    VkComputePipelineCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = m, .pName = "main"},
                                      .layout = layout};
    VkPipeline p;
    OK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, NULL, &p));
    return p;
}

static void paso(VkCommandBuffer cb, VkPipeline p, const Img *in0, const Img *in1, const Img *in2, const Img *out,
                 const float pc[8], int gx, int gy) {
    const Img *ins[3] = {in0, in1, in2};
    VkDescriptorImageInfo ii[3], oi = {VK_NULL_HANDLE, out->view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet w[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        if (!ins[i]) continue;
        ii[i] = (VkDescriptorImageInfo){lineal, ins[i]->view, VK_IMAGE_LAYOUT_GENERAL};
        w[n++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = (uint32_t)i,
                                        .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .pImageInfo = &ii[i]};
    }
    w[n++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 3, .descriptorCount = 1,
                                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &oi};
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, p);
    push(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, (uint32_t)n, w);
    vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 32, pc);
    vkCmdDispatch(cb, (uint32_t)gx, (uint32_t)gy, 1);
    barrera(cb);
}

int main(void) {
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_2};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    uint32_t nd = 8;
    VkPhysicalDevice pds[8];
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS || vkEnumeratePhysicalDevices(inst, &nd, pds) < 0 || !nd) {
        fprintf(stderr, "framegen_test: sin Vulkan, no se prueba\n");
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
    printf("GPU: %s\n", pp.deviceName);
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
    push = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(dev, "vkCmdPushDescriptorSetKHR");

    VkDescriptorSetLayoutBinding b[4];
    for (int i = 0; i < 4; i++)
        b[i] = (VkDescriptorSetLayoutBinding){(uint32_t)i, i < 3 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                                                 : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                              1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo dl = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                          .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                          .bindingCount = 4, .pBindings = b};
    VkDescriptorSetLayout dsl;
    OK(vkCreateDescriptorSetLayout(dev, &dl, NULL, &dsl));
    VkPushConstantRange pr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 32};
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &dsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pr};
    OK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
    VkSamplerCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR,
                              .minFilter = VK_FILTER_LINEAR, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
    OK(vkCreateSampler(dev, &si, NULL, &lineal));
    VkPipeline p_store = pipeline(fg_store_spv, sizeof fg_store_spv), p_luma = pipeline(fg_luma_spv, sizeof fg_luma_spv),
               p_motion = pipeline(fg_motion_spv, sizeof fg_motion_spv),
               p_median = pipeline(fg_median_spv, sizeof fg_median_spv),
               p_interp = pipeline(fg_interp_spv, sizeof fg_interp_spv);

    int lw = (W + 3) / 4, lh = (H + 3) / 4, bw = (lw + 3) / 4, bh = (lh + 3) / 4;
    Img src[2] = {imagen(W, H), imagen(W, H)}, hist[2] = {imagen(W, H), imagen(W, H)}, luma[2] = {imagen(lw, lh), imagen(lw, lh)},
        mv0 = imagen(bw, bh), mv1 = imagen(bw, bh), out = imagen(W, H);
    VkDeviceSize n = (VkDeviceSize)W * H * 8;
    Buf up[2] = {bufer(n), bufer(n)}, down = bufer(n);
    float *f = malloc(sizeof(float) * 4 * W * H), *ideal = malloc(sizeof(float) * 4 * W * H),
          *a = malloc(sizeof(float) * 4 * W * H), *c = malloc(sizeof(float) * 4 * W * H);
    cuadro(a, X0, Y0);
    cuadro(c, X0 + DX, Y0 + DY);
    cuadro(ideal, X0 + DX / 2, Y0 + DY / 2);
    for (int k = 0; k < 2; k++) {
        float *s = k ? c : a;
        for (int i = 0; i < 4 * W * H; i++) ((uint16_t *)up[k].p)[i] = a_half(s[i]);
    }

    VkCommandPoolCreateInfo pci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = fam};
    VkCommandPool pool;
    OK(vkCreateCommandPool(dev, &pci, NULL, &pool));
    VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cb;
    OK(vkAllocateCommandBuffers(dev, &cai, &cb));
    VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    OK(vkBeginCommandBuffer(cb, &bi));
    Img *todas[] = {&src[0], &src[1], &hist[0], &hist[1], &luma[0], &luma[1], &mv0, &mv1, &out};
    for (unsigned i = 0; i < sizeof todas / sizeof *todas; i++) {
        VkImageMemoryBarrier ib = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                                   .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = todas[i]->img, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &ib);
    }
    VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {W, H, 1}};
    for (int k = 0; k < 2; k++) vkCmdCopyBufferToImage(cb, up[k].buf, src[k].img, VK_IMAGE_LAYOUT_GENERAL, 1, &bc);
    barrera(cb);
    // como vk_framegen_store, para el cuadro anterior y el actual (recorte completo)
    for (int k = 0; k < 2; k++) {
        float ps[8] = {0, 0, W, H, W, H};
        paso(cb, p_store, &src[k], NULL, NULL, &hist[k], ps, (W + 15) / 16, (H + 15) / 16);
        float pl[8] = {(float)lw, (float)lh};
        paso(cb, p_luma, &hist[k], NULL, NULL, &luma[k], pl, (lw + 15) / 16, (lh + 15) / 16);
    }
    // como vk_framegen_interp (RADIO 8, CASTIGO 0.02, t = 0.5, escala 4)
    float pm[8] = {(float)bw, (float)bh, (float)lw, (float)lh, 8, 0.02f, 4};
    paso(cb, p_motion, &luma[1], &luma[0], NULL, &mv0, pm, (bw + 7) / 8, (bh + 7) / 8);
    float pd[8] = {(float)bw, (float)bh};
    paso(cb, p_median, &mv0, NULL, NULL, &mv1, pd, (bw + 7) / 8, (bh + 7) / 8);
    float pi[8] = {W, H, 0.5f, 4, 4};
    paso(cb, p_interp, &hist[0], &hist[1], &mv1, &out, pi, (W + 15) / 16, (H + 15) / 16);
    vkCmdCopyImageToBuffer(cb, out.img, VK_IMAGE_LAYOUT_GENERAL, down.buf, 1, &bc);
    Buf vb = bufer((VkDeviceSize)bw * bh * 8);
    VkBufferImageCopy vc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {(uint32_t)bw, (uint32_t)bh, 1}};
    vkCmdCopyImageToBuffer(cb, mv1.img, VK_IMAGE_LAYOUT_GENERAL, vb.buf, 1, &vc);
    OK(vkEndCommandBuffer(cb));
    VkSubmitInfo sub = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
    OK(vkQueueSubmit(q, 1, &sub, VK_NULL_HANDLE));
    OK(vkQueueWaitIdle(q));

    for (int i = 0; i < 4 * W * H; i++) f[i] = de_half(((uint16_t *)down.p)[i]);
    if (getenv("FG_PPM")) {   // el generado y el ideal, uno al lado del otro
        FILE *o = fopen(getenv("FG_PPM"), "wb");
        fprintf(o, "P6 %d %d 255\n", 2 * W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < 2 * W; x++)
                for (int ch = 0; ch < 3; ch++) {
                    float t = x < W ? f[4 * (y * W + x) + ch] : ideal[4 * (y * W + x - W) + ch];
                    fputc((int)(fminf(fmaxf(t, 0), 1) * 255), o);
                }
        fclose(o);
    }
    // vector del bloque que cae en el centro del cuadrado (en píxeles reales)
    int cx = (X0 + DX / 2 + LADO / 2) / 16, cy = (Y0 + DY / 2 + LADO / 2) / 16;
    uint16_t *v = (uint16_t *)vb.p + 4 * (cy * bw + cx);
    if (getenv("FG_VECTORES"))
        for (int y = 0; y < bh; y++, printf("\n"))
            for (int x = 0; x < bw; x++) {
                uint16_t *u = (uint16_t *)vb.p + 4 * (y * bw + x);
                printf("(%3.0f,%3.0f) ", de_half(u[0]) * 4, de_half(u[1]) * 4);
            }
    printf("vector en el cuadrado: (%.1f, %.1f) px, se esperaba (%d, %d)\n", de_half(v[0]) * 4, de_half(v[1]) * 4, DX, DY);
    // centro de masa de lo rojo del cuadro armado, y error medio contra el ideal y contra la mezcla simple
    double sx = 0, sy = 0, sw = 0, e_fg = 0, e_mez = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = 4 * (y * W + x);
            if (f[i] > 0.6f && f[i + 1] < 0.4f) sx += x, sy += y, sw += 1;
            for (int ch = 0; ch < 3; ch++) {
                e_fg += fabs(f[i + ch] - ideal[i + ch]);
                e_mez += fabs(0.5f * (a[i + ch] + c[i + ch]) - ideal[i + ch]);
            }
        }
    e_fg /= 3.0 * W * H;
    e_mez /= 3.0 * W * H;
    double ex = X0 + DX / 2 + (LADO - 1) / 2.0, ey = Y0 + DY / 2 + (LADO - 1) / 2.0;
    printf("cuadrado en (%.1f, %.1f), se esperaba (%.1f, %.1f); %.0f px rojos (ideal %d)\n", sx / sw, sy / sw, ex, ey, sw,
           LADO * LADO);
    printf("error medio: generado %.4f, mezcla simple %.4f\n", e_fg, e_mez);
    // Umbrales: el cuadrado a menos de 4 px de su lugar (en el borde que avanza queda una estela: ahí solo uno de los dos
    // cuadros ve el fondo), cerca del área ideal, y el error menos de la mitad del de mezclar los dos cuadros sin más.
    int ok = sw > 0 && fabs(sx / sw - ex) < 4 && fabs(sy / sw - ey) < 4 && fabs(sw - LADO * LADO) < LADO * LADO * 0.15 &&
             e_fg < e_mez * 0.5;
    printf("%s\n", ok ? "framegen_test: OK" : "framegen_test: FALLA");
    return ok ? 0 : 1;
}
