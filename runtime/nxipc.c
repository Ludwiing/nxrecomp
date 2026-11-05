// IPC de Horizon: los mensajes CMIF que el juego escribe en su TLS y manda con svcSendSyncRequest.
// Cada sesión apunta a un objeto de servicio implementado aquí. Una sesión convertida en
// dominio lleva varios objetos, cada uno con su id (el SDK lo usa con fsp-srv, hid, am...).
// Formato del mensaje (TLS + 0, 0x100 bytes):
//   palabra 0: tipo (4 pedido, 5 control, 2 cierre), cantidades de descriptores X, A, B, W
//   palabra 1: tamaño de los datos en palabras, banderas de C, ¿hay cabecera especial?
//   [cabecera especial: pid, handles a copiar, handles a mover] [X] [A, B, W]
//   datos alineados a 16: [cabecera de dominio] "SFCI" versión comando token parámetros...
//   [descriptores C]
// La respuesta tiene la misma forma, con "SFCO" y el resultado.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxkern.h"
#include "nxsvc.h"

#define R_OK_ 0u
#define R_SM_NOT_REGISTERED 0xE15u   // sm, 7
#define R_CMIF_UNKNOWN_CMD 0x25A0Bu  // cmif, 221
#define R_SESSION_CLOSED 0xF601u     // kernel, 123
#define R_INVALID_HANDLE 0xE401u

struct KSession {
    ServiceObject *obj;              // el objeto de la sesión (si no es dominio)
    int domain;
    ServiceObject *objs[64];         // objetos del dominio: id = índice + 1
};

static uint32_t session_handle(ServiceObject *o) {
    KSession *s = calloc(1, sizeof *s);
    s->obj = o;
    nx_kernel_lock();
    uint32_t h = nx_handle_new(KO_SESSION, s, 1);
    nx_kernel_unlock();
    return h;
}

void nx_session_close(KSession *s) { (void)s; } // los objetos de servicio viven lo que el proceso

uint32_t nx_connect_named_port(const char *name, uint64_t *handle) {
    if (strcmp(name, "sm:")) {
        fprintf(stderr, "[RED] puerto con nombre desconocido: %s\n", name);
        return 0xF201u; // NotFound
    }
    *handle = session_handle(nx_service_sm());
    return R_OK_;
}

// ---------- lectura y escritura de mensajes ----------
static uint32_t rd32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static void parse_buf(const uint32_t *w, NxBuffer *b) { // descriptor A/B/W: 3 palabras
    uint64_t size = w[0] | (uint64_t)(w[2] >> 24 & 0xF) << 32;
    uint64_t addr = w[1] | (uint64_t)(w[2] >> 28 & 0xF) << 32 | (uint64_t)(w[2] >> 2 & 0x7) << 36;
    b->addr = addr;
    b->size = size;
}

