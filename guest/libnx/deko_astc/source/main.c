// Prueba de texturas ASTC con deko3d: sube cuatro texturas (bloques 4x4, 8x5, 10x10 y 12x12, de
// 160x160 texels) con el motor de copia y las dibuja texel a texel una al lado de la otra. Los bloques
// vienen de romfs:/astc_WxH.bin (tools/gen_astc.py); la prueba compara la imagen con la
// decodificación de Mesa de esos mismos bloques.
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
#define TEX 160
#define NTEX 4

static const struct {
    int w, h;
    DkImageFormat fmt;
} formatos[NTEX] = {
    {4, 4, DkImageFormat_RGBA_ASTC_4x4},
    {8, 5, DkImageFormat_RGBA_ASTC_8x5},
    {10, 10, DkImageFormat_RGBA_ASTC_10x10},
    {12, 12, DkImageFormat_RGBA_ASTC_12x12},
};

static DkDevice g_device;
static DkMemBlock g_framebufferMemBlock, g_codeMemBlock, g_cmdbufMemBlock, g_texMemBlock, g_datosMemBlock;
static DkImage g_framebuffers[FB_NUM], g_tex[NTEX];
static DkSwapchain g_swapchain;
static uint32_t g_codeMemOffset;
static DkShader g_vertexShader, g_fragmentShader;
static DkCmdBuf g_cmdbuf;
static DkCmdList g_cmdsBindFramebuffer[FB_NUM], g_cmdsRender;
static DkQueue g_renderQueue;

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

    // Texturas ASTC: un bloque de memoria para las cuatro.
    DkImageLayout texLayout[NTEX];
    uint32_t texOff[NTEX], texTotal = 0;
    for (int i = 0; i < NTEX; i++) {
        dkImageLayoutMakerDefaults(&lm, g_device);
        lm.format = formatos[i].fmt;
        lm.dimensions[0] = TEX;
        lm.dimensions[1] = TEX;
        dkImageLayoutInitialize(&texLayout[i], &lm);
        uint32_t al = dkImageLayoutGetAlignment(&texLayout[i]);
        texTotal = (texTotal + al - 1) & ~(al - 1);
        texOff[i] = texTotal;
        texTotal += dkImageLayoutGetSize(&texLayout[i]);
    }

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, g_device, FB_NUM * fbSize);
    mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_framebufferMemBlock = dkMemBlockCreate(&mm);
    dkMemBlockMakerDefaults(&mm, g_device, (texTotal + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1));
    mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_texMemBlock = dkMemBlockCreate(&mm);

    DkImage const *swapchainImages[FB_NUM];
    for (unsigned i = 0; i < FB_NUM; i++) {
        swapchainImages[i] = &g_framebuffers[i];
        dkImageInitialize(&g_framebuffers[i], &fbLayout, g_framebufferMemBlock, i * fbSize);
    }
    for (int i = 0; i < NTEX; i++) dkImageInitialize(&g_tex[i], &texLayout[i], g_texMemBlock, texOff[i]);

    DkSwapchainMaker swapchainMaker;
    dkSwapchainMakerDefaults(&swapchainMaker, g_device, nwindowGetDefault(), swapchainImages, FB_NUM);
    g_swapchain = dkSwapchainCreate(&swapchainMaker);

    dkMemBlockMakerDefaults(&mm, g_device, CODEMEMSIZE);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    g_codeMemBlock = dkMemBlockCreate(&mm);
    loadShader(&g_vertexShader, "romfs:/shaders/pantalla_vsh.dksh");
    loadShader(&g_fragmentShader, "romfs:/shaders/astc_fsh.dksh");

    dkMemBlockMakerDefaults(&mm, g_device, CMDMEMSIZE);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_cmdbufMemBlock = dkMemBlockCreate(&mm);
    DkCmdBufMaker cmdbufMaker;
    dkCmdBufMakerDefaults(&cmdbufMaker, g_device);
    g_cmdbuf = dkCmdBufCreate(&cmdbufMaker);
    dkCmdBufAddMemory(g_cmdbuf, g_cmdbufMemBlock, 0, CMDMEMSIZE);

    DkQueueMaker queueMaker;
    dkQueueMakerDefaults(&queueMaker, g_device);
    queueMaker.flags = DkQueueFlags_Graphics;
    g_renderQueue = dkQueueCreate(&queueMaker);

    // Datos: los bloques de cada textura, y detrás los descriptores de imagen y de sampler.
    uint32_t datosTam = 0, datosOff[NTEX];
    for (int i = 0; i < NTEX; i++) {
        datosOff[i] = datosTam;
        datosTam += ((TEX + formatos[i].w - 1) / formatos[i].w) * ((TEX + formatos[i].h - 1) / formatos[i].h) * 16;
    }
    uint32_t descOff = (datosTam + 255) & ~255u;
    dkMemBlockMakerDefaults(&mm, g_device, (descOff + 4096 + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1));
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_datosMemBlock = dkMemBlockCreate(&mm);
    uint8_t *datos = dkMemBlockGetCpuAddr(g_datosMemBlock);
    for (int i = 0; i < NTEX; i++) {
        char ruta[64];
        snprintf(ruta, sizeof ruta, "romfs:/astc_%dx%d.bin", formatos[i].w, formatos[i].h);
        FILE *f = fopen(ruta, "rb");
        uint32_t n = ((TEX + formatos[i].w - 1) / formatos[i].w) * ((TEX + formatos[i].h - 1) / formatos[i].h) * 16;
        if (f) {
            fread(datos + datosOff[i], 1, n, f);
            fclose(f);
        }
    }
    DkImageDescriptor *imgDesc = (DkImageDescriptor *)(datos + descOff);
    DkSamplerDescriptor *smpDesc = (DkSamplerDescriptor *)(datos + descOff + NTEX * sizeof(DkImageDescriptor));
    for (int i = 0; i < NTEX; i++) {
        DkImageView v;
        dkImageViewDefaults(&v, &g_tex[i]);
        dkImageDescriptorInitialize(&imgDesc[i], &v, false, false);
    }
    DkSampler smp;
    dkSamplerDefaults(&smp);
    dkSamplerDescriptorInitialize(smpDesc, &smp);
    DkGpuAddr datosGpu = dkMemBlockGetGpuAddr(g_datosMemBlock);

    // Subir las texturas con el motor de copia.
    for (int i = 0; i < NTEX; i++) {
        DkImageView v;
        dkImageViewDefaults(&v, &g_tex[i]);
        DkCopyBuf src = {datosGpu + datosOff[i], 0, 0};
        DkImageRect dst = {0, 0, 0, TEX, TEX, 1};
        dkCmdBufCopyBufferToImage(g_cmdbuf, &src, &v, &dst, 0);
    }
    dkQueueSubmitCommands(g_renderQueue, dkCmdBufFinishList(g_cmdbuf));
    dkQueueWaitIdle(g_renderQueue);
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
    DkResHandle handles[NTEX];
    for (int i = 0; i < NTEX; i++) handles[i] = dkMakeTextureHandle(i, 0);

    dkCmdBufBindImageDescriptorSet(g_cmdbuf, datosGpu + descOff, NTEX);
    dkCmdBufBindSamplerDescriptorSet(g_cmdbuf, datosGpu + descOff + NTEX * sizeof(DkImageDescriptor), 1);
    dkCmdBufSetViewports(g_cmdbuf, 0, &viewport, 1);
    dkCmdBufSetScissors(g_cmdbuf, 0, &scissor, 1);
    dkCmdBufClearColorFloat(g_cmdbuf, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    dkCmdBufBindShaders(g_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);
    dkCmdBufBindTextures(g_cmdbuf, DkStage_Fragment, 0, handles, NTEX);
    dkCmdBufBindRasterizerState(g_cmdbuf, &rasterizerState);
    dkCmdBufBindColorState(g_cmdbuf, &colorState);
    dkCmdBufBindColorWriteState(g_cmdbuf, &colorWriteState);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
    g_cmdsRender = dkCmdBufFinishList(g_cmdbuf);
}

static void graphicsUpdate(void) {
    int slot = dkQueueAcquireImage(g_renderQueue, g_swapchain);
    dkQueueSubmitCommands(g_renderQueue, g_cmdsBindFramebuffer[slot]);
    dkQueueSubmitCommands(g_renderQueue, g_cmdsRender);
    dkQueuePresentImage(g_renderQueue, g_swapchain, slot);
}

static void graphicsExit(void) {
    dkQueueWaitIdle(g_renderQueue);
    dkQueueDestroy(g_renderQueue);
    dkCmdBufDestroy(g_cmdbuf);
    dkMemBlockDestroy(g_cmdbufMemBlock);
    dkMemBlockDestroy(g_datosMemBlock);
    dkMemBlockDestroy(g_codeMemBlock);
    dkSwapchainDestroy(g_swapchain);
    dkMemBlockDestroy(g_texMemBlock);
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
