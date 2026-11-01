// Dibujo de la GPU Maxwell sobre OpenGL 4.6, con un contexto EGL sin ventana.
//
// Cada draw del motor 3D se vuelve un draw de OpenGL:
//   - los shaders de la etapa de vértice y de fragmento se traducen a GLSL (nxshader.c) y se guardan
//     en una caché por dirección y contenido; cada uno es un programa separable de OpenGL;
//   - los constbufs y los vertex buffers se copian de la memoria del juego en cada draw;
//   - los render targets son texturas de OpenGL que reflejan la memoria del juego: se cargan antes
//     de dibujar si la CPU pudo cambiarla, y se escriben de vuelta (con su "block linear") antes de
//     cada syncpoint o semáforo, que es cuando el juego puede mirarlas.
// El contexto se toma al primer draw de un envío y se suelta al terminarlo.
#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
#include "nxgl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxgpu.h"
#include "nxshader.h"

#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT // de EXT_texture_sRGB, que glcorearb.h no trae
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

static EGLDisplay dpy;
static EGLContext ctx;
static int estado; // 0 sin probar, 1 listo, -1 sin OpenGL
static int actual; // el contexto está tomado por el hilo de la GPU
static GLuint pipeline, vao, fbo, ubo_ring, vbo_ring, ibo_ring;
static int traza = -1;
static uint32_t envio; // número de envío a la GPU (para no volver a mirar una textura en el mismo)

#define RING (64u << 20)
static GLintptr ubo_off, vbo_off, ibo_off;

static void APIENTRY depurar(GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei len, const GLchar *msg, const void *u) {
    (void)src, (void)id, (void)len, (void)u;
    if (type == GL_DEBUG_TYPE_ERROR || sev == GL_DEBUG_SEVERITY_HIGH) fprintf(stderr, "[nxgl] OpenGL: %s\n", msg);
}

static int iniciar(void) {
    if (estado) return estado > 0;
    estado = -1;
    if (getenv("NX_SIN_GPU")) return 0;
    PFNEGLQUERYDEVICESEXTPROC qd = (void *)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDeviceEXT devs[8];
    EGLint n = 0;
    if (!qd || !gpd || !qd(8, devs, &n) || n == 0) {
        fprintf(stderr, "[RED] GPU: no hay dispositivos EGL; no se dibuja\n");
        return 0;
    }
    for (int i = 0; i < n && !ctx; i++) {
        dpy = gpd(EGL_PLATFORM_DEVICE_EXT, devs[i], NULL);
        if (!dpy || !eglInitialize(dpy, NULL, NULL)) continue;
        eglBindAPI(EGL_OPENGL_API);
        EGLint ca[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 6,
                       EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                       EGL_CONTEXT_OPENGL_DEBUG, getenv("NX_TRAZA_GL") ? EGL_TRUE : EGL_FALSE, EGL_NONE};
        ctx = eglCreateContext(dpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ca);
    }
    if (!ctx || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "[RED] GPU: no pude crear un contexto OpenGL 4.6; no se dibuja\n");
        return 0;
    }
    actual = 1;
    traza = getenv("NX_TRAZA_GL") != NULL;
    if (traza) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(depurar, NULL);
    }
    fprintf(stderr, "[nxgl] %s, OpenGL %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glCreateProgramPipelines(1, &pipeline);
    glBindProgramPipeline(pipeline);
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glCreateFramebuffers(1, &fbo);
    GLuint b[3];
    glCreateBuffers(3, b);
    ubo_ring = b[0], vbo_ring = b[1], ibo_ring = b[2];
    glNamedBufferData(ubo_ring, RING + 65536, NULL, GL_STREAM_DRAW);
    glNamedBufferData(vbo_ring, RING, NULL, GL_STREAM_DRAW);
    glNamedBufferData(ibo_ring, RING, NULL, GL_STREAM_DRAW);
    glVertexArrayElementBuffer(vao, ibo_ring);
    estado = 1;
    return 1;
}

static void tomar(void) {
    if (!actual) {
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
        actual = 1;
    }
}

// Copia `size` bytes a un anillo y devuelve el offset (alineado a `align`).
static GLintptr subir(GLuint buf, GLintptr *off, const void *data, size_t size, size_t align) {
    GLintptr o = (*off + align - 1) & ~(GLintptr)(align - 1);
    if (o + size > RING) {
        glFinish(); // el anillo da la vuelta: que nada lo esté usando
        o = 0;
    }
    glNamedBufferSubData(buf, o, size, data);
    *off = o + size;
    return o;
}

// ---------- render targets ----------

typedef struct {
    uint32_t fmt;
    GLenum internal, format, type;
    uint32_t bpp;
} Formato;

static const Formato formatos[] = {
    {0xC0, GL_RGBA32F, GL_RGBA, GL_FLOAT, 16},
    {0xC6, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 8},
    {0xCA, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8},
    {0xCB, GL_RG32F, GL_RG, GL_FLOAT, 8},
    {0xCE, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8},
    {0xCF, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4},
    {0xD0, GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, 4},
    {0xD1, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4},
    {0xD5, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4},
    {0xD6, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4},
    {0xD7, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4},
    {0xDA, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4},
    {0xDE, GL_RG16F, GL_RG, GL_HALF_FLOAT, 4},
    {0xE0, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4},
    {0xE5, GL_R32F, GL_RED, GL_FLOAT, 4},
    {0xE6, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4},
    {0xE8, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2},
    {0xEA, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2},
    {0xEE, GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2},
    {0xF2, GL_R16F, GL_RED, GL_HALF_FLOAT, 2},
    {0xF3, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1},
};

static const Formato *formato(uint32_t fmt) {
    for (size_t i = 0; i < sizeof formatos / sizeof *formatos; i++)
        if (formatos[i].fmt == fmt) return &formatos[i];
    return NULL;
}

typedef struct {
    uint64_t addr;
    uint32_t w, h, fmt, tile;
    const Formato *f;
    GLuint tex;
    int stale, dirty;
} Rt;

static Rt rts[64];
static int nrts;
static uint8_t *tmp;
static size_t tmp_cap;

static uint64_t rt_bytes(const Rt *r) {
    if (r->tile >> 12 & 1) return (uint64_t)r->w * r->f->bpp * r->h;
    uint32_t bh = 8u << (r->tile >> 4 & 15);
    uint32_t gobs = (r->w * r->f->bpp + 63) / 64;
    return (uint64_t)gobs * 64 * ((r->h + bh - 1) / bh * bh);
}

