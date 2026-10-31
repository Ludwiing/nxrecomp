// Módulo "sdk" de prueba: exporta funciones y datos que importa "main", como el SDK de
// Nintendo exporta nn::* y la libc a los juegos.
#include "../mini.h"

int sdk_counter = 5;
static int sdk_inits;

__attribute__((constructor)) static void sdk_init(void) { sdk_inits = 42; }

void sdk_printf(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    vpr(f, ap);
    va_end(ap);
    flush();
}

long sdk_add(long a, long b) { return a + b; }
long sdk_sub(long a, long b) { return a - b; }
const char *sdk_version(void) { return "SDK de prueba 1.0"; }
int sdk_init_value(void) { return sdk_inits; }
