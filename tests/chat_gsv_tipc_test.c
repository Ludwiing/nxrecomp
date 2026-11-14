// GSV 1202 → sesión de chat (nn::chat::gsv), por el nxipc.c y el nxkern.c reales, con los bytes TIPC que lee el
// SDK de Nintendo Switch Sports Resort (sdk 0x123f7d50 y 0x12323cb0):
//   cmd 1  : tipo 0x11 sin datos → TLS+8 resultado, TLS+12 u32 id (el SDK exige id <= 0x3ff)
//   cmd 11 : tipo 0x1b, TLS+8 u32 id → TLS+8 descriptor (1 handle copiado), TLS+12 handle, TLS+16 resultado
// El SDK adjunta ese handle como SystemEvent y lo consulta con TryWait (WaitSynchronization, timeout 0). Antes la
// sesión respondía sin handle: TryWait esperaba sobre el handle 0 y el juego quedaba en un bucle de svc 0x18.
// No hay GameChat en el host: el evento existe y nunca se señala.
// También comprueba que NX_TRACE_WAITS no escribe una línea por cada espera repetida.
#include "../runtime/nxsvc_am.c"

#include <assert.h>
#include <sys/mman.h>

// Lo que nxsvc_am.c y nxkern.c enlazan del resto del runtime y esta prueba no usa.
uint64_t nx_now_ns(void) { return 0; }
uint64_t nx_ticks(void) { return 0; }
const char *nx_describir_dir(uint64_t a, char *buf, size_t n) { (void)a; if (n) buf[0] = 0; return buf; }
void nx_dispatch(Ctx *c, uint64_t addr) { (void)c; (void)addr; abort(); }
int nx_leer_seguro(uint64_t a, void *out, size_t n) { memcpy(out, (void *)(uintptr_t)a, n); return 1; }
void nx_pila_invitado(const Ctx *c) { (void)c; }
void nx_set_crash_ctx(Ctx *c) { (void)c; }
const int nx_main_priority = 44, nx_main_core = 0;
const uint64_t nx_main_addr = 0, nx_entry_addr = 0, nx_main_stack = 0, nx_system_resource = 0, nx_program_id = 0;
const NxModule nx_modules[1];
const int nx_nmodules = 0;
uint32_t nx2_vi2(ServiceObject *self, NxRequest *r) { (void)self; (void)r; abort(); }
uint32_t nx_nvdrv(ServiceObject *self, NxRequest *r) { (void)self; (void)r; abort(); }
uint32_t nx_syncpt_read(uint32_t id) { (void)id; abort(); }
void nx_map_fixed(uint64_t addr, uint64_t size, const char *what) { // el TLS de los hilos (nx_kernel_main_thread)
    (void)what;
    assert(mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                -1, 0) == (void *)(uintptr_t)addr);
}
ServiceObject *nx_object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    *o = (ServiceObject){name, fn, state};
    return o;
}

static ServiceObject *raiz;
ServiceObject *nx_service_sm(void) { return raiz; }

static uint32_t enviar(Ctx *c, uint32_t *tls, uint32_t sesion, uint32_t tipo, const uint32_t *datos, uint32_t n) {
    memset(tls, 0, 0x100);
    tls[0] = tipo;
    tls[1] = n;
    memcpy(tls + 2, datos, n * 4);
    return nx_send_sync_request(c, sesion);
}

// svcWaitSynchronization(handles, 1, timeout 0), como nn::os TryWait; devuelve el resultado (w0).
static uint32_t try_wait(uint32_t h) {
    uint32_t hs[1] = {h};
    Ctx w = {0};
    w.x[1] = (uintptr_t)hs; w.x[2] = 1; w.x[3] = 0;
    nx_svc(&w, 0x18);
    return (uint32_t)w.x[0];
}

static int contar_lineas(const char *ruta, const char *prefijo) {
    FILE *f = fopen(ruta, "r");
    char linea[512];
    int n = 0;
    while (f && fgets(linea, sizeof linea, f)) n += !strncmp(linea, prefijo, strlen(prefijo));
    if (f) fclose(f);
    return n;
}

