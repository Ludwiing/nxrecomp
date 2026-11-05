// NVN en el host (HLE): objetos de NVN que viven en la memoria que el juego reserva para ellos (dispositivo, por ahora).
// Cada nvnp_<nombre> reemplaza al relleno débil de nxnvn.c. Los argumentos llegan en x0..x7 (AAPCS64) y el resultado
// vuelve con ret_x.
//
// La tabla de constantes del dispositivo y el comportamiento de cada función siguen a totk-pc-recomp
// (runtime/nvn_api_core.c, MIT, Copyright (c) 2026 Valiantsin Dzerakh), que a su vez lo tomó de MacNativeGames
// d2r_gfx gfx/nvn/hle_nvn.c (MIT, Copyright (c) 2026 jhaizhou). Ver LICENSE.totk-pc-recomp.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxnvn.h"

#define NVN_MAGIC(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define DEV_MAGIC NVN_MAGIC('D', 'E', 'V', 0)

static int traza(void) {
    static int t = -1;
    if (t < 0) t = getenv("NX_TRAZA_NVN") != NULL;
    return t;
}

// El juego reserva NVNdeviceBuilder (64 bytes) y NVNdevice (12 KiB); el estado del host cabe de sobra en ellos.
typedef struct { uint32_t flags; } DevBuilder;
typedef struct {
    uint32_t magic, flags, origin, depth_mode;
    uint64_t dbg_cb, dbg_data;
} Dev;
_Static_assert(sizeof(DevBuilder) <= 64 && sizeof(Dev) <= 12288, "los objetos del host deben caber en los de NVN");

// Valores del Tegra X1 (nvn_DeviceConstantsNX.h, medidos en SMO). TotK exige la ABI 55.15 o posterior.
static const int devinfo[] = {
    [0x00] = 55, [0x01] = 15,
    [0x02] = 14, [0x03] = 65536, [0x04] = 256, [0x05] = 8, [0x06] = 16, [0x07] = 4, [0x08] = 16, [0x09] = 32,
    [0x0A] = 16, [0x0B] = 4, [0x0C] = 4, [0x0D] = 4, [0x0E] = 16, [0x0F] = 32, [0x10] = 32, [0x11] = 256, [0x12] = 256,
    [0x13] = 4, [0x14] = 8, [0x15] = 512, [0x16] = 256, [0x17] = 2, [0x18] = 256, [0x19] = 4096, [0x1A] = 131072,
    [0x1B] = 16, [0x1C] = 1536, [0x1D] = 1024, [0x1E] = 64, [0x1F] = 1536, [0x20] = 65535, [0x21] = 65535, [0x22] = 65535,
    [0x23] = 8, [0x24] = 1048576, [0x25] = 4096, [0x26] = 16, [0x27] = 4096, [0x28] = 1, [0x29] = 1, [0x2A] = 1,
    [0x2B] = 262144, [0x2C] = 15, [0x2D] = 2048, [0x2E] = 1, [0x2F] = 1, [0x30] = 14, [0x31] = 5, [0x32] = 1, [0x33] = 8,
    [0x34] = 8, [0x35] = 4, [0x36] = 32, [0x37] = 128, [0x38] = 32, [0x39] = 128, [0x3A] = 65536, [0x3B] = 1, [0x3C] = 4,
    [0x3D] = 16384, [0x3E] = 134217728, [0x3F] = 2048, [0x40] = 16384, [0x41] = 16384, [0x42] = 1, [0x43] = 1, [0x44] = 1,
    [0x45] = 1, [0x46] = 5, [0x47] = 1, [0x48] = 1, [0x49] = 1, [0x4A] = 1, [0x4B] = 1, [0x4C] = 1, [0x4D] = 16,
    [0x4E] = 1024, [0x4F] = 32, [0x50] = 4096, [0x51] = 65536, [0x52] = 65536, [0x53] = 256, [0x54] = 16384,
    [0x55] = 262144, [0x56] = 4096, [0x57] = 1, [0x58] = 4, [0x59] = 2, [0x5A] = 0, [0x5B] = 4096, [0x5C] = 16384,
    [0x5D] = 4096, [0x5E] = 128, [0x5F] = 4096,   // 0x5F sale del límite del pool de samplers (sin verificar)
};

static void fatal(const char *msg, uint64_t v) {
    fflush(stdout);
    fprintf(stderr, "[RED] NVN: %s (0x%llx)\n", msg, (unsigned long long)v);
    exit(1);
}

// ---------- Dispositivo

// void nvnDeviceBuilderSetDefaults(NVNdeviceBuilder *b)
void nvnp_nvnDeviceBuilderSetDefaults(Ctx *c) { memset((void *)arg_x(c, 0), 0, sizeof(DevBuilder)); }
// void nvnDeviceBuilderSetFlags(NVNdeviceBuilder *b, int flags)
void nvnp_nvnDeviceBuilderSetFlags(Ctx *c) { ((DevBuilder *)arg_x(c, 0))->flags = (uint32_t)arg_x(c, 1); }

