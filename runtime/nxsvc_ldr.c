#include "nxsvc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// El estado de nn::ro que rtld guarda en su propia memoria y publica en el SDK al enlazar (rtld de Switch 2):
// las dos listas empiezan vacías ({anterior, siguiente} apuntando a sí mismas, como las deja rtld al arrancar), la
// marca de depuración en cero y la ranura del buscador manual vacía: nn::ro::Initialize* escribe en ella su
// LookupGlobalManual. Se usan direcciones del anfitrión, como el resto del runtime (el invitado comparte el mapa).
enum { RO_AUTO_LIST, RO_MANUAL_LIST, RO_DEBUG_FLAG, RO_LOOKUP_AUTO, RO_LOOKUP_MANUAL };
static struct {
    uint64_t manual_list[2];
    uint64_t debug_flag;
    uint64_t lookup_manual;
    uint64_t auto_list[2];
} rtld_ro;

int nx_rtld_publicar_ro(const uint64_t (*vars)[2], int n) {
    rtld_ro.manual_list[0] = rtld_ro.manual_list[1] = (uint64_t)(uintptr_t)rtld_ro.manual_list;
    int hechas = 0;
    for (int k = 0; k < n; k++) {
        uint64_t valor;
        switch (vars[k][0]) {
        case RO_MANUAL_LIST: valor = (uint64_t)(uintptr_t)rtld_ro.manual_list; break;
        case RO_DEBUG_FLAG: valor = (uint64_t)(uintptr_t)&rtld_ro.debug_flag; break;
        case RO_LOOKUP_MANUAL: valor = (uint64_t)(uintptr_t)&rtld_ro.lookup_manual; break;
        default:
            // g_pAutoLoadList y g_pLookupGlobalAutoFunctionPointer: en rtld son la lista de los ModuleObject de
            // los módulos cargados y su buscador sobre ella. Aquí no hay ModuleObject: se dejan en cero (un uso
            // falla en ese punto, visible) en vez de publicar una lista vacía que respondería mal.
            if (getenv("NX_TRACE_BOOT"))
                fprintf(stderr, "[boot] rtld: nn::ro índice %llu en 0x%llx sin publicar (falta la lista de módulos)\n",
                        (unsigned long long)vars[k][0], (unsigned long long)vars[k][1]);
            continue;
        }
        memcpy((void *)(uintptr_t)vars[k][1], &valor, sizeof valor);
        hechas++;
    }
    return hechas;
}

// Con módulos de nn::ro traducidos, el SDK los reubica y enlaza él mismo dentro de LoadModule (sdk de Switch 2,
// LoadModule+0x5f0..0x8f0): por cada importación llama *g_pLookupGlobalAutoFunctionPointer(nombre) y espera la
// dirección absoluta o 0 (luego prueba los módulos manuales y, si el símbolo es débil, deja 0); al terminar recorre
// *g_pAutoLoadList para volver a enlazar los módulos anteriores. Aquí: la búsqueda es la tabla de símbolos globales
// del arranque (nxmain.c) y la lista, vacía y circular. Sin módulos traducidos no se publica nada de esto.
int nx_rtld_publicar_busqueda(const uint64_t (*vars)[2], int n, uint64_t buscar) {
    rtld_ro.auto_list[0] = rtld_ro.auto_list[1] = (uint64_t)(uintptr_t)rtld_ro.auto_list;
    int hechas = 0;
    for (int k = 0; k < n; k++) {
        uint64_t valor;
        if (vars[k][0] == RO_AUTO_LIST) valor = (uint64_t)(uintptr_t)rtld_ro.auto_list;
        else if (vars[k][0] == RO_LOOKUP_AUTO && buscar) valor = buscar;
        else continue;
        memcpy((void *)(uintptr_t)vars[k][1], &valor, sizeof valor);
        hechas++;
    }
    return hechas;
}

// ---------- nn::ro: lo que el juego registra y lo que pide cargar ----------
// Algunos títulos no traen todo su código en el ExeFS: lo arman en memoria (p. ej. un main que es solo un
// cargador) o lo leen de la RomFS, y lo cargan con nn::ro::LoadModule, que pide a ldr:ro el comando 0
// (MapManualLoadModuleMemory). NXrecomp traduce el código antes de ejecutar, así que hay dos pasadas:
//   1. NRO sin traducir: el comando 0 lo identifica (cabecera, SHA-256, si figura en un NRR registrado), lo vuelca
//      con NX_VOLCAR_NRO=<carpeta> y sigue rechazado: no hay código para esa memoria.
//   2. Recompilado con nxrecomp.py --modulos-ro <carpeta>: el NRO tiene una base fija detrás de los módulos del
//      arranque; el comando 0 copia allí los bytes que arma el juego (mismo SHA-256), pone el bss en cero y devuelve
//      esa base. El SDK reubica, enlaza (búsqueda global publicada por nxmain.c) y llama su init como en Horizon.
//
// ABI (Switchbrew «RO services»; la misma que usa el nn::ro del SDK de Switch 2, visto en LoadModule+0x88):
//   cmd 0 MapManualLoadModuleMemory(pid, u64 nro_address, u64 nro_size, u64 bss_address, u64 bss_size) -> u64
//   cmd 2 RegisterModuleInfo(pid, u64 nrr_address, u64 nrr_size)
// Los datos crudos empiezan por el u64 de relleno del ClientProcessId.

