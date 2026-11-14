// A physical NVN pool larger than the free VRAM must still be created (in system memory), not refused.
// VMA's pHeapSizeLimit stands in for a full VRAM: device-local heaps get 16 MiB, the pool asks for 64 MiB.
#include <stdio.h>
#include "vk_phys_mem.h"

#define OK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", #x, r_); return 1; } } while (0)

int main(void) {
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    uint32_t nd = 1;
    VkPhysicalDevice phys;
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS || vkEnumeratePhysicalDevices(inst, &nd, &phys) < 0 || !nd) {
        fprintf(stderr, "vk_pool_fisico_test: sin Vulkan, no se prueba\n");
        return 77;
    }
    float prio = 1;
    VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                   .pQueuePriorities = &prio};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                              .pQueueCreateInfos = &qci};
    VkDevice dev;
    OK(vkCreateDevice(phys, &dci, NULL, &dev));

    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    VkDeviceSize limits[VK_MAX_MEMORY_HEAPS];
    int has_other_heap = 0;
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++) {
        int vram = mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        limits[i] = vram ? 16u << 20 : VK_WHOLE_SIZE;
        has_other_heap |= !vram;
    }
    if (!has_other_heap) {   // integrated GPU: all memory is device-local, nothing to fall back to
        fprintf(stderr, "vk_pool_fisico_test: la GPU no tiene memoria de sistema aparte, no se prueba\n");
        return 77;
    }
    VmaAllocatorCreateInfo ai = {.physicalDevice = phys, .device = dev, .instance = inst,
                                 .vulkanApiVersion = VK_API_VERSION_1_3, .pHeapSizeLimit = limits};
    VmaAllocator vma;
    OK(vmaCreateAllocator(&ai, &vma));

    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 64u << 20,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VkBuffer buf;
    OK(vkCreateBuffer(dev, &bi, NULL, &buf));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, buf, &req);

    // alloc_physical restricts the pool to memory types valid for BOTH a buffer and an optimal-tiling image, which on
    // NVIDIA excludes the host-visible-only types. The test must use the same restriction, or it would fall back to a
    // host-visible type the real path never reaches.
    VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
                            .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {256, 256, 1}, .mipLevels = 1, .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
    VkDeviceImageMemoryRequirements di = {.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS, .pCreateInfo = &ii};
    VkMemoryRequirements2 ri = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    vkGetDeviceImageMemoryRequirements(dev, &di, &ri);
    req.memoryTypeBits &= ri.memoryRequirements.memoryTypeBits;
    if (!req.memoryTypeBits) {
        fprintf(stderr, "vk_pool_fisico_test: ningún tipo de memoria sirve para buffer e imagen a la vez, no se prueba\n");
        return 77;
    }
    // Without a non-device-local memory type among those (an integrated GPU, or an unusual driver) there is nothing to
    // fall back to: the pool can only live in the device-local heap, so this scenario does not apply.
    int tiene_fallback = 0;
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((req.memoryTypeBits >> i & 1) && !(mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            tiene_fallback = 1;
    if (!tiene_fallback) {
        fprintf(stderr, "vk_pool_fisico_test: los tipos de imagen son todos device-local, no hay fallback, no se prueba\n");
        return 77;
    }

    int fallos = 0;
    VmaAllocation small, big;
    int dl = 0;
    VkMemoryRequirements sreq = req;
    sreq.size = 4u << 20;
    OK(vk_phys_mem_alloc(vma, &sreq, &small, &dl));
    if (!dl) { fprintf(stderr, "MAL: un pool que cabe en la VRAM no quedó en la VRAM\n"); fallos++; }
    OK(vk_phys_mem_alloc(vma, &req, &big, &dl));
    if (dl) { fprintf(stderr, "MAL: el pool de 64 MiB debía caer en memoria de sistema\n"); fallos++; }
    OK(vmaBindBufferMemory(vma, big, buf));

    vmaFreeMemory(vma, small);
    vkDestroyBuffer(dev, buf, NULL);
    vmaFreeMemory(vma, big);
    vmaDestroyAllocator(vma);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    printf("vk_pool_fisico_test: %s\n", fallos ? "MAL" : "OK");
    return fallos != 0;
}
