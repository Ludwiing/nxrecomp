// Atómicos LSE con signo (LDSMAX/LDSMIN) de 8, 16 y 32 bits, con resultados conocidos: Unicorn los
// compara sin signo y no sirve de referencia para ellos (ver tests/fuzz.py).
#include "nxrt.h"
#include <stdio.h>

static int fallos;
#define CHECK(expr, want)                                                                   \
    do {                                                                                    \
        uint64_t got_ = (uint64_t)(expr);                                                   \
        if (got_ != (uint64_t)(want)) {                                                     \
            fprintf(stderr, "FALLO %s: %#llx, se esperaba %#llx\n", #expr,                  \
                    (unsigned long long)got_, (unsigned long long)(want));                  \
            fallos++;                                                                       \
        }                                                                                   \
    } while (0)

int main(void) {
    uint32_t w = 5;
    CHECK(nx_ldsmin32((uint64_t)(uintptr_t)&w, 0xfffffffe), 5);  // devuelve lo que había
    CHECK(w, 0xfffffffe);                                        // min(5, -2) = -2
    w = 5;
    CHECK(nx_ldsmax32((uint64_t)(uintptr_t)&w, 0xfffffffe), 5);
    CHECK(w, 5);                                                 // max(5, -2) = 5
    w = 0xfffffffe;                                              // solo cuentan los 32 bits bajos de Rs
    CHECK(nx_ldsmax32((uint64_t)(uintptr_t)&w, 0xffffffff00000003ull), 0xfffffffe);
    CHECK(w, 3);

    uint16_t h = 0x8000;                                         // -32768
    CHECK(nx_ldsmax16((uint64_t)(uintptr_t)&h, 0x7fff), 0x8000);
    CHECK(h, 0x7fff);
    CHECK(nx_ldsmin16((uint64_t)(uintptr_t)&h, 0xffff), 0x7fff); // min(32767, -1) = -1
    CHECK(h, 0xffff);

    uint8_t b = 0x7f;
    CHECK(nx_ldsmin8((uint64_t)(uintptr_t)&b, 0x80), 0x7f);      // min(127, -128) = -128
    CHECK(b, 0x80);
    CHECK(nx_ldsmax8((uint64_t)(uintptr_t)&b, 0xff), 0x80);      // max(-128, -1) = -1
    CHECK(b, 0xff);
    CHECK(nx_ldumax8((uint64_t)(uintptr_t)&b, 0x01), 0xff);      // sin signo: 255 sigue siendo mayor
    CHECK(b, 0xff);

    if (fallos)
        return 1;
    puts("lse_test: bien");
    return 0;
}