// Memoria del juego <-> imagen lineal (filas de w*bpp bytes).
static void rt_copia(Rt *r, uint8_t *lin, int a_memoria) {
    uint64_t avail;
    uint8_t *mem = nx_gpu_ptr_n(r->addr, &avail);
    if (!mem || avail < rt_bytes(r)) {
        fprintf(stderr, "[RED] GPU: el render target en 0x%llx no cabe en su memoria (%llu de %llu bytes)\n",
                (unsigned long long)r->addr, (unsigned long long)avail, (unsigned long long)rt_bytes(r));
        return;
    }
    uint32_t bpp = r->f->bpp, row = r->w * bpp;
    if (r->tile >> 12 & 1) {
        for (uint32_t y = 0; y < r->h; y++)
            if (a_memoria) memcpy(mem + (uint64_t)y * row, lin + (uint64_t)y * row, row);
            else memcpy(lin + (uint64_t)y * row, mem + (uint64_t)y * row, row);
        return;
    }
    uint32_t bhl = r->tile >> 4 & 15, gobs = (row + 63) / 64;
    for (uint32_t y = 0; y < r->h; y++)
        for (uint32_t x = 0; x < row; x += 16) { // de a 16 bytes: un trozo contiguo dentro del GOB
            uint64_t o = nx_bl_offset(x, y, gobs, bhl);
            uint32_t k = row - x < 16 ? row - x : 16;
            if (a_memoria) memcpy(mem + o, lin + (uint64_t)y * row + x, k);
            else memcpy(lin + (uint64_t)y * row + x, mem + o, k);
        }
}

static uint8_t *tmp_buf(size_t n) {
    if (n > tmp_cap) tmp = realloc(tmp, tmp_cap = n);
    return tmp;
}

