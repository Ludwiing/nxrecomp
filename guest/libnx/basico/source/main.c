// Homebrew de prueba hecho con libnx, sin tocar: el arranque normal de libnx inicializa
// sm, set:sys, applet, hid, time y fs. consoleDebugInit(debugDevice_SVC) manda stderr a
// svcOutputDebugString.
#include <stdio.h>
#include <switch.h>

int main(void) {
    consoleDebugInit(debugDevice_SVC);
    fprintf(stderr, "Hola desde libnx %s, recompilado a x86_64.\n", "4.12");
    return 0;
}
