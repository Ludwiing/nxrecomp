// Los servicios que un juego abre al arrancar y que, sin red ni cuenta de Nintendo, solo tienen que
// responder algo razonable: "sin conexión", "no hay", "aceptado". La mayoría va por una tabla: cada
// servicio dice qué comandos devuelven un objeto (otro servicio de la tabla) o un evento, y los demás
// responden bien con ceros (se avisa una vez por comando, para saber qué usa cada juego). Aparte, los
// que necesitan algo de verdad: fuentes compartidas (pl:u), números aleatorios (csrng), DLC (aoc:u),
// la red (nifm) y los sockets (bsd).
//   NX_FUENTE=archivo.ttf   la fuente que reemplaza a las del sistema (por defecto una del anfitrión)
//   NX_AOC=carpeta          los DLC: <índice>.img (RomFS), los pone NX2Linux
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

#include "nxkern.h"
#include "nx_pl.h"
#include "nxsvc.h"

#define OK 0u

// ---------- la tabla ----------
enum { OBJETO = 1, EVENTO, ERROR, MEMORIA };
typedef struct {
    uint32_t cmd;
    uint8_t tipo;
    const char *objeto; // OBJETO: nombre en la tabla; ERROR: no se usa
    uint32_t valor;     // ERROR: el resultado; MEMORIA: tamaño
} StubCmd;
typedef struct {
    const char *nombre;
    StubCmd cmds[12];
} StubDef;

#define R_SIN_RED 0x1686Eu // nifm 180: sin conexión a internet
#define R_COMANDO_DESCONOCIDO ((221u << 9) | 10u) // sf::cmif::ResultUnknownCommandId = 0x1ba0a

