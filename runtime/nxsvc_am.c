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
    case 5: nx_out8(r, 1); return OK;         // GetOperationMode: 1 = en la base
    case 6: nx_out32(r, 1); return OK;        // GetPerformanceMode: 1 = rendimiento de base
    case 8: nx_out8(r, 0); return OK;         // GetBootMode
    case 9: nx_out8(r, 1); return OK;         // GetCurrentFocusState: con el foco
    case 50: nx_out8(r, 0); return OK;        // IsVrModeEnabled
    case 60: nx_out32(r, 1920); nx_out32(r, 1080); return OK; // GetDefaultDisplayResolution
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
    case 9: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; } // GetLibraryAppletLaunchableEvent
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

static uint32_t application_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: return R_AM_NO_DATA;              // PopLaunchParameter
    case 20: nx_out64(r, 0); return OK;       // EnsureSaveData
    case 21: nx_out64(r, nx_lang_code("es-419")); return OK; // GetDesiredLanguage
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
    (void)r;
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
