// Regresión: Mario Tennis Fever (Switch 2) abortaba al elegir modalidad con 2202-0710 (0x58cca) en
// nn::hid::GetNpadJoyHoldType, llamado por nn::hid::ShowControllerSupport. El SDK de Switch 2 no pide el modo de
// sujeción por IPC: lo lee de la NpadCondition de la memoria compartida de hid (+0x3E200; u32 en +8, válido si el
// byte de +12 no es cero). Esta prueba recorre el camino del SDK sobre el HLE de hid: CreateAppletResource,
// GetSharedMemoryHandle, la consulta de la NpadCondition y SetNpadJoyHoldType/GetNpadJoyHoldType por IPC.
// Cada escenario corre en un proceso hijo: el estado de hid es global y NX_PLATAFORMA se lee al publicar.
// Sin CHECK(): una comprobación que falla escribe dónde y termina con código 1 (sin SIGABRT ni volcado de núcleo);
// los hijos además ponen RLIMIT_CORE=0 solo para sí mismos.
#include "../runtime/nxsvc_hid.c"
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            fprintf(stderr, "%s:%d: %s: falla `%s`\n", __FILE__, __LINE__, __func__, #cond); \
            fflush(stderr);                                                               \
            _exit(1);                                                                     \
        }                                                                                 \
    } while (0)

#define HID_RESULT_NPAD_JOY_HOLD_TYPE_NOT_SET 0x58ccau // 2202-0710, el que hacía abortar al juego
#define HID_RESULT_LAST_ACTIVE_NPAD_NOT_SET 0x5decau // 2202-0751
#define HID_NPAD_ID_PLAYER_1 0u // HidNpadIdType_No1 en libnx: jugador 1

static uint8_t *shmem_host;
static int input_attached;

