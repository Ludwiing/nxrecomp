// Corre un SPIR-V de cómputo (el que deja totk_xlate) con buffers leídos de archivos y guarda los de almacenamiento
// de vuelta en sus archivos. Lo usa tests/sm86_programa_test.py. Código propio (MIT); el andamiaje es el de
// tests/framegen_test.c.
//
//   build/sm86_programa_vk <shader.spv> <grupos x> <binding>:<u|s>:<archivo> ...
//       u = uniforme, s = almacenamiento (se escribe de vuelta); todos en el set 0. El binding 0 (support buffer de
//       Ryujinx) se agrega con ceros si no se da. 77 si no hay Vulkan.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define OK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", #x, r_); exit(1); } } while (0)
#define MAXB 16

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

typedef struct { VkBuffer buf; VkDeviceMemory mem; void *p; VkDeviceSize n; } Buf;

static Buf bufer(VkDeviceSize n, VkBufferUsageFlags uso) {
    Buf b = {.n = n};
    VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = n, .usage = uso};
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

static void *leer(const char *ruta, long *n) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror(ruta); exit(1); }
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *p = malloc((size_t)*n + 16);
    if (fread(p, 1, (size_t)*n, f) != (size_t)*n) { perror(ruta); exit(1); }
    fclose(f);
    return p;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: %s shader.spv grupos binding:u|s:archivo ...\n", argv[0]);
        return 2;
    }
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_2};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    uint32_t nd = 8;
    VkPhysicalDevice pds[8];
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS || vkEnumeratePhysicalDevices(inst, &nd, pds) < 0 || !nd) {
        fprintf(stderr, "sm86_programa_vk: sin Vulkan\n");
        return 77;
    }
    // la GPU dedicada primero; si no se puede crear el dispositivo (p. ej. sin memoria de video), la siguiente
    VkPhysicalDevice orden[8];
    uint32_t no = 0;
    for (int dedicada = 1; dedicada >= 0; dedicada--)
        for (uint32_t i = 0; i < nd; i++) {
            VkPhysicalDeviceProperties pp;
            vkGetPhysicalDeviceProperties(pds[i], &pp);
            if ((pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) == dedicada) orden[no++] = pds[i];
        }
    uint32_t fam = 0;
    VkResult r = VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t k = 0; k < no && r != VK_SUCCESS; k++) {
        phys = orden[k];
        uint32_t nq = 16;
        VkQueueFamilyProperties qf[16];
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
        for (uint32_t i = 0; i < nq; i++)
            if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { fam = i; break; }
        float prio = 1;
        VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = fam,
                                       .queueCount = 1, .pQueuePriorities = &prio};
        VkPhysicalDeviceFeatures tiene = {0}, feat = {0};   // lo que puede pedir el SPIR-V de Ryujinx, si lo tiene
        vkGetPhysicalDeviceFeatures(phys, &tiene);
        feat.shaderInt64 = tiene.shaderInt64, feat.shaderInt16 = tiene.shaderInt16;
        feat.shaderFloat64 = tiene.shaderFloat64;
        VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                                  .pQueueCreateInfos = &qci, .pEnabledFeatures = &feat};
        r = vkCreateDevice(phys, &dci, NULL, &dev);
        VkPhysicalDeviceProperties pp;
        vkGetPhysicalDeviceProperties(phys, &pp);
        fprintf(stderr, "%s: %s\n", pp.deviceName, r == VK_SUCCESS ? "dispositivo creado" : "no se pudo crear el dispositivo");
    }
    if (r != VK_SUCCESS) return 77;
    VkQueue q;
    vkGetDeviceQueue(dev, fam, 0, &q);

    int nb = 0, tiene0 = 0;
    uint32_t binding[MAXB];
    char tipo[MAXB];
    const char *ruta[MAXB];
    Buf b[MAXB];
    for (int i = 3; i < argc && nb < MAXB - 1; i++) {
        char *a = argv[i], *p1 = strchr(a, ':'), *p2 = p1 ? strchr(p1 + 1, ':') : NULL;
        if (!p2) { fprintf(stderr, "argumento mal: %s\n", a); return 2; }
        binding[nb] = (uint32_t)atoi(a), tipo[nb] = p1[1], ruta[nb] = p2 + 1;
        long n;
        void *d = leer(ruta[nb], &n);
        b[nb] = bufer((VkDeviceSize)(n < 16 ? 16 : n), tipo[nb] == 's' ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                        : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        memcpy(b[nb].p, d, (size_t)n);
        free(d);
        tiene0 |= binding[nb] == 0;
        nb++;
    }
    if (!tiene0) {   // support buffer de Ryujinx: ceros
        binding[nb] = 0, tipo[nb] = 'u', ruta[nb] = NULL;
        b[nb] = bufer(4096, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        memset(b[nb].p, 0, 4096);
        nb++;
    }
    VkDescriptorSetLayoutBinding lb[MAXB];
    VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAXB}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAXB}};
    for (int i = 0; i < nb; i++)
        lb[i] = (VkDescriptorSetLayoutBinding){binding[i], tipo[i] == 's' ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                                                         : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo dl = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                          .bindingCount = (uint32_t)nb, .pBindings = lb};
    VkDescriptorSetLayout dsl;
    OK(vkCreateDescriptorSetLayout(dev, &dl, NULL, &dsl));
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &dsl};
    VkPipelineLayout layout;
    OK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
    long n;
    uint32_t *code = leer(argv[1], &n);
    VkShaderModuleCreateInfo smi = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = (size_t)n,
                                    .pCode = code};
    VkShaderModule sm;
    OK(vkCreateShaderModule(dev, &smi, NULL, &sm));
    VkComputePipelineCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                       .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                 .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = sm, .pName = "main"},
                                       .layout = layout};
    VkPipeline pipe;
    OK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, NULL, &pipe));
    VkDescriptorPoolCreateInfo dpi = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
                                      .poolSizeCount = 2, .pPoolSizes = ps};
    VkDescriptorPool pool;
    OK(vkCreateDescriptorPool(dev, &dpi, NULL, &pool));
    VkDescriptorSetAllocateInfo dai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = pool,
                                       .descriptorSetCount = 1, .pSetLayouts = &dsl};
    VkDescriptorSet set;
    OK(vkAllocateDescriptorSets(dev, &dai, &set));
    VkDescriptorBufferInfo bi[MAXB];
    VkWriteDescriptorSet w[MAXB];
    for (int i = 0; i < nb; i++) {
        bi[i] = (VkDescriptorBufferInfo){b[i].buf, 0, VK_WHOLE_SIZE};
        w[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
                                      .dstBinding = binding[i], .descriptorCount = 1,
                                      .descriptorType = lb[i].descriptorType, .pBufferInfo = &bi[i]};
    }
    vkUpdateDescriptorSets(dev, (uint32_t)nb, w, 0, NULL);
    VkCommandPoolCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = fam};
    VkCommandPool cp;
    OK(vkCreateCommandPool(dev, &cpci, NULL, &cp));
    VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cp,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cb;
    OK(vkAllocateCommandBuffers(dev, &cai, &cb));
    VkCommandBufferBeginInfo cbi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    OK(vkBeginCommandBuffer(cb, &cbi));
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    vkCmdDispatch(cb, (uint32_t)atoi(argv[2]), 1, 1);
    VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
    OK(vkEndCommandBuffer(cb));
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
    OK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
    OK(vkQueueWaitIdle(q));
    for (int i = 0; i < nb; i++) {
        if (tipo[i] != 's') continue;
        FILE *f = fopen(ruta[i], "wb");
        if (!f || fwrite(b[i].p, 1, (size_t)b[i].n, f) != (size_t)b[i].n) { perror(ruta[i]); return 1; }
        fclose(f);
    }
    return 0;
}
