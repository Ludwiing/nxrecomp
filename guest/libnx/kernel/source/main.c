// Lo del kernel que nxnative vio fallar con juegos reales: el id del hilo en TLS+0x110 (SDK 22),
// pausar un hilo y leer su contexto (recolectores de basura), los ticks de CPU de un hilo,
// WaitForAddress de 64 bits (SDK 19), SignalToAddress tipo 2 con cuenta <= 0 y SetHeapSize de más.
// Salida en guest/esperado/kernel.txt.
#include <stdio.h>
#include <string.h>
#include <switch.h>

static volatile u64 contador;
static volatile int parar;
static void trabajador(void *arg) {
    (void)arg;
    while (!parar) contador++;
}

static u32 palabra = 5;
static void esperador(void *arg) {
    (void)arg;
    svcWaitForAddress(&palabra, 2, 5, 1000000000); // WaitIfEqual
}

static u32 tls_id(void) { return *(u32 *)((u8 *)armGetTls() + 0x110); }
static u32 id_hilo_otro;
static void mirar_tls(void *arg) {
    (void)arg;
    u64 id;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    id_hilo_otro = tls_id() == (u32)id;
}

int main(int argc, char **argv) {
    consoleDebugInit(debugDevice_SVC);
    u64 id;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    Thread t;
    threadCreate(&t, mirar_tls, NULL, NULL, 0x4000, 0x2C, -2);
    threadStart(&t);
    threadWaitForExit(&t);
    threadClose(&t);
    fprintf(stderr, "id del hilo en TLS+0x110: %s, %s\n", tls_id() == (u32)id ? "sí" : "no", id_hilo_otro ? "sí" : "no");

    threadCreate(&t, trabajador, NULL, NULL, 0x4000, 0x2C, -2);
    threadStart(&t);
    svcSleepThread(20000000);
    Result rc = threadPause(&t);
    u64 a = contador;
    svcSleepThread(20000000);
    u64 b = contador;
    ThreadContext ctx;
    Result rc2 = threadDumpContext(&ctx, &t);
    u64 sp = ctx.sp, base = (u64)t.stack_mirror, tope = base + t.stack_sz;
    fprintf(stderr, "pausado: %s (0x%x), contexto: %s (0x%x)\n", R_SUCCEEDED(rc) && a == b ? "sí" : "no", rc,
           R_SUCCEEDED(rc2) && sp > base && sp <= tope ? "sí" : "no", rc2);
    threadResume(&t);
    svcSleepThread(20000000);
    fprintf(stderr, "reanudado: %s\n", contador > b ? "sí" : "no");
    parar = 1;
    threadWaitForExit(&t);
    threadClose(&t);

    u64 ticks = 0;
    svcGetInfo(&ticks, InfoType_ThreadTickCount, CUR_THREAD_HANDLE, UINT64_MAX);
    fprintf(stderr, "ticks de CPU del hilo: %s\n", ticks > 0 ? "sí" : "no");

    static u64 doble __attribute__((aligned(8))) = 0x100000001ull;
    Result igual = svcWaitForAddress(&doble, 3, 0x100000001ll, 1000000);
    Result distinto = svcWaitForAddress(&doble, 3, 0x1ll, 1000000);
    fprintf(stderr, "WaitForAddress de 64 bits: igual 0x%x, distinto 0x%x\n", igual, distinto);

    threadCreate(&t, esperador, NULL, NULL, 0x4000, 0x2C, -2);
    threadStart(&t);
    svcSleepThread(20000000);
    Result s = svcSignalToAddress(&palabra, 2, 5, -1);
    threadWaitForExit(&t);
    threadClose(&t);
    fprintf(stderr, "SignalToAddress tipo 2, cuenta -1: 0x%x, la palabra queda en %u\n", s, palabra);

    void *heap;
    fprintf(stderr, "SetHeapSize de 64 GiB: 0x%x\n", svcSetHeapSize(&heap, 64ull << 30));
    return 0;
}