uint32_t nx_send_sync_request(Ctx *c, uint32_t handle) {
    nx_kernel_lock();
    KSession *s = nx_handle_get(handle, KO_SESSION);
    nx_kernel_unlock();
    if (!s) return R_INVALID_HANDLE;
    uint8_t *tls = (uint8_t *)(uintptr_t)c->tpidrro;
    uint32_t w0 = rd32(tls), w1 = rd32(tls + 4);
    uint32_t type = w0 & 0xFFFF, nx = w0 >> 16 & 0xF, na = w0 >> 20 & 0xF, nb = w0 >> 24 & 0xF, nw = w0 >> 28 & 0xF;
    // Las variantes con contexto usan la misma estructura y semántica CMIF.
    if (type == 6) type = 4;
    if (type == 7) type = 5;
    uint32_t data_words = w1 & 0x3FF, cflags = w1 >> 10 & 0xF;
    int tipc = type >= 16; // TIPC: comando en el tipo HIPC, sin cabecera CMIF.
    if (type == 2) return R_OK_; // cierre de sesión

    NxRequest r;
    memset(&r, 0, sizeof r);
    uint32_t p = 8;
    if (w1 >> 31) { // cabecera especial
        uint32_t sh = rd32(tls + p);
        p += 4;
        if (sh & 1) {
            memcpy(&r.pid, tls + p, 8);
            p += 8;
        }
        int ncopy = sh >> 1 & 0xF, nmove = sh >> 5 & 0xF;
        for (int k = 0; k < ncopy + nmove && k < 8; k++, p += 4) r.in_handles[r.nin_handles++] = rd32(tls + p);
    }
    for (uint32_t k = 0; k < nx; k++, p += 8) { // X: puntero de entrada
        uint32_t a = rd32(tls + p), b = rd32(tls + p + 4);
        r.x[r.nx].addr = b | (uint64_t)(a >> 12 & 0xF) << 32 | (uint64_t)(a >> 6 & 0x7) << 36;
        r.x[r.nx].size = a >> 16;
        r.nx++;
    }
    for (uint32_t k = 0; k < na; k++, p += 12) parse_buf((const uint32_t *)(tls + p), &r.a[r.na++]);
    for (uint32_t k = 0; k < nb; k++, p += 12) parse_buf((const uint32_t *)(tls + p), &r.b[r.nb++]);
    p += 12 * nw;
    uint32_t data = p, data_end = p + data_words * 4;
    uint32_t raw = (data + 15) & ~15u; // los datos de CMIF van alineados a 16 dentro del mensaje
    if (cflags >= 2) { // C: listas de recepción, después de los datos
        uint32_t q = data_end;
        for (uint32_t k = 0; k < cflags - 2 && k < 4; k++, q += 8) {
            uint32_t a = rd32(tls + q), b = rd32(tls + q + 4);
            r.c[r.nc].addr = a | (uint64_t)(b & 0xFFFF) << 32;
            r.c[r.nc].size = b >> 16;
            r.nc++;
        }
    }

    ServiceObject *target = s->obj;
    uint32_t domain_cmd = 0, domain_id = 0;
    if (s->domain && type != 5 && !tipc) {
        domain_cmd = tls[raw];
        domain_id = rd32(tls + raw + 4);
        uint32_t nobj = tls[raw + 1];
        uint16_t dsize = (uint16_t)(tls[raw + 2] | tls[raw + 3] << 8);
        for (uint32_t k = 0; k < nobj && k < 8; k++) r.in_objects[r.nin_objects++] = rd32(tls + raw + 16 + dsize + 4 * k);
        target = domain_id >= 1 && domain_id <= 64 ? s->objs[domain_id - 1] : NULL;
        raw += 16;
        if (domain_cmd == 2) { // cerrar un objeto del dominio
            if (target) s->objs[domain_id - 1] = NULL;
            target = NULL;
        }
    }
    if (tipc) {
        r.cmd = type - 16;
        r.in = tls + data;
        r.in_size = data_words * 4;
    } else {
        r.cmd = rd32(tls + raw + 8);
        r.in = tls + raw + 16;
        r.in_size = data_end > raw + 16 ? data_end - raw - 16 : 0;
    }

    uint32_t result;
    if (type == 5) { // control: dominios y tamaño del buffer de punteros
        if (r.cmd == 0) { // ConvertCurrentObjectToDomain
            s->domain = 1;
            s->objs[0] = s->obj;
            nx_out32(&r, 1);
            result = R_OK_;
        } else if (r.cmd == 2 || r.cmd == 4) { // CloneCurrentObject(Ex)
            // Un clon conserva el dominio y sus objetos, no vuelve a CMIF plano.
            // Las sesiones viven hasta finalizar el proceso (nx_session_close).
            nx_kernel_lock();
            r.out_move[r.nout_move++] = nx_handle_new(KO_SESSION, s, 1);
            nx_kernel_unlock();
            result = R_OK_;
        } else if (r.cmd == 3) { // QueryPointerBufferSize
            nx_out32(&r, 0x8000);
            result = R_OK_;
        } else {
            result = R_CMIF_UNKNOWN_CMD;
        }
    } else if (domain_cmd == 2) {
        result = R_OK_;
    } else if (!target) {
        fprintf(stderr, "[RED] pedido a un objeto inexistente (id %u)\n", domain_id);
        result = R_SESSION_CLOSED;
    } else {
        r.session_is_domain = s->domain && !tipc;
        result = target->handle(target, &r);
        static int traza = -1; // NX_TRAZA_IPC: cada comando, con su objeto y su resultado
        if (traza < 0) traza = getenv("NX_TRAZA_IPC") != NULL;
        if (traza) fprintf(stderr, "[ipc] %s %u → 0x%x\n", target->name, r.cmd, result);
        if (result == R_CMIF_UNKNOWN_CMD)
            fprintf(stderr, "[RED] %s: comando %u no implementado\n", target->name, r.cmd);
    }

    // Objetos nuevos: en un dominio van como ids; si no, cada uno es una sesión nueva (handle movido).
    uint32_t out_ids[8], nout_ids = 0;
    for (int k = 0; k < r.nout_objects; k++) {
        if (s->domain && !tipc) {
            for (int j = 0; j < 64; j++)
                if (!s->objs[j]) {
                    s->objs[j] = r.out_objects[k];
                    out_ids[nout_ids++] = (uint32_t)j + 1;
                    break;
                }
        } else {
            r.out_move[r.nout_move++] = session_handle(r.out_objects[k]);
        }
    }

    // Respuesta
    memset(tls, 0, 0x100);
    uint32_t q = 8;
    int special = r.nout_copy + r.nout_move > 0;
    if (special) {
        wr32(tls + q, (uint32_t)r.nout_copy << 1 | (uint32_t)r.nout_move << 5);
        q += 4;
        for (int k = 0; k < r.nout_copy; k++, q += 4) wr32(tls + q, r.out_copy[k]);
        for (int k = 0; k < r.nout_move; k++, q += 4) wr32(tls + q, r.out_move[k]);
    }
    if (tipc) {
        // libnx tipcParseResponse: resultado y datos inmediatamente después de
        // los handles, sin alineación a 16 y sin las palabras "SFCO".
        wr32(tls + q, result);
        memcpy(tls + q + 4, r.out, r.out_size);
        wr32(tls, 0);
        wr32(tls + 4, (r.out_size + 7) / 4 | (special ? 1u << 31 : 0));
        return R_OK_;
    }
    uint32_t rraw = (q + 15) & ~15u;
    uint32_t o = rraw;
    if (s->domain && type != 5) {
        wr32(tls + o, nout_ids);
        o += 16;
    }
    memcpy(tls + o, "SFCO", 4);
    wr32(tls + o + 8, result);
    memcpy(tls + o + 16, r.out, r.out_size);
    o += 16 + r.out_size;
    for (uint32_t k = 0; k < nout_ids; k++, o += 4) wr32(tls + o, out_ids[k]);
    uint32_t words = (o - q + 3) / 4 + 4; // incluye el relleno hasta 16 (como hace el kernel)
    wr32(tls, 0);
    wr32(tls + 4, words | (special ? 1u << 31 : 0));
    return R_OK_;
}

