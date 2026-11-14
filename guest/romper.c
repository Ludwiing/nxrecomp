// svcBreak con NotificationOnlyFlag (bit 31): sin depurador, el kernel vuelve y el proceso sigue
// (mesosphere kern_svc_exception.cpp). PreLoadDll/PostLoadDll los usa nn::ro alrededor de cargar un módulo.
#include "mini.h"

static void romper(u64 razon, u64 dir, u64 tam) {
    register u64 x0 __asm__("x0") = razon;
    register u64 x1 __asm__("x1") = dir;
    register u64 x2 __asm__("x2") = tam;
    __asm__ volatile("svc #0x26" : "+r"(x0), "+r"(x1), "+r"(x2) : : "memory");
}

static u8 info[0x80];

int nx_main(void) {
    romper(0x80000003, (u64)(uintptr_t)info, sizeof info);   // PreLoadDll, solo aviso
    pr("sigue tras PreLoadDll\n");
    romper(0x80000004, (u64)(uintptr_t)info, sizeof info);   // PostLoadDll, solo aviso
    pr("sigue tras PostLoadDll\n");
    flush();
    return 0;
}
