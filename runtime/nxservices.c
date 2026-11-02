// sm: (el administrador de servicios) y los servicios pequeños: set, set:sys y apm.
// Los grandes van en nxsvc_*.c. Lo que no está implementado se avisa con [RED] y su nombre.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxkern.h"
#include "nxsvc.h"

ServiceObject *nx_object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    o->name = name;
    o->handle = fn;
    o->state = state;
    return o;
}

int nx_en_base(void) {
    const char *m = getenv("NX_MODO");
    return !(m && (!strcmp(m, "portatil") || !strcmp(m, "portátil") || !strcmp(m, "0")));
}

uint64_t nx_lang_code(const char *s) {
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
        nx_out64(r, nx_lang_code(getenv("NX_IDIOMA") ? getenv("NX_IDIOMA") : "es-419"));
        return 0;
    case 1: case 5: { // GetAvailableLanguageCodes(2): lista en el buffer de salida
        NxBuffer *b = r->nb ? &r->b[0] : r->nc ? &r->c[0] : NULL;
        int n = 0;
        if (b)
            for (; n < NLANG && (uint64_t)(n + 1) * 8 <= b->size; n++) {
                uint64_t v = nx_lang_code(languages[n]);
                memcpy((void *)(uintptr_t)(b->addr + 8 * (uint64_t)n), &v, 8);
            }
        nx_out32(r, (uint32_t)n);
        return 0;
    }
    case 3: case 6: // GetAvailableLanguageCodeCount(2)
        nx_out32(r, NLANG);
        return 0;
    case 4: nx_out32(r, 1); return 0;          // GetRegionCode: América
    case 7: case 9: { // GetKeyCodeMap(2): el mapa del teclado (vacío)
        NxBuffer *b = nx_out_buffer(r, 0);
        if (b) memset((void *)(uintptr_t)b->addr, 0, b->size);
        return 0;
    }
    case 8: nx_out8(r, 0); return 0;           // GetQuestFlag: no es una consola de tienda
    case 10: case 11: { // GetFirmwareVersionForDebug, GetDeviceNickName
        NxBuffer *b = nx_out_buffer(r, 0);
        if (b) memset((void *)(uintptr_t)b->addr, 0, b->size);
        if (b && r->cmd == 11) snprintf((char *)(uintptr_t)b->addr, b->size, "nxrecomp");
        return 0;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- set:sys: versión del sistema ----------
#define FW_MAJOR 18
#define FW_MINOR 1
#define FW_MICRO 0

static uint32_t setsys_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 3: case 4: { // GetFirmwareVersion(2): estructura de 0x100 bytes en el buffer de salida
        NxBuffer *b = r->nc ? &r->c[0] : r->nb ? &r->b[0] : NULL;
        if (!b || b->size < 0x100) return 0x7F4u; // buffer inválido
        uint8_t *v = (uint8_t *)(uintptr_t)b->addr;
        memset(v, 0, 0x100);
        v[0] = FW_MAJOR, v[1] = FW_MINOR, v[2] = FW_MICRO, v[4] = 1;
        strcpy((char *)v + 0x08, "NX");
        strcpy((char *)v + 0x28, "0000000000000000000000000000000000000000");
        strcpy((char *)v + 0x68, "18.1.0");
        strcpy((char *)v + 0x80, "NintendoSDK Firmware for NX 18.1.0-1.0");
        return 0;
    }
    case 23: nx_out32(r, 1); return 0;        // GetColorSetId: tema oscuro
    case 37: nx_out64(r, 0); return 0;        // GetSettingsItemValueSize: no hay ese ajuste
    case 38: nx_out64(r, 0); return 0;        // GetSettingsItemValue
    case 90: { uint8_t id[16] = {0x6e, 0x78}; nx_out_bytes(r, id, 16); return 0; } // GetMiiAuthorId
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- apm: modo de rendimiento ----------
static uint32_t apm_session_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: return 0;                         // SetPerformanceConfiguration
    case 1: nx_out32(r, 0x00010000); return 0; // GetPerformanceConfiguration: CPU 1020 MHz, GPU 384 MHz
    case 2: return 0;                         // SetCpuOverclockEnabled
    }
    return NX_R_UNKNOWN_CMD;
}
static uint32_t apm_handle(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out_object(r, nx_object("apm ISession", apm_session_handle, NULL)); return 0; // OpenSession
    case 1: nx_out32(r, nx_en_base()); return 0; // GetPerformanceMode: 0 normal (portátil), 1 base
    case 6: nx_out8(r, 0); return 0;           // IsEnabledSustainedPerformanceMode
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
    {"fsp-srv", "fsp-srv", nx_fsp},
    {"set:sys", "set:sys", setsys_handle},
    {"set:fd", "set:fd", setsys_handle},
    {"apm", "apm", apm_handle},
    {"appletOE", "appletOE", nx_am_oe},
    {"appletAE", "appletAE", nx_am_oe},
    {"acc:u0", "acc:u0", nx_acc},
    {"acc:u1", "acc:u1", nx_acc},
    {"acc:su", "acc:su", nx_acc},
    {"hwopus", "hwopus", nx_hwopus},
    {"hid", "hid", nx_hid},
    {"time:u", "time:u", nx_time},
    {"time:a", "time:a", nx_time},
    {"time:s", "time:s", nx_time},
    {"vi:u", "vi:u", nx_vi},
    {"vi:s", "vi:s", nx_vi},
    {"vi:m", "vi:m", nx_vi},
    {"nvdrv", "nvdrv", nx_nvdrv},
    {"audout:u", "audout:u", nx_audout},
    {"audren:u", "audren:u", nx_audren},
    {"nvdrv:a", "nvdrv:a", nx_nvdrv},
    {"nvdrv:s", "nvdrv:s", nx_nvdrv},
    {"nvdrv:t", "nvdrv:t", nx_nvdrv},
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
                nx_out_object(r, nx_object(registry[k].label, registry[k].fn, NULL));
                return 0;
            }
        ServiceObject *otro = nx_servicio_otro(name);
        if (otro) {
            nx_out_object(r, otro);
            return 0;
        }
        fprintf(stderr, "[RED] servicio no implementado: %s\n", name);
        return 0xE15u; // sm: no registrado
    }
    }
    return NX_R_UNKNOWN_CMD;
}

ServiceObject *nx_service_sm(void) { return nx_object("sm:", sm_handle, NULL); }