// ---------- ayudas para los servicios ----------
void nx_out32(NxRequest *r, uint32_t v) {
    memcpy(r->out + r->out_size, &v, 4);
    r->out_size += 4;
}
void nx_out64(NxRequest *r, uint64_t v) {
    r->out_size = (r->out_size + 7) & ~7u;
    memcpy(r->out + r->out_size, &v, 8);
    r->out_size += 8;
}
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { r->out_copy[r->nout_copy++] = h; }
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n) {
    memcpy(r->out + r->out_size, p, n);
    r->out_size += n;
}
void nx_out8(NxRequest *r, uint8_t v) { r->out[r->out_size++] = v; }

uint64_t nx_in64(NxRequest *r, uint32_t off) {
    uint64_t v = 0;
    if (off + 8 <= r->in_size) memcpy(&v, r->in + off, 8);
    return v;
}
uint32_t nx_in32(NxRequest *r, uint32_t off) {
    uint32_t v = 0;
    if (off + 4 <= r->in_size) memcpy(&v, r->in + off, 4);
    return v;
}

// Buffers "auto-select" de libnx: llegan los dos descriptores (A y X de entrada, B y C de
// salida) y se usa el que tiene tamaño.
NxBuffer *nx_in_buffer(NxRequest *r, int k) {
    if (k < r->nx && r->x[k].size) return &r->x[k];
    if (k < r->na && r->a[k].size) return &r->a[k];
    return k < r->nx ? &r->x[k] : k < r->na ? &r->a[k] : NULL;
}
NxBuffer *nx_out_buffer(NxRequest *r, int k) {
    if (k < r->nc && r->c[k].size) return &r->c[k];
    if (k < r->nb && r->b[k].size) return &r->b[k];
    return k < r->nc ? &r->c[k] : k < r->nb ? &r->b[k] : NULL;
}