static const StubDef stubs[] = {
    // red, amigos, informes, control parental
    {"friends:u", {{0, OBJETO, "IFriendService"}, {1, OBJETO, "INotificationService"}, {2, OBJETO, "IDaemonSuspendSessionService"}}},
    {"friends:v", {{0, OBJETO, "IFriendService"}, {1, OBJETO, "INotificationService"}}},
    // Switch 2: el mismo servicio con otro nombre (nn::friends::detail::ipc crea las mismas tres interfaces).
    {"friend:u", {{0, OBJETO, "IFriendService"}, {1, OBJETO, "INotificationService"}, {2, OBJETO, "IDaemonSuspendSessionService"}}},
    {"IFriendService", {{10100, ERROR, 0, R_SIN_RED}, {10101, ERROR, 0, R_SIN_RED}}},
    {"INotificationService", {{0, EVENTO}}},
    {"IDaemonSuspendSessionService", {{0}}},
    {"prepo:u", {{0}}}, {"prepo:a", {{0}}}, {"prepo:a2", {{0}}}, {"prepo:s", {{0}}},
    {"pctl", {{0, OBJETO, "IParentalControlService"}, {1, OBJETO, "IParentalControlService"}}},
    {"pctl:a", {{0, OBJETO, "IParentalControlService"}, {1, OBJETO, "IParentalControlService"}}},
    {"pctl:s", {{0, OBJETO, "IParentalControlService"}, {1, OBJETO, "IParentalControlService"}}},
    {"pctl:r", {{0, OBJETO, "IParentalControlService"}, {1, OBJETO, "IParentalControlService"}}},
    {"IParentalControlService", {{1031, 0}, {1451, EVENTO}, {1457, EVENTO}, {1473, EVENTO}}},
    {"olsc:u", {{0}}},
    {"npns:u", {{0}}},
    {"ssl", {{0, OBJETO, "ISslContext"}}},
    {"ISslContext", {{0}}},
    // contenido descargado por el sistema (bcat): no hay nada
    {"bcat:u", {{0, OBJETO, "IBcatService"}, {1, OBJETO, "IDeliveryCacheStorageService"}, {2, OBJETO, "IDeliveryCacheStorageService"}}},
    {"bcat:a", {{0, OBJETO, "IBcatService"}, {1, OBJETO, "IDeliveryCacheStorageService"}, {2, OBJETO, "IDeliveryCacheStorageService"}}},
    {"bcat:m", {{0, OBJETO, "IBcatService"}, {1, OBJETO, "IDeliveryCacheStorageService"}, {2, OBJETO, "IDeliveryCacheStorageService"}}},
    {"bcat:s", {{0, OBJETO, "IBcatService"}, {1, OBJETO, "IDeliveryCacheStorageService"}, {2, OBJETO, "IDeliveryCacheStorageService"}}},
    {"IBcatService", {{10100, OBJETO, "IDeliveryCacheProgressService"}, {10101, OBJETO, "IDeliveryCacheProgressService"}}},
    {"IDeliveryCacheProgressService", {{0, EVENTO}}},
    {"IDeliveryCacheStorageService", {{0, OBJETO, "IDeliveryCacheFileService"}, {1, OBJETO, "IDeliveryCacheDirectoryService"}}},
    {"IDeliveryCacheFileService", {{0, ERROR, 0, 0x1407Au}}},      // bcat: no existe
    {"IDeliveryCacheDirectoryService", {{0, ERROR, 0, 0x1407Au}}},
    // amiibo, cámara IR, capturas, juego local, Mii, filtro de palabras, registro, multimedia
    {"nfp:user", {{0, OBJETO, "INfpUser"}}},
    {"nfc:user", {{0, OBJETO, "INfpUser"}}},
    {"INfpUser", {{17, EVENTO}, {18, EVENTO}, {23, EVENTO}}},
    {"irs", {{303, MEMORIA, 0, 0x8000}, {311, 0}}},
    {"caps:u", {{0}}}, {"caps:su", {{0}}}, {"caps:c", {{0}}}, {"caps:a", {{0}}},
    {"ldn:u", {{0, OBJETO, "IUserLocalCommunicationService"}}},
    {"IUserLocalCommunicationService", {{100, EVENTO}, {400, ERROR, 0, 0xCBu}}}, // ldn: deshabilitado
    {"mii:e", {{0, OBJETO, "IDatabaseService"}}},
    {"mii:u", {{0, OBJETO, "IDatabaseService"}}},
    {"IDatabaseService", {{0}}},
    {"lm", {{0, OBJETO, "ILogger"}}},
    {"ILogger", {{0}}},
    {"mm:u", {{0}}},
    {"audin:u", {{0}}},
    {"spl:", {{0}}},
    {"notif:a", {{0}}},
    {"ectx:aw", {{0}}},
    {"pdm:ntfy", {{0}}},
    {"sfdnsres", {{0}}},
    {"nifm:a", {{4, OBJETO, "IGeneralService"}, {5, OBJETO, "IGeneralService"}}},
    {"nifm:s", {{4, OBJETO, "IGeneralService"}, {5, OBJETO, "IGeneralService"}}},
    // Switch 2: GameChat (nn::chat::Initialize abre chat:u al arrancar Nintendo Switch Sports Resort y aborta si
    // sm no lo encuentra). nn::chat::detail::CreateUserServiceByHipc pide al comando 0 la interfaz con la que
    // trabaja (sin ella el proxy queda con vtable nula). Sin red ni chat: sus comandos responden bien con ceros.
    {"chat:u", {{0, OBJETO, "IChatUserService"}}},
    {"IChatUserService", {{0}}},
    {"nifm:u", {{4, OBJETO, "IGeneralService"}, {5, OBJETO, "IGeneralService"}}},
    {"IGeneralService", {{4, OBJETO, "IRequest"}, {12, ERROR, 0, R_SIN_RED}, {18, ERROR, 0, 0x2586Eu}}},
    {"IRequest", {{0}}},
    {"aoc:u", {{0}}},
    // Nombrados por el SDK de juegos grandes (Tears of the Kingdom): los que una aplicación casi nunca usa.
    {"acc:aa", {{0}}}, {"auddev", {{0}}}, {"audrec:u", {{0}}}, {"btm:u", {{0}}}, {"hid:dbg", {{0}}},
    {"hid:sys", {{0}}}, {"hidbus", {{0}}}, {"lbl", {{0}}}, {"nim:eca", {{0}}}, {"nsd:u", {{0}}},
    {"IPurchaseEventManager", {{0, EVENTO}, {1, ERROR, 0, 0x6A9Cu}}},
};

static const StubDef *buscar(const char *nombre) {
    for (size_t i = 0; i < sizeof stubs / sizeof *stubs; i++)
        if (!strcmp(stubs[i].nombre, nombre)) return &stubs[i];
    return NULL;
}

static uint32_t stub(ServiceObject *self, NxRequest *r);

// Respuestas que no son solo ceros.
static uint32_t especial(const StubDef *d, NxRequest *r, int *hecho);