static uint64_t in64(const NxRequest *r, uint32_t off) {
    uint64_t v = 0;
    if (r->in && off + 8 <= r->in_size) memcpy(&v, r->in + off, 8);
    return v;
}

// SHA-256 (FIPS 180-4), para comparar el NRO con los hashes de los NRR registrados.
typedef struct {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len;
} Sha256;

static uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_bloque(Sha256 *s, const uint8_t *p) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d, s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void sha256_inicio(Sha256 *s) {
    static const uint32_t H0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, H0, sizeof H0);
    s->len = 0;
}

static void sha256_datos(Sha256 *s, const uint8_t *p, size_t n) {
    while (n) {
        size_t usado = s->len & 63, k = 64 - usado < n ? 64 - usado : n;
        memcpy(s->buf + usado, p, k);
        s->len += k, p += k, n -= k;
        if ((s->len & 63) == 0) sha256_bloque(s, s->buf);
    }
}

static void sha256_fin(Sha256 *s, uint8_t out[32]) {
    uint64_t bits = s->len * 8;
    uint8_t relleno[72] = {0x80};
    size_t n = ((s->len & 63) < 56 ? 56 : 120) - (s->len & 63);
    sha256_datos(s, relleno, n);
    uint8_t largo[8];
    for (int i = 0; i < 8; i++) largo[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha256_datos(s, largo, 8);
    for (int i = 0; i < 8; i++)
        out[4 * i] = s->h[i] >> 24, out[4 * i + 1] = s->h[i] >> 16, out[4 * i + 2] = s->h[i] >> 8, out[4 * i + 3] = s->h[i];
}

void nx_sha256(const void *datos, size_t n, uint8_t out[32]) {
    Sha256 s;
    sha256_inicio(&s);
    sha256_datos(&s, datos, n);
    sha256_fin(&s, out);
}

static void hex(const uint8_t *p, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", p[i]);
    out[2 * n] = 0;
}

// Hashes de los NRO que permiten los NRR registrados (cmd 2), en orden de registro.
#define RO_MAX_HASHES 256
static struct {
    uint8_t hash[RO_MAX_HASHES][32];
    int n;
    uint64_t programa;
} nrr;

// Cabecera de un NRR: en Switch 1 la firma es RSA-2048 y el ProgramId está en 0x330; en Switch 2 (visto en la
// RomFS de Mario Tennis Fever, `/.nrr/nrr.nrr`) la clave y las firmas miden 0x180 (RSA-3072) y todo se corre 0x180.
// Se elige la variante cuyo campo Size coincide con el tamaño pedido y cuya tabla de hashes cabe en él.
int nx_ro_registrar_nrr(uint64_t addr, uint64_t size) {
    static const uint32_t base[] = {0x330, 0x4b0};
    uint8_t cab[0x4d0];
    if (size < sizeof cab || !nx_leer_seguro(addr, cab, sizeof cab) || memcmp(cab, "NRR0", 4)) return -1;
    for (unsigned v = 0; v < sizeof base / sizeof base[0]; v++) {
        uint64_t programa;
        uint32_t tam, off, cuantos;
        memcpy(&programa, cab + base[v], 8);
        memcpy(&tam, cab + base[v] + 8, 4);
        memcpy(&off, cab + base[v] + 0x10, 4);
        memcpy(&cuantos, cab + base[v] + 0x14, 4);
        if (tam != size || off < base[v] + 0x18 || (uint64_t)off + 32ull * cuantos > size) continue;
        int nuevos = 0;
        for (uint32_t k = 0; k < cuantos && nrr.n < RO_MAX_HASHES; k++)
            if (nx_leer_seguro(addr + off + 32ull * k, nrr.hash[nrr.n], 32)) nrr.n++, nuevos++;
        nrr.programa = programa;
        fprintf(stderr, "[ro] NRR registrado en 0x%llx (0x%llx bytes, formato %s): programa %016llx, %u hash%s de NRO\n",
                (unsigned long long)addr, (unsigned long long)size, v ? "Switch 2" : "Switch 1",
                (unsigned long long)programa, cuantos, cuantos == 1 ? "" : "es");
        return nuevos;
    }
    fprintf(stderr, "[ro] NRR en 0x%llx (0x%llx bytes) con una cabecera que no reconozco\n", (unsigned long long)addr,
            (unsigned long long)size);
    return -1;
}

// Identifica el NRO que el juego pide mapear y, con NX_VOLCAR_NRO=<carpeta>, guarda una copia <sha256>.nro (solo
// lectura de la memoria del invitado; nunca escribe en ella). Devuelve 1 si figura en un NRR registrado, 0 si no y
// -1 si no se pudo leer o no es un NRO.
int nx_ro_describir_nro(uint64_t nro, uint64_t tam, uint64_t bss, uint64_t bss_tam) {
    fprintf(stderr, "[ro] MapManualLoadModuleMemory: nro=0x%llx tam=0x%llx bss=0x%llx tam_bss=0x%llx\n",
            (unsigned long long)nro, (unsigned long long)tam, (unsigned long long)bss, (unsigned long long)bss_tam);
    uint8_t cab[0x80];
    if (tam < sizeof cab || !nx_leer_seguro(nro, cab, sizeof cab) || memcmp(cab + 0x10, "NRO0", 4)) {
        fprintf(stderr, "[ro]   la dirección no tiene un NRO legible\n");
        return -1;
    }
    uint32_t c[8], bss_nro;
    memcpy(c, cab + 0x18, 4);       // tamaño total
    memcpy(c + 1, cab + 0x20, 24);  // text, ro y data: {desplazamiento, tamaño}
    memcpy(&bss_nro, cab + 0x38, 4);
    char id[65];
    hex(cab + 0x40, 32, id);
    fprintf(stderr, "[ro]   NRO0: tamaño 0x%x, text 0x%x+0x%x, ro 0x%x+0x%x, data 0x%x+0x%x, bss 0x%x, build id %s\n",
            c[0], c[1], c[2], c[3], c[4], c[5], c[6], bss_nro, id);
    if (c[0] != tam) fprintf(stderr, "[ro]   el tamaño de la cabecera no coincide con el pedido\n");

    const char *carpeta = getenv("NX_VOLCAR_NRO");
    char ruta[4096] = "";
    FILE *volcado = NULL;
    if (carpeta && *carpeta) {
        mkdir(carpeta, 0755); // la carpeta del juego/nro que pone NX2Linux puede no existir todavía
        snprintf(ruta, sizeof ruta, "%s/nro-%llx.tmp", carpeta, (unsigned long long)nro);
        volcado = fopen(ruta, "wbx");
        if (!volcado) fprintf(stderr, "[ro]   no se pudo crear %s\n", ruta);
    }
    Sha256 s;
    sha256_inicio(&s);
    static uint8_t trozo[1 << 16];
    for (uint64_t hecho = 0; hecho < tam;) {
        size_t k = tam - hecho < sizeof trozo ? (size_t)(tam - hecho) : sizeof trozo;
        if (!nx_leer_seguro(nro + hecho, trozo, k)) {
            fprintf(stderr, "[ro]   no se pudo leer el NRO en +0x%llx\n", (unsigned long long)hecho);
            if (volcado) fclose(volcado), remove(ruta);
            return -1;
        }
        sha256_datos(&s, trozo, k);
        if (volcado && fwrite(trozo, 1, k, volcado) != k) fclose(volcado), volcado = NULL, remove(ruta);
        hecho += k;
    }
    uint8_t h[32];
    sha256_fin(&s, h);
    char hh[65];
    hex(h, 32, hh);
    int esta = 0;
    for (int k = 0; k < nrr.n && !esta; k++) esta = !memcmp(nrr.hash[k], h, 32);
    fprintf(stderr, "[ro]   sha256 %s: %s\n", hh,
            esta ? "figura en un NRR registrado" : nrr.n ? "NO figura en los NRR registrados" : "no hay NRR registrado");
    if (volcado) {
        char final[4096];
        snprintf(final, sizeof final, "%s/%s.nro", carpeta, hh);
        if (fclose(volcado) == 0 && rename(ruta, final) == 0)
            fprintf(stderr, "[ro]   copia en %s\n", final);
        else
            remove(ruta);
    }
    fprintf(stderr, "[ro]   este código no está traducido: hay que recompilar con él (nxrecomp.py --modulos-ro, la "
                    "carpeta de NX_VOLCAR_NRO); hasta entonces no se mapea\n");
    return esta;
}

static const NxRoModulo *traducidos;
static int ntraducidos;
static uint8_t cargados[64];

void nx_ro_modulos_traducidos(const NxRoModulo *m, int n) {
    traducidos = m, ntraducidos = n < (int)sizeof cargados ? n : (int)sizeof cargados;
    memset(cargados, 0, sizeof cargados);
}

// SHA-256 de tam bytes del invitado; 0 si alguna página no se pudo leer.
static int sha256_invitado(uint64_t a, uint64_t tam, uint8_t out[32]) {
    static uint8_t trozo[1 << 16];
    Sha256 s;
    sha256_inicio(&s);
    for (uint64_t hecho = 0; hecho < tam;) {
        size_t k = tam - hecho < sizeof trozo ? (size_t)(tam - hecho) : sizeof trozo;
        if (!nx_leer_seguro(a + hecho, trozo, k)) return 0;
        sha256_datos(&s, trozo, k);
        hecho += k;
    }
    sha256_fin(&s, out);
    return 1;
}

// MapManualLoadModuleMemory de un NRO traducido de antemano: lo que hace ro en Horizon (los bytes del NRO pasan a
// ser el módulo, con el bss detrás) en la dirección fija para la que se tradujo su código. El SDK reubica, enlaza y
// llama su init después, sobre esa copia. -1 si el NRO no es ninguno de los traducidos.
static int mapear_traducido(NxRequest *r, uint64_t nro, uint64_t tam, uint64_t bss_tam, uint32_t *res) {
    if (!ntraducidos) return -1;
    uint8_t h[32];
    if (!sha256_invitado(nro, tam, h)) return -1;
    for (int k = 0; k < ntraducidos; k++) {
        const NxRoModulo *m = &traducidos[k];
        if (memcmp(m->sha256, h, 32)) continue;
        if (tam != m->tam || bss_tam < m->bss) {
            fprintf(stderr, "[ro] MapManualLoadModuleMemory: %s con tamaños distintos de los traducidos (0x%llx/0x%llx)\n",
                    m->nombre, (unsigned long long)tam, (unsigned long long)bss_tam);
            *res = NX_R_UNKNOWN_CMD;
            return 1;
        }
        if (cargados[k]) {
            // Horizon lo mapearía otra vez en otra dirección; el código traducido solo existe en la suya.
            fprintf(stderr, "[ro] MapManualLoadModuleMemory: %s ya está cargado en 0x%llx; una segunda copia no "
                            "tiene código traducido\n", m->nombre, (unsigned long long)m->base);
            *res = NX_R_UNKNOWN_CMD;
            return 1;
        }
        if (!nx_leer_seguro(nro, (void *)(uintptr_t)m->base, tam)) {
            fprintf(stderr, "[ro] MapManualLoadModuleMemory: no se pudo copiar %s\n", m->nombre);
            *res = NX_R_UNKNOWN_CMD;
            return 1;
        }
        memset((void *)(uintptr_t)(m->base + tam), 0, m->bss);
        cargados[k] = 1;
        r->out_size = (r->out_size + 7) & ~7u;
        memcpy(r->out + r->out_size, &m->base, 8);
        r->out_size += 8;
        fprintf(stderr, "[ro] MapManualLoadModuleMemory: %s (0x%llx bytes, bss 0x%llx) en 0x%llx, código traducido\n",
                m->nombre, (unsigned long long)tam, (unsigned long long)m->bss, (unsigned long long)m->base);
        *res = 0;
        return 1;
    }
    return -1;
}

// Lo demás de ldr:ro solo registra o quita metadatos: los módulos del ExeFS ya están enlazados de forma estática.
// MapManualLoadModuleMemory no se finge: solo entrega un NRO cuyo código se tradujo para esa dirección.
uint32_t nx_ldr_ro(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: {
        uint32_t res;
        if (mapear_traducido(r, in64(r, 8), in64(r, 0x10), in64(r, 0x20), &res) > 0) return res;
        nx_ro_describir_nro(in64(r, 8), in64(r, 0x10), in64(r, 0x18), in64(r, 0x20));
        return NX_R_UNKNOWN_CMD;
    }
    case 1: // UnmapManualLoadModuleMemory(pid, dirección): el módulo traducido queda libre para otra carga.
        for (int k = 0; k < ntraducidos; k++)
            if (traducidos[k].base == in64(r, 8)) cargados[k] = 0;
        return 0;
    case 2: // RegisterModuleInfo
        if (r->in_size >= 0x18) nx_ro_registrar_nrr(in64(r, 8), in64(r, 0x10));
        return 0;
    case 3:  // UnregisterModuleInfo.
    case 4:  // RegisterProcessHandle.
    case 10: // RegisterModuleInfoWithUserProcessHandle.
        return 0;
    default:
        return NX_R_UNKNOWN_CMD;
    }
}
