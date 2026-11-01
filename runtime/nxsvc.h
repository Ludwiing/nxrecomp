// Servicios de Horizon implementados en el host. Cada objeto (una sesión, o un objeto dentro
// de un dominio) tiene una función que atiende sus comandos CMIF.
#pragma once
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
    uint32_t out_handles[8];
    int nout_handles, out_handles_move;
    struct ServiceObject *out_objects[8];
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
uint32_t nx_in32(NxRequest *r, uint32_t off);
uint64_t nx_in64(NxRequest *r, uint32_t off);

ServiceObject *nx_service_sm(void);
