#include "mini.h"

__attribute__((naked, noinline)) u64 retorno_br(u64 value) {
    __asm__ volatile("mov x9, x30\n"
                     "mov x30, #0\n"
                     "add x0, x0, #7\n"
                     "br x9\n");
}

__attribute__((naked, noinline)) u64 salto_local_br(u64 value) {
    __asm__ volatile("mov x9, x30\n"
                     "adr x10, 1f\n"
                     "mov x30, #0\n"
                     "cbz x0, 1f\n"
                     "br x10\n"
                     "1: add x0, x0, #7\n"
                     "br x9\n");
}

int nx_main(void) {
    u64 a = retorno_br(35);
    u64 b = retorno_br(1);
    u64 local = salto_local_br(35);
    u64 zero = salto_local_br(0);
    pr("BR al retorno: %U %U %U %U\n", a, b, local, zero);
    flush();
    return a == 42 && b == 8 && local == 42 && zero == 7 ? 0 : 1;
}
