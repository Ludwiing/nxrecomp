// Driver optimization off for pipelines whose shaders are too big for the driver's compiler. A giant Switch 2 (SM86)
// shader can hang or crash the NVIDIA compiler inside vkCreate*Pipelines; built without optimization it runs slower but
// gets built. TOTK_VK_NOOPT=1: every pipeline. TOTK_VK_NOOPT_WORDS=N: only pipelines with a stage of more than N SPIR-V
// words (0: none). Both default to off, so pipelines stay optimized as before. Same idea as NS2_VK_NOOPT in MTF
// ShadowRacket, which took it from Lichii.
#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "cfg.h"

// largest_stage_words: the biggest SPIR-V among the shader stages the pipeline compiles (0 when it compiles none).
static inline VkPipelineCreateFlags vk_noopt_flags(uint64_t largest_stage_words) {
    static int all = -1, limit = -1;   // RESTART settings: read once (a race only reads the same values twice)
    if (all < 0) all = cfg_bool("TOTK_VK_NOOPT");
    if (limit < 0) limit = cfg_int("TOTK_VK_NOOPT_WORDS");
    return all || (limit > 0 && largest_stage_words > (uint64_t)limit) ? VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0;
}