static uint32_t stub(ServiceObject *self, NxRequest *r) {
    const StubDef *d = self->state;
    int hecho = 0;
    uint32_t rc = especial(d, r, &hecho);
    if (hecho) return rc;
    for (int i = 0; i < 12; i++) {
        const StubCmd *c = &d->cmds[i];
        if (!c->tipo || c->cmd != r->cmd) continue;
        switch (c->tipo) {
        case OBJETO: nx_out_object(r, nx_object(c->objeto, stub, (void *)buscar(c->objeto))); return OK;
        case EVENTO: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
        case ERROR: return c->valor;
        case MEMORIA: { KShmem *s; nx_out_copy_handle(r, nx_shmem_new(c->valor, &s)); return OK; }
        }
    }
    // NX_SERVICIOS_ESTRICTOS=1 (diagnóstico): un comando que la tabla no conoce responde lo que responde Horizon,
    // sf::cmif::ResultUnknownCommandId (módulo 10, 221: Atmosphère sf_results.hpp / sf_cmif_service_dispatch.cpp),
    // en vez de éxito con ceros. Sirve para ver si un juego dependía de esa respuesta genérica.
    static int estricto = -1;
    if (estricto < 0) estricto = getenv("NX_SERVICIOS_ESTRICTOS") != NULL;
    if (estricto) {
        fprintf(stderr, "[stub] %s %u: comando desconocido, 0x%x (UnknownCommandId) por NX_SERVICIOS_ESTRICTOS\n",
                d->nombre, r->cmd, R_COMANDO_DESCONOCIDO);
        return R_COMANDO_DESCONOCIDO;
    }
    // Cualquier otro: bien, con ceros. Se avisa una vez por servicio y comando.
    static struct { const StubDef *d; uint32_t cmd; } vistos[512];
    static int nvistos;
    int visto = 0;
    for (int i = 0; i < nvistos; i++) visto |= vistos[i].d == d && vistos[i].cmd == r->cmd;
    if (!visto && nvistos < 512) {
        vistos[nvistos].d = d, vistos[nvistos++].cmd = r->cmd;
        fprintf(stderr, "[stub] %s %u\n", d->nombre, r->cmd);
    }
    static const uint8_t ceros[0x20];
    nx_out_bytes(r, ceros, sizeof ceros);
    return OK;
}

// ---------- fuentes compartidas (pl:u) ----------
// El sistema tiene 6 fuentes (estándar, chino simplificado y extendido, tradicional, coreano y los
// íconos de Nintendo) en memoria compartida, cada una con 8 bytes de cabecera: el magic de BFTTF y el
// tamaño cifrado. No son nuestras: se pone una fuente del anfitrión en los 6 lugares.
static KShmem *fuentes;
static uint32_t fuentes_h, fuente_off[6], fuente_tam[6];

static void cargar_fuentes(void) {
    if (fuentes) return;
    fuentes_h = nx_shmem_new(0x1100000, &fuentes);
    const char *rutas[] = {getenv("NX_FUENTE"), "/usr/share/fonts/noto/NotoSans-Regular.ttf",
                           "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf"};
    FILE *f = NULL;
    for (size_t i = 0; i < sizeof rutas / sizeof *rutas && !f; i++)
        if (rutas[i] && *rutas[i]) f = fopen(rutas[i], "rb");
    uint8_t *datos = NULL;
    long n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        rewind(f);
        datos = malloc((size_t)n);
        if (fread(datos, 1, (size_t)n, f) != (size_t)n) n = 0;
        fclose(f);
    }
    if (!n || (uint64_t)(n + 8) * 6 > fuentes->size) {
        fprintf(stderr, "[nxrt] pl:u: no hay una fuente TTF del anfitrión para las fuentes del sistema\n");
        free(datos);
        return;
    }
    uint32_t off = 0;
    for (int i = 0; i < 6; i++) {
        uint32_t magic = 0x18029A7Fu, tam = __builtin_bswap32((uint32_t)n ^ 0x49621806u);
        memcpy(fuentes->host + off, &magic, 4);
        memcpy(fuentes->host + off + 4, &tam, 4);
        memcpy(fuentes->host + off + 8, datos, (size_t)n);
        fuente_off[i] = off + 8, fuente_tam[i] = (uint32_t)n;
        off = (off + 8 + (uint32_t)n + 0xFFF) & ~0xFFFu;
    }
    free(datos);
}

