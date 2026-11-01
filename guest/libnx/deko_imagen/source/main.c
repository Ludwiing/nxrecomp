// Prueba de imágenes con deko3d: un shader de cómputo pinta un patrón en una imagen RGBA8 de 160x160
// (imageStore) y después se dibuja texel a texel en la esquina de la pantalla.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deko3d.h>
#include <switch.h>

#define FB_NUM 2
#define FB_WIDTH 1280
#define FB_HEIGHT 720
#define CODEMEMSIZE (64 * 1024)
#define CMDMEMSIZE (64 * 1024)
#define TAM 160

static DkDevice g_device;
static DkMemBlock g_framebufferMemBlock, g_codeMemBlock, g_cmdbufMemBlock, g_imgMemBlock, g_descMemBlock;
static DkImage g_framebuffers[FB_NUM], g_img;
static DkSwapchain g_swapchain;
static uint32_t g_codeMemOffset;
static DkShader g_vertexShader, g_fragmentShader, g_computeShader;
static DkCmdBuf g_cmdbuf;
static DkCmdList g_cmdsBindFramebuffer[FB_NUM], g_cmdsRender;
static DkQueue g_queue;

static void loadShader(DkShader *pShader, const char *path) {
    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    rewind(f);
    uint32_t codeOffset = g_codeMemOffset;
    g_codeMemOffset += (size + DK_SHADER_CODE_ALIGNMENT - 1) & ~(DK_SHADER_CODE_ALIGNMENT - 1);
    fread((uint8_t *)dkMemBlockGetCpuAddr(g_codeMemBlock) + codeOffset, size, 1, f);
    fclose(f);
    DkShaderMaker shaderMaker;
    dkShaderMakerDefaults(&shaderMaker, g_codeMemBlock, codeOffset);
    dkShaderInitialize(pShader, &shaderMaker);
}

