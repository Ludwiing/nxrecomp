// Los servicios: sm: (el administrador), set (idioma del sistema) y fsp-srv (por ahora,
// el RomFS del juego como IStorage). Lo que no está implementado se avisa con [RED] y su nombre.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxkern.h"
#include "nxsvc.h"

static ServiceObject *object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    o->name = name;
    o->handle = fn;
    o->state = state;
    return o;
}

static uint64_t lang(const char *s) {
    uint64_t v = 0;
    memcpy(&v, s, strlen(s) < 8 ? strlen(s) : 8);
    return v;
}

// ---------- set: idioma ----------
static const char *const languages[] = {"ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru",
                                        "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans", "zh-Hant", "pt-BR"};
#define NLANG (int)(sizeof languages / sizeof *languages)

static uint32_t set_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // GetLanguageCode
        nx_out64(r, lang(getenv("NX_IDIOMA") ? getenv("NX_IDIOMA") : "es-419"));
        return 0;
    case 1: case 5: { // GetAvailableLanguageCodes(2): lista en el buffer de salida
        NxBuffer *b = r->nb ? &r->b[0] : r->nc ? &r->c[0] : NULL;
        int n = 0;
        if (b)
            for (; n < NLANG && (uint64_t)(n + 1) * 8 <= b->size; n++) {
                uint64_t v = lang(languages[n]);
                memcpy((void *)(uintptr_t)(b->addr + 8 * (uint64_t)n), &v, 8);
            }
        nx_out32(r, (uint32_t)n);
        return 0;
    }
    case 3: case 6: // GetAvailableLanguageCodeCount(2)
        nx_out32(r, NLANG);
        return 0;
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- fsp-srv: el RomFS como IStorage ----------
typedef struct {
    FILE *f;
    uint64_t size;
} Storage;

static uint32_t storage_handle(ServiceObject *self, NxRequest *r) {
    Storage *s = self->state;
    switch (r->cmd) {
    case 0: { // Read(offset, size) → buffer B
        uint64_t off = nx_in64(r, 0), size = nx_in64(r, 8);
        if (!r->nb || size > r->b[0].size) return 0x2EE202u; // fs: tamaño fuera de rango
        if (off > s->size || size > s->size - off) return 0x2EE202u;
        fseeko(s->f, (off_t)off, SEEK_SET);
        if (fread((void *)(uintptr_t)r->b[0].addr, 1, size, s->f) != size) return 0x2EE202u;
        return 0;
    }
    case 4: // GetSize
        nx_out64(r, s->size);
        return 0;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t fsp_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: case 1003: return 0; // SetCurrentProcess, DisableAutoSaveDataCreation
    case 200: { // OpenDataStorageByCurrentProcess: el RomFS del juego
        const char *path = getenv("NX_ROMFS");
        FILE *f = path ? fopen(path, "rb") : NULL;
        if (!f) {
            fprintf(stderr, "[RED] el juego abrió su RomFS, pero no hay imagen: define NX_ROMFS=<RomFS.img>\n");
            return 0x7D402u; // fs: no encontrado
        }
        Storage *s = calloc(1, sizeof *s);
        s->f = f;
        fseeko(f, 0, SEEK_END);
        s->size = (uint64_t)ftello(f);
        nx_out_object(r, object("IStorage (RomFS)", storage_handle, s));
        return 0;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- sm: ----------
static const struct {
    const char *name;
    const char *label;
    uint32_t (*fn)(ServiceObject *, NxRequest *);
} registry[] = {
    {"set", "set", set_handle},
    {"fsp-srv", "fsp-srv", fsp_handle},
};

static uint32_t sm_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: return 0; // RegisterClient
    case 1: { // GetServiceHandle(nombre de 8 bytes)
        char name[9] = {0};
        memcpy(name, r->in, r->in_size < 8 ? r->in_size : 8);
        for (size_t k = 0; k < sizeof registry / sizeof *registry; k++)
            if (!strcmp(registry[k].name, name)) {
                nx_out_object(r, object(registry[k].label, registry[k].fn, NULL));
                return 0;
            }
        fprintf(stderr, "[RED] servicio no implementado: %s\n", name);
        return 0xE15u; // sm: no registrado
    }
    }
    return NX_R_UNKNOWN_CMD;
}

ServiceObject *nx_service_sm(void) { return object("sm:", sm_handle, NULL); }