static void rt_cargar(Rt *r) {
    if (traza) fprintf(stderr, "[nxgl] cargo el render target 0x%llx\n", (unsigned long long)r->addr);
    uint8_t *lin = tmp_buf((size_t)r->w * r->h * r->f->bpp);
    rt_copia(r, lin, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(r->tex, 0, 0, 0, r->w, r->h, r->f->format, r->f->type, lin);
    r->stale = 0;
}

static void rt_escribir(Rt *r) {
    if (traza) fprintf(stderr, "[nxgl] escribo el render target 0x%llx\n", (unsigned long long)r->addr);
    size_t n = (size_t)r->w * r->h * r->f->bpp;
    uint8_t *lin = tmp_buf(n);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(r->tex, 0, r->f->format, r->f->type, (GLsizei)n, lin);
    rt_copia(r, lin, 1);
    r->dirty = 0;
}

static Rt *rt_buscar(uint64_t addr, uint32_t w, uint32_t h, uint32_t fmt, uint32_t tile) {
    for (int i = 0; i < nrts; i++)
        if (rts[i].addr == addr && rts[i].w == w && rts[i].h == h && rts[i].fmt == fmt && rts[i].tile == tile)
            return &rts[i];
    const Formato *f = formato(fmt);
    if (!f) {
        static uint32_t avisado[16];
        for (int i = 0; i < 16; i++)
            if (avisado[i] == fmt) return NULL;
            else if (!avisado[i]) {
                avisado[i] = fmt;
                fprintf(stderr, "[RED] GPU: render target con formato 0x%x\n", fmt);
                break;
            }
        return NULL;
    }
    // Otro render target en la misma dirección con otra forma: el viejo ya no sirve.
    for (int i = 0; i < nrts; i++)
        if (rts[i].addr == addr) {
            if (rts[i].dirty) rt_escribir(&rts[i]);
            glDeleteTextures(1, &rts[i].tex);
            rts[i] = rts[--nrts];
            break;
        }
    if (nrts == 64) {
        if (rts[0].dirty) rt_escribir(&rts[0]);
        glDeleteTextures(1, &rts[0].tex);
        rts[0] = rts[--nrts];
    }
    Rt *r = &rts[nrts++];
    *r = (Rt){addr, w, h, fmt, tile, f, 0, 1, 0};
    glCreateTextures(GL_TEXTURE_2D, 1, &r->tex);
    glTextureStorage2D(r->tex, 1, f->internal, w, h);
    return r;
}

void nx_gl_flush(void) {
    if (estado <= 0) return;
    for (int i = 0; i < nrts; i++)
        if (rts[i].dirty) {
            tomar();
            rt_escribir(&rts[i]);
        }
}

void nx_gl_release(void) {
    if (estado <= 0 || !actual) return;
    nx_gl_flush();
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    actual = 0;
}


void nx_gl_invalidate_all(void) {
    envio++;
    for (int i = 0; i < nrts; i++)
        if (!rts[i].dirty) rts[i].stale = 1;
}

// ---------- shaders ----------

typedef struct {
    uint64_t addr;
    int stage;
    NxShaderKey key;
    uint8_t *code;
    uint32_t size;
    GLuint prog;
    NxShaderInfo info;
} Shader;

static Shader *shaders;
static int nshaders, cap_shaders;

static Shader *shader(uint64_t addr, int stage, const NxShaderKey *key) {
    uint64_t avail;
    const uint8_t *code = nx_gpu_ptr_n(addr, &avail);
    if (!code) return NULL;
    for (int i = 0; i < nshaders; i++) {
        Shader *s = &shaders[i];
        if (s->addr == addr && s->stage == stage && !memcmp(&s->key, key, sizeof *key) && s->size <= avail &&
            !memcmp(s->code, code, s->size))
            return s->prog ? s : NULL;
    }
    if (nshaders == cap_shaders) shaders = realloc(shaders, sizeof *shaders * (cap_shaders = cap_shaders * 2 + 16));
    Shader *s = &shaders[nshaders++];
    memset(s, 0, sizeof *s);
    s->addr = addr, s->stage = stage, s->key = *key;
    char *glsl = nx_shader_glsl(code, avail, stage, key, &s->info);
    s->size = s->info.code_size;
    s->code = malloc(s->size);
    memcpy(s->code, code, s->size);
    if (!glsl) return NULL;
    if (traza) fprintf(stderr, "[nxgl] shader de etapa %d en 0x%llx:\n%s", stage, (unsigned long long)addr, glsl);
    const char *src = glsl;
    s->prog = glCreateShaderProgramv(stage == NX_SH_FRAGMENT ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER, 1, &src);
    GLint ok = 0;
    glGetProgramiv(s->prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(s->prog, sizeof log, NULL, log);
        fprintf(stderr, "[RED] GPU: el shader traducido de 0x%llx no compila:\n%s\n%s", (unsigned long long)addr, log, glsl);
        glDeleteProgram(s->prog);
        s->prog = 0;
    }
    free(glsl);
    return s->prog ? s : NULL;
}


// ---------- texturas ----------
//
// Un handle de textura (en el constbuf que elige SetBindlessTexture) tiene el índice del descriptor
// de imagen (TIC, 20 bits) y el del sampler (TSC, 12 bits); los dos son entradas de 32 bytes en sus
// pools. Las texturas se copian a OpenGL deshaciendo el "block linear" de cada nivel de mipmap y se
// guardan en una caché; en cada envío se vuelve a mirar si la memoria cambió.

typedef struct {
    uint8_t fmt, comp; // formato de imagen del TIC y tipo de componente (0 = cualquiera)
    GLenum internal, srgb, format, type;
    uint8_t bpb, bw, bh, depth; // bytes por bloque, tamaño del bloque, es de profundidad
} TexFmt;

static const TexFmt tex_fmts[] = {
    {0x01, 7, GL_RGBA32F, 0, GL_RGBA, GL_FLOAT, 16, 1, 1, 0},
    {0x01, 4, GL_RGBA32UI, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16, 1, 1, 0},
    {0x01, 3, GL_RGBA32I, 0, GL_RGBA_INTEGER, GL_INT, 16, 1, 1, 0},
    {0x02, 0, GL_RGB32F, 0, GL_RGB, GL_FLOAT, 12, 1, 1, 0},
    {0x03, 7, GL_RGBA16F, 0, GL_RGBA, GL_HALF_FLOAT, 8, 1, 1, 0},
    {0x03, 2, GL_RGBA16, 0, GL_RGBA, GL_UNSIGNED_SHORT, 8, 1, 1, 0},
    {0x03, 1, GL_RGBA16_SNORM, 0, GL_RGBA, GL_SHORT, 8, 1, 1, 0},
    {0x03, 4, GL_RGBA16UI, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8, 1, 1, 0},
    {0x03, 3, GL_RGBA16I, 0, GL_RGBA_INTEGER, GL_SHORT, 8, 1, 1, 0},
    {0x04, 7, GL_RG32F, 0, GL_RG, GL_FLOAT, 8, 1, 1, 0},
    {0x04, 4, GL_RG32UI, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, 8, 1, 1, 0},
    {0x04, 3, GL_RG32I, 0, GL_RG_INTEGER, GL_INT, 8, 1, 1, 0},
    {0x07, 2, GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 1, 1, 0},
    {0x08, 2, GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 1, 1, 0},
    {0x08, 1, GL_RGBA8_SNORM, 0, GL_RGBA, GL_BYTE, 4, 1, 1, 0},
    {0x08, 4, GL_RGBA8UI, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4, 1, 1, 0},
    {0x08, 3, GL_RGBA8I, 0, GL_RGBA_INTEGER, GL_BYTE, 4, 1, 1, 0},
    {0x09, 2, GL_RGB10_A2, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4, 1, 1, 0},
    {0x09, 4, GL_RGB10_A2UI, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 4, 1, 1, 0},
    {0x0C, 7, GL_RG16F, 0, GL_RG, GL_HALF_FLOAT, 4, 1, 1, 0},
    {0x0C, 2, GL_RG16, 0, GL_RG, GL_UNSIGNED_SHORT, 4, 1, 1, 0},
    {0x0C, 1, GL_RG16_SNORM, 0, GL_RG, GL_SHORT, 4, 1, 1, 0},
    {0x0C, 4, GL_RG16UI, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4, 1, 1, 0},
    {0x0C, 3, GL_RG16I, 0, GL_RG_INTEGER, GL_SHORT, 4, 1, 1, 0},
    {0x0F, 7, GL_R32F, 0, GL_RED, GL_FLOAT, 4, 1, 1, 0},
    {0x0F, 4, GL_R32UI, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 4, 1, 1, 0},
    {0x0F, 3, GL_R32I, 0, GL_RED_INTEGER, GL_INT, 4, 1, 1, 0},
    {0x12, 0, GL_RGBA4, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 2, 1, 1, 0},
    {0x14, 0, GL_RGB5_A1, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 2, 1, 1, 0},
    {0x15, 0, GL_RGB565, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, 2, 1, 1, 0},
    {0x18, 2, GL_RG8, 0, GL_RG, GL_UNSIGNED_BYTE, 2, 1, 1, 0},
    {0x18, 1, GL_RG8_SNORM, 0, GL_RG, GL_BYTE, 2, 1, 1, 0},
    {0x18, 4, GL_RG8UI, 0, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2, 1, 1, 0},
    {0x18, 3, GL_RG8I, 0, GL_RG_INTEGER, GL_BYTE, 2, 1, 1, 0},
    {0x1B, 7, GL_R16F, 0, GL_RED, GL_HALF_FLOAT, 2, 1, 1, 0},
    {0x1B, 2, GL_R16, 0, GL_RED, GL_UNSIGNED_SHORT, 2, 1, 1, 0},
    {0x1B, 1, GL_R16_SNORM, 0, GL_RED, GL_SHORT, 2, 1, 1, 0},
    {0x1B, 4, GL_R16UI, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2, 1, 1, 0},
    {0x1B, 3, GL_R16I, 0, GL_RED_INTEGER, GL_SHORT, 2, 1, 1, 0},
    {0x1D, 2, GL_R8, 0, GL_RED, GL_UNSIGNED_BYTE, 1, 1, 1, 0},
    {0x1D, 1, GL_R8_SNORM, 0, GL_RED, GL_BYTE, 1, 1, 1, 0},
    {0x1D, 4, GL_R8UI, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1, 1, 1, 0},
    {0x1D, 3, GL_R8I, 0, GL_RED_INTEGER, GL_BYTE, 1, 1, 1, 0},
    {0x20, 0, GL_RGB9_E5, 0, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 4, 1, 1, 0},
    {0x21, 0, GL_R11F_G11F_B10F, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4, 1, 1, 0},
    {0x24, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0, 8, 4, 4, 0},
    {0x25, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0, 0, 16, 4, 4, 0},
    {0x26, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0, 16, 4, 4, 0},
    {0x27, 1, GL_COMPRESSED_SIGNED_RED_RGTC1, 0, 0, 0, 8, 4, 4, 0},
    {0x27, 0, GL_COMPRESSED_RED_RGTC1, 0, 0, 0, 8, 4, 4, 0},
    {0x28, 1, GL_COMPRESSED_SIGNED_RG_RGTC2, 0, 0, 0, 16, 4, 4, 0},
    {0x28, 0, GL_COMPRESSED_RG_RGTC2, 0, 0, 0, 16, 4, 4, 0},
    {0x17, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 0, 0, 16, 4, 4, 0},
    {0x10, 0, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, 0, 0, 0, 16, 4, 4, 0},
    {0x11, 0, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 0, 0, 0, 16, 4, 4, 0},
    {0x29, 0, GL_DEPTH24_STENCIL8, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4, 1, 1, 1},
    {0x2F, 0, GL_DEPTH_COMPONENT32F, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 4, 1, 1, 1},
    {0x30, 0, GL_DEPTH32F_STENCIL8, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8, 1, 1, 1},
    {0x3A, 0, GL_DEPTH_COMPONENT16, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2, 1, 1, 1},
};

static const TexFmt *tex_fmt(uint32_t fmt, uint32_t comp) {
    for (size_t i = 0; i < sizeof tex_fmts / sizeof *tex_fmts; i++)
        if (tex_fmts[i].fmt == fmt && (tex_fmts[i].comp == 0 || tex_fmts[i].comp == comp)) return &tex_fmts[i];
    for (size_t i = 0; i < sizeof tex_fmts / sizeof *tex_fmts; i++) // el tipo no coincide: el primero del formato
        if (tex_fmts[i].fmt == fmt) return &tex_fmts[i];
    return NULL;
}

// Lo que importa de un TIC.
typedef struct {
    uint64_t addr;
    uint32_t fmt, comp, swz[4], srgb, version, type;
    uint32_t w, h, d, levels, pitch, bhl, bdl;
} Tic;

static void tic_leer(const uint32_t *w, Tic *t) {
    t->fmt = w[0] & 0x7F;
    t->comp = w[0] >> 7 & 7;
    for (int i = 0; i < 4; i++) t->swz[i] = w[0] >> (19 + i * 3) & 7;
    t->addr = (uint64_t)(w[2] & 0xFFFF) << 32 | w[1];
    t->version = w[2] >> 21 & 7;
    t->pitch = (w[3] & 0xFFFF) << 5;
    t->bhl = w[3] >> 3 & 7;
    t->bdl = w[3] >> 6 & 7;
    t->levels = (w[3] >> 28 & 15) + 1;
    t->w = (w[4] & 0xFFFF) + 1;
    t->srgb = w[4] >> 22 & 1;
    t->type = w[4] >> 23 & 15;
    t->h = (w[5] & 0xFFFF) + 1;
    t->d = (w[5] >> 16 & 0x3FFF) + 1;
    if (t->version < 3) t->levels = 1;
}

// Bloques "block linear" con profundidad: el alto (y la profundidad) del bloque se achica en los
// niveles chicos, como hace el hardware (la regla de yuzu, AdjustTileSize).
static uint32_t ajustar(uint32_t shift, uint32_t unidad, uint32_t dim) {
    if (shift == 0) return 0;
    uint32_t x = unidad << (shift - 1);
    if (x >= dim)
        while (--shift) {
            x >>= 1;
            if (x < dim) break;
        }
    return shift;
}

static uint64_t bl3(uint32_t xb, uint32_t y, uint32_t z, uint32_t wgobs, uint32_t rows, uint32_t bhl, uint32_t bdl) {
    uint32_t gh = 1u << bhl, gd = 1u << bdl;
    uint32_t bpc = (rows + 8 * gh - 1) / (8 * gh); // bloques por columna
    uint64_t block = ((uint64_t)(z / gd) * bpc + y / (8 * gh)) * wgobs + xb / 64;
    uint64_t gob = (z % gd) * gh + (y / 8) % gh;
    uint32_t gx = xb % 64, gy = y % 8;
    return block * 512 * gh * gd + gob * 512 + (gx / 32) * 256 + (gy / 2) * 64 + ((gx % 32) / 16) * 32 + (gy % 2) * 16 + gx % 16;
}

typedef struct {
    uint32_t tic[8];
    uint64_t addr;
    GLenum target;
    GLuint tex;
    uint64_t hash, bytes;
    uint32_t visto; // envío en que se comprobó la memoria por última vez
} Tex;

static Tex *texs;
static int ntexs, cap_texs;
static GLuint samplers_obj[256];
static uint32_t samplers_tsc[256][8];
static int nsamplers;

static uint64_t hash_mem(const uint8_t *p, uint64_t n) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
    uint64_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t v;
        memcpy(&v, p + i, 8);
        h = (h ^ v) * 0x100000001B3ull;
        h ^= h >> 29;
    }
    for (; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// Tamaño en bytes de la textura completa (todas las capas y niveles) en la memoria del juego.
static uint64_t tex_layout(const Tic *t, const TexFmt *f, uint32_t layers, uint64_t *level_off, uint64_t *layer_size) {
    if (t->version < 3) { // lineal con pitch
        *layer_size = (uint64_t)t->pitch * ((t->h + f->bh - 1) / f->bh);
        level_off[0] = 0;
        return *layer_size * layers;
    }
    uint64_t off = 0;
    uint32_t d3 = t->type == 2 ? t->d : 1;
    for (uint32_t l = 0; l < t->levels; l++) {
        uint32_t w = t->w >> l ? t->w >> l : 1, h = t->h >> l ? t->h >> l : 1, d = d3 >> l ? d3 >> l : 1;
        uint32_t bw = (w + f->bw - 1) / f->bw, rows = (h + f->bh - 1) / f->bh;
        uint32_t bhl = ajustar(t->bhl, 8, rows), bdl = ajustar(t->bdl, 1, d);
        uint32_t gh = 1u << bhl, gd = 1u << bdl, wgobs = (bw * f->bpb + 63) / 64;
        level_off[l] = off;
        off += (uint64_t)wgobs * 512 * ((rows + 8 * gh - 1) / (8 * gh)) * gh * ((d + gd - 1) / gd) * gd;
    }
    if (layers > 1) { // las capas van alineadas al bloque del nivel 0
        uint32_t rows = (t->h + f->bh - 1) / f->bh;
        uint32_t shift = 9 + ajustar(t->bhl, 8, rows) + ajustar(t->bdl, 1, d3);
        off = (off + (1ull << shift) - 1) >> shift << shift;
    }
    *layer_size = off;
    return off * layers;
}

static void tex_subir(Tex *x, const Tic *t, const TexFmt *f, const uint8_t *mem, uint32_t layers) {
    uint64_t level_off[16], layer_size;
    tex_layout(t, f, layers, level_off, &layer_size);
    uint32_t d3 = t->type == 2 ? t->d : 1;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (uint32_t l = 0; l < t->levels; l++) {
        uint32_t w = t->w >> l ? t->w >> l : 1, h = t->h >> l ? t->h >> l : 1, d = d3 >> l ? d3 >> l : 1;
        uint32_t bw = (w + f->bw - 1) / f->bw, rows = (h + f->bh - 1) / f->bh, row = bw * f->bpb;
        uint32_t bhl = ajustar(t->bhl, 8, rows), bdl = ajustar(t->bdl, 1, d);
        uint32_t wgobs = (row + 63) / 64;
        uint32_t nz = t->type == 2 ? d : layers;
        size_t n = (size_t)row * rows * nz;
        uint8_t *lin = tmp_buf(n);
        for (uint32_t z = 0; z < nz; z++) {
            const uint8_t *base = mem + (t->type == 2 ? level_off[l] : layer_size * z + level_off[l]);
            uint32_t zz = t->type == 2 ? z : 0;
            for (uint32_t y = 0; y < rows; y++) {
                uint8_t *dst = lin + ((size_t)z * rows + y) * row;
                if (t->version < 3) memcpy(dst, base + (uint64_t)y * t->pitch, row);
                else
                    for (uint32_t xb = 0; xb < row; xb += 16) {
                        uint32_t k = row - xb < 16 ? row - xb : 16;
                        memcpy(dst + xb, base + bl3(xb, y, zz, wgobs, rows, bhl, bdl), k);
                    }
            }
        }
        int comprimido = f->format == 0;
        GLenum fmt = f->format;
        switch (x->target) {
        case GL_TEXTURE_2D:
            if (comprimido) glCompressedTextureSubImage2D(x->tex, l, 0, 0, w, h, f->internal, (GLsizei)((size_t)row * rows), lin);
            else glTextureSubImage2D(x->tex, l, 0, 0, w, h, fmt, f->type, lin);
            break;
        default: // 3D, arreglos y cubos: z es la profundidad, la capa o la cara
            if (comprimido) glCompressedTextureSubImage3D(x->tex, l, 0, 0, 0, w, h, nz, f->internal, (GLsizei)n, lin);
            else glTextureSubImage3D(x->tex, l, 0, 0, 0, w, h, nz, fmt, f->type, lin);
        }
    }
}

static GLuint textura(const uint32_t *tic_words, int shader_type) {
    Tic t;
    tic_leer(tic_words, &t);
    // Una textura que es un render target se lee directo de la GPU.
    for (int i = 0; i < nrts; i++)
        if (rts[i].addr == t.addr && (shader_type & 15) == NX_TEX_2D) {
            if (rts[i].stale) rt_cargar(&rts[i]);
            return rts[i].tex;
        }
    GLenum target;
    switch (shader_type & 15) {
    case NX_TEX_3D: target = GL_TEXTURE_3D; break;
    case NX_TEX_CUBE: target = GL_TEXTURE_CUBE_MAP; break;
    case NX_TEX_2D_ARRAY: target = GL_TEXTURE_2D_ARRAY; break;
    case NX_TEX_CUBE_ARRAY: target = GL_TEXTURE_CUBE_MAP_ARRAY; break;
    default: target = GL_TEXTURE_2D;
    }
    const TexFmt *f = tex_fmt(t.fmt, t.comp);
    if (!f) {
        static uint32_t avisado[32];
        for (int i = 0; i < 32; i++)
            if (avisado[i] == t.fmt + 1) break;
            else if (!avisado[i]) {
                avisado[i] = t.fmt + 1;
                fprintf(stderr, "[RED] GPU: textura con formato 0x%x (componente %u)\n", t.fmt, t.comp);
                break;
            }
        return 0;
    }
    uint32_t layers = target == GL_TEXTURE_CUBE_MAP ? 6 : target == GL_TEXTURE_2D_ARRAY ? (t.type == 5 || t.type == 4 ? t.d : 1)
                    : target == GL_TEXTURE_CUBE_MAP_ARRAY ? t.d * 6 : 1;
    if (t.type == 3 && target == GL_TEXTURE_CUBE_MAP) layers = 6;
    uint64_t level_off[16], layer_size;
    uint64_t bytes = tex_layout(&t, f, target == GL_TEXTURE_3D ? 1 : layers, level_off, &layer_size);
    uint64_t avail;
    const uint8_t *mem = nx_gpu_ptr_n(t.addr, &avail);
    if (!mem || avail < bytes) return 0;
    Tex *x = NULL;
    for (int i = 0; i < ntexs; i++)
        if (texs[i].target == target && !memcmp(texs[i].tic, tic_words, 32)) {
            x = &texs[i];
            break;
        }
    if (x && x->visto == envio) return x->tex;
    uint64_t h = hash_mem(mem, bytes);
    if (x) {
        x->visto = envio;
        if (x->hash == h) return x->tex;
        x->hash = h;
        tex_subir(x, &t, f, mem, layers);
        return x->tex;
    }
    if (ntexs == cap_texs) texs = realloc(texs, sizeof *texs * (cap_texs = cap_texs * 2 + 64));
    x = &texs[ntexs++];
    memset(x, 0, sizeof *x);
    memcpy(x->tic, tic_words, 32);
    x->target = target, x->hash = h, x->bytes = bytes, x->visto = envio, x->addr = t.addr;
    glCreateTextures(target, 1, &x->tex);
    GLenum internal = t.srgb && f->srgb ? f->srgb : f->internal;
    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) glTextureStorage2D(x->tex, t.levels, internal, t.w, t.h);
    else glTextureStorage3D(x->tex, t.levels, internal, t.w, t.h, target == GL_TEXTURE_3D ? t.d : layers);
    glTextureParameteri(x->tex, GL_TEXTURE_MAX_LEVEL, t.levels - 1);
    if (!f->depth) { // el swizzle del descriptor: 0 cero, 2-5 R/G/B/A, 6-7 uno
        GLint sw[4];
        for (int i = 0; i < 4; i++) {
            uint32_t s = t.swz[i];
            sw[i] = s == 2 ? GL_RED : s == 3 ? GL_GREEN : s == 4 ? GL_BLUE : s == 5 ? GL_ALPHA : s >= 6 ? GL_ONE : GL_ZERO;
        }
        glTextureParameteriv(x->tex, GL_TEXTURE_SWIZZLE_RGBA, sw);
    }
    tex_subir(x, &t, f, mem, layers);
    if (traza)
        fprintf(stderr, "[nxgl] textura 0x%llx: formato 0x%x/%u %ux%ux%u, %u niveles, tipo %u, versión %u, bloque %u/%u, swizzle %u%u%u%u\n",
                (unsigned long long)t.addr, t.fmt, t.comp, t.w, t.h, t.d, t.levels, t.type, t.version, t.bhl, t.bdl, t.swz[0],
                t.swz[1], t.swz[2], t.swz[3]);
    return x->tex;
}

