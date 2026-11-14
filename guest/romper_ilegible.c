// svcBreak con un búfer de resultado que no está mapeado: el diagnóstico no puede fallar por leerlo (antes
// era *(uint32_t *)x1: SIGSEGV dentro del manejo del Break).
#include "mini.h"

int nx_main(void) {
    register u64 x0 __asm__("x0") = 0;   // Panic
    register u64 x1 __asm__("x1") = 0x10;
    register u64 x2 __asm__("x2") = 4;
    __asm__ volatile("svc #0x26" : "+r"(x0), "+r"(x1), "+r"(x2) : : "memory");
    return 0;
}
