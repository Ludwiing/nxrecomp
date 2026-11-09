// runtime/nvn/nvn2_program.h: secciones de control sintéticas de NVN2.
//   cc -O1 -Iruntime/nvn tests/nvn2_program_test.c -o build/nvn2_program_test && build/nvn2_program_test
#include <stdio.h>

#include "nvn2_program.h"

static int fallas;
static void ver(const char *nombre, int cond) {
    printf("%s: %s\n", cond ? "OK" : "MAL", nombre);
    fallas += !cond;
}

static void control(uint8_t *p, uint32_t largo, uint32_t c1o, uint32_t c1t, uint16_t etapa, uint16_t gx, uint16_t gy,
                    uint16_t gz) {
    memset(p, 0, NVN2_CONTROL_TAM);
    uint32_t m = NVN2_CONTROL_MAGIA;
    memcpy(p, &m, 4);
    memcpy(p + 0x38, &largo, 4);
    memcpy(p + 0x40, &c1o, 4);
    memcpy(p + 0x44, &c1t, 4);
    memcpy(p + 0x56, &etapa, 2);
    uint16_t g[3] = {gx, gy, gz};
    memcpy(p + 0x58, g, 6);
}

int main(void) {
    uint8_t p[NVN2_CONTROL_TAM];
    Nvn2Control c;
    control(p, 0x400, 0x480, 0x40, NVN2_FRAGMENT, 0, 0, 0);
    ver("fragment: lee largo, c1 y etapa; el código empieza tras la cabecera de 128 bytes",
        nvn2_control_leer(p, sizeof p, 0x1000, &c) && c.largo_codigo == 0x400 && c.c1_off == 0x480 && c.c1_tam == 0x40 &&
            c.grafica && nvn2_codigo_inicio(&c) == 128);
    control(p, 0x800, 0x800, 0x100, NVN2_COMPUTE, 64, 1, 1);
    ver("compute: grupo de trabajo 64x1x1, el código empieza en 0",
        nvn2_control_leer(p, sizeof p, 0x1000, &c) && !c.grafica && c.grupo[0] == 64 && c.grupo[1] == 1 &&
            nvn2_codigo_inicio(&c) == 0);
    control(p, 0x400, 0, 0, NVN2_COMPUTE, 0, 1, 1);
    ver("compute sin grupo de trabajo: rechazado", !nvn2_control_leer(p, sizeof p, 0, &c));
    control(p, 0x408, 0, 0, NVN2_VERTEX, 0, 0, 0);
    ver("largo que no es múltiplo de 16 (instrucción de Ampere): rechazado", !nvn2_control_leer(p, sizeof p, 0, &c));
    control(p, 0x1000, 0, 0, NVN2_VERTEX, 0, 0, 0);
    ver("código más largo que el área: rechazado", !nvn2_control_leer(p, sizeof p, 0x1000, &c));
    control(p, 0x400, 0, 0, 9, 0, 0, 0);
    ver("etapa desconocida: rechazada", !nvn2_control_leer(p, sizeof p, 0, &c));
    p[0] ^= 1;
    ver("sin la magia: rechazado", !nvn2_control_leer(p, sizeof p, 0, &c));
    ver("sección corta: rechazada", !nvn2_control_leer(p, 0x40, 0, &c));
    return fallas ? 1 : 0;
}