static GLuint sampler(const uint32_t *tsc) {
    for (int i = 0; i < nsamplers; i++)
        if (!memcmp(samplers_tsc[i], tsc, 32)) return samplers_obj[i];
    if (nsamplers == 256) {
        glDeleteSamplers(1, &samplers_obj[0]);
        nsamplers = 0;
    }
    GLuint s;
    glCreateSamplers(1, &s);
    static const GLenum wrap[] = {GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER,
                                  GL_CLAMP_TO_EDGE, GL_MIRROR_CLAMP_TO_EDGE, GL_MIRROR_CLAMP_TO_EDGE, GL_MIRROR_CLAMP_TO_EDGE};
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wrap[tsc[0] & 7]);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wrap[tsc[0] >> 3 & 7]);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_R, wrap[tsc[0] >> 6 & 7]);
    if (tsc[0] >> 9 & 1) {
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_FUNC, GL_NEVER + (tsc[0] >> 10 & 7));
    }
    uint32_t mag = tsc[1] & 3, min = tsc[1] >> 4 & 3, mip = tsc[1] >> 6 & 3;
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, mag == 2 ? GL_LINEAR : GL_NEAREST);
    GLenum mf = min == 2 ? (mip == 3 ? GL_LINEAR_MIPMAP_LINEAR : mip == 2 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR)
                         : (mip == 3 ? GL_NEAREST_MIPMAP_LINEAR : mip == 2 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, mf);
    int32_t bias = (int32_t)(tsc[1] << 7) >> 19; // 13 bits con signo, 8 de fracción
    glSamplerParameterf(s, GL_TEXTURE_LOD_BIAS, bias / 256.0f);
    glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, (tsc[2] & 0xFFF) / 256.0f);
    glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, (tsc[2] >> 12 & 0xFFF) / 256.0f);
    static const float aniso[] = {1, 2, 4, 6, 8, 10, 12, 16};
    glSamplerParameterf(s, GL_TEXTURE_MAX_ANISOTROPY, aniso[tsc[0] >> 20 & 7]);
    float border[4];
    memcpy(border, &tsc[4], 16);
    glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, border);
    memcpy(samplers_tsc[nsamplers], tsc, 32);
    samplers_obj[nsamplers++] = s;
    return s;
}

