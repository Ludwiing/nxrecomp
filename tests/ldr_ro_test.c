#include "nxsvc.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// En el runtime lee la memoria del invitado con process_vm_readv; aquí la memoria es la del propio proceso.
// Las direcciones de la "zona prohibida" fallan como una página sin mapear.
static uintptr_t prohibida_desde, prohibida_hasta;
int nx_leer_seguro(uint64_t a, void *out, size_t n) {
    if (a < prohibida_hasta && a + n > prohibida_desde) return 0;
    memcpy(out, (const void *)(uintptr_t)a, n);
    return 1;
}

static void hexa(const uint8_t *p, char *out) {
    for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", p[i]);
}

static uint32_t mapear(uint64_t nro, uint64_t tam, uint64_t bss, uint64_t bss_tam) {
    uint64_t in[5] = {0, nro, tam, bss, bss_tam};
    NxRequest r = {.cmd = 0, .in = (uint8_t *)in, .in_size = sizeof in};
    uint32_t res = nx_ldr_ro(NULL, &r);
    assert(r.out_size == 0 && r.nout_copy == 0 && r.nout_move == 0);
    return res;
}

int main(void) {
    const uint32_t supported[] = {1, 2, 3, 4, 10};
    for (size_t i = 0; i < sizeof supported / sizeof supported[0]; ++i) {
        NxRequest r = {.cmd = supported[i]};
        assert(nx_ldr_ro(NULL, &r) == 0);
        assert(r.out_size == 0 && r.nout_objects == 0 && r.nout_copy == 0);
    }

    // Dynamic NRO mapping cannot succeed without allocating memory in guest address space.
    NxRequest map = {.cmd = 0};
    assert(nx_ldr_ro(NULL, &map) == NX_R_UNKNOWN_CMD);
    assert(map.out_size == 0);

    // SHA-256: vectores de FIPS 180-2 ("abc" y el de 56 bytes, que obliga a un segundo bloque de relleno).
    uint8_t h[32];
    char hx[65];
    nx_sha256("abc", 3, h), hexa(h, hx);
    assert(!strcmp(hx, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    nx_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, h), hexa(h, hx);
    assert(!strcmp(hx, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    // Un NRO sintético (cabecera NRO0 de 0x80 bytes y cuerpo) y un NRR con el formato de Switch 2 (ProgramId en
    // 0x4b0, como /.nrr/nrr.nrr de Mario Tennis Fever) que lista su hash.
    static uint8_t nro[0x3000] __attribute__((aligned(0x1000)));
    memcpy(nro + 0x10, "NRO0", 4);
    const uint32_t cab[] = {0, sizeof nro, 0, 0x0, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000, 0x800}; // desde 0x14
    memcpy(nro + 0x14, cab, sizeof cab);
    for (int i = 0x80; i < (int)sizeof nro; i++) nro[i] = (uint8_t)(i * 7);
    uint8_t hash_nro[32];
    nx_sha256(nro, sizeof nro, hash_nro);

    static uint8_t nrr2[0x1000];
    memcpy(nrr2, "NRR0", 4);
    const uint64_t programa = 0x0400d4201c6ea000ull;
    const uint32_t campos[] = {sizeof nrr2, 0, 0x4d0, 1};
    memcpy(nrr2 + 0x4b0, &programa, 8);
    memcpy(nrr2 + 0x4b8, campos, sizeof campos);
    memcpy(nrr2 + 0x4d0, hash_nro, 32);

    // Sin NRR registrado: identifica el NRO pero lo sigue rechazando.
    assert(nx_ro_describir_nro((uintptr_t)nro, sizeof nro, 0, 0) == 0);
    // Un NRR que no es NRR0 o con un tamaño que no coincide no registra nada.
    assert(nx_ro_registrar_nrr((uintptr_t)nro, sizeof nro) == -1);
    assert(nx_ro_registrar_nrr((uintptr_t)nrr2, sizeof nrr2 - 0x10) == -1);
    // RegisterModuleInfo por IPC (pid de relleno, dirección, tamaño) registra el hash; sigue respondiendo 0.
    uint64_t in_nrr[3] = {0, (uintptr_t)nrr2, sizeof nrr2};
    NxRequest reg = {.cmd = 2, .in = (uint8_t *)in_nrr, .in_size = sizeof in_nrr};
    assert(nx_ldr_ro(NULL, &reg) == 0);
    assert(nx_ro_describir_nro((uintptr_t)nro, sizeof nro, 0, 0) == 1);

    // El formato de Switch 1 (ProgramId en 0x330, hashes después de la cabecera de 0x350).
    static uint8_t nrr1[0x1000];
    memcpy(nrr1, "NRR0", 4);
    const uint32_t campos1[] = {sizeof nrr1, 0, 0x350, 2};
    memcpy(nrr1 + 0x330, &programa, 8);
    memcpy(nrr1 + 0x338, campos1, sizeof campos1);
    memset(nrr1 + 0x350, 0xab, 64);
    assert(nx_ro_registrar_nrr((uintptr_t)nrr1, sizeof nrr1) == 2);

    // Un NRO distinto del registrado no figura; uno ilegible o sin la marca NRO0 da -1.
    nro[0x2fff] ^= 1;
    assert(nx_ro_describir_nro((uintptr_t)nro, sizeof nro, 0, 0) == 0);
    nro[0x2fff] ^= 1;
    prohibida_desde = (uintptr_t)nro + 0x2000, prohibida_hasta = prohibida_desde + 0x1000;
    assert(nx_ro_describir_nro((uintptr_t)nro, sizeof nro, 0, 0) == -1);
    prohibida_desde = prohibida_hasta = 0;
    assert(nx_ro_describir_nro((uintptr_t)nrr2, sizeof nrr2, 0, 0) == -1);

    // El comando 0 por IPC: identifica, no finge el mapeo y no escribe en la memoria del NRO.
    uint8_t antes[sizeof nro];
    memcpy(antes, nro, sizeof nro);
    assert(mapear((uintptr_t)nro, sizeof nro, 0x1234000, 0x5000) == NX_R_UNKNOWN_CMD);
    assert(!memcmp(antes, nro, sizeof nro));

    // NX_VOLCAR_NRO=<carpeta>: una copia exacta con el SHA-256 por nombre y sin restos temporales.
    char carpeta[] = "/tmp/ldr_ro_test.XXXXXX";
    assert(mkdtemp(carpeta));
    setenv("NX_VOLCAR_NRO", carpeta, 1);
    assert(mapear((uintptr_t)nro, sizeof nro, 0, 0) == NX_R_UNKNOWN_CMD);
    unsetenv("NX_VOLCAR_NRO");
    char ruta[256];
    hexa(hash_nro, hx);
    snprintf(ruta, sizeof ruta, "%s/%s.nro", carpeta, hx);
    FILE *f = fopen(ruta, "rb");
    assert(f);
    static uint8_t copia[sizeof nro + 1];
    assert(fread(copia, 1, sizeof copia, f) == sizeof nro && !memcmp(copia, nro, sizeof nro));
    fclose(f);
    DIR *d = opendir(carpeta);
    int entradas = 0;
    for (struct dirent *e; (e = readdir(d));) entradas += e->d_name[0] != '.';
    closedir(d);
    assert(entradas == 1);
    unlink(ruta);
    rmdir(carpeta);

    // NX_VOLCAR_NRO con una carpeta que no existe (la de NX2Linux, <juego>/nro, la primera vez): la crea.
    char madre[] = "/tmp/ldr_ro_test.XXXXXX";
    assert(mkdtemp(madre));
    snprintf(carpeta, sizeof carpeta, "%s", madre);
    char hija[256];
    snprintf(hija, sizeof hija, "%s/nro", madre);
    setenv("NX_VOLCAR_NRO", hija, 1);
    assert(mapear((uintptr_t)nro, sizeof nro, 0, 0) == NX_R_UNKNOWN_CMD);
    unsetenv("NX_VOLCAR_NRO");
    snprintf(ruta, sizeof ruta, "%s/%s.nro", hija, hx);
    assert(access(ruta, R_OK) == 0);
    unlink(ruta), rmdir(hija), rmdir(madre);

    // Un módulo traducido de antemano: su base fija (aquí, memoria del propio proceso) con basura previa.
    static uint8_t destino[0x3000 + 0x800] __attribute__((aligned(0x1000)));
    memset(destino, 0xcc, sizeof destino);
    NxRoModulo traducido = {"ro_prueba", (uintptr_t)destino, sizeof nro, 0x800, {0}};
    memcpy(traducido.sha256, hash_nro, 32);
    nx_ro_modulos_traducidos(&traducido, 1);

    // Otro NRO (un byte distinto): no es el traducido, sigue rechazado y la base queda intacta.
    nro[0x2fff] ^= 1;
    assert(mapear((uintptr_t)nro, sizeof nro, 0, 0x800) == NX_R_UNKNOWN_CMD);
    nro[0x2fff] ^= 1;
    assert(destino[0] == 0xcc);
    // Tamaño de bss menor que el del módulo: rechazado sin copiar.
    assert(mapear((uintptr_t)nro, sizeof nro, 0, 0x7ff) == NX_R_UNKNOWN_CMD);
    assert(destino[0] == 0xcc);

    // El NRO traducido: copia exacta en la base, bss en cero y la base como respuesta.
    uint64_t in_map[5] = {0, (uintptr_t)nro, sizeof nro, 0x1234000, 0x800};
    NxRequest m0 = {.cmd = 0, .in = (uint8_t *)in_map, .in_size = sizeof in_map};
    assert(nx_ldr_ro(NULL, &m0) == 0);
    uint64_t dada;
    assert(m0.out_size == 8);
    memcpy(&dada, m0.out, 8);
    assert(dada == (uintptr_t)destino);
    assert(!memcmp(destino, nro, sizeof nro));
    for (size_t i = sizeof nro; i < sizeof destino; i++) assert(destino[i] == 0);
    // Una segunda carga sin descargar no tiene código traducido en otra dirección: rechazada.
    assert(mapear((uintptr_t)nro, sizeof nro, 0, 0x800) == NX_R_UNKNOWN_CMD);
    // UnmapManualLoadModuleMemory(pid, base) la libera y la carga vuelve a funcionar.
    uint64_t in_unmap[2] = {0, (uintptr_t)destino};
    NxRequest u = {.cmd = 1, .in = (uint8_t *)in_unmap, .in_size = sizeof in_unmap};
    assert(nx_ldr_ro(NULL, &u) == 0);
    NxRequest m1 = {.cmd = 0, .in = (uint8_t *)in_map, .in_size = sizeof in_map};
    assert(nx_ldr_ro(NULL, &m1) == 0 && m1.out_size == 8);
    nx_ro_modulos_traducidos(NULL, 0);

    NxRequest unknown = {.cmd = 99};
    assert(nx_ldr_ro(NULL, &unknown) == NX_R_UNKNOWN_CMD);
    // rtld publishes its nn::ro state into the SDK variables the recompiler lists ({index, address}).
    uint64_t auto_list = 0, manual_list = 0, debug_flag = 0, lookup_auto = 0, lookup_manual = 0;
    const uint64_t vars[][2] = {
        {0, (uint64_t)(uintptr_t)&auto_list},   {1, (uint64_t)(uintptr_t)&manual_list},
        {2, (uint64_t)(uintptr_t)&debug_flag},  {3, (uint64_t)(uintptr_t)&lookup_auto},
        {4, (uint64_t)(uintptr_t)&lookup_manual},
    };
    assert(nx_rtld_publicar_ro(vars, 5) == 3);
    // nn::ro::InitializeWithPortName: *g_pLookupGlobalManualFunctionPointer = LookupGlobalManual.
    uint64_t *slot = (uint64_t *)(uintptr_t)lookup_manual;
    assert(slot && *slot == 0);
    *slot = 0x8ee9980;
    // The manual list starts empty and circular, as rtld leaves it: {prev, next} point at the head.
    uint64_t *head = (uint64_t *)(uintptr_t)manual_list;
    assert(head && head[0] == manual_list && head[1] == manual_list);
    assert(debug_flag && *(uint64_t *)(uintptr_t)debug_flag == 0);
    // The auto-load list and its lookup need rtld's ModuleObjects: left unpublished (zero), not faked.
    assert(auto_list == 0 && lookup_auto == 0);
    // Con módulos de nn::ro traducidos: la lista del arranque vacía y circular, y la búsqueda global.
    assert(nx_rtld_publicar_busqueda(vars, 5, 0x5555000) == 2);
    uint64_t *lista = (uint64_t *)(uintptr_t)auto_list;
    assert(lista && lista[0] == auto_list && lista[1] == auto_list);
    assert(lookup_auto == 0x5555000);
    // Sin función de búsqueda solo se publica la lista; lo demás no se toca.
    lookup_auto = 0;
    assert(nx_rtld_publicar_busqueda(vars, 5, 0) == 1 && lookup_auto == 0);
    assert(*slot == 0x8ee9980 && head[0] == manual_list);
    assert(nx_rtld_publicar_ro(vars, 0) == 0);
    puts("ldr:ro: NRR/NRO identification, translated-module mapping and refusal of untranslated NROs: PASS");
    return 0;
}
