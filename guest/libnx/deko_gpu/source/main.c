// Prueba de lo que usan los juegos con NVN y los ejemplos de deko3d no: draws indirectos (los
// argumentos en otra entrada del GPFIFO), contador de muestras (oclusión), marcas de tiempo en ticks,
// variables (semáforos acquire/release), alpha test y logic op.
//
// Fila de arriba (y 100-300): blanco con Draw, rojo con DrawIndirect, verde con DrawIndexedIndirect,
// azul con alpha test (mitad izquierda descartada) y un rectángulo blanco con XOR de rojo (cian).
// Fila de abajo (y 400-500): un cuadro por cada comprobación de la CPU, verde si pasó y rojo si no:
// muestras contadas, marca de tiempo, variable.
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
#define DATOS 4096

typedef struct {
    float rect[4], color[4];
} Uniforme;

static DkDevice g_device;
static DkMemBlock g_framebufferMemBlock, g_codeMemBlock, g_cmdbufMemBlock, g_datos, g_varMem;
static DkImage g_framebuffers[FB_NUM];
static DkSwapchain g_swapchain;
static uint32_t g_codeMemOffset;
static DkShader g_vsh, g_fsh;
static DkCmdBuf g_cmdbuf;
static DkCmdList g_cmdsBindFramebuffer[FB_NUM], g_cmdsRender;
static DkQueue g_queue;
static DkVariable g_var;

// Dentro de g_datos: el uniform buffer, los argumentos indirectos, los índices y los reportes.
enum { OFF_UBO = 0, OFF_IND = 0x100, OFF_IND_IDX = 0x140, OFF_IDX = 0x180, OFF_REP = 0x200 };

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

static void rect(float x0, float y0, float x1, float y1, float r, float g, float b, float a) {
    Uniforme u = {{x0, y0, x1, y1}, {r, g, b, a}};
    dkCmdBufPushConstants(g_cmdbuf, dkMemBlockGetGpuAddr(g_datos) + OFF_UBO, 0x100, 0, sizeof u, &u);
}

