// Homebrew de prueba con las API de libnx, como las usa cualquier homebrew: hilos con Mutex y
// CondVar de libnx, idioma, versión del sistema, relojes, archivos y carpetas en la SD con
// fopen/opendir de C, lectura de los mandos y memoria. Salida en guest/esperado/sistema.txt.
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>

static Mutex mtx;
static u64 counter;
static void count(void *arg) {
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        mutexLock(&mtx);
        counter++;
        mutexUnlock(&mtx);
    }
}

static Mutex qm;
static CondVar qcv;
static int ready, value;
static void producer(void *arg) {
    (void)arg;
    svcSleepThread(1000000);
    mutexLock(&qm);
    value = 2026;
    ready = 1;
    condvarWakeAll(&qcv);
    mutexUnlock(&qm);
}

int main(void) {
    consoleDebugInit(debugDevice_SVC);
    FILE *o = stderr;
    fprintf(o, "Homebrew con libnx: hilos, idioma, relojes, SD, mandos y memoria.\n");
    fprintf(o, "sistema: %u.%u.%u\n", HOSVER_MAJOR(hosversionGet()), HOSVER_MINOR(hosversionGet()),
            HOSVER_MICRO(hosversionGet()));

    Thread t[4];
    for (int i = 0; i < 4; i++) {
        threadCreate(&t[i], count, NULL, NULL, 0x10000, 0x2C, -2);
        threadStart(&t[i]);
    }
    for (int i = 0; i < 4; i++) {
        threadWaitForExit(&t[i]);
        threadClose(&t[i]);
    }
    fprintf(o, "4 hilos de libnx × 50000 con Mutex: %llu\n", (unsigned long long)counter);

    Thread p;
    threadCreate(&p, producer, NULL, NULL, 0x10000, 0x2C, -2);
    threadStart(&p);
    mutexLock(&qm);
    while (!ready) condvarWait(&qcv, &qm);
    mutexUnlock(&qm);
    threadWaitForExit(&p);
    threadClose(&p);
    fprintf(o, "CondVar: llegó %d\n", value);

    Result rc = setInitialize();
    u64 code = 0;
    SetLanguage lang;
    if (R_SUCCEEDED(rc)) rc = setGetSystemLanguage(&code);
    if (R_SUCCEEDED(rc)) rc = setMakeLanguage(code, &lang);
    fprintf(o, "idioma: %s (SetLanguage %d), rc 0x%x\n", (char *)&code, (int)lang, rc);
    setExit();

    u64 now = 0;
    rc = timeGetCurrentTime(TimeType_UserSystemClock, &now);
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    Result rc2 = timeToCalendarTimeWithMyRule(now, &cal, &info);
    fprintf(o, "reloj: rc 0x%x, hora válida: %s; calendario: rc 0x%x, año válido: %s\n", rc,
            now > 1700000000ull ? "sí" : "no", rc2, cal.year >= 2026 ? "sí" : "no");

    mkdir("sdmc:/nxrecomp_prueba", 0777);
    FILE *f = fopen("sdmc:/nxrecomp_prueba/hola.txt", "w");
    fprintf(o, "SD: crear archivo: %s\n", f ? "sí" : "no");
    if (f) {
        fprintf(f, "Escrito por un homebrew de Switch recompilado a Linux.\n");
        fclose(f);
    }
    char line[128] = {0};
    f = fopen("sdmc:/nxrecomp_prueba/hola.txt", "r");
    if (f) {
        fgets(line, sizeof line, f);
        fclose(f);
    }
    fprintf(o, "SD: leído: %s", line);
    struct stat st;
    stat("sdmc:/nxrecomp_prueba/hola.txt", &st);
    fprintf(o, "SD: tamaño %lld bytes\n", (long long)st.st_size);
    rename("sdmc:/nxrecomp_prueba/hola.txt", "sdmc:/nxrecomp_prueba/chao.txt");
    DIR *d = opendir("sdmc:/nxrecomp_prueba");

    struct dirent *e;
    while (d && (e = readdir(d))) fprintf(o, "SD: en la carpeta: %s\n", e->d_name);
    if (d) closedir(d);
    remove("sdmc:/nxrecomp_prueba/chao.txt");
    rmdir("sdmc:/nxrecomp_prueba");
    fprintf(o, "SD: borrado, la carpeta existe: %s\n", stat("sdmc:/nxrecomp_prueba", &st) == 0 ? "sí" : "no");

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    padUpdate(&pad);
    fprintf(o, "mandos: botones apretados 0x%llx\n", (unsigned long long)padGetButtons(&pad));
    fprintf(o, "appletMainLoop: %s\n", appletMainLoop() ? "sigue" : "termina");

    u8 *big = malloc(64 << 20);
    u64 sum = 0;
    if (big) {
        for (u32 i = 0; i < (64u << 20); i += 4096) big[i] = (u8)(i >> 12), sum += big[i];
        free(big);
    }
    fprintf(o, "malloc de 64 MiB: %s, suma %llu\n", big ? "sí" : "no", (unsigned long long)sum);
    fprintf(o, "Fin.\n");
    return 0;
}