// NVNboolean nvnDeviceInitialize(NVNdevice *d, const NVNdeviceBuilder *b)
void nvnp_nvnDeviceInitialize(Ctx *c) {
    Dev *d = (Dev *)arg_x(c, 0);
    const DevBuilder *b = (const DevBuilder *)arg_x(c, 1);
    memset(d, 0, sizeof *d);
    d->magic = DEV_MAGIC;
    d->flags = b->flags;
    if (traza()) fprintf(stderr, "[nvn] DeviceInitialize %p flags 0x%x\n", (void *)d, d->flags);
    ret_x(c, 1);
}
// void nvnDeviceFinalize(NVNdevice *d)
void nvnp_nvnDeviceFinalize(Ctx *c) { ((Dev *)arg_x(c, 0))->magic = 0; }

// void nvnDeviceGetInteger(const NVNdevice *d, NVNdeviceInfo what, int *out)
void nvnp_nvnDeviceGetInteger(Ctx *c) {
    uint32_t w = (uint32_t)arg_x(c, 1);
    if (w >= sizeof devinfo / sizeof *devinfo) fatal("propiedad del dispositivo sin valor", w);
    *(int *)arg_x(c, 2) = devinfo[w];
    if (traza()) fprintf(stderr, "[nvn] DeviceGetInteger 0x%x -> %d\n", w, devinfo[w]);
}

// NVNboolean nvnDeviceRegisterFastClear*: el registro es solo una optimización; "no registrado" usa el borrado normal.
void nvnp_nvnDeviceRegisterFastClearColor(Ctx *c) { ret_x(c, 0); }
void nvnp_nvnDeviceRegisterFastClearColori(Ctx *c) { ret_x(c, 0); }
void nvnp_nvnDeviceRegisterFastClearColorui(Ctx *c) { ret_x(c, 0); }
void nvnp_nvnDeviceRegisterFastClearDepth(Ctx *c) { ret_x(c, 0); }

// NVNimageHandle nvnDeviceGetImageHandle(const NVNdevice *d, int id)
void nvnp_nvnDeviceGetImageHandle(Ctx *c) { ret_x(c, (uint64_t)(int64_t)(int)arg_x(c, 1)); }
// NVNtextureHandle nvnDeviceGetTextureHandle(const NVNdevice *d, int texture_id, int sampler_id):
// id de textura (20 bits) | id de sampler << 20.
void nvnp_nvnDeviceGetTextureHandle(Ctx *c) {
    int tid = (int)arg_x(c, 1), sid = (int)arg_x(c, 2);
    if (tid < 0 || tid >= (1 << 20) || sid < 0 || sid >= (1 << 12)) fatal("ids del handle de textura fuera de rango", arg_x(c, 1));
    ret_x(c, (uint64_t)tid | (uint64_t)sid << 20);
}
// NVNtextureHandle nvnDeviceGetTexelFetchHandle(const NVNdevice *d, int texture_id)
void nvnp_nvnDeviceGetTexelFetchHandle(Ctx *c) {
    int tid = (int)arg_x(c, 1);
    if (tid < 0 || tid >= (1 << 20)) fatal("id de texel fetch fuera de rango", arg_x(c, 1));
    ret_x(c, (uint64_t)tid);
}
// nvnDeviceGetSeparate{Texture,Sampler}Handle(const NVNdevice *d, int id): el índice; el de sampler va en el bit 20.
void nvnp_nvnDeviceGetSeparateTextureHandle(Ctx *c) { ret_x(c, (uint64_t)(int64_t)(int)arg_x(c, 1)); }
void nvnp_nvnDeviceGetSeparateSamplerHandle(Ctx *c) { ret_x(c, (uint64_t)(int64_t)(int)arg_x(c, 1) << 20); }

// void nvnDeviceInstallDebugCallback(NVNdevice *d, PFNNVNDEBUGCALLBACKPROC cb, void *data, NVNboolean enable)
void nvnp_nvnDeviceInstallDebugCallback(Ctx *c) {
    Dev *d = (Dev *)arg_x(c, 0);
    d->dbg_cb = (arg_x(c, 3) & 0xff) ? arg_x(c, 1) : 0;
    d->dbg_data = arg_x(c, 2);
}
// void nvnDeviceSetWindowOriginMode(NVNdevice *d, NVNwindowOriginMode mode): 0 = abajo a la izquierda (GL)
void nvnp_nvnDeviceSetWindowOriginMode(Ctx *c) { ((Dev *)arg_x(c, 0))->origin = (uint32_t)arg_x(c, 1); }
// void nvnDeviceSetDepthMode(NVNdevice *d, NVNdepthMode mode): 0 = near en -w (GL)
void nvnp_nvnDeviceSetDepthMode(Ctx *c) { ((Dev *)arg_x(c, 0))->depth_mode = (uint32_t)arg_x(c, 1); }
// void nvnDeviceFinalizeCommandHandle(NVNdevice *d, NVNcommandHandle h)
void nvnp_nvnDeviceFinalizeCommandHandle(Ctx *c) { (void)c; }
