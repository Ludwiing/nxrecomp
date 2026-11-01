// Prueba del traductor de shaders: lee programas .dksh (deko3d), los traduce a GLSL y los compila
// con el OpenGL de la máquina (EGL, sin ventana).
//   shader_test a.dksh [b.dksh...]            muestra el GLSL y si compila
//   shader_test --dibujar vs.dksh fs.dksh f.ppm   dibuja 3 vértices en 256x256 y guarda la imagen
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxshader.h"

typedef struct {
    int stage;
    uint8_t *code;
    size_t size;
    uint8_t *cb1;
    uint32_t cb1_size;
} Prog;

static uint8_t *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(*n);
    if (fread(d, 1, *n, f) != *n) *n = 0;
    fclose(f);
    return d;
}

// DKSH: cabecera {magic, header_sz, control_sz, code_sz, programs_off, num_programs}; cada programa
// {type, entrypoint, num_gprs, constbuf1_off, constbuf1_sz, ...}. El código empieza en control_sz;
// entrypoint apunta a la cabecera SPH.
static int load_dksh(const char *path, Prog *p) {
    size_t n;
    uint8_t *d = read_file(path, &n);
    if (!d || n < 0x30 || memcmp(d, "DKSH", 4)) return 0;
    uint32_t *h = (uint32_t *)d;
    uint32_t control = h[2], code_sz = h[3], prog = h[4];
    uint32_t *ph = (uint32_t *)(d + prog);
    static const int stages[] = {NX_SH_VERTEX, NX_SH_FRAGMENT, NX_SH_GEOMETRY, NX_SH_TESS_CTRL, NX_SH_TESS_EVAL, NX_SH_COMPUTE};
    p->stage = stages[ph[0] % 6];
    p->code = d + control + ph[1];
    p->size = code_sz - ph[1];
    p->cb1 = d + control + ph[3];
    p->cb1_size = ph[4];
    return 1;
}

static int compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "no compila:\n%s\n", log);
        return 0;
    }
    return (int)s;
}

static void egl_init(void) {
    PFNEGLQUERYDEVICESEXTPROC qd = (void *)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDeviceEXT devs[8];
    EGLint n = 0;
    qd(8, devs, &n);
    EGLDisplay d = gpd(EGL_PLATFORM_DEVICE_EXT, devs[0], NULL);
    eglInitialize(d, NULL, NULL);
    eglBindAPI(EGL_OPENGL_API);
    EGLint ca[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 6, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                   EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext c = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ca);
    eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c);
}

static char *translate(Prog *p, int binding, NxShaderInfo *info) {
    NxShaderKey key = {0};
    key.cb_binding = (uint8_t)binding;
    key.tex_binding = 8;
    return nx_shader_glsl(p->code, p->size, p->stage, &key, info);
}

// ---------- comparación: el GLSL original contra su traducción ----------

static const char *vs_comun =
    "#version 460\n"
    "layout(location = 0) out vec4 v0; layout(location = 1) out vec4 v1;\n"
    "layout(location = 2) out vec4 v2; layout(location = 3) out vec4 v3;\n"
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID & 1) * 2 - 1), float((gl_VertexID >> 1) * 2 - 1));\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "    vec2 u = p * 0.5 + 0.5;\n"
    "    v0 = vec4(u, u.x * u.y, 1.0 - u.x);\n"
    "    v1 = vec4(u * 8.0 - 4.0, u.yx * 3.0);\n"
    "    v2 = vec4(u.yx * 100.0, u * -37.0);\n"
    "    v3 = vec4(u * 16.0, u * 7.3);\n"
    "}\n";

#define TAM 32

static GLuint link2(GLuint a, GLuint b) {
    GLuint p = glCreateProgram();
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "no enlaza:\n%s\n", log);
        return 0;
    }
    return p;
}

static void render(GLuint prog, float *out) {
    GLuint tex, fb, vao;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA32F, TAM, TAM);
    glCreateFramebuffers(1, &fb);
    glNamedFramebufferTexture(fb, GL_COLOR_ATTACHMENT0, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glViewport(0, 0, TAM, TAM);
    float zero[4] = {0};
    glClearBufferfv(GL_COLOR, 0, zero);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glReadPixels(0, 0, TAM, TAM, GL_RGBA, GL_FLOAT, out);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
}

