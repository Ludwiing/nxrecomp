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

static EGLDisplay dpy;
static EGLContext ctx;
static int estado; // 0 sin probar, 1 listo, -1 sin OpenGL
static int actual; // el contexto está tomado por el hilo de la GPU
static GLuint pipeline, vao, fbo, ubo_ring, vbo_ring, ibo_ring;
static int traza = -1;

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

void nx_gl_cpu_write(uint64_t gpu, uint64_t size) {
    if (estado <= 0) return;
    for (int i = 0; i < nrts; i++)
        if (rts[i].addr < gpu + size && gpu < rts[i].addr + rt_bytes(&rts[i])) {
            if (rts[i].dirty) {
                tomar();
                rt_escribir(&rts[i]);
            }
            rts[i].stale = 1;
        }
}

void nx_gl_invalidate_all(void) {
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

static int vertices(const uint32_t *R, const NxShaderInfo *vi, uint32_t first, uint32_t count, int indexed) {
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
        if (!indexed && stride) { // solo los vértices del draw
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
    kv.cb_binding = 0;
    kf.cb_binding = 18;
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
    if (!vertices(R, &vs->info, d->first, d->count, d->indexed)) return 1;
    constbufs(d, 0, &vs->info, 0);
    if (fs) constbufs(d, 4, &fs->info, 18);

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
            mode, d->count, fmt == 0 ? GL_UNSIGNED_BYTE : fmt == 1 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void *)o, 1,
            (GLint)R[0x50D], R[0x50E] + d->instance);
    } else {
        glDrawArraysInstancedBaseInstance(mode, d->first, d->count, 1, R[0x50E] + d->instance);
    }
    if (traza)
        fprintf(stderr, "[nxgl] draw %u: %u vértices desde %u%s\n", mode, d->count, d->first, d->indexed ? " (índices)" : "");
    return 1;
}
