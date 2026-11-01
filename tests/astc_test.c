// Prueba diferencial del decodificador ASTC: bloques aleatorios decodificados por nxastc.c y por el
// llvmpipe de Mesa (OpenGL por software, que trae su propio decodificador), comparados byte a byte.
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxastc.h"

#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0

static int mesa(void) {
    PFNEGLQUERYDEVICESEXTPROC qd = (void *)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDeviceEXT devs[8];
    EGLint n = 0;
    qd(8, devs, &n);
    for (int i = 0; i < n; i++) {
        EGLDisplay d = gpd(EGL_PLATFORM_DEVICE_EXT, devs[i], NULL);
        if (!d || !eglInitialize(d, NULL, NULL)) continue;
        eglBindAPI(EGL_OPENGL_API);
        EGLint ca[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                       EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
        EGLContext c = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ca);
        if (!c || !eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) continue;
        if (strstr((const char *)glGetString(GL_RENDERER), "llvmpipe")) return 1;
        eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    return 0;
}

extern int nx_astc_depurar;

static void depurar(const char *hex, int bw, int bh) {
    uint8_t b[16], out[12 * 12 * 4];
    for (int k = 0; k < 16; k++) sscanf(hex + (15 - k) * 2, "%2hhx", &b[k]);
    nx_astc_depurar = 1;
    nx_astc_bloque(b, bw, bh, 0, out, bw * 4);
    nx_astc_depurar = 0;
}

// Para guest/libnx/deko_astc: las cuatro texturas de 160x160 que dibuja.
static const int captura_fmt[4][2] = {{4, 4}, {8, 5}, {10, 10}, {12, 12}};

static void generar(const char *dir) {
    uint32_t x = 12345;
    for (int t = 0; t < 4; t++) {
        int bw = captura_fmt[t][0], bh = captura_fmt[t][1], n = ((160 + bw - 1) / bw) * ((160 + bh - 1) / bh);
        uint8_t *b = malloc((size_t)n * 16), tmp[12 * 12 * 4];
        for (int i = 0; i < n; i++)
            for (int intento = 0; intento < 500; intento++) {
                for (int k = 0; k < 16; k++) x ^= x << 13, x ^= x >> 17, x ^= x << 5, b[i * 16 + k] = (uint8_t)x;
                nx_astc_bloque(b + i * 16, bw, bh, 0, tmp, bw * 4);
                if (!(tmp[0] == 255 && tmp[1] == 0 && tmp[2] == 255 && tmp[3] == 255)) break;
            }
        char ruta[512];
        snprintf(ruta, sizeof ruta, "%s/astc_%dx%d.bin", dir, bw, bh);
        FILE *f = fopen(ruta, "wb");
        fwrite(b, 16, (size_t)n, f);
        fclose(f);
        free(b);
    }
}

// Compara la captura (PPM de 1280x720) con lo que Mesa decodifica de los mismos bloques.
static int comparar_captura(const char *dir, const char *ppm) {
    FILE *f = fopen(ppm, "rb");
    if (!f) return 2;
    int w, h, mx;
    if (fscanf(f, "P6 %d %d %d", &w, &h, &mx) != 3) return 2;
    fgetc(f);
    uint8_t *img = malloc((size_t)w * h * 3);
    if (fread(img, 3, (size_t)w * h, f) != (size_t)w * h) return 2;
    fclose(f);
    int malos = 0;
    for (int t = 0; t < 4; t++) {
        int bw = captura_fmt[t][0], bh = captura_fmt[t][1], cols = (160 + bw - 1) / bw, filas = (160 + bh - 1) / bh;
        char ruta[512];
        snprintf(ruta, sizeof ruta, "%s/astc_%dx%d.bin", dir, bw, bh);
        size_t n;
        FILE *g = fopen(ruta, "rb");
        if (!g) return 2;
        uint8_t *src = malloc((size_t)cols * filas * 16), *ref = malloc((size_t)cols * bw * filas * bh * 4);
        n = fread(src, 16, (size_t)cols * filas, g);
        fclose(g);
        (void)n;
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_ASTC_4x4_KHR + (t == 0 ? 0 : t == 1 ? 5 : t == 2 ? 11 : 13),
                               cols * bw, filas * bh, 0, cols * filas * 16, src);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, ref);
        int dist = 0;
        for (int y = 0; y < 160; y++)
            for (int xx = 0; xx < 160; xx++) {
                const uint8_t *a = img + ((size_t)y * w + t * 160 + xx) * 3, *b = ref + ((size_t)y * cols * bw + xx) * 4;
                if (abs(a[0] - b[0]) > 1 || abs(a[1] - b[1]) > 1 || abs(a[2] - b[2]) > 1) {
                    if (dist++ < 3)
                        fprintf(stderr, "ASTC %dx%d texel (%d,%d): captura %d %d %d, Mesa %d %d %d\n", bw, bh, xx, y, a[0], a[1], a[2], b[0],
                                b[1], b[2]);
                }
            }
        if (dist) fprintf(stderr, "ASTC %dx%d: %d texels distintos\n", bw, bh, dist);
        malos += dist;
        free(src), free(ref);
    }
    return malos != 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--generar")) {
        generar(argv[2]);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--captura")) {
        if (!mesa()) return 77;
        return comparar_captura(argv[2], argv[3]);
    }
    if (argc == 5 && !strcmp(argv[1], "--bloque")) {
        depurar(argv[2], atoi(argv[3]), atoi(argv[4]));
        return 0;
    }
    if (!mesa()) {
        fprintf(stderr, "no encontré el llvmpipe de Mesa: no hay referencia para ASTC\n");
        return 77;
    }
    int nbloques = argc > 1 ? atoi(argv[1]) : 4096;
    static const int tam[][2] = {{4, 4}, {5, 4}, {5, 5}, {6, 5}, {6, 6}, {8, 5}, {8, 6}, {8, 8}, {10, 5}, {10, 6}, {10, 8}, {10, 10}, {12, 10}, {12, 12}};
    uint32_t x = 2463534242u;
    int malos = 0;
    for (int t = 0; t < 14; t++)
        for (int srgb = 0; srgb < 2; srgb++) {
            int bw = tam[t][0], bh = tam[t][1], cols = 64, filas = (nbloques + cols - 1) / cols;
            int w = cols * bw, h = filas * bh;
            uint8_t *src = malloc((size_t)cols * filas * 16), *a = malloc((size_t)w * h * 4), *b = malloc((size_t)w * h * 4);
            // Bloques aleatorios; la mayoría de los que resultan de error se descartan, para probar sobre
            // todo bloques válidos (quedan algunos de error, y uno de cada 16 es de color constante).
            for (int i = 0; i < cols * filas; i++) {
                for (int intento = 0; intento < 500; intento++) {
                    for (int k = 0; k < 16; k++) x ^= x << 13, x ^= x >> 17, x ^= x << 5, src[i * 16 + k] = (uint8_t)x;
                    if (i % 16 == 0) src[i * 16] = 0xFC, src[i * 16 + 1] = (src[i * 16 + 1] & 0xF0) | 0x0D;
                    uint8_t tmp[12 * 12 * 4];
                    nx_astc_bloque(src + i * 16, bw, bh, srgb, tmp, bw * 4);
                    if (!(tmp[0] == 255 && tmp[1] == 0 && tmp[2] == 255 && tmp[3] == 255) || (intento == 0 && x % 10 == 0)) break;
                }
            }
            nx_astc_imagen(src, w, h, bw, bh, srgb, a);
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            GLenum fmt = (srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR : GL_COMPRESSED_RGBA_ASTC_4x4_KHR) + t;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, cols * filas * 16, src);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, b);
            int errores = 0, distintos = 0, cat[3] = {0};
            for (int i = 0; i < cols * filas; i++) {
                int bx = i % cols, by = i / cols, mal = 0, err = 1, nerr = 1;
                size_t o0 = 0;
                for (int yy = 0; yy < bh; yy++)
                    for (int xx = 0; xx < bw; xx++) {
                        size_t o = ((size_t)(by * bh + yy) * w + bx * bw + xx) * 4;
                        if (memcmp(a + o, b + o, 4) && !mal) mal = 1, o0 = o;
                        if (!(b[o] == 255 && b[o + 1] == 0 && b[o + 2] == 255 && b[o + 3] == 255)) err = 0;
                        if (!(a[o] == 255 && a[o + 1] == 0 && a[o + 2] == 255 && a[o + 3] == 255)) nerr = 0;
                    }
                if (mal) {
                    int c = nerr && !err ? 0 : err && !nerr ? 1 : 2; // 0: solo nosotros error, 1: solo Mesa, 2: valores
                    if (cat[c]++ < (getenv("ASTC_VER") ? 3 : 0)) {
                        fprintf(stderr, "%dx%d%s [%s] bloque ", bw, bh, srgb ? " sRGB" : "", c == 0 ? "error nuestro" : c == 1 ? "error de Mesa" : "valores");
                        for (int k = 15; k >= 0; k--) fprintf(stderr, "%02x", src[i * 16 + k]);
                        fprintf(stderr, ": nuestro %d %d %d %d, Mesa %d %d %d %d\n", a[o0], a[o0 + 1], a[o0 + 2], a[o0 + 3], b[o0], b[o0 + 1], b[o0 + 2], b[o0 + 3]);
                        if (getenv("ASTC_VER")[0] == '2')
                            for (int yy = 0; yy < bh; yy++) {
                                for (int xx = 0; xx < bw; xx++) {
                                    size_t o = ((size_t)(by * bh + yy) * w + bx * bw + xx) * 4;
                                    fprintf(stderr, " %3d,%3d,%3d,%3d%s", a[o], a[o + 1], a[o + 2], a[o + 3], memcmp(a + o, b + o, 4) ? "*" : " ");
                                }
                                fprintf(stderr, "   |");
                                for (int xx = 0; xx < bw; xx++) {
                                    size_t o = ((size_t)(by * bh + yy) * w + bx * bw + xx) * 4;
                                    fprintf(stderr, " %3d,%3d,%3d,%3d", b[o], b[o + 1], b[o + 2], b[o + 3]);
                                }
                                fprintf(stderr, "\n");
                            }
                    }
                }
                distintos += mal;
                errores += err;
            }
            if (distintos) printf("   (solo nuestro error %d, solo de Mesa %d, valores %d)\n", cat[0], cat[1], cat[2]);
            printf("%2dx%-2d %s: %d de %d bloques distintos (%d son bloques de error)\n", bw, bh, srgb ? "sRGB  " : "lineal", distintos,
                   cols * filas, errores);
            malos += distintos;
            glDeleteTextures(1, &tex);
            free(src), free(a), free(b);
        }
    return malos != 0;
}
