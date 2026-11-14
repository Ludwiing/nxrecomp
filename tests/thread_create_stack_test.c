// HLE de nn::os::CreateThread (Switch 2): el ThreadType debe quedar como lo deja el SDK para que
// nn::os::GetCurrentStackInfo (mismo código en Sports Resort, Mario Tennis Fever y Ocarina of Time) acepte el sp
// del hilo: pila en +0x50, tamaño en +0x58, fibra actual en +0x70 (0 = sin fibra, usa la pila del hilo).
// Sin esos campos, Nintendo Switch Sports Resort abortaba en GetCurrentStackInfo en su hilo 131.
#include "nxkern.h"

// Las comprobaciones de esta prueba son asserts: deben ejecutarse también si se compila con -DNDEBUG.
#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void nx_thread_create(Ctx *c);

// Lo que nx_svc enlaza del resto del runtime; esta prueba no llega a usarlo.
uint64_t nx_now_ns(void) { return 0; }
uint64_t nx_ticks(void) { return 0; }
uint32_t nx_send_sync_request(Ctx *c, uint32_t handle) { (void)c; (void)handle; abort(); }
uint32_t nx_connect_named_port(const char *name, uint64_t *handle) { (void)name; (void)handle; abort(); }
void nx_session_close(KSession *s) { (void)s; }
const char *nx_describir_dir(uint64_t a, char *buf, size_t n) { (void)a; if (n) buf[0] = 0; return buf; }
void nx_dispatch(Ctx *c, uint64_t addr) { (void)c; (void)addr; abort(); }
int nx_leer_seguro(uint64_t a, void *out, size_t n) { memcpy(out, (void *)(uintptr_t)a, n); return 1; }
void nx_pila_invitado(const Ctx *c) { (void)c; }
void nx_set_crash_ctx(Ctx *c) { (void)c; }
const int nx_main_priority = 44, nx_main_core = 0;
const uint64_t nx_main_addr = 0, nx_entry_addr = 0, nx_main_stack = 0, nx_system_resource = 0, nx_program_id = 0;

// La comprobación de nn::os::detail::DebugImplByHorizon::GetCurrentStackInfo, sin la rama de la pila alternativa.
static int sp_en_pila(const uint8_t *tt, uint64_t sp) {
    uint64_t fibra, base, tam;
    memcpy(&fibra, tt + 0x70, 8);
    const uint8_t *p = fibra ? (const uint8_t *)(uintptr_t)fibra + 0x40 : tt + 0x50;
    memcpy(&base, p, 8);
    memcpy(&tam, p + 8, 8);
    return sp >= base && sp < base + tam;
}

int main(void) {
    // El kernel guarda el TLS de cada hilo en una región fija; el runtime la reserva al arrancar (nx_map_fixed).
    // El mmap queda fuera de assert: tiene efecto lateral y sin él nx_thread_create escribe en memoria sin mapear.
    void *tls = mmap((void *)(uintptr_t)NX_TLS_BASE, NX_TLS_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (tls != (void *)(uintptr_t)NX_TLS_BASE) {
        int err = tls == MAP_FAILED ? errno : 0;
        fprintf(stderr, "FALLO: no se pudo reservar el TLS en 0x%llx: %s\n", (unsigned long long)NX_TLS_BASE,
                tls == MAP_FAILED ? strerror(err) : "el kernel devolvió otra dirección");
        if (tls != MAP_FAILED) munmap(tls, NX_TLS_SIZE);
        return 1;
    }
    static uint8_t pila[0x20000] __attribute__((aligned(16)));
    uint8_t *tt = malloc(0x200);
    memset(tt, 0xAB, 0x200);               // memoria del juego sin inicializar
    Ctx c = {0};
    c.x[0] = (uintptr_t)tt;
    c.x[1] = 0x8000000;                    // entrada (no se arranca el hilo)
    c.x[2] = 0x1234;
    c.x[3] = (uintptr_t)pila;
    c.x[4] = sizeof pila;
    c.x[5] = 44;                           // prioridad
    nx_thread_create(&c);
    assert(c.x[0] == 0);
    uint64_t v;
    memcpy(&v, tt + 0x50, 8); assert(v == (uintptr_t)pila);
    memcpy(&v, tt + 0x58, 8); assert(v == sizeof pila);
    memcpy(&v, tt + 0x70, 8); assert(v == 0);
    uint64_t tope = ((uintptr_t)pila + sizeof pila) & ~0xFull;
    assert(sp_en_pila(tt, tope - 0x1950));  // un sp dentro de la pila, como el del aborto (tope - 0x1950)
    assert(!sp_en_pila(tt, tope + 0x20));   // fuera de la pila sigue fallando
    puts("OK: CreateThread HLE deja pila, tamaño y fibra en el ThreadType; GetCurrentStackInfo acepta el sp");
    return 0;
}