ServiceObject *nx_object(const char *name, uint32_t (*handle)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = malloc(sizeof *o);
    *o = (ServiceObject){name, handle, state};
    return o;
}
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { r->out_copy[r->nout_copy++] = h; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { r->out_size = (r->out_size + 7) & ~7u; memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
uint64_t nx_in64(NxRequest *r, uint32_t off) {
    uint64_t v = 0;
    if (off + 8 <= r->in_size) memcpy(&v, r->in + off, 8);
    return v;
}
uint32_t nx_shmem_new(uint64_t size, KShmem **out) {
    static KShmem k;
    CHECK(size == HID_SHMEM_SIZE && !shmem_host);
    shmem_host = calloc(1, size); // el kernel entrega la memoria compartida en cero
    k = (KShmem){.fd = -1, .size = size, .host = shmem_host};
    *out = &k;
    return 0x1234;
}
void nx_input_attach(uint8_t *p) { CHECK(p == shmem_host); input_attached = 1; }
uint32_t nx_event_new(KEvent **out) { (void)out; CHECK(0); return 0; }
void nx_event_signal(KEvent *event) { (void)event; CHECK(0); }

// Lo que hace nn::hid::detail::GetNpadJoyHoldType del SDK de Switch 2 (0x93d0160 en Mario Tennis Fever).
static uint32_t sdk_get_npad_joy_hold_type(const uint8_t *hid, uint32_t *out) {
    const uint8_t *cond = hid + 0x3E200;
    if (__atomic_load_n(cond + 12, __ATOMIC_ACQUIRE) == 0) return HID_RESULT_NPAD_JOY_HOLD_TYPE_NOT_SET;
    *out = __atomic_load_n((const uint32_t *)(cond + 8), __ATOMIC_ACQUIRE);
    return 0;
}

// Lo que hace nn::hid::detail::GetLastActiveNpadController del SDK de Switch 2.
static uint32_t sdk_get_last_active_npad(const uint8_t *hid, uint32_t *out) {
    const uint8_t *cond = hid + 0x3E200;
    if (__atomic_load_n(cond + 4, __ATOMIC_ACQUIRE) == 0) return HID_RESULT_LAST_ACTIVE_NPAD_NOT_SET;
    *out = __atomic_load_n((const uint32_t *)cond, __ATOMIC_ACQUIRE);
    return 0;
}

// Ningún byte de la zona de la NpadCondition (0x3E200..0x3E210) escrito por el runtime.
static int npad_condition_zone_untouched(const uint8_t *hid) {
    for (int i = 0; i < 0x10; i++)
        if (hid[0x3E200 + i]) return 0;
    return 1;
}

static uint64_t ipc_get_hold_type(void) {
    uint64_t aruid = 0x52;
    NxRequest r = {.cmd = 121, .in = (uint8_t *)&aruid, .in_size = sizeof aruid};
    CHECK(nx_hid(NULL, &r) == 0 && r.out_size == 8);
    uint64_t v;
    memcpy(&v, r.out, sizeof v);
    return v;
}

static uint32_t ipc_set_hold_type(uint64_t type) {
    uint64_t in[2] = {0x52, type}; // aruid, modo
    NxRequest r = {.cmd = 120, .in = (uint8_t *)in, .in_size = sizeof in};
    uint32_t res = nx_hid(NULL, &r);
    CHECK(r.out_size == 0);
    return res;
}

// CreateAppletResource(aruid) y GetSharedMemoryHandle, como en nn::hid::InitializeNpad.
static void attach_shared_memory(void) {
    uint64_t aruid = 0x52;
    NxRequest r = {.cmd = 0, .in = (uint8_t *)&aruid, .in_size = sizeof aruid};
    CHECK(nx_hid(NULL, &r) == 0 && r.nout_objects == 1);
    ServiceObject *res = r.out_objects[0];
    CHECK(!strcmp(res->name, "IAppletResource"));
    r = (NxRequest){.cmd = 0};
    CHECK(res->handle(res, &r) == 0 && r.nout_copy == 1 && r.out_copy[0] == 0x1234);
    CHECK(shmem_host && input_attached);
    free(res);
}

static void switch2(void) {
    // Sin publicar la NpadCondition, la memoria en cero da el resultado del cierre: así fallaba antes.
    static uint8_t zero[HID_SHMEM_SIZE];
    uint32_t hold = 0xFFFFFFFF;
    CHECK(sdk_get_npad_joy_hold_type(zero, &hold) == HID_RESULT_NPAD_JOY_HOLD_TYPE_NOT_SET);
    uint32_t last_npad = 0xFFFFFFFF;
    CHECK(sdk_get_last_active_npad(zero, &last_npad) == HID_RESULT_LAST_ACTIVE_NPAD_NOT_SET);

    attach_shared_memory();
    // La consulta de ShowControllerSupport: el modo por defecto (vertical) ya es válido.
    hold = 0xFFFFFFFF;
    CHECK(sdk_get_npad_joy_hold_type(shmem_host, &hold) == 0 && hold == NPAD_JOY_HOLD_VERTICAL);
    CHECK(ipc_get_hold_type() == NPAD_JOY_HOLD_VERTICAL);
    // El runtime presenta un Npad No1 (jugador 1) desde el arranque, igual que nxinput.
    CHECK(sdk_get_last_active_npad(shmem_host, &last_npad) == 0 && last_npad == HID_NPAD_ID_PLAYER_1);

    // SetNpadJoyHoldType(horizontal): el comando 121 y la NpadCondition cambian juntos.
    CHECK(ipc_set_hold_type(NPAD_JOY_HOLD_HORIZONTAL) == 0);
    CHECK(sdk_get_npad_joy_hold_type(shmem_host, &hold) == 0 && hold == NPAD_JOY_HOLD_HORIZONTAL);
    CHECK(ipc_get_hold_type() == NPAD_JOY_HOLD_HORIZONTAL);

    // Un modo fuera de 0/1 se rechaza con resultado explícito y no cambia ni el estado ni la memoria compartida.
    uint8_t before[0x10];
    memcpy(before, shmem_host + 0x3E200, sizeof before);
    CHECK(set_npad_joy_hold_type(7) == 0);
    CHECK(set_npad_joy_hold_type(~0ull) == 0);
    CHECK(!memcmp(before, shmem_host + 0x3E200, sizeof before));
    CHECK(ipc_get_hold_type() == NPAD_JOY_HOLD_HORIZONTAL);

    // De vuelta a vertical.
    CHECK(ipc_set_hold_type(NPAD_JOY_HOLD_VERTICAL) == 0);
    CHECK(sdk_get_npad_joy_hold_type(shmem_host, &hold) == 0 && hold == NPAD_JOY_HOLD_VERTICAL);
    CHECK(sdk_get_last_active_npad(shmem_host, &last_npad) == 0 && last_npad == HID_NPAD_ID_PLAYER_1);
}

static void switch2_set_before_attach(void) {
    // Un modo pedido antes de mapear la memoria compartida se publica al mapearla.
    CHECK(ipc_set_hold_type(NPAD_JOY_HOLD_HORIZONTAL) == 0);
    attach_shared_memory();
    uint32_t hold = 0xFFFFFFFF;
    CHECK(sdk_get_npad_joy_hold_type(shmem_host, &hold) == 0 && hold == NPAD_JOY_HOLD_HORIZONTAL);
}

static void switch1(void) {
    // Switch 1: el modo va por IPC (120/121) y la zona 0x3E200 de la memoria compartida no se toca.
    attach_shared_memory();
    CHECK(npad_condition_zone_untouched(shmem_host));
    CHECK(ipc_get_hold_type() == NPAD_JOY_HOLD_VERTICAL);
    CHECK(ipc_set_hold_type(NPAD_JOY_HOLD_HORIZONTAL) == 0);
    CHECK(ipc_get_hold_type() == NPAD_JOY_HOLD_HORIZONTAL);
    CHECK(npad_condition_zone_untouched(shmem_host));
}

static void invalid_mode(void) {
    // Comando 120 con modo fuera de 0/1: el resultado de Horizon no está medido; el runtime termina con exit(1)
    // (sin señal ni volcado) en vez de devolver un resultado inventado. Se redirige stderr para no ensuciar la salida.
    attach_shared_memory();
    CHECK(freopen("/dev/null", "w", stderr));
    ipc_set_hold_type(7);
    _exit(99); // no debe llegar aquí
}

static void run(const char *name, const char *plataforma, void (*fn)(void), int want_status) {
    fflush(stdout);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        struct rlimit no_core = {0, 0}; // solo este hijo: un fallo inesperado no deja volcado
        setrlimit(RLIMIT_CORE, &no_core);
        if (plataforma) setenv("NX_PLATAFORMA", plataforma, 1);
        else unsetenv("NX_PLATAFORMA");
        fn();
        _exit(0);
    }
    int st;
    CHECK(waitpid(pid, &st, 0) == pid);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != want_status) {
        if (WIFSIGNALED(st)) fprintf(stderr, "%s: terminó por la señal %d (se esperaba salida %d)\n", name, WTERMSIG(st), want_status);
        else fprintf(stderr, "%s: salida %d (se esperaba %d)\n", name, WEXITSTATUS(st), want_status);
        exit(1);
    }
}

int main(void) {
    run("switch2", "switch2", switch2, 0);
    run("switch2_set_before_attach", "switch2", switch2_set_before_attach, 0);
    run("switch1 sin NX_PLATAFORMA", NULL, switch1, 0);
    run("switch1 NX_PLATAFORMA=switch1", "switch1", switch1, 0);
    run("modo inválido", "switch2", invalid_mode, 1);
    puts("HID NpadCondition: Switch 2 publica modo y Npad No1 en +0x3E200, Switch 1 no toca la zona, "
         "SetNpadJoyHoldType 0/1 por IPC y rutas no implementadas comprobadas: PASS");
}
