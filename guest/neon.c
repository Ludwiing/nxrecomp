// Programa de prueba de punto flotante y NEON. Con -O3 el compilador vectoriza los
// bucles, así que en la Switch salen instrucciones SIMD (FMLA, LD1, ZIP, SQXTN, ADDV...).
// Se compila con -ffp-contract=off en las dos versiones para que ninguna fusione a*b+c
// por su cuenta: así los resultados de punto flotante tienen que coincidir bit a bit.
#include "mini.h"

static u32 fbits(float f) {
    u32 u;
    memcpy(&u, &f, 4);
    return u;
}
static u64 dbits(double d) {
    u64 u;
    memcpy(&u, &d, 8);
    return u;
}

// imprime un float con 4 decimales (redondeado) y sus bits exactos
static void pf(const char *name, float f) {
    s64 v = (s64)(f * 10000.0f + (f < 0 ? -0.5f : 0.5f));
    pr("%s = %s%D.%c%c%c%c (0x%x)\n", name, v < 0 ? "-" : "", (v < 0 ? -v : v) / 10000, '0' + (int)((v < 0 ? -v : v) / 1000 % 10),
       '0' + (int)((v < 0 ? -v : v) / 100 % 10), '0' + (int)((v < 0 ? -v : v) / 10 % 10), '0' + (int)((v < 0 ? -v : v) % 10),
       fbits(f));
}

#define N 1024
static float xs[N], ys[N], zs[N];
static s16 aud[N];
static u8 img[N], img2[N];
static s32 ints[N];

typedef struct {
    float m[4][4];
} Mat4;

__attribute__((noinline)) static Mat4 matmul(const Mat4 *a, const Mat4 *b) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a->m[i][k] * b->m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

__attribute__((noinline)) static float dot(const float *a, const float *b, int n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

__attribute__((noinline)) static void saxpy(float *z, float a, const float *x, const float *y, int n) {
    for (int i = 0; i < n; i++) z[i] = a * x[i] + y[i];
}

__attribute__((noinline)) static void normalize(float *v, int n) {
    for (int i = 0; i + 2 < n; i += 3) {
        float l = __builtin_sqrtf(v[i] * v[i] + v[i + 1] * v[i + 1] + v[i + 2] * v[i + 2]);
        float inv = l > 0 ? 1.0f / l : 0.0f;
        v[i] *= inv;
        v[i + 1] *= inv;
        v[i + 2] *= inv;
    }
}

// mezcla de audio con saturación a 16 bits
__attribute__((noinline)) static void mix(s16 *out, const s16 *a, const s16 *b, int n) {
    for (int i = 0; i < n; i++) {
        s32 v = (s32)a[i] + (s32)b[i] * 3 / 2;
        out[i] = (s16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}

// mezcla alfa de una imagen en escala de grises
__attribute__((noinline)) static void blend(u8 *dst, const u8 *src, u8 alpha, int n) {
    for (int i = 0; i < n; i++) dst[i] = (u8)((src[i] * alpha + dst[i] * (255 - alpha) + 127) / 255);
}

__attribute__((noinline)) static u32 sum_abs_diff(const u8 *a, const u8 *b, int n) {
    u32 s = 0;
    for (int i = 0; i < n; i++) s += (u32)(a[i] > b[i] ? a[i] - b[i] : b[i] - a[i]);
    return s;
}

__attribute__((noinline)) static void to_int(s32 *o, const float *f, int n) {
    for (int i = 0; i < n; i++) o[i] = (s32)(f[i] * 100.0f);
}

__attribute__((noinline)) static float minmax(const float *f, int n, float *mx) {
    float lo = f[0], hi = f[0];
    for (int i = 1; i < n; i++) {
        lo = f[i] < lo ? f[i] : lo;
        hi = f[i] > hi ? f[i] : hi;
    }
    *mx = hi;
    return lo;
}

__attribute__((noinline)) static u32 popcount_all(const u8 *p, int n) {
    u32 s = 0;
    for (int i = 0; i < n; i++) s += (u32)__builtin_popcount(p[i]);
    return s;
}

__attribute__((noinline)) static double leibniz(int n) {
    double s = 0;
    for (int i = 0; i < n; i++) s += (i & 1 ? -4.0 : 4.0) / (2 * i + 1);
    return s;
}

static u32 seed = 2026;
static u32 rnd(void) {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

int nx_main(void) {
    pr("Punto flotante y NEON en la Switch, recompilado a x86_64.\n");
    for (int i = 0; i < N; i++) {
        xs[i] = (float)((int)(rnd() % 2001) - 1000) / 37.0f;
        ys[i] = (float)((int)(rnd() % 2001) - 1000) / 91.0f;
        aud[i] = (s16)(rnd() & 0xFFFF);
        img[i] = (u8)rnd();
        img2[i] = (u8)rnd();
    }

    Mat4 a, b;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            a.m[i][j] = xs[i * 4 + j];
            b.m[i][j] = ys[i * 4 + j];
        }
    Mat4 r = matmul(&a, &b);
    pf("matriz[0][0]", r.m[0][0]);
    pf("matriz[3][2]", r.m[3][2]);

    pf("producto punto", dot(xs, ys, N));
    saxpy(zs, 2.5f, xs, ys, N);
    pf("saxpy[777]", zs[777]);
    normalize(zs, N);
    pf("normalizado[3]", zs[3]);
    pf("normalizado[1020]", zs[1020]);

    s16 mixed[N];
    mix(mixed, aud, aud + 1, N - 1);
    s64 msum = 0;
    for (int i = 0; i < N - 1; i++) msum += mixed[i];
    pr("audio mezclado: suma %D, muestra[100] = %d\n", msum, (s32)mixed[100]);

    blend(img2, img, 77, N);
    pr("imagen mezclada: [0] = %u, [500] = %u, SAD = %u\n", (u32)img2[0], (u32)img2[500], sum_abs_diff(img, img2, N));

    to_int(ints, xs, N);
    s64 isum = 0;
    for (int i = 0; i < N; i++) isum += ints[i];
    pr("float a entero: suma %D\n", isum);

    float mx, mn = minmax(xs, N, &mx);
    pf("mínimo", mn);
    pf("máximo", mx);
    pr("bits encendidos en la imagen: %u\n", popcount_all(img, N));

    double pi = leibniz(200000);
    pr("pi por Leibniz: 0x%X (%D millonésimas)\n", dbits(pi), (s64)(pi * 1000000.0));
    pr("Fin.\n");
    flush();
    return 0;
}

#ifdef NATIVE_REF
int main(void) { return nx_main(); }
#endif