int main(void) {
    nx_kernel_main_thread();

    // La sesión de chat sale de GSV 1202; se publica como el puerto "sm:" para enviarle TIPC por nxipc.c.
    NxRequest init = {.cmd = 1202};
    assert(nx2_gsv_session(NULL, &init) == 0 && init.nout_objects == 1);
    raiz = init.out_objects[0];
    assert(!strcmp(raiz->name, "nx2:chatgsv"));
    uint32_t tls[64];
    Ctx c = {.tpidrro = (uintptr_t)tls};
    uint64_t sesion;
    assert(nx_connect_named_port("sm:", &sesion) == 0);

    // cmd 1: sin handles; resultado y luego el id.
    assert(enviar(&c, tls, (uint32_t)sesion, 0x11, NULL, 0) == 0);
    assert(tls[1] == 2 && tls[2] == 0);
    uint32_t id = tls[3];
    assert(id >= 1 && id <= 0x3ff);
    assert(enviar(&c, tls, (uint32_t)sesion, 0x11, NULL, 0) == 0 && tls[3] == id); // estable

    // cmd 11(id): descriptor especial con un handle copiado, el handle y después el resultado.
    assert(enviar(&c, tls, (uint32_t)sesion, 0x1b, &id, 1) == 0);
    assert(tls[1] == ((1u << 31) | 1) && tls[2] == (1u << 1) && tls[4] == 0);
    uint32_t h1 = tls[3];
    assert(h1 && nx_handle_get(h1, KO_EVENT));
    assert(enviar(&c, tls, (uint32_t)sesion, 0x1b, &id, 1) == 0 && tls[4] == 0);
    uint32_t h2 = tls[3];
    assert(h2 && h2 != h1 && nx_handle_get(h2, KO_EVENT) == nx_handle_get(h1, KO_EVENT)); // mismo evento

    // Un id que la sesión no dio: error, sin handle.
    uint32_t otro = id + 1;
    assert(enviar(&c, tls, (uint32_t)sesion, 0x1b, &otro, 1) == 0);
    assert(!(tls[1] & (1u << 31)) && tls[2] == 0xE401u);

    // TryWait: timeout 0 sobre el evento sin señalar → TimedOut (0xEA01), no InvalidHandle (0xE401) como con 0.
    assert(try_wait(h1) == 0xEA01u);
    assert(try_wait(0) == 0xE401u);

    // Cerrar una copia no afecta a la otra ni al evento de la sesión.
    Ctx cierre = {0};
    cierre.x[0] = h1;
    nx_svc(&cierre, 0x16);
    assert((uint32_t)cierre.x[0] == 0 && !nx_handle_get(h1, KO_EVENT) && nx_handle_get(h2, KO_EVENT));
    assert(try_wait(h2) == 0xEA01u);
    cierre.x[0] = h2;
    nx_svc(&cierre, 0x16);
    assert((uint32_t)cierre.x[0] == 0);
    assert(enviar(&c, tls, (uint32_t)sesion, 0x1b, &id, 1) == 0 && tls[4] == 0 && nx_handle_get(tls[3], KO_EVENT));

    // NX_TRACE_WAITS: 100000 TryWait idénticos no escriben 100000 líneas; una espera distinta sale en el acto.
    char ruta[] = "/tmp/chat-gsv-traza-XXXXXX";
    int fd = mkstemp(ruta);
    assert(fd >= 0);
    fflush(stderr);
    int err = dup(2);
    dup2(fd, 2);
    setenv("NX_TRACE_WAITS", "1", 1);
    uint32_t h3 = tls[3];
    for (int k = 0; k < 100000; k++) assert(try_wait(h3) == 0xEA01u);
    assert(try_wait(0) == 0xE401u);
    unsetenv("NX_TRACE_WAITS");
    fflush(stderr);
    dup2(err, 2);
    close(err);
    close(fd);
    int lineas = contar_lineas(ruta, "[wait-sync]");
    FILE *f = fopen(ruta, "r");
    char texto[8192] = {0};
    size_t leidos = fread(texto, 1, sizeof texto - 1, f);
    fclose(f);
    unlink(ruta);
    assert(leidos > 0);
    assert(lineas >= 8 && lineas <= 40);
    assert(strstr(texto, "se repitió 100000 veces seguidas"));
    assert(strstr(texto, "handles= 0\n"));
    printf("chat GSV TIPC: id %u, evento por copia de handle, TryWait 0xEA01, cierre independiente; "
           "100001 esperas trazadas en %d líneas: PASS\n", id, lineas);
    return 0;
}
