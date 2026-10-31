// Ejecuta cada prueba del fuzzer: carga el estado inicial, corre la instrucción
// traducida y escribe el estado final para compararlo con Unicorn.
#include "nxrt.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CODE 0x01000000ull         // donde Unicorn ejecuta la instrucción
#define LIT_BASE (CODE - 0x100000ull) // LDR literal alcanza ±1 MiB desde ella
#define LIT_SIZE 0x201000ull
#define SCRATCH 0x10000000ull
#define SCRATCH_SIZE 0x10000ull

typedef struct {
    uint64_t x[31], sp, nzcv;
    uint64_t v[64];
    uint64_t mem;
} St;

typedef void (*TestFn)(Ctx *);
extern const TestFn nx_tests[];
extern const int nx_ntests;
extern const uint32_t nx_words[];

void nx_dispatch(Ctx *c, uint64_t a) { nx_bad_jump(c, 0, a); }
const char *const nx_imports[] = {0};
const int nx_nimports = 0;
const NxSymbol nx_symbols[] = {{0, 0}};
const int nx_nsymbols = 0;

static uint32_t crc_table[256];
static volatile int current = -1;

// Si una traducción accede fuera de la memoria de prueba, dice cuál fue.
static void on_fault(int sig) {
    char msg[64];
    int n = snprintf(msg, sizeof msg, "FALLO %d %d\n", sig, current);
    write(2, msg, n);
    _exit(3);
}

static uint32_t crc32_mem(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "uso: driver entrada salida\n");
        return 2;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        crc_table[i] = c;
    }
    FILE *in = fopen(argv[1], "rb"), *out = fopen(argv[2], "wb");
    if (!in || !out) return 2;
    nx_map_fixed(SCRATCH, SCRATCH_SIZE, "la memoria de prueba");
    nx_map_fixed(LIT_BASE, LIT_SIZE, "la zona de literales");
    uint32_t *lit = (uint32_t *)(uintptr_t)LIT_BASE; // mismo patrón que lit_pattern() en fuzz.py
    for (uint32_t k = 0; k < LIT_SIZE / 4; k++) lit[k] = k * 0x9E3779B1u;
    signal(SIGSEGV, on_fault);
    signal(SIGBUS, on_fault);
    uint8_t *mem = (uint8_t *)(uintptr_t)SCRATCH;

    for (int t = 0; t < nx_ntests; t++) {
        St s;
        if (fread(&s, sizeof s, 1, in) != 1) return 2;
        Ctx c;
        memset(&c, 0, sizeof c);
        memcpy(c.x, s.x, sizeof c.x);
        c.sp = s.sp;
        c.n = s.nzcv >> 31 & 1;
        c.z = s.nzcv >> 30 & 1;
        c.c = s.nzcv >> 29 & 1;
        c.v = s.nzcv >> 28 & 1;
        memcpy(c.vr, s.v, sizeof c.vr);
        c.excl_addr = ~0ull;
        for (uint32_t k = 0; k < SCRATCH_SIZE; k++) mem[k] = (uint8_t)(k * 7 + t * 13);

        memcpy((void *)(uintptr_t)CODE, &nx_words[t], 4); // las cargas literales leen de aquí
        current = t;
        nx_tests[t](&c);

        memcpy(s.x, c.x, sizeof c.x);
        s.sp = c.sp;
        s.nzcv = (uint64_t)c.n << 31 | (uint64_t)c.z << 30 | (uint64_t)c.c << 29 | (uint64_t)c.v << 28;
        memcpy(s.v, c.vr, sizeof c.vr);
        s.mem = crc32_mem(mem, SCRATCH_SIZE);
        fwrite(&s, sizeof s, 1, out);
    }
    fclose(out);
    return 0;
}
