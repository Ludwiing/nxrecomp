// Programa de prueba. Se compila dos veces:
//  - para la Switch (ARM64, sin libc, habla con Horizon por svc), y de ahí se recompila;
//  - con -DNATIVE_REF directo para x86_64, como referencia de lo que debe imprimir.
// Ejercita aritmética de 32 y 64 bits, banderas, recursión, punteros a función,
// varargs, .data, .rodata y un .bss de 5 MB.
#include "mini.h"

// ---------- pruebas ----------
static u64 fib(int n) {
    u64 a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        u64 t = a + b;
        a = b;
        b = t;
    }
    return a;
}

__attribute__((noinline)) static u64 fib_rec(int n) {
    return n < 2 ? (u64)n : fib_rec(n - 1) + fib_rec(n - 2);
}

static u32 crc32(const char *s) {
    u32 c = 0xFFFFFFFFu;
    for (; *s; s++) {
        c ^= (u8)*s;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static u64 fnv1a(const char *s) {
    u64 h = 0xcbf29ce484222325ull;
    for (; *s; s++) {
        h ^= (u8)*s;
        h *= 0x100000001b3ull;
    }
    return h;
}

static u32 lcg = 12345;
static s64 rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return (s64)((lcg >> 8) & 0xFFFFF) - 0x80000;
}

__attribute__((noinline)) static void quicksort(s64 *a, int lo, int hi) {
    while (lo < hi) {
        s64 p = a[(lo + hi) / 2];
        int i = lo, j = hi;
        while (i <= j) {
            while (a[i] < p) i++;
            while (a[j] > p) j--;
            if (i <= j) {
                s64 t = a[i];
                a[i] = a[j];
                a[j] = t;
                i++;
                j--;
            }
        }
        if (j - lo < hi - i) {
            quicksort(a, lo, j);
            lo = i;
        } else {
            quicksort(a, i, hi);
            hi = j;
        }
    }
}

typedef s64 (*op_fn)(s64, s64);
static s64 op_suma(s64 a, s64 b) { return a + b; }
static s64 op_resta(s64 a, s64 b) { return a - b; }
static s64 op_mult(s64 a, s64 b) { return a * b; }
static s64 op_div(s64 a, s64 b) { return b ? a / b : 0; }
static s64 op_mod(s64 a, s64 b) { return b ? a % b : 0; }
// Global y no const: el compilador no puede resolver las llamadas, así que salen como BLR.
op_fn operaciones[5] = {op_suma, op_resta, op_mult, op_div, op_mod};
const char *const nombres[5] = {"suma", "resta", "multiplicación", "división", "módulo"};

#define LIMITE 5000000
static u8 criba[LIMITE]; // va al .bss

static u32 contar_primos(void) {
    u32 n = 0;
    for (u32 i = 2; i < LIMITE; i++) {
        if (criba[i]) continue;
        n++;
        for (u64 j = (u64)i * i; j < LIMITE; j += i) criba[j] = 1;
    }
    return n;
}

static const char *dia(int d) {
    switch (d) {
    case 0: return "domingo";
    case 1: return "lunes";
    case 2: return "martes";
    case 3: return "miércoles";
    case 4: return "jueves";
    case 5: return "viernes";
    default: return "sábado";
    }
}

int nx_main(void) {
    u64 t0 = ticks();
    pr("¡Hola desde la Switch! Este código era ARM64 y ahora corre nativo en x86_64.\n");
    pr("fib(90) = %U\n", fib(90));
    pr("fib_rec(27) = %U\n", fib_rec(27));
    pr("crc32(\"Ludwiing\") = 0x%x\n", crc32("Ludwiing"));
    pr("fnv1a(\"Omarchy\") = 0x%X\n", fnv1a("Omarchy"));

    s64 v[200];
    s64 suma = 0;
    for (int i = 0; i < 200; i++) suma += v[i] = rnd();
    quicksort(v, 0, 199);
    int ordenado = 1;
    for (int i = 1; i < 200; i++) ordenado &= v[i - 1] <= v[i];
    pr("quicksort de 200 números: mínimo %D, máximo %D, suma %D, ordenado: %s\n", v[0], v[199], suma,
       ordenado ? "sí" : "no");

    for (int i = 0; i < 5; i++) pr("%s(-1234567, 89) = %D\n", nombres[i], operaciones[i](-1234567, 89));

    s32 a = -7;
    u32 b = 0x80000001u;
    pr("32 bits: %d / 2 = %d, %d %% 3 = %d, 0x%x >> 3 = 0x%x, asr = %d\n", a, a / 2, a, a % 3, b, b >> 3,
       (s32)b >> 3);
    pr("64 bits: 0x%X * 3 = 0x%X, alto de 2^63 * 6 = %U\n", (u64)0xDEADBEEFCAFEull, (u64)0xDEADBEEFCAFEull * 3,
       (u64)(((unsigned __int128)1 << 63) * 6 >> 64));
    pr("hoy es %s\n", dia((int)(fib(10) % 7)));

    pr("primos menores que %u: %u\n", (u32)LIMITE, contar_primos());

    u64 t1 = ticks();
    pr("reloj de la Switch: %s\n", t1 > t0 ? "avanza" : "detenido");
    pr("Fin. Saliendo con svcExitProcess.\n");
    flush();
    return 0;
}

#ifdef NATIVE_REF
int main(void) { return nx_main(); }
#endif
