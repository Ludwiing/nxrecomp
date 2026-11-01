// am (applet manager): appletOE / appletAE. Una aplicación abre su "proxy" y de ahí saca los
// objetos que dicen si tiene el foco, si la consola está en la base, los mensajes del sistema,
// la resolución, etc. Aquí la consola está siempre en la base (1080p) y con el foco.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "nxkern.h"
#include "nxsvc.h"

#define R_AM_NO_MESSAGE 0x680u      // am, 3
#define R_AM_NO_DATA 0x2A8u         // am, 2: no hay parámetro de lanzamiento
#define OK 0

static KEvent *msg_event;
static uint32_t msg_event_h;

static uint32_t common_state(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // GetEventHandle: se señala cuando hay mensajes
        if (!msg_event_h) msg_event_h = nx_event_new(&msg_event);
        nx_out_copy_handle(r, msg_event_h);
        return OK;
    case 1: return R_AM_NO_MESSAGE;           // ReceiveMessage
    case 5: nx_out8(r, nx_en_base()); return OK;  // GetOperationMode: 0 portátil, 1 en la base
    case 6: nx_out32(r, nx_en_base()); return OK; // GetPerformanceMode: van juntos
    case 8: nx_out8(r, 0); return OK;         // GetBootMode
    case 9: nx_out8(r, 1); return OK;         // GetCurrentFocusState: con el foco
    case 50: nx_out8(r, 0); return OK;        // IsVrModeEnabled
    case 60: // GetDefaultDisplayResolution
        nx_out32(r, nx_en_base() ? 1920 : 1280);
        nx_out32(r, nx_en_base() ? 1080 : 720);
        return OK;
    case 61: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    case 66: return OK;                       // SetCpuBoostMode
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t self_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // Exit: el sistema termina la aplicación
        fflush(stdout);
        _exit(0);
    case 1: case 2: case 10: case 11: case 12: case 13: case 14: case 16: case 19:
    case 50: case 62: case 65: case 68: case 72: case 100:
        return OK; // avisos y permisos: se aceptan
    case 9: { // GetLibraryAppletLaunchableEvent: ya se puede (libnx lo espera antes de abrir un applet)
        KEvent *e;
        uint32_t h = nx_event_new(&e);
        nx_event_signal(e);
        nx_out_copy_handle(r, h);
        return OK;
    }
    case 40: nx_out64(r, 1); return OK;       // CreateManagedDisplayLayer
    case 91: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t window_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: nx_out64(r, 0x1000); return OK;   // GetAppletResourceUserId
    case 10: return OK;                       // AcquireForegroundRights
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t audio_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 3: case 4: return OK;
    case 1: case 2: { float v = 1.0f; nx_out_bytes(r, &v, 4); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t display_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    (void)r;
    return NX_R_UNKNOWN_CMD;
}

// ---------- IStorage: los datos que van y vienen entre la aplicación, el sistema y los applets ----------
typedef struct {
    uint8_t *data;
    uint64_t size;
} Storage;

static uint32_t storage_accessor(ServiceObject *self, NxRequest *r) {
    Storage *s = self->state;
    switch (r->cmd) {
    case 0: nx_out64(r, s->size); return OK; // GetSize
    case 10: case 11: { // Write(offset, buffer de entrada) / Read(offset, buffer de salida)
        uint64_t off = nx_in64(r, 0);
        NxBuffer *b = r->cmd == 10 ? nx_in_buffer(r, 0) : nx_out_buffer(r, 0);
        if (!b || off > s->size) return 0x1A2A0u; // am 503: fuera de rango
        uint64_t n = b->size < s->size - off ? b->size : s->size - off;
        if (r->cmd == 10) memcpy(s->data + off, (void *)(uintptr_t)b->addr, n);
        else memcpy((void *)(uintptr_t)b->addr, s->data + off, n);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t storage(ServiceObject *self, NxRequest *r) {
    switch (r->cmd) {
    case 0: case 1: nx_out_object(r, nx_object("IStorageAccessor", storage_accessor, self->state)); return OK; // Open
    }
    return NX_R_UNKNOWN_CMD;
}

ServiceObject *nx_storage_new(const void *data, uint64_t size) {
    Storage *s = calloc(1, sizeof *s);
    s->data = calloc(1, size ? size : 1);
    s->size = size;
    if (data) memcpy(s->data, data, size);
    return nx_object("IStorage", storage, s);
}

// ---------- applets: el sistema los abriría encima del juego; aquí terminan al momento ----------
// con la respuesta de un usuario que acepta: el selector de usuario elige al único, el teclado
// devuelve texto vacío, el de mandos deja un jugador y el de errores solo cierra.
typedef struct {
    uint32_t id;
    ServiceObject *salida;
    int empezado;
    KEvent *estado, *datos; // se avisan al empezar (terminó) y si deja datos de salida
    uint32_t estado_h, datos_h, interactivo_h;
} Applet;

static ServiceObject *salida_de_applet(uint32_t id) {
    switch (id) {
    case 0x10: { // PlayerSelect: resultado 0 y el usuario
        uint8_t o[0x18] = {0};
        nx_usuario(o + 8);
        return nx_storage_new(o, sizeof o);
    }
    case 0x11: { // Swkbd: resultado 0 (aceptado) y texto UTF-16 vacío
        uint8_t o[0x7D8] = {0};
        return nx_storage_new(o, sizeof o);
    }
    case 0x0C: { // Controller: un jugador, el 1
        uint8_t o[0xC] = {1};
        return nx_storage_new(o, sizeof o);
    }
    }
    return NULL;
}

static uint32_t applet_accessor(ServiceObject *self, NxRequest *r) {
    Applet *a = self->state;
    switch (r->cmd) {
    case 0: nx_out_copy_handle(r, a->estado_h); return OK;       // GetAppletStateChangedEvent
    case 105: nx_out_copy_handle(r, a->datos_h); return OK;      // GetPopOutDataEvent
    case 106: nx_out_copy_handle(r, a->interactivo_h); return OK; // GetPopInteractiveOutDataEvent: nunca
    case 1: nx_out8(r, (uint8_t)a->empezado); return OK; // IsCompleted
    case 10: // Start
        a->empezado = 1;
        a->salida = salida_de_applet(a->id);
        if (a->salida) nx_event_signal(a->datos);
        nx_event_signal(a->estado);
        if (!a->salida && a->id != 0x0E)
            fprintf(stderr, "[nxrt] applet 0x%x: se cierra sin respuesta\n", a->id);
        return OK;
    case 20: case 25: case 100: case 103: case 150: return OK; // RequestExit, Terminate, PushInData, ...
    case 30: return OK;                                         // GetResult
    case 101: // PopOutData
        if (!a->salida) return R_AM_NO_DATA;
        nx_out_object(r, a->salida);
        a->salida = NULL;
        return OK;
    case 104: return R_AM_NO_DATA;                             // PopInteractiveOutData
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t application_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: { // PopLaunchParameter(tipo): 2 = el usuario preseleccionado (magic, elegido, uid)
        if (nx_in32(r, 0) != 2) return R_AM_NO_DATA;
        uint8_t p[0x88] = {0};
        uint32_t magic = 0xC79497CAu, si = 1;
        memcpy(p, &magic, 4);
        memcpy(p + 4, &si, 4);
        nx_usuario(p + 8);
        nx_out_object(r, nx_storage_new(p, sizeof p));
        return OK;
    }
    case 20: nx_out64(r, 0); return OK;       // EnsureSaveData
    case 21: nx_out64(r, nx_lang_code(getenv("NX_IDIOMA") ? getenv("NX_IDIOMA") : "es-419")); return OK; // GetDesiredLanguage
    case 25: nx_out64(r, 0); return OK;       // ExtendSaveData: no hizo falta más
    case 26: nx_out64(r, 0x4000000); nx_out64(r, 0x4000000); return OK; // QuerySaveDataSize
    case 27: nx_out64(r, 0); return OK;       // CreateCacheStorage
    case 30: case 31: case 32: return OK;     // BeginBlockingHomeButton..., EndBlocking...
    case 60: case 70: case 71: case 80: case 123: case 124: return OK;
    case 122: nx_out32(r, (uint32_t)-1); return OK; // GetPreviousProgramIndex: no hubo otro
    case 150: case 151: return R_AM_NO_DATA;  // notificaciones e invitaciones: no hay
    case 22: case 66: case 67: case 90: case 100: case 101: case 102: return OK;
    case 23: { char v[16] = "1.0.0"; nx_out_bytes(r, v, 16); return OK; } // GetDisplayVersion
    case 40: nx_out8(r, 1); return OK;        // NotifyRunning
    case 50: { uint8_t id[16] = {0x6e, 0x78}; nx_out_bytes(r, id, 16); return OK; } // GetPseudoDeviceId
    case 130: case 140: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t library_applet_creator(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // CreateLibraryApplet(id, modo)
        Applet *a = calloc(1, sizeof *a);
        a->id = nx_in32(r, 0);
        KEvent *e;
        a->estado_h = nx_event_new(&a->estado);
        a->datos_h = nx_event_new(&a->datos);
        a->interactivo_h = nx_event_new(&e);
        nx_out_object(r, nx_object("ILibraryAppletAccessor", applet_accessor, a));
        return OK;
    }
    case 10: case 11: case 12: // CreateStorage(tamaño), CreateTransferMemoryStorage, CreateHandleStorage
        nx_out_object(r, nx_storage_new(NULL, r->cmd == 10 ? nx_in64(r, 0) : nx_in64(r, 8)));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t debug_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    (void)r;
    return NX_R_UNKNOWN_CMD;
}

static uint32_t application_proxy(ServiceObject *self, NxRequest *r) {
    (void)self;
    static const struct {
        uint32_t cmd;
        const char *name;
        uint32_t (*fn)(ServiceObject *, NxRequest *);
    } children[] = {
        {0, "ICommonStateGetter", common_state},       {1, "ISelfController", self_controller},
        {2, "IWindowController", window_controller},   {3, "IAudioController", audio_controller},
        {4, "IDisplayController", display_controller}, {11, "ILibraryAppletCreator", library_applet_creator},
        {20, "IApplicationFunctions", application_functions}, {1000, "IDebugFunctions", debug_functions},
    };
    for (size_t k = 0; k < sizeof children / sizeof *children; k++)
        if (children[k].cmd == r->cmd) {
            nx_out_object(r, nx_object(children[k].name, children[k].fn, NULL));
            return OK;
        }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_am_oe(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 100: case 200: case 201: case 300: case 350: // Open*Proxy
        nx_out_object(r, nx_object("IApplicationProxy", application_proxy, NULL));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}
