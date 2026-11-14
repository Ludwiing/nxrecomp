// Memory for an NVN physical pool. Switch 2 has 12 GB of unified memory and a game's physical pools can add up to more
// than the PC's VRAM: device-local is preferred, not required, so VMA falls back to system memory (slower, but the pool
// exists and the virtual pools that map it keep working). *device_local says where it landed.
#pragma once
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

static inline VkResult vk_phys_mem_alloc(VmaAllocator vma, const VkMemoryRequirements *req, VmaAllocation *out,
                                         int *device_local) {
    VmaAllocationCreateInfo aci = {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                   .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    VmaAllocationInfo info;
    VkResult res = vmaAllocateMemory(vma, req, &aci, out, &info);
    if (res == VK_SUCCESS) {
        VkMemoryPropertyFlags f;
        vmaGetMemoryTypeProperties(vma, info.memoryType, &f);
        *device_local = !!(f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    return res;
}