static void estado(DkLogicOp logic, DkCompareOp alfa) {
    DkColorState cs;
    dkColorStateDefaults(&cs);
    cs.logicOp = logic;
    cs.alphaCompareOp = alfa;
    dkCmdBufBindColorState(g_cmdbuf, &cs);
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

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, g_device, FB_NUM * fbSize);
    mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_framebufferMemBlock = dkMemBlockCreate(&mm);
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
    loadShader(&g_vsh, "romfs:/shaders/rect_vsh.dksh");
    loadShader(&g_fsh, "romfs:/shaders/rect_fsh.dksh");

    dkMemBlockMakerDefaults(&mm, g_device, CMDMEMSIZE);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_cmdbufMemBlock = dkMemBlockCreate(&mm);
    DkCmdBufMaker cmdbufMaker;
    dkCmdBufMakerDefaults(&cmdbufMaker, g_device);
    g_cmdbuf = dkCmdBufCreate(&cmdbufMaker);
    dkCmdBufAddMemory(g_cmdbuf, g_cmdbufMemBlock, 0, CMDMEMSIZE);

    dkMemBlockMakerDefaults(&mm, g_device, DATOS);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_datos = dkMemBlockCreate(&mm);
    uint8_t *datos = dkMemBlockGetCpuAddr(g_datos);
    memset(datos, 0, DATOS);
    DkDrawIndirectData ind = {6, 1, 0, 0};
    memcpy(datos + OFF_IND, &ind, sizeof ind);
    DkDrawIndexedIndirectData indi = {6, 1, 0, 0, 0};
    memcpy(datos + OFF_IND_IDX, &indi, sizeof indi);
    uint16_t idx[6] = {0, 1, 2, 3, 4, 5};
    memcpy(datos + OFF_IDX, idx, sizeof idx);
    dkMemBlockMakerDefaults(&mm, g_device, DK_MEMBLOCK_ALIGNMENT);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached;
    g_varMem = dkMemBlockCreate(&mm);
    dkVariableInitialize(&g_var, g_varMem, 0);

    DkQueueMaker queueMaker;
    dkQueueMakerDefaults(&queueMaker, g_device);
    queueMaker.flags = DkQueueFlags_Graphics;
    g_queue = dkQueueCreate(&queueMaker);

    for (unsigned i = 0; i < FB_NUM; i++) {
        DkImageView imageView;
        dkImageViewDefaults(&imageView, &g_framebuffers[i]);
        dkCmdBufBindRenderTarget(g_cmdbuf, &imageView, NULL);
        g_cmdsBindFramebuffer[i] = dkCmdBufFinishList(g_cmdbuf);
    }

    // Estado común
    DkGpuAddr ubo = dkMemBlockGetGpuAddr(g_datos) + OFF_UBO;
    DkViewport viewport = {0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f};
    DkScissor scissor = {0, 0, FB_WIDTH, FB_HEIGHT};
    DkShader const *shaders[] = {&g_vsh, &g_fsh};
    DkRasterizerState rs;
    dkRasterizerStateDefaults(&rs);
    rs.cullMode = DkFace_None;
    DkColorWriteState cw;
    dkColorWriteStateDefaults(&cw);
    dkCmdBufSetViewports(g_cmdbuf, 0, &viewport, 1);
    dkCmdBufSetScissors(g_cmdbuf, 0, &scissor, 1);
    dkCmdBufBindShaders(g_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);
    dkCmdBufBindUniformBuffer(g_cmdbuf, DkStage_Vertex, 0, ubo, 0x100);
    dkCmdBufBindUniformBuffer(g_cmdbuf, DkStage_Fragment, 0, ubo, 0x100);
    dkCmdBufBindRasterizerState(g_cmdbuf, &rs);
    dkCmdBufBindColorWriteState(g_cmdbuf, &cw);
    DkCmdList comun = dkCmdBufFinishList(g_cmdbuf);

    // Mediciones, una vez: muestras de un rectángulo de 200x200, marcas de tiempo y una variable.
    DkGpuAddr rep = dkMemBlockGetGpuAddr(g_datos) + OFF_REP;
    dkQueueSubmitCommands(g_queue, g_cmdsBindFramebuffer[0]);
    dkQueueSubmitCommands(g_queue, comun);
    estado(DkLogicOp_Copy, DkCompareOp_Always);
    dkCmdBufReportCounter(g_cmdbuf, DkCounter_Timestamp, rep + 0x40);
    dkCmdBufResetCounter(g_cmdbuf, DkCounter_SamplesPassed);
    rect(50, 100, 250, 300, 1, 1, 1, 1);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    dkCmdBufReportCounter(g_cmdbuf, DkCounter_SamplesPassed, rep);
    dkCmdBufReportCounter(g_cmdbuf, DkCounter_Timestamp, rep + 0x50);
    dkCmdBufSignalVariable(g_cmdbuf, &g_var, DkVarOp_Set, 41, DkPipelinePos_Bottom);
    dkCmdBufWaitVariable(g_cmdbuf, &g_var, DkVarCompareOp_Equal, 41);
    dkCmdBufSignalVariable(g_cmdbuf, &g_var, DkVarOp_Add, 1, DkPipelinePos_Top);
    dkQueueSubmitCommands(g_queue, dkCmdBufFinishList(g_cmdbuf));
    dkQueueWaitIdle(g_queue);
    uint64_t muestras, t0, t1;
    memcpy(&muestras, datos + OFF_REP, 8);
    memcpy(&t0, datos + OFF_REP + 0x48, 8);
    memcpy(&t1, datos + OFF_REP + 0x58, 8);
    uint64_t ahora = armGetSystemTick();
    // Las marcas de la GPU van en ticks de 614,4 MHz (384 por cada 625 ns); la CPU, a 19,2 MHz. Las dos
    // cuentan desde el arranque: la marca tiene que caer en el último segundo.
    uint64_t gpu_ns = t1 / 384 * 625, cpu_ns = ahora / 12 * 625;
    int ok_muestras = muestras == 200 * 200;
    int ok_tiempo = t0 && t1 >= t0 && cpu_ns + 1000 >= gpu_ns && cpu_ns - gpu_ns < 1000000000ull;
    int ok_var = dkVariableRead(&g_var) == 42;
    fprintf(stderr, "muestras %llu, marcas %llu %llu (CPU %llu), variable %u\n", (unsigned long long)muestras,
           (unsigned long long)t0, (unsigned long long)t1, (unsigned long long)ahora, dkVariableRead(&g_var));
    dkCmdBufClear(g_cmdbuf);

    // Lo que se dibuja en cada cuadro
    dkCmdBufCallList(g_cmdbuf, comun);
    dkCmdBufClearColorFloat(g_cmdbuf, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    estado(DkLogicOp_Copy, DkCompareOp_Always);
    rect(50, 100, 250, 300, 1, 1, 1, 1);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    rect(300, 100, 500, 300, 1, 0, 0, 1);
    dkCmdBufDrawIndirect(g_cmdbuf, DkPrimitive_Triangles, dkMemBlockGetGpuAddr(g_datos) + OFF_IND);
    rect(550, 100, 750, 300, 0, 1, 0, 1);
    dkCmdBufBindIdxBuffer(g_cmdbuf, DkIdxFormat_Uint16, dkMemBlockGetGpuAddr(g_datos) + OFF_IDX);
    dkCmdBufDrawIndexedIndirect(g_cmdbuf, DkPrimitive_Triangles, dkMemBlockGetGpuAddr(g_datos) + OFF_IND_IDX);
    estado(DkLogicOp_Copy, DkCompareOp_Greater);
    dkCmdBufSetAlphaRef(g_cmdbuf, 0.5f);
    rect(800, 100, 1000, 300, 0, 0, 1, -1);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    estado(DkLogicOp_Copy, DkCompareOp_Always);
    rect(1050, 100, 1250, 300, 1, 1, 1, 1);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    estado(DkLogicOp_Xor, DkCompareOp_Always);
    rect(1050, 100, 1250, 300, 1, 0, 0, 1);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    estado(DkLogicOp_Copy, DkCompareOp_Always);
    int oks[3] = {ok_muestras, ok_tiempo, ok_var};
    for (int k = 0; k < 3; k++) {
        rect(50 + k * 150, 400, 150 + k * 150, 500, oks[k] ? 0 : 1, oks[k] ? 1 : 0, 0, 1);
        dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    }
    g_cmdsRender = dkCmdBufFinishList(g_cmdbuf);
}

static void graphicsUpdate(void) {
    int slot = dkQueueAcquireImage(g_queue, g_swapchain);
    dkQueueSubmitCommands(g_queue, g_cmdsBindFramebuffer[slot]);
    dkQueueSubmitCommands(g_queue, g_cmdsRender);
    dkQueuePresentImage(g_queue, g_swapchain, slot);
}

int main(int argc, char *argv[]) {
    romfsInit();
    consoleDebugInit(debugDevice_SVC);
    graphicsInitialize();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        graphicsUpdate();
    }
    dkQueueWaitIdle(g_queue);
    romfsExit();
    return 0;
}