// Texturas de prueba en las unidades 0-4 (las del GLSL original); la traducción usa 8 en adelante.
static GLuint texturas[5];

static void crear_texturas(void) {
    static float d[16 * 16 * 16 * 4 * 6];
    uint32_t x = 777;
    for (size_t i = 0; i < sizeof d / sizeof *d; i++) {
        x = x * 1664525 + 1013904223;
        d[i] = (float)(x >> 8) / 16777216.0f;
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &texturas[0]);
    glTextureStorage2D(texturas[0], 5, GL_RGBA32F, 16, 16);
    for (int l = 0; l < 5; l++) glTextureSubImage2D(texturas[0], l, 0, 0, 16 >> l, 16 >> l, GL_RGBA, GL_FLOAT, d + l * 300);
    glTextureParameteri(texturas[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texturas[1]);
    glTextureStorage3D(texturas[1], 1, GL_RGBA32F, 8, 8, 4);
    glTextureSubImage3D(texturas[1], 0, 0, 0, 0, 8, 8, 4, GL_RGBA, GL_FLOAT, d + 7);
    glCreateTextures(GL_TEXTURE_3D, 1, &texturas[2]);
    glTextureStorage3D(texturas[2], 1, GL_RGBA32F, 8, 8, 8);
    glTextureSubImage3D(texturas[2], 0, 0, 0, 0, 8, 8, 8, GL_RGBA, GL_FLOAT, d + 13);
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texturas[3]);
    glTextureStorage2D(texturas[3], 1, GL_RGBA32F, 8, 8);
    glTextureSubImage3D(texturas[3], 0, 0, 0, 0, 8, 8, 6, GL_RGBA, GL_FLOAT, d + 21);
    glCreateTextures(GL_TEXTURE_2D, 1, &texturas[4]);
    glTextureStorage2D(texturas[4], 1, GL_DEPTH_COMPONENT32F, 16, 16);
    glTextureSubImage2D(texturas[4], 0, 0, 0, 16, 16, GL_DEPTH_COMPONENT, GL_FLOAT, d + 5);
    glTextureParameteri(texturas[4], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(texturas[4], GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    for (int i = 0; i < 5; i++) glBindTextureUnit(i, texturas[i]);
}

static int iguales(float a, float b) {
    if (a != a || b != b) return a != a && b != b;
    float d = a > b ? a - b : b - a, m = a < 0 ? -a : a;
    return d <= 1e-4f * (m > 1 ? m : 1);
}

static int comparar(const char *orig, const char *dksh) {
    size_t n;
    char *src = (char *)read_file(orig, &n);
    Prog fs;
    if (!src || !load_dksh(dksh, &fs)) return 2;
    src = realloc(src, n + 1);
    src[n] = 0;
    NxShaderInfo info;
    char *tr = translate(&fs, 18, &info);
    if (!texturas[0]) crear_texturas();
    for (int i = 0; i < info.ntex; i++) { // el handle de deko3d para el binding b del fragmento es 420 + b
        int b = info.tex[i].handle - 420;
        if (b >= 0 && b < 5) glBindTextureUnit(8 + i, texturas[b]);
    }
    GLuint v = compile(GL_VERTEX_SHADER, vs_comun), a = compile(GL_FRAGMENT_SHADER, src), b = compile(GL_FRAGMENT_SHADER, tr);
    if (!v || !a || !b) {
        fprintf(stderr, "%s", tr);
        return 1;
    }
    GLuint pa = link2(v, a), pb = link2(v, b);
    if (!pa || !pb) return 1;
    // Constantes: buffers 0-3 del GLSL original = ranuras 2-5 de la traducción.
    static uint32_t datos[4][1024];
    uint32_t x = 12345;
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 1024; i++) {
            x = x * 1103515245 + 12345;
            float f = (float)((int)(x >> 8) % 20000 - 10000) / 997.0f;
            if (i % 8 >= 4) datos[k][i] = x >> 16; else memcpy(&datos[k][i], &f, 4); // mitad floats, mitad enteros
        }
    for (int k = 0; k < 4; k++) {
        GLuint buf;
        glCreateBuffers(1, &buf);
        static uint8_t big[65536];
        memset(big, 0, sizeof big);
        memcpy(big, datos[k], sizeof datos[k]);
        glNamedBufferData(buf, 65536, big, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, k, buf);
        glBindBufferBase(GL_UNIFORM_BUFFER, 18 + 2 + k, buf);
    }
    static float ra[TAM * TAM * 4], rb[TAM * TAM * 4];
    render(pa, ra);
    render(pb, rb);
    int malos = 0;
    for (int i = 0; i < TAM * TAM * 4; i++)
        if (!iguales(ra[i], rb[i]) && malos++ < 8)
            fprintf(stderr, "%s: píxel (%d,%d).%c: original %.9g, traducido %.9g\n", orig, i / 4 % TAM, i / 4 / TAM,
                    "xyzw"[i % 4], ra[i], rb[i]);
    if (malos) {
        fprintf(stderr, "%s: %d valores distintos\n", orig, malos);
        if (getenv("NX_VER_GLSL")) fprintf(stderr, "%s", tr);
    }
    return malos != 0 || info.errors;
}

int main(int argc, char **argv) {
    egl_init();
    if (argc >= 4 && !strcmp(argv[1], "--comparar")) {
        int bad = 0;
        for (int i = 2; i + 1 < argc; i += 2) bad += comparar(argv[i], argv[i + 1]) != 0;
        return bad != 0;
    }
    if (argc == 5 && !strcmp(argv[1], "--dibujar")) {
        Prog vs, fs;
        if (!load_dksh(argv[2], &vs) || !load_dksh(argv[3], &fs)) return 2;
        NxShaderInfo vi, fi;
        char *vg = translate(&vs, 0, &vi), *fg = translate(&fs, 18, &fi);
        int a = compile(GL_VERTEX_SHADER, vg), b = compile(GL_FRAGMENT_SHADER, fg);
        if (!a || !b) return 1;
        GLuint prog = glCreateProgram();
        glAttachShader(prog, a);
        glAttachShader(prog, b);
        glLinkProgram(prog);
        GLint ok;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetProgramInfoLog(prog, sizeof log, NULL, log);
            fprintf(stderr, "no enlaza:\n%s\n", log);
            return 1;
        }
        GLuint buf[2];
        glCreateBuffers(2, buf);
        static uint8_t zero[65536];
        glNamedBufferData(buf[0], 65536, zero, GL_STATIC_DRAW);
        glNamedBufferSubData(buf[0], 0, vs.cb1_size, vs.cb1);
        glNamedBufferData(buf[1], 65536, zero, GL_STATIC_DRAW);
        glNamedBufferSubData(buf[1], 0, fs.cb1_size, fs.cb1);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, buf[0]);
        glBindBufferBase(GL_UNIFORM_BUFFER, 18 + 1, buf[1]);
        GLuint tex, fb, vao;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 1, GL_RGBA8, 256, 256);
        glCreateFramebuffers(1, &fb);
        glNamedFramebufferTexture(fb, GL_COLOR_ATTACHMENT0, tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glViewport(0, 0, 256, 256);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        static uint8_t px[256 * 256 * 4];
        glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, px);
        FILE *f = fopen(argv[4], "wb");
        fprintf(f, "P6\n256 256\n255\n");
        for (int y = 255; y >= 0; y--)
            for (int x = 0; x < 256; x++) fwrite(px + (y * 256 + x) * 4, 1, 3, f);
        fclose(f);
        return 0;
    }
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        Prog p;
        if (!load_dksh(argv[i], &p)) {
            fprintf(stderr, "%s: no es un DKSH\n", argv[i]);
            bad++;
            continue;
        }
        NxShaderInfo info;
        char *g = translate(&p, p.stage == NX_SH_FRAGMENT ? 18 : 0, &info);
        printf("// ---- %s (etapa %d, %d errores)\n", argv[i], p.stage, info.errors);
        for (int k = 0; k < info.ntex; k++) printf("// tex%d: handle %d, tipo 0x%x\n", k, info.tex[k].handle, info.tex[k].type);
        printf("%s", g);
        GLenum type = p.stage == NX_SH_FRAGMENT ? GL_FRAGMENT_SHADER
                      : p.stage == NX_SH_COMPUTE ? GL_COMPUTE_SHADER
                      : p.stage == NX_SH_GEOMETRY ? GL_GEOMETRY_SHADER : GL_VERTEX_SHADER;
        if (!compile(type, g) || info.errors) bad++;
        free(g);
    }
    return bad != 0;
}
