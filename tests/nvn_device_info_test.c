#include <stdio.h>

#include "nvn_device_info.h"

static int fail;
static void check(const char *name, int ok) {
    printf("%s: %s\n", ok ? "OK" : "FAIL", name);
    fail += !ok;
}

int main(void) {
    static const int switch1[0x60] = {[0x00] = 55, [0x01] = 15, [0x17] = 2, [0x1C] = 1536, [0x1D] = 1024, [0x5F] = 4096};
    int value = -9;
    check("Switch 1 conserva API 55.15", nvn_device_info_get(0, 0, switch1, 0x60, &value) && value == 55);
    check("Switch 1 conserva el límite 1536 en 0x1C",
          nvn_device_info_get(0, 0x1C, switch1, 0x60, &value) && value == 1536);
    check("Switch 1 rechaza propiedades fuera de su tabla", !nvn_device_info_get(0, 0x60, switch1, 0x60, &value));
    check("Switch 2 anuncia la NVN API mínima que requiere Sports Resort (231.41)",
          nvn_device_info_get(1, 0, switch1, 0x60, &value) && value == 231 &&
              nvn_device_info_get(1, 1, switch1, 0x60, &value) && value == 41);
    check("Switch 2 mantiene 16 vertex bindings en el host", nvn_device_info_get(1, 0x06, switch1, 0x60, &value) && value == 16);
    check("Switch 2 anuncia 32 atributos de vértice", nvn_device_info_get(1, 0x0E, switch1, 0x60, &value) && value == 32);
    check("Switch 2 devuelve límite 1024 en 0x1C y 0x1D",
          nvn_device_info_get(1, 0x1C, switch1, 0x60, &value) && value == 1024 &&
              nvn_device_info_get(1, 0x1D, switch1, 0x60, &value) && value == 1024);
    check("Switch 2 devuelve 16 niveles de textura", nvn_device_info_get(1, 0x1B, switch1, 0x60, &value) && value == 16);
    check("Switch 2 devuelve shader-code version 201", nvn_device_info_get(1, 0x2E, switch1, 0x60, &value) && value == 201);
    check("propiedad runtime-only baja de 0x60 usa fallback X1",
          nvn_device_info_get(1, 0x17, switch1, 0x60, &value) && value == 2 && nvn_device_info_switch2_is_runtime(0x17));
    check("propiedad runtime-only nueva queda en cero",
          nvn_device_info_get(1, 0x62, switch1, 0x60, &value) && value == 0 && nvn_device_info_switch2_is_runtime(0x62));
    check("Switch 2 devuelve cero y permite continuar ante enum desconocido",
          nvn_device_info_get(1, 0xAA, switch1, 0x60, &value) && value == 0 && nvn_device_info_switch2_is_unknown(0xAA));
    check("Switch 1 sigue rechazando enum fuera de su rango", !nvn_device_info_get(0, 0xAA, switch1, 0x60, &value));
    check("el helper rechaza salida nula", !nvn_device_info_get(1, 0, switch1, 0x60, NULL));
    return fail ? 1 : 0;
}
