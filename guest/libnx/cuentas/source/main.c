// Lo que un juego pide al arrancar además de lo básico: la cuenta (usuario preseleccionado, perfil,
// avatar), sus partidas guardadas (de la cuenta y del dispositivo), las fuentes del sistema, números
// aleatorios, la red (sin conexión), el control parental y un applet (el teclado).
// Salida en guest/esperado/cuentas.txt.
#include <stdio.h>
#include <string.h>
#include <switch.h>

int main(int argc, char **argv) {
    consoleDebugInit(debugDevice_SVC);
    FILE *o = stderr;
    Result rc = accountInitialize(AccountServiceType_Application);
    s32 n = 0;
    accountGetUserCount(&n);
    AccountUid uid = {0}, pre = {0};
    s32 total = 0;
    accountListAllUsers(&uid, 1, &total);
    Result rp = accountGetPreselectedUser(&pre);
    fprintf(o, "cuentas: 0x%x, %d usuario(s), preseleccionado 0x%x: %s\n", rc, n, rp,
            accountUidIsValid(&pre) && !memcmp(&pre, &uid, sizeof uid) ? "el mismo" : "otro");
    AccountProfile prof;
    AccountProfileBase base;
    AccountUserData datos;
    u32 tam = 0;
    if (R_SUCCEEDED(accountGetProfile(&prof, uid))) {
        accountProfileGet(&prof, &datos, &base);
        accountProfileGetImageSize(&prof, &tam);
        u8 jpg[4] = {0};
        u32 real = 0;
        static u8 img[64 * 1024];
        accountProfileLoadImage(&prof, img, sizeof img, &real);
        memcpy(jpg, img, 4);
        fprintf(o, "perfil: apodo %s, avatar JPEG: %s\n", base.nickname[0] ? "sí" : "no",
                tam == real && jpg[0] == 0xFF && jpg[1] == 0xD8 ? "sí" : "no");
        accountProfileClose(&prof);
    }

    // Partidas guardadas: escribir, cerrar, volver a abrir y leer.
    FsFileSystem fs;
    rc = fsOpen_SaveData(&fs, 0, uid);
    fprintf(o, "guardado de la cuenta: 0x%x\n", rc);
    if (R_SUCCEEDED(rc)) {
        fsFsCreateFile(&fs, "/partida.bin", 16, 0);
        FsFile f;
        if (R_SUCCEEDED(fsFsOpenFile(&fs, "/partida.bin", FsOpenMode_Write, &f))) {
            fsFileWrite(&f, 0, "nivel 7, 3 vidas", 16, FsWriteOption_Flush);
            fsFileClose(&f);
        }
        fsFsCommit(&fs);
        fsFsClose(&fs);
        fsOpen_SaveData(&fs, 0, uid);
        char buf[17] = {0};
        u64 leidos = 0;
        if (R_SUCCEEDED(fsFsOpenFile(&fs, "/partida.bin", FsOpenMode_Read, &f))) {
            fsFileRead(&f, 0, buf, 16, 0, &leidos);
            fsFileClose(&f);
        }
        fsFsClose(&fs);
        fprintf(o, "leído de vuelta: \"%s\"\n", buf);
    }
    rc = fsOpen_DeviceSaveData(&fs, 0);
    FsDirEntryType tipo;
    fprintf(o, "guardado del dispositivo: 0x%x, aparte de la cuenta: %s\n", rc,
            R_SUCCEEDED(rc) && R_FAILED(fsFsGetEntryType(&fs, "/partida.bin", &tipo)) ? "sí" : "no");
    if (R_SUCCEEDED(rc)) fsFsClose(&fs);

    rc = plInitialize(PlServiceType_User);
    PlFontData fuente = {0};
    Result rf = plGetSharedFontByType(&fuente, PlSharedFontType_Standard);
    fprintf(o, "fuentes: 0x%x 0x%x, TTF: %s\n", rc, rf,
            R_SUCCEEDED(rf) && fuente.size > 1000 && fuente.address && *(u32 *)fuente.address == 0x00000100 ? "sí" : "no");

    csrngInitialize();
    u8 azar[32] = {0}, ceros[32] = {0};
    csrngGetRandomBytes(azar, sizeof azar);
    fprintf(o, "csrng: %s\n", memcmp(azar, ceros, sizeof azar) ? "bytes al azar" : "ceros");

    rc = nifmInitialize(NifmServiceType_User);
    NifmInternetConnectionType t;
    u32 fuerza;
    NifmInternetConnectionStatus st;
    fprintf(o, "nifm: 0x%x, conexión: %s\n", rc, R_FAILED(nifmGetInternetConnectionStatus(&t, &fuerza, &st)) ? "no hay" : "hay");

    rc = pctlInitialize();
    bool restringido = true;
    pctlIsRestrictionEnabled(&restringido);
    fprintf(o, "control parental: 0x%x, restringido: %s\n", rc, restringido ? "sí" : "no");

    SwkbdConfig kbd;
    char texto[64] = "x";
    rc = swkbdCreate(&kbd, 0);
    if (R_SUCCEEDED(rc)) {
        rc = swkbdShow(&kbd, texto, sizeof texto);
        swkbdClose(&kbd);
    }
    fprintf(o, "teclado: 0x%x, texto \"%s\"\n", rc, texto);
    return 0;
}
