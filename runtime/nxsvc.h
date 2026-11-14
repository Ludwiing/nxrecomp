// Servicios de Horizon implementados en el host. Cada objeto (una sesión, o un objeto dentro
// de un dominio) tiene una función que atiende sus comandos CMIF.
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t addr, size;
} NxBuffer;

struct ServiceObject;

typedef struct NxRequest {
    uint32_t cmd;
    uint8_t *in;                     // parámetros de entrada (después de la cabecera SFCI)
    uint32_t in_size;
    uint64_t pid;
    uint32_t in_handles[8];
    int nin_handles;
    uint32_t in_objects[8];
    int nin_objects;
    NxBuffer a[4], b[4], x[4], c[4]; // A: entrada, B: salida, X: puntero de entrada, C: puntero de salida
    int na, nb, nx, nc;
    uint8_t out[0x80];
    uint32_t out_size;
    uint32_t out_copy[8], out_move[8]; // handles de salida: copiados (eventos) y movidos (sesiones)
    int nout_copy, nout_move;
    struct ServiceObject *out_objects[8];
    uint8_t out_object_as_session[8]; // algunas interfaces TIPC salen de un servicio CMIF en dominio
    int nout_objects;
    int session_is_domain;
} NxRequest;

typedef struct ServiceObject {
    const char *name;
    uint32_t (*handle)(struct ServiceObject *self, NxRequest *r);
    void *state;
} ServiceObject;

#define NX_R_UNKNOWN_CMD 0x25A0Bu

void nx_out32(NxRequest *r, uint32_t v);
void nx_out64(NxRequest *r, uint64_t v);
void nx_out_object(NxRequest *r, ServiceObject *o);
void nx_out_session(NxRequest *r, ServiceObject *o);
void nx_out_copy_session(NxRequest *r, ServiceObject *o);
void nx_out_copy_handle(NxRequest *r, uint32_t h);
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n);
void nx_out8(NxRequest *r, uint8_t v);
NxBuffer *nx_in_buffer(NxRequest *r, int k);
NxBuffer *nx_out_buffer(NxRequest *r, int k);
uint32_t nx_in32(NxRequest *r, uint32_t off);
uint64_t nx_in64(NxRequest *r, uint32_t off);

ServiceObject *nx_service_sm(void);
// El usuario de la consola (nxsvc_acc.c) y un IStorage de am con una copia de `data` (nxsvc_am.c).
void nx_usuario(uint8_t uid[16]);
const char *nx_usuario_nombre(void);
ServiceObject *nx_storage_new(const void *data, uint64_t size);
ServiceObject *nx_object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state);
uint64_t nx_lang_code(const char *s);
// Modo de la consola (NX_MODO=base|portatil, por defecto base): 1 en la base, 0 portátil.
int nx_en_base(void);

// Servicios por archivo
uint32_t nx_am_oe(ServiceObject *self, NxRequest *r);
uint32_t nx_ldr_ro(ServiceObject *self, NxRequest *r);
// Las escrituras de rtld en nn::ro::detail::g_p* al enlazar (vars: {índice, dirección}, de programa.c
// nx_rtld_ro_vars); devuelve cuántas publicó.
int nx_rtld_publicar_ro(const uint64_t (*vars)[2], int n);
// nn::ro (nxsvc_ldr.c): los hashes de un NRR que registra el juego (cmd 2) y la descripción del NRO que pide mapear
// (cmd 0, sin soporte: identifica el módulo y, con NX_VOLCAR_NRO=<carpeta>, guarda una copia). nx_sha256 es FIPS 180-4.
int nx_ro_registrar_nrr(uint64_t addr, uint64_t size);
int nx_ro_describir_nro(uint64_t nro, uint64_t tam, uint64_t bss, uint64_t bss_tam);
void nx_sha256(const void *datos, size_t n, uint8_t out[32]);
// Módulos de nn::ro traducidos de antemano: el comando 0 entrega uno cuando el SHA-256 del NRO que pide mapear es el
// suyo (copia la imagen a su base fija, pone el bss en cero y devuelve la base). nxmain.c los registra al arrancar.
typedef struct NxRoModulo {
    const char *nombre;
    uint64_t base, tam, bss;
    uint8_t sha256[32];
} NxRoModulo;
void nx_ro_modulos_traducidos(const NxRoModulo *m, int n);
// Con módulos traducidos, lo que rtld publica para que nn::ro enlace sus importaciones: la lista de módulos del
// arranque (vacía y circular: no hay ModuleObject de rtld) y la función de búsqueda global (dirección del invitado).
int nx_rtld_publicar_busqueda(const uint64_t (*vars)[2], int n, uint64_t buscar);
// Lectura de la memoria del invitado que devuelve 0 en vez de fallar (nxrt.c).
int nx_leer_seguro(uint64_t a, void *out, size_t n);
uint32_t nx_acc(ServiceObject *self, NxRequest *r);
uint32_t nx_hwopus(ServiceObject *self, NxRequest *r);
ServiceObject *nx_servicio_otro(const char *nombre); // nxsvc_otros.c: stubs y servicios menores
uint32_t nx_hid(ServiceObject *self, NxRequest *r);
uint32_t nx_time(ServiceObject *self, NxRequest *r);
uint32_t nx_fsp(ServiceObject *self, NxRequest *r);
uint32_t nx_vi(ServiceObject *self, NxRequest *r);
uint32_t nx2_vi2(ServiceObject *self, NxRequest *r);
uint32_t nx_nvdrv(ServiceObject *self, NxRequest *r);
void nx2_syncpt_publish(uint32_t id, uint32_t value);
struct KEvent;
void nx2_syncpt_event_signaled(struct KEvent *event);
uint32_t nx_audout(ServiceObject *self, NxRequest *r);
uint32_t nx_audren(ServiceObject *self, NxRequest *r);