static uint32_t pl(ServiceObject *self, NxRequest *r) {
    (void)self;
    cargar_fuentes();
    uint32_t tipo = r->in_size >= 4 ? nx_in32(r, 0) % 6 : 0;
    switch (r->cmd) {
    case 0: return OK;                              // RequestLoad
    case 1: nx_out32(r, 1); return OK;              // GetLoadState: cargada
    case 2: nx_out32(r, fuente_tam[tipo]); return OK; // GetSize
    case 3: nx_out32(r, fuente_off[tipo]); return OK; // GetSharedMemoryAddressOffset
    case 4: nx_out_copy_handle(r, fuentes_h); return OK; // GetSharedMemoryNativeHandle
    case 5: { // GetSharedFontInOrderOfPriority(idioma) → cargadas, cuántas; tipos, offsets y tamaños
        uint32_t *v[3];
        for (int k = 0; k < 3; k++) {
            NxBuffer *b = nx_out_buffer(r, k);
            v[k] = b && b->size >= 24 ? (uint32_t *)(uintptr_t)b->addr : NULL;
        }
        for (uint32_t i = 0; i < 6; i++) {
            if (v[0]) v[0][i] = i;
            if (v[1]) v[1][i] = fuente_off[i];
            if (v[2]) v[2][i] = fuente_tam[i];
        }
        nx_out32(r, 1);
        nx_out32(r, 6);
        return OK;
    }
    // Switch 2 (nx_pl.h): la memoria compartida de las fuentes y cada fuente integrada.
    case 2000:
        if (!fuentes) return NX_R_UNKNOWN_CMD;
        nx_out32(r, (uint32_t)fuentes->size);
        nx_out_copy_handle(r, fuentes_h);
        return OK;
    case 2010: {
        uint8_t res[16];
        int cargada = fuentes && nx_pl_fuente_integrada(fuente_off, fuente_tam, fuentes->host, fuentes->size, r->in,
                                                        r->in_size, res);
        if (!cargada)
            fprintf(stderr, "[nxrt] pl:u: fuente integrada %u no disponible (se responde «no cargada»)\n",
                    r->in_size ? r->in[0] : 0);
        if (getenv("NX_TRAZA_PL")) {
            fprintf(stderr, "[pl] 2010 entrada");
            for (uint32_t i = 0; i < r->in_size && i < 16; i++) fprintf(stderr, " %02x", r->in[i]);
            fprintf(stderr, " → salida");
            for (int i = 0; i < 16; i++) fprintf(stderr, " %02x", res[i]);
            fprintf(stderr, "\n");
        }
        nx_out_bytes(r, res, sizeof res);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- csrng: números aleatorios (std::random_device los pide en la primera línea de nnMain) ----------
static uint32_t csrng(ServiceObject *self, NxRequest *r) {
    (void)self;
    if (r->cmd != 0) return NX_R_UNKNOWN_CMD;
    NxBuffer *b = nx_out_buffer(r, 0);
    if (b) {
        uint8_t *p = (uint8_t *)(uintptr_t)b->addr;
        nx_vigia_tocar(b->addr, b->size);
        for (uint64_t k = 0; k < b->size;) {
            ssize_t n = getrandom(p + k, b->size - k, 0);
            if (n <= 0) break;
            k += (uint64_t)n;
        }
    }
    return OK;
}

// ---------- bsd: sockets, sin red ----------
// Cada llamada falla con ENETDOWN, salvo las que esperan: Poll y Select duermen su tiempo (Dark Souls
// giraba un núcleo al 100 % si volvían al momento).
static uint32_t bsd(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out32(r, 0); nx_out32(r, 0); return OK; // RegisterClient
    case 1: return OK;                                   // StartMonitoring
    case 5: case 6: { // Select(n, ..., timeout), Poll(nfds, timeout ms)
        int64_t ms = r->cmd == 6 ? (int32_t)nx_in32(r, 4) : 100;
        if (ms < 0 || ms > 1000) ms = 1000;
        struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
        nanosleep(&ts, NULL);
        nx_out32(r, 0);
        nx_out32(r, 0);
        return OK;
    }
    case 26: nx_out32(r, 0); nx_out32(r, 0); return OK; // Close
    }
    nx_out32(r, (uint32_t)-1);
    nx_out32(r, 50); // ENETDOWN (FreeBSD)
    return OK;
}

// ---------- ngc: filtro de palabras: nada es grosero ----------
static uint32_t ngc(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out32(r, 1); return OK; // GetContentVersion
    case 1: nx_out32(r, 0); return OK; // Check
    case 2: { // Mask: el texto vuelve igual
        NxBuffer *i = nx_in_buffer(r, 0), *o = nx_out_buffer(r, 0);
        if (i && o) memcpy((void *)(uintptr_t)o->addr, (void *)(uintptr_t)i->addr, i->size < o->size ? i->size : o->size);
        nx_out32(r, 0);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- aoc:u (DLC) ----------
static int dlc_indices(uint32_t *out, int max) {
    const char *dir = getenv("NX_AOC");
    DIR *d = dir ? opendir(dir) : NULL;
    int n = 0;
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        char *fin;
        unsigned long k = strtoul(e->d_name, &fin, 10);
        if (fin != e->d_name && !strcmp(fin, ".img")) out[n++] = (uint32_t)k;
    }
    closedir(d);
    for (int i = 1; i < n; i++) // en orden
        for (int j = i; j > 0 && out[j] < out[j - 1]; j--) {
            uint32_t t = out[j];
            out[j] = out[j - 1], out[j - 1] = t;
        }
    return n;
}

static int especial_aoc(NxRequest *r, uint32_t *rc) {
    uint32_t idx[256];
    int n = dlc_indices(idx, 256);
    const char *e = getenv("NX_TITLE_ID");
    uint64_t base = (e && *e ? strtoull(e, NULL, 16) : nx_program_id) + 0x1000;
    switch (r->cmd) {
    case 0: case 2: nx_out32(r, (uint32_t)n); *rc = OK; return 1; // Count*
    case 1: case 3: { // List*(offset, cuántos) → índices
        uint32_t off = nx_in32(r, 0), k = 0;
        NxBuffer *b = nx_out_buffer(r, 0);
        uint32_t *p = b ? (uint32_t *)(uintptr_t)b->addr : NULL;
        for (uint32_t i = off; i < (uint32_t)n && p && (k + 1) * 4 <= b->size; i++) p[k++] = idx[i];
        nx_out32(r, k);
        *rc = OK;
        return 1;
    }
    case 4: case 5: nx_out64(r, base); *rc = OK; return 1; // GetAddOnContentBaseId
    case 8: case 10: { KEvent *ev; nx_out_copy_handle(r, nx_event_new(&ev)); *rc = OK; return 1; }
    case 50: // CheckAddOnContentMountStatus; this runtime does not mount AOC images yet.
        nx_out32(r, 0); // false: the requested add-on content is not mounted
        *rc = OK;
        return 1;
    case 100: case 101:
        nx_out_object(r, nx_object("IPurchaseEventManager", stub, (void *)buscar("IPurchaseEventManager")));
        *rc = OK;
        return 1;
    }
    return 0;
}

/* nn::mii::CharInfo, the 0x58-byte result used by IDatabaseService.
 * Layout follows the public SwIPC interface definition. Field values model
 * the six built-in Miis described by nxnative's MIT-licensed HLE reference;
 * no game- or build-specific addresses/data are used. */
typedef struct __attribute__((packed)) {
    uint8_t create_id[16];
    uint16_t nickname[11];
    uint8_t font_region, favorite_color, gender, height, build, type, region_move;
    uint8_t faceline_type, faceline_color, faceline_wrinkle, faceline_make;
    uint8_t hair_type, hair_color, hair_flip;
    uint8_t eye_type, eye_color, eye_scale, eye_aspect, eye_rotate, eye_x, eye_y;
    uint8_t eyebrow_type, eyebrow_color, eyebrow_scale, eyebrow_aspect, eyebrow_rotate, eyebrow_x, eyebrow_y;
    uint8_t nose_type, nose_scale, nose_y;
    uint8_t mouth_type, mouth_color, mouth_scale, mouth_aspect, mouth_y;
    uint8_t beard_color, beard_type, mustache_type, mustache_scale, mustache_y;
    uint8_t glass_type, glass_color, glass_scale, glass_y;
    uint8_t mole_type, mole_scale, mole_x, mole_y, reserved;
} MiiCharInfo;
_Static_assert(sizeof(MiiCharInfo) == 0x58, "Mii CharInfo ABI size");

enum { MII_SOURCE_USER = 1, MII_SOURCE_DEFAULT = 2, MII_DEFAULT_COUNT = 6 };

static uint64_t mii_nonce(void) {
    static _Atomic uint64_t seq = 1;
    uint64_t value = 0;
    if (getrandom(&value, sizeof value, 0) == (ssize_t)sizeof value) return value;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec ^ atomic_fetch_add(&seq, 1);
}

static void mii_build(MiiCharInfo *m, unsigned gender, int index) {
    memset(m, 0, sizeof *m);
    uint64_t id0 = index >= 0 ? 0x4d49494e44454641ull + (uint64_t)index : mii_nonce();
    uint64_t id1 = index >= 0 ? 0x554c54444d484f53ull : mii_nonce();
    memcpy(m->create_id, &id0, sizeof id0);
    memcpy(m->create_id + sizeof id0, &id1, sizeof id1);
    m->create_id[6] = (uint8_t)((m->create_id[6] & 0x0f) | 0x40);
    m->create_id[8] = (uint8_t)((m->create_id[8] & 0x3f) | 0x80);
    if (index >= 0) m->favorite_color = (uint8_t)(index * 2);
    static const char name[] = "no name";
    for (size_t i = 0; i < sizeof name - 1; i++) m->nickname[i] = (uint16_t)name[i];
    m->gender = (uint8_t)(gender != 0);
    m->height = 64; m->build = 64;
    m->hair_type = 33; m->hair_color = 1;
    m->eye_type = 2; m->eye_color = 8; m->eye_scale = 4; m->eye_aspect = 3; m->eye_rotate = 4; m->eye_x = 2; m->eye_y = 12;
    m->eyebrow_type = 6; m->eyebrow_color = 1; m->eyebrow_scale = 4; m->eyebrow_aspect = 3; m->eyebrow_rotate = 6; m->eyebrow_x = 2; m->eyebrow_y = 10;
    m->nose_type = 1; m->nose_scale = 4; m->nose_y = 9;
    m->mouth_type = 23; m->mouth_color = 19; m->mouth_scale = 4; m->mouth_aspect = 3; m->mouth_y = 13;
    m->beard_color = 8; m->mustache_scale = 4; m->mustache_y = 10;
    m->glass_color = 8; m->glass_scale = 4; m->glass_y = 10;
    m->mole_scale = 4; m->mole_x = 2; m->mole_y = 20;
}

static uint32_t mii_service(NxRequest *r) {
    switch (r->cmd) {
    case 0: // IsUpdated(source): the host's database does not change.
    case 1: // IsFullDatabase(): it is empty.
        nx_out8(r, 0);
        return OK;
    case 2: { // GetCount(source)
        if (r->in_size < 4) return R_COMANDO_DESCONOCIDO;
        uint32_t source = nx_in32(r, 0);
        nx_out32(r, (source & MII_SOURCE_DEFAULT) ? MII_DEFAULT_COUNT : 0);
        return OK;
    }
    case 3: case 4: { // Get(source) / Get1(source), output buffer B
        if (r->in_size < 4) return R_COMANDO_DESCONOCIDO;
        uint32_t source = nx_in32(r, 0);
        NxBuffer *b = nx_out_buffer(r, 0);
        uint32_t stride = r->cmd == 3 ? sizeof(MiiCharInfo) + 4 : sizeof(MiiCharInfo);
        uint32_t count = (source & MII_SOURCE_DEFAULT) && b && b->addr ? (uint32_t)(b->size / stride) : 0;
        if (count > MII_DEFAULT_COUNT) count = MII_DEFAULT_COUNT;
        if (count) {
            nx_vigia_tocar(b->addr, (uint64_t)count * stride);
            uint8_t *dst = (uint8_t *)(uintptr_t)b->addr;
            for (uint32_t i = 0; i < count; i++) {
                MiiCharInfo m;
                mii_build(&m, i >= 3, (int)i);
                memcpy(dst + (size_t)i * stride, &m, sizeof m);
                if (r->cmd == 3) {
                    uint32_t origin = MII_SOURCE_DEFAULT;
                    memcpy(dst + (size_t)i * stride + sizeof m, &origin, sizeof origin);
                }
            }
        }
        nx_out32(r, count);
        return OK;
    }
    case 6: { // BuildRandom(age, gender, race) -> CharInfo
        if (r->in_size < 12) return R_COMANDO_DESCONOCIDO;
        uint32_t gender = nx_in32(r, 4);
        if (gender > 2) return 0x87Eu; // Mii InvalidArgument
        if (gender == 2) gender = (uint32_t)(mii_nonce() & 1);
        MiiCharInfo m;
        mii_build(&m, gender, -1);
        nx_out_bytes(r, &m, sizeof m);
        return OK;
    }
    case 7: { // BuildDefault(index) -> CharInfo
        if (r->in_size < 4) return R_COMANDO_DESCONOCIDO;
        uint32_t index = nx_in32(r, 0);
        MiiCharInfo m;
        mii_build(&m, index >= 3, index < MII_DEFAULT_COUNT ? (int)index : 0);
        nx_out_bytes(r, &m, sizeof m);
        return OK;
    }
    case 22: // SetInterfaceVersion
        return OK;
    default:
        return R_COMANDO_DESCONOCIDO;
    }
}

// ---------- nifm: IRequest pasa por "en espera" antes de decir que no hay red ----------
// (Pikmin 4 competía con su propio arranque si la respuesta llegaba en la primera consulta.)
static uint32_t especial(const StubDef *d, NxRequest *r, int *hecho) {
    uint32_t rc = OK;
    *hecho = 1;
    if (!strcmp(d->nombre, "aoc:u") && especial_aoc(r, &rc)) return rc;
    if (!strcmp(d->nombre, "IDatabaseService")) return mii_service(r);
    if (!strcmp(d->nombre, "IRequest")) {
        static int consultas;
        switch (r->cmd) {
        case 0: nx_out32(r, consultas++ ? 1 : 2); return OK; // GetRequestState: OnHold, después Free
        case 1: return consultas > 1 ? R_SIN_RED : OK;      // GetResult
        case 2: { // GetSystemEventReadableHandles: dos eventos, ya avisando
            KEvent *a, *b;
            uint32_t ha = nx_event_new(&a), hb = nx_event_new(&b);
            nx_event_signal(a);
            nx_out_copy_handle(r, ha);
            nx_out_copy_handle(r, hb);
            return OK;
        }
        }
    }
    if (!strcmp(d->nombre, "IGeneralService") && r->cmd == 21) { nx_out8(r, 0); return OK; } // IsAnyInternetRequestAccepted
    if (!strcmp(d->nombre, "IParentalControlService") && r->cmd == 1031) { nx_out8(r, 0); return OK; } // IsRestrictionEnabled
    // Initialize (4.0.0+): sin entradas ni salidas; éxito para una aplicación (Skyline, MPL-2.0, y Ryujinx, GPL-3.0:
    // solo el comportamiento). Explícito para no avisar como si fuera un hueco.
    if (!strcmp(d->nombre, "IParentalControlService") && r->cmd == 1) return OK;
    if (!strcmp(d->nombre, "fatal:u")) {
        fprintf(stderr, "[nxrt] el juego lanzó un error fatal: 0x%x (%04u-%04u)\n", nx_in32(r, 0), 2000 + (nx_in32(r, 0) & 0x1FF),
                nx_in32(r, 0) >> 9 & 0x1FFF);
        fflush(stdout);
        exit(1);
    }
    *hecho = 0;
    return OK;
}

static const StubDef fatal_def = {"fatal:u", {{0}}};

// Lo que sm: no encuentra en su tabla propia se busca aquí.
ServiceObject *nx_servicio_otro(const char *nombre) {
    if (!strcmp(nombre, "pl:u") || !strcmp(nombre, "pl:s")) return nx_object(nombre, pl, NULL);
    if (!strcmp(nombre, "ldr:ro")) return nx_object(nombre, nx_ldr_ro, NULL);
    if (!strcmp(nombre, "csrng")) return nx_object(nombre, csrng, NULL);
    if (!strcmp(nombre, "bsd:u") || !strcmp(nombre, "bsd:s") || !strcmp(nombre, "bsd:a")) return nx_object(nombre, bsd, NULL);
    if (!strcmp(nombre, "ngc:u") || !strcmp(nombre, "ngct:u")) return nx_object(nombre, ngc, NULL);
    if (!strcmp(nombre, "fatal:u")) return nx_object(nombre, stub, (void *)&fatal_def);
    const StubDef *d = buscar(nombre);
    return d ? nx_object(nombre, stub, (void *)d) : NULL;
}