static void graphicsInitialize(void) {
    DkDeviceMaker deviceMaker;
    dkDeviceMakerDefaults(&deviceMaker);
    g_device = dkDeviceCreate(&deviceMaker);

    DkImageLayoutMaker lm;
    dkImageLayoutMakerDefaults(&lm, g_device);
    lm.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression;
    lm.format = DkImageFormat_RGBA8_Unorm;
    lm.dimensions[0] = FB_WIDTH;
    lm.dimensions[1] = FB_HEIGHT;
    DkImageLayout fbLayout;
    dkImageLayoutInitialize(&fbLayout, &lm);
    uint32_t fbSize = dkImageLayoutGetSize(&fbLayout), fbAlign = dkImageLayoutGetAlignment(&fbLayout);
    fbSize = (fbSize + fbAlign - 1) & ~(fbAlign - 1);

    dkImageLayoutMakerDefaults(&lm, g_device);
    lm.flags = DkImageFlags_UsageLoadStore;
    lm.format = DkImageFormat_RGBA8_Unorm;
    lm.dimensions[0] = TAM;
    lm.dimensions[1] = TAM;
    DkImageLayout imgLayout;
    dkImageLayoutInitialize(&imgLayout, &lm);

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, g_device, FB_NUM * fbSize);
    mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_framebufferMemBlock = dkMemBlockCreate(&mm);
    uint32_t imgSize = dkImageLayoutGetSize(&imgLayout);
    dkMemBlockMakerDefaults(&mm, g_device, (imgSize + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1));
    mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_imgMemBlock = dkMemBlockCreate(&mm);
    dkImageInitialize(&g_img, &imgLayout, g_imgMemBlock, 0);

    DkImage const *swapchainImages[FB_NUM];
    for (unsigned i = 0; i < FB_NUM; i++) {
        swapchainImages[i] = &g_framebuffers[i];
        dkImageInitialize(&g_framebuffers[i], &fbLayout, g_framebufferMemBlock, i * fbSize);
    }
    DkSwapchainMaker swapchainMaker;
    dkSwapchainMakerDefaults(&swapchainMaker, g_device, nwindowGetDefault(), swapchainImages, FB_NUM);
    g_swapchain = dkSwapchainCreate(&swapchainMaker);

    dkMemBlockMakerDefaults(&mm, g_device, CODEMEMSIZE);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    g_codeMemBlock = dkMemBlockCreate(&mm);
    loadShader(&g_vertexShader, "romfs:/shaders/pantalla_vsh.dksh");
    loadShader(&g_fragmentShader, "romfs:/shaders/mostrar_fsh.dksh");
    loadShader(&g_computeShader, "romfs:/shaders/patron_csh.dksh");

    dkMemBlockMakerDefaults(&mm, g_device, CMDMEMSIZE);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_cmdbufMemBlock = dkMemBlockCreate(&mm);
    DkCmdBufMaker cmdbufMaker;
    dkCmdBufMakerDefaults(&cmdbufMaker, g_device);
    g_cmdbuf = dkCmdBufCreate(&cmdbufMaker);
    dkCmdBufAddMemory(g_cmdbuf, g_cmdbufMemBlock, 0, CMDMEMSIZE);

    DkQueueMaker queueMaker;
    dkQueueMakerDefaults(&queueMaker, g_device);
    queueMaker.flags = DkQueueFlags_Graphics | DkQueueFlags_Compute;
    g_queue = dkQueueCreate(&queueMaker);

    // Descriptores: la imagen (para cómputo y para leerla como textura) y un sampler.
    dkMemBlockMakerDefaults(&mm, g_device, DK_MEMBLOCK_ALIGNMENT);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_descMemBlock = dkMemBlockCreate(&mm);
    DkImageDescriptor *imgDesc = dkMemBlockGetCpuAddr(g_descMemBlock);
    DkSamplerDescriptor *smpDesc = (DkSamplerDescriptor *)(imgDesc + 1);
    DkImageView v;
    dkImageViewDefaults(&v, &g_img);
    dkImageDescriptorInitialize(imgDesc, &v, true, false);
    DkSampler smp;
    dkSamplerDefaults(&smp);
    dkSamplerDescriptorInitialize(smpDesc, &smp);
    DkGpuAddr desc = dkMemBlockGetGpuAddr(g_descMemBlock);
    DkResHandle hImg = dkMakeImageHandle(0), hTex = dkMakeTextureHandle(0, 0);

    // El cómputo: pintar la imagen una vez.
    DkShader const *cs = &g_computeShader;
    dkCmdBufBindImageDescriptorSet(g_cmdbuf, desc, 1);
    dkCmdBufBindSamplerDescriptorSet(g_cmdbuf, desc + sizeof(DkImageDescriptor), 1);
    dkCmdBufBindShaders(g_cmdbuf, DkStageFlag_Compute, &cs, 1);
    dkCmdBufBindImages(g_cmdbuf, DkStage_Compute, 0, &hImg, 1);
    dkCmdBufDispatchCompute(g_cmdbuf, TAM / 8, TAM / 8, 1);
    dkCmdBufBarrier(g_cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
    dkQueueSubmitCommands(g_queue, dkCmdBufFinishList(g_cmdbuf));
    dkQueueWaitIdle(g_queue);
    dkCmdBufClear(g_cmdbuf);

    for (unsigned i = 0; i < FB_NUM; i++) {
        DkImageView imageView;
        dkImageViewDefaults(&imageView, &g_framebuffers[i]);
        dkCmdBufBindRenderTarget(g_cmdbuf, &imageView, NULL);
        g_cmdsBindFramebuffer[i] = dkCmdBufFinishList(g_cmdbuf);
    }
    DkViewport viewport = {0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f};
    DkScissor scissor = {0, 0, FB_WIDTH, FB_HEIGHT};
    DkShader const *shaders[] = {&g_vertexShader, &g_fragmentShader};
    DkRasterizerState rasterizerState;
    DkColorState colorState;
    DkColorWriteState colorWriteState;
    dkRasterizerStateDefaults(&rasterizerState);
    rasterizerState.cullMode = DkFace_None;
    dkColorStateDefaults(&colorState);
    dkColorWriteStateDefaults(&colorWriteState);
    dkCmdBufBindImageDescriptorSet(g_cmdbuf, desc, 1);
    dkCmdBufBindSamplerDescriptorSet(g_cmdbuf, desc + sizeof(DkImageDescriptor), 1);
    dkCmdBufSetViewports(g_cmdbuf, 0, &viewport, 1);
    dkCmdBufSetScissors(g_cmdbuf, 0, &scissor, 1);
    dkCmdBufClearColorFloat(g_cmdbuf, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    dkCmdBufBindShaders(g_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);
    dkCmdBufBindTextures(g_cmdbuf, DkStage_Fragment, 0, &hTex, 1);
    dkCmdBufBindRasterizerState(g_cmdbuf, &rasterizerState);
    dkCmdBufBindColorState(g_cmdbuf, &colorState);
    dkCmdBufBindColorWriteState(g_cmdbuf, &colorWriteState);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
    g_cmdsRender = dkCmdBufFinishList(g_cmdbuf);
}

static void graphicsUpdate(void) {
    int slot = dkQueueAcquireImage(g_queue, g_swapchain);
    dkQueueSubmitCommands(g_queue, g_cmdsBindFramebuffer[slot]);
    dkQueueSubmitCommands(g_queue, g_cmdsRender);
    dkQueuePresentImage(g_queue, g_swapchain, slot);
}

static void graphicsExit(void) {
    dkQueueWaitIdle(g_queue);
    dkQueueDestroy(g_queue);
    dkCmdBufDestroy(g_cmdbuf);
    dkMemBlockDestroy(g_cmdbufMemBlock);
    dkMemBlockDestroy(g_descMemBlock);
    dkMemBlockDestroy(g_codeMemBlock);
    dkSwapchainDestroy(g_swapchain);
    dkMemBlockDestroy(g_imgMemBlock);
    dkMemBlockDestroy(g_framebufferMemBlock);
    dkDeviceDestroy(g_device);
}

int main(int argc, char *argv[]) {
    romfsInit();
    graphicsInitialize();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        graphicsUpdate();
    }
    graphicsExit();
    romfsExit();
    return 0;
}
