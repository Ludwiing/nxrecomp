// Prueba de mandos con libnx: dibuja la consola en pantalla (así el runtime cuenta cuadros) y
// escribe por svcOutputDebugString cada botón que baja y sube, hasta que se aprieta +.
#include <stdio.h>
#include <switch.h>

static const char *const names[] = {"A", "B", "X", "Y", "StickL", "StickR", "L", "R", "ZL", "ZR", "+", "-",
                                    "Izquierda", "Arriba", "Derecha", "Abajo"};

static void list(const char *what, u64 bits) {
    if (!bits) return;
    fprintf(stderr, "%s:", what);
    for (int i = 0; i < 16; i++)
        if (bits & BIT(i)) fprintf(stderr, " %s", names[i]);
    fprintf(stderr, "\n");
}

int main(void) {
    consoleInit(NULL);
    consoleDebugInit(debugDevice_SVC);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    fprintf(stderr, "Prueba de mandos: esperando botones.\n");
    printf("Prueba de mandos\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad), up = padGetButtonsUp(&pad);
        list("bajan", down & 0xFFFF);
        list("suben", up & 0xFFFF);
        if (down & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    fprintf(stderr, "Fin: salí con +.\n");
    consoleExit(NULL);
    return 0;
}