static void texturas(const NxDraw *d, int stage, const NxShaderInfo *info, int base) {
    const uint32_t *R = d->regs;
    int slot = R[0x982] & 7;
    uint64_t cb = d->cb_addr[stage][slot];
    uint64_t tic_pool = (uint64_t)R[0x55D] << 32 | R[0x55E], tsc_pool = (uint64_t)R[0x557] << 32 | R[0x558];
    for (int i = 0; i < info->ntex; i++) {
        uint64_t avail;
        const uint32_t *hp = cb ? nx_gpu_ptr_n(cb + info->tex[i].handle * 4u, &avail) : NULL;
        GLuint tex = 0, smp = 0;
        if (hp && avail >= 4) {
            uint32_t h = *hp, tic = h & 0xFFFFF, tsc = R[0x48D] & 1 ? tic : h >> 20;
            const uint32_t *tw = nx_gpu_ptr_n(tic_pool + tic * 32ull, &avail);
            if (tw && avail >= 32) tex = textura(tw, info->tex[i].type);
            const uint32_t *sw = nx_gpu_ptr_n(tsc_pool + tsc * 32ull, &avail);
            if (sw && avail >= 32) smp = sampler(sw);
        }
        glBindTextureUnit(base + i, tex);
        glBindSampler(base + i, smp);
    }
}

