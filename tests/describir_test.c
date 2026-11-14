// runtime/nx_describir.h: módulo y símbolo de una dirección del invitado (svcBreak y la pila del invitado). MIT.
#include <stdio.h>
#include <string.h>
#include "nx_describir.h"

const NxModule nx_modules[] = {{"rtld", 0x8000000, 0x4000, 0, 0, 0, 0}, {"main", 0x8004000, 0x10000, 0, 0, 0, 0},
                               {"sdk", 0x8100000, 0x8000, 0, 0, 0, 0}};
const int nx_nmodules = 3;
const NxSymbol nx_symbols[] = {{"_start", 0x8000000}, {"nnMain", 0x8004100}, {"helper", 0x8004800},
                               {"nn::diag::Abort", 0x8100200}};
const int nx_nsymbols = 4;

static int fallos;
static void caso(uint64_t a, const char *esperado) {
    char b[256];
    nx_describir(a, b, sizeof b);
    if (strcmp(b, esperado)) printf("MAL: 0x%llx -> %s, se esperaba %s\n", (unsigned long long)a, b, esperado), fallos++;
}

int main(void) {
    caso(0x8004120, "main+0x120 (nnMain+0x20)");
    caso(0x8004800, "main+0x800 (helper+0x0)");
    caso(0x8100210, "sdk+0x210 (nn::diag::Abort+0x10)");
    caso(0x8004010, "main+0x10");                 // antes del primer símbolo del módulo: no toma el de rtld
    caso(0x8100100, "sdk+0x100");                 // idem: el último símbolo <= a es de main
    caso(0x8200000, "?");                         // fuera de los módulos
    caso(0, "?");
    if (!fallos) printf("OK: dirección del invitado -> módulo+offset (símbolo+offset), sin tomar símbolos de otro módulo\n");
    return fallos != 0;
}
