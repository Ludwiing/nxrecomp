// hid: controles. El juego mapea una memoria compartida de 0x40000 bytes donde el sistema
// escribe el estado de los mandos (npad), la pantalla táctil, el teclado y el mouse.
// Por ahora queda en cero: ningún botón apretado. Los mandos de verdad (SDL) van después.
#include <stdlib.h>
#include <string.h>

#include "nxkern.h"
#include "nxsvc.h"

#define HID_SHMEM_SIZE 0x40000
#define OK 0

static KShmem *hid_shmem;
static uint32_t hid_shmem_h;

static uint32_t applet_resource(ServiceObject *self, NxRequest *r) {
    (void)self;
    if (r->cmd == 0) { // GetSharedMemoryHandle
        if (!hid_shmem_h) hid_shmem_h = nx_shmem_new(HID_SHMEM_SIZE, &hid_shmem);
        nx_out_copy_handle(r, hid_shmem_h);
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_hid(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // CreateAppletResource(aruid)
        nx_out_object(r, nx_object("IAppletResource", applet_resource, NULL));
        return OK;
    // Activar dispositivos y configurar mandos: se aceptan.
    case 1: case 11: case 21: case 31: case 40: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73:
    case 79: case 80: case 82: case 87: case 100: case 102: case 103: case 104: case 108: case 109: case 120: case 121:
    case 122: case 123: case 124: case 125: case 126: case 128: case 129: case 130: case 131: case 132: case 133:
    case 134: case 135: case 136: case 200: case 201: case 202: case 203: case 206: case 211: case 300: case 301:
    case 302: case 303: case 304: case 305: case 306: case 307: case 308: case 309: case 310: case 311: case 1000:
    case 1003:
        return OK;
    case 101: nx_out32(r, 0x3F); return OK;   // GetSupportedNpadStyleSet
    case 106: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; } // AcquireNpadStyleSetUpdateEventHandle
    case 107: return OK;                      // DisconnectNpad
    case 127: nx_out32(r, 0); return OK;      // GetNpadJoyHoldType? (variantes según versión)
    }
    return NX_R_UNKNOWN_CMD;
}
