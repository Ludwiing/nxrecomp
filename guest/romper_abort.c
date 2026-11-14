// svcBreak sin el bit de aviso: el proceso termina (código 1 en NXrecomp). Con tamaño 4, el resultado de
// diagAbortWithResult se lee del búfer: aquí 0x1234 (2052-0009).
#include "mini.h"

static u32 resultado = 0x1234;

int nx_main(void) {
    pr("antes del Break\n");
    flush();
    register u64 x0 __asm__("x0") = 1;   // Assert
    register u64 x1 __asm__("x1") = (u64)(uintptr_t)&resultado;
    register u64 x2 __asm__("x2") = 4;
    __asm__ volatile("svc #0x26" : "+r"(x0), "+r"(x1), "+r"(x2) : : "memory");
    pr("no debería llegar aquí\n");
    flush();
    return 0;
}