void nx_gl_cpu_write(uint64_t gpu, uint64_t size) {
    if (estado <= 0) return;
    for (int i = 0; i < ntexs; i++) // las texturas de esa memoria se vuelven a mirar
        if (texs[i].addr < gpu + size && gpu < texs[i].addr + texs[i].bytes) texs[i].visto = 0;
    for (int i = 0; i < nrts; i++)
        if (rts[i].addr < gpu + size && gpu < rts[i].addr + rt_bytes(&rts[i])) {
            if (rts[i].dirty) {
                tomar();
                rt_escribir(&rts[i]);
            }
            rts[i].stale = 1;
        }
}

// ---------- estado ----------

static GLenum gl_compare(uint32_t v) {
    if (v >= 0x200 && v <= 0x207) return v;
    if (v >= 1 && v <= 8) return GL_NEVER + v - 1;
    return GL_ALWAYS;
}
static GLenum gl_blend_eq(uint32_t v) {
    switch (v) {
    case 1: return GL_FUNC_ADD;
    case 2: return GL_FUNC_SUBTRACT;
    case 3: return GL_FUNC_REVERSE_SUBTRACT;
    case 4: return GL_MIN;
    case 5: return GL_MAX;
    }
    return v >= 0x8000 ? v : GL_FUNC_ADD;
}
static GLenum gl_blend_factor(uint32_t v) {
    if (v & 0x4000) {
        v &= 0x3FFF;
        return v == 0 ? GL_ZERO : v == 1 ? GL_ONE : v;
    }
    switch (v) {
    case 1: return GL_ZERO;
    case 2: return GL_ONE;
    case 3: return GL_SRC_COLOR;
    case 4: return GL_ONE_MINUS_SRC_COLOR;
    case 5: return GL_SRC_ALPHA;
    case 6: return GL_ONE_MINUS_SRC_ALPHA;
    case 7: return GL_DST_ALPHA;
    case 8: return GL_ONE_MINUS_DST_ALPHA;
    case 9: return GL_DST_COLOR;
    case 10: return GL_ONE_MINUS_DST_COLOR;
    case 11: return GL_SRC_ALPHA_SATURATE;
    case 16: return GL_SRC1_COLOR;
    case 17: return GL_ONE_MINUS_SRC1_COLOR;
    case 18: return GL_SRC1_ALPHA;
    case 19: return GL_ONE_MINUS_SRC1_ALPHA;
    case 0x21: return GL_CONSTANT_COLOR;
    case 0x22: return GL_ONE_MINUS_CONSTANT_COLOR;
    case 0x23: return GL_CONSTANT_ALPHA;
    case 0x24: return GL_ONE_MINUS_CONSTANT_ALPHA;
    }
    return GL_ONE;
}
static float f32(uint32_t v) {
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static void estado_raster(const uint32_t *R) {
    // Viewport 0: la GPU hace x = sx * ndc + tx; OpenGL necesita origen, tamaño y hacia dónde va y.
    float sx = f32(R[0x280]), sy = f32(R[0x281]), sz = f32(R[0x282]);
    float tx = f32(R[0x283]), ty = f32(R[0x284]), tz = f32(R[0x285]);
    if (!(R[0x64B] & 1)) sx = 1, sy = 1, tx = 0, ty = 0;
    float ax = sx < 0 ? -sx : sx, ay = sy < 0 ? -sy : sy;
    int zero_uno = R[0x64F] & 1;
    glClipControl(sy < 0 ? GL_UPPER_LEFT : GL_LOWER_LEFT, zero_uno ? GL_ZERO_TO_ONE : GL_NEGATIVE_ONE_TO_ONE);
    glViewportIndexedf(0, tx - ax, ty - ay, 2 * ax, 2 * ay);
    if (zero_uno) glDepthRangeIndexed(0, tz, tz + sz);
    else glDepthRangeIndexed(0, tz - sz, tz + sz);
    if (R[0x380] & 1) {
        uint32_t x0 = R[0x381] & 0xFFFF, x1 = R[0x381] >> 16, y0 = R[0x382] & 0xFFFF, y1 = R[0x382] >> 16;
        glEnable(GL_SCISSOR_TEST);
        glScissorIndexed(0, x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    // Caras: la GPU mide el giro sobre las coordenadas de ventana tal cual (filas hacia abajo), al
    // revés si SetWindowOrigin.FlipY. OpenGL con origen arriba (UPPER_LEFT) lo mide invertido.
    int ccw = R[0x647] == 0x901 || R[0x647] == 1, flip = R[0x4EB] >> 4 & 1, arriba = sy < 0;
    glFrontFace(ccw ^ arriba ^ flip ? GL_CCW : GL_CW);
    if (R[0x646] & 1) {
        glEnable(GL_CULL_FACE);
        uint32_t c = R[0x648];
        glCullFace(c == 0x404 || c == 1 ? GL_FRONT : c == 0x408 || c == 3 ? GL_FRONT_AND_BACK : GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
    if (R[0x4B3] & 1) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(gl_compare(R[0x4C3]));
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(R[0x4BA] & 1);
    if (R[0x591] & 1) {
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(R[0x592]);
    } else {
        glDisable(GL_PRIMITIVE_RESTART);
    }
    // Blending y máscaras de color por render target.
    glBlendColor(f32(R[0x4C7]), f32(R[0x4C8]), f32(R[0x4C9]), f32(R[0x4CA]));
    int indep = R[0x4B9] & 1;
    for (int i = 0; i < 8; i++) {
        uint32_t m = R[0x680 + i];
        glColorMaski(i, m & 1, m >> 4 & 1, m >> 8 & 1, m >> 12 & 1);
        if (!(R[0x4D8 + i] & 1)) {
            glDisablei(GL_BLEND, i);
            continue;
        }
        glEnablei(GL_BLEND, i);
        const uint32_t *b = indep ? &R[0x780 + i * 8] : NULL;
        uint32_t eqc = b ? b[1] : R[0x4D0], src = b ? b[2] : R[0x4D1], dst = b ? b[3] : R[0x4D2];
        uint32_t eqa = b ? b[4] : R[0x4D3], srca = b ? b[5] : R[0x4D4], dsta = b ? b[6] : R[0x4D6];
        glBlendEquationSeparatei(i, gl_blend_eq(eqc), gl_blend_eq(eqa));
        glBlendFuncSeparatei(i, gl_blend_factor(src), gl_blend_factor(dst), gl_blend_factor(srca), gl_blend_factor(dsta));
    }
}

// Atributos de vértice: tamaño (enum de la GPU) → componentes y bits por componente.
static int attr_forma(uint32_t size, int *n, int *bits) {
    switch (size) {
    case 0x01: *n = 4, *bits = 32; return 1;
    case 0x02: *n = 3, *bits = 32; return 1;
    case 0x04: *n = 2, *bits = 32; return 1;
    case 0x12: *n = 1, *bits = 32; return 1;
    case 0x03: *n = 4, *bits = 16; return 1;
    case 0x05: *n = 3, *bits = 16; return 1;
    case 0x0F: *n = 2, *bits = 16; return 1;
    case 0x1B: *n = 1, *bits = 16; return 1;
    case 0x0A: *n = 4, *bits = 8; return 1;
    case 0x13: *n = 3, *bits = 8; return 1;
    case 0x18: *n = 2, *bits = 8; return 1;
    case 0x1D: *n = 1, *bits = 8; return 1;
    case 0x30: *n = 4, *bits = 10; return 1;
    }
    return 0;
}

static void claves(const uint32_t *R, NxShaderKey *kv) {
    for (int i = 0; i < 32; i++) {
        uint32_t a = R[0x458 + i], type = a >> 27 & 7;
        kv->attr_int[i] = (a >> 6 & 1) ? 0 : type == 3 ? 1 : type == 4 ? 2 : 0;
    }
}

static int vertices(const uint32_t *R, const NxShaderInfo *vi, const NxDraw *d) {
    uint32_t first = d->first, count = d->count;
    int indexed = d->indexed;
    uint32_t streams = 0;
    for (int i = 0; i < 16; i++) { // OpenGL da 16 atributos
        if (!(vi->inputs >> i & 1)) {
            glDisableVertexArrayAttrib(vao, i);
            continue;
        }
        uint32_t a = R[0x458 + i];
        uint32_t buf = a & 31, off = a >> 7 & 0x3FFF, size = a >> 21 & 63, type = a >> 27 & 7;
        int n, bits;
        if ((a >> 6 & 1) || !attr_forma(size, &n, &bits)) { // constante
            glDisableVertexArrayAttrib(vao, i);
            glVertexAttrib4f(i, 0, 0, 0, 0);
            continue;
        }
        glEnableVertexArrayAttrib(vao, i);
        glVertexArrayAttribBinding(vao, i, buf);
        if (bits == 10) {
            glVertexArrayAttribFormat(vao, i, 4, type == 1 || type == 6 ? GL_INT_2_10_10_10_REV : GL_UNSIGNED_INT_2_10_10_10_REV,
                                      type == 1 || type == 2, off);
        } else if (type == 3 || type == 4) {
            GLenum t = bits == 32 ? (type == 3 ? GL_INT : GL_UNSIGNED_INT) : bits == 16 ? (type == 3 ? GL_SHORT : GL_UNSIGNED_SHORT)
                                                                                         : (type == 3 ? GL_BYTE : GL_UNSIGNED_BYTE);
            glVertexArrayAttribIFormat(vao, i, n, t, off);
        } else {
            GLenum t;
            if (type == 7) t = bits == 32 ? GL_FLOAT : GL_HALF_FLOAT;
            else {
                int sg = type == 1 || type == 6;
                t = bits == 32 ? (sg ? GL_INT : GL_UNSIGNED_INT) : bits == 16 ? (sg ? GL_SHORT : GL_UNSIGNED_SHORT) : (sg ? GL_BYTE : GL_UNSIGNED_BYTE);
            }
            glVertexArrayAttribFormat(vao, i, n, t, type == 1 || type == 2, off);
        }
        streams |= 1u << buf;
    }
    for (int s = 0; s < 16; s++) {
        if (!(streams >> s & 1)) continue;
        uint32_t cfg = R[0x700 + s * 4];
        if (!(cfg >> 12 & 1)) continue;
        uint32_t stride = cfg & 0xFFF;
        uint64_t start = (uint64_t)R[0x701 + s * 4] << 32 | R[0x702 + s * 4];
        uint64_t limit = (uint64_t)R[0x7C0 + s * 2] << 32 | R[0x7C1 + s * 2];
        uint64_t avail;
        const uint8_t *p = nx_gpu_ptr_n(start, &avail);
        if (!p) return 0;
        uint64_t size = limit >= start ? limit - start + 1 : 0;
        int por_instancia = R[0x620 + s] & 1;
        if (por_instancia && stride) { // hasta la última instancia del draw
            uint32_t div = R[0x703 + s * 4] ? R[0x703 + s * 4] : 1;
            uint64_t need = ((uint64_t)R[0x50E] + d->instance + d->ninstances + div - 1) / div * stride;
            if (need < size) size = need;
        } else if (!indexed && stride) { // solo los vértices del draw
            uint64_t need = (uint64_t)(first + count) * stride;
            if (need < size) size = need;
        }
        if (size > avail) size = avail;
        if (size > RING / 2) size = RING / 2;
        GLintptr o = subir(vbo_ring, &vbo_off, p, size, 256);
        glVertexArrayVertexBuffer(vao, s, vbo_ring, o, stride);
        glVertexArrayBindingDivisor(vao, s, (R[0x620 + s] & 1) ? (R[0x703 + s * 4] ? R[0x703 + s * 4] : 1) : 0);
    }
    return 1;
}

static void constbufs(const NxDraw *d, int stage, const NxShaderInfo *info, int base) {
    for (int s = 0; s < 18; s++) {
        if (!(info->cbufs >> s & 1)) continue;
        uint64_t addr = d->cb_addr[stage][s];
        uint32_t size = d->cb_size[stage][s];
        uint64_t avail;
        const void *p = addr ? nx_gpu_ptr_n(addr, &avail) : NULL;
        if (!p) {
            static const uint8_t ceros[16];
            p = ceros, size = 16, avail = 16;
        }
        if (size > avail) size = (uint32_t)avail;
        if (size > 65536) size = 65536;
        GLintptr o = subir(ubo_ring, &ubo_off, p, size, 256);
        glBindBufferRange(GL_UNIFORM_BUFFER, base + s, ubo_ring, o, 65536); // el bloque declara 64 KiB
    }
}

int nx_gl_draw(const NxDraw *d) {
    if (!iniciar()) return 0;
    tomar();
    const uint32_t *R = d->regs;
    uint64_t region = (uint64_t)R[0x582] << 32 | R[0x583];
    NxShaderKey kv = {0}, kf = {0};
    claves(R, &kv);
    kv.cb_binding = 0, kv.tex_binding = 0;
    kf.cb_binding = 18, kf.tex_binding = 32;
    Shader *vs = NULL, *fs = NULL;
    for (int i = 0; i < 6; i++) {
        uint32_t cfg = R[0x800 + i * 0x10];
        if (!(cfg & 1)) continue;
        int stage = cfg >> 4 & 15;
        uint64_t addr = region + R[0x801 + i * 0x10];
        if (stage == NX_SH_VERTEX) vs = shader(addr, NX_SH_VERTEX, &kv);
        else if (stage == NX_SH_FRAGMENT) fs = shader(addr, NX_SH_FRAGMENT, &kf);
        else {
            static int avisado;
            if (!avisado++) fprintf(stderr, "[RED] GPU: etapa de shader %d (solo hay vértice y fragmento)\n", stage);
        }
    }
    if (!vs) return 1;
    glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, vs->prog);
    glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, fs ? fs->prog : 0);

    // Render targets: RenderTargetControl dice cuántos y en qué orden.
    uint32_t ctl = R[0x487], nt = ctl & 15;
    GLenum bufs[8];
    int any = 0;
    for (uint32_t i = 0; i < 8; i++) {
        bufs[i] = GL_NONE;
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, 0, 0);
    }
    for (uint32_t i = 0; i < nt && i < 8; i++) {
        uint32_t t = ctl >> (4 + i * 3) & 7, b = 0x200 + t * 0x10;
        uint64_t addr = (uint64_t)R[b] << 32 | R[b + 1];
        if (!addr || !R[b + 4]) continue;
        Rt *r = rt_buscar(addr, R[b + 2], R[b + 3], R[b + 4], R[b + 5]);
        if (!r) continue;
        if (r->stale) rt_cargar(r);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, r->tex, 0);
        bufs[i] = GL_COLOR_ATTACHMENT0 + i;
        r->dirty = 1;
        any = 1;
    }
    if (!any) return 1;
    glNamedFramebufferDrawBuffers(fbo, 8, bufs);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    estado_raster(R);
    if (!vertices(R, &vs->info, d)) return 1;
    if (vs->info.uses_instance) glProgramUniform1ui(vs->prog, 0, d->instance);
    constbufs(d, 0, &vs->info, 0);
    texturas(d, 0, &vs->info, 0);
    if (fs) constbufs(d, 4, &fs->info, 18), texturas(d, 4, &fs->info, 32);

    GLenum mode = d->topology <= 6 || (d->topology >= 10 && d->topology <= 13) ? d->topology : GL_TRIANGLES;
    if (d->topology == 7 || d->topology == 8 || d->topology == 9) {
        static int avisado;
        if (!avisado++) fprintf(stderr, "[RED] GPU: primitiva %u (cuadriláteros/polígonos)\n", d->topology);
        return 1;
    }
    if (d->indexed) {
        static const uint32_t isz[] = {1, 2, 4};
        uint32_t fmt = R[0x5F6] & 3, sz = isz[fmt > 2 ? 2 : fmt];
        uint64_t start = (uint64_t)R[0x5F2] << 32 | R[0x5F3], avail;
        const uint8_t *p = nx_gpu_ptr_n(start + (uint64_t)d->first * sz, &avail);
        if (!p || avail < (uint64_t)d->count * sz) return 1;
        GLintptr o = subir(ibo_ring, &ibo_off, p, (size_t)d->count * sz, 4);
        glDrawElementsInstancedBaseVertexBaseInstance(
            mode, d->count, fmt == 0 ? GL_UNSIGNED_BYTE : fmt == 1 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void *)o, d->ninstances,
            (GLint)R[0x50D], R[0x50E] + d->instance);
    } else {
        glDrawArraysInstancedBaseInstance(mode, d->first, d->count, d->ninstances, R[0x50E] + d->instance);
    }
    if (traza)
        fprintf(stderr, "[nxgl] draw %u: %u vértices desde %u, %u instancias desde %u%s\n", mode, d->count, d->first,
                d->ninstances, d->instance, d->indexed ? " (índices)" : "");
    return 1;
}
