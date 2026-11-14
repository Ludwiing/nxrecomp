// hid: controles. El juego mapea una memoria compartida de 0x40000 bytes donde el sistema
// escribe el estado de los mandos (npad), la pantalla táctil, el teclado y el mouse.
// Los mandos (teclado y mandos de SDL) los escribe nxinput.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxkern.h"
#include "nxinput.h"
#include "nxsvc.h"

#define HID_SHMEM_SIZE 0x40000
#define OK 0

static KShmem *hid_shmem;
static uint32_t hid_shmem_h;

// NpadCondition: estado de los mandos que el servidor de hid publica en la memoria compartida. El SDK de Switch 2
// ya no pide GetNpadJoyHoldType por IPC: nn::hid::detail::GetNpadJoyHoldType lee
// NpadSharedMemoryAccessor::GetNpadCondition(), un puntero que NpadSharedMemoryAccessor::Attach fija en
// memoria compartida + 0x3E200, y devuelve el u32 de +8 si el byte de +12 no es cero; si es cero devuelve
// 2202-0710 (0x58cca) y nn::hid::GetNpadJoyHoldType aborta (lo llama ShowControllerSupport). Medido en el SDK
// de Mario Tennis Fever (0400d4201c6ea000). En +0 (u32) y +4 (u8) está GetLastActiveNpadController (2202-0751
// si +4 es cero). NX2Linux publica No1/jugador 1 porque nxinput presenta ese Npad como conectado desde el arranque.
// El desplazamiento solo está medido en el SDK de Switch 2: con otra plataforma no se escribe nada ahí (en Switch 1
// esa zona es reservada y el SDK pide el modo por el comando 121).
#define HID_NPAD_CONDITION 0x3E200
#define NPAD_JOY_HOLD_VERTICAL 0
#define NPAD_JOY_HOLD_HORIZONTAL 1
#define NPAD_ID_PLAYER_1 0u // HidNpadIdType_No1 (libnx: jugador 1)

// El modo de sujeción de los Joy-Con de la aplicación: vertical hasta que el juego pida otro con
// SetNpadJoyHoldType (comando 120). El comando 121 y la NpadCondition salen de este mismo estado.
static uint32_t npad_joy_hold_type = NPAD_JOY_HOLD_VERTICAL;

static int npad_condition_in_shmem(void) {
    const char *plataforma = getenv("NX_PLATAFORMA"); // lo pone NX2Linux según el NPDM/ExeFS (como nxinput.c)
    return plataforma && !strcmp(plataforma, "switch2");
}

static void publish_npad_condition(void) {
    if (!hid_shmem || !npad_condition_in_shmem()) return;
    uint8_t *cond = hid_shmem->host + HID_NPAD_CONDITION;
    __atomic_store_n((uint32_t *)cond, NPAD_ID_PLAYER_1, __ATOMIC_RELAXED);
    __atomic_store_n((uint32_t *)(cond + 8), __atomic_load_n(&npad_joy_hold_type, __ATOMIC_ACQUIRE), __ATOMIC_RELAXED);
    __atomic_store_n(cond + 4, (uint8_t)1, __ATOMIC_RELEASE); // No1 está conectado desde que nxinput adjunta
    __atomic_store_n(cond + 12, (uint8_t)1, __ATOMIC_RELEASE); // el modo de sujeción ya es válido
}

// Cambia el modo de sujeción y lo publica. Devuelve 0 sin tocar el estado si el modo no es vertical ni horizontal
// (el comando 120 decide qué hacer con eso).
static int set_npad_joy_hold_type(uint64_t type) {
    if (type != NPAD_JOY_HOLD_VERTICAL && type != NPAD_JOY_HOLD_HORIZONTAL) return 0;
    __atomic_store_n(&npad_joy_hold_type, (uint32_t)type, __ATOMIC_RELEASE);
    publish_npad_condition();
    return 1;
}

static uint32_t active_vibration_devices(ServiceObject *self, NxRequest *r) {
    (void)self;
    // ActivateVibrationDevice: the SDK requires a live service object even
    // when the host has no rumble-capable controller connected.
    return r->cmd == 0 ? OK : NX_R_UNKNOWN_CMD;
}

static uint32_t applet_resource(ServiceObject *self, NxRequest *r) {
    (void)self;
    if (r->cmd == 0) { // GetSharedMemoryHandle
        if (!hid_shmem_h) {
            hid_shmem_h = nx_shmem_new(HID_SHMEM_SIZE, &hid_shmem);
            nx_input_attach(hid_shmem->host); // desde aquí el sistema escribe los mandos
            publish_npad_condition();
        }
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
    case 203: // CreateActiveVibrationDeviceList
        nx_out_object(r, nx_object("IActiveVibrationDeviceList", active_vibration_devices, NULL));
        return OK;
    // Activar dispositivos y configurar mandos: se aceptan.
    case 1: case 11: case 21: case 31: case 40: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73:
    case 79: case 80: case 82: case 87: case 100: case 102: case 103: case 104: case 108: case 109:
    case 122: case 123: case 124: case 125: case 126: case 128: case 129: case 130: case 131: case 132: case 133:
    case 134: case 135: case 136: case 200: case 201: case 202: case 206: case 211: case 300: case 301:
    case 302: case 303: case 304: case 305: case 306: case 307: case 308: case 309: case 310: case 311: case 1000:
    case 1003: case 3150: // Switch 2 SetMouseLibraryVersion; no mouse library is exposed by this host
    case 3110: // nn::hid::InitializeJcms(buffer, tamaño) (Switch 2, sensor de ratón del Joy-Con 2): sin sensor en el host,
               // el búfer queda en cero (sin muestras). Sports Resort aborta al arrancar si falla.
    case 214:  // nn::hid::detail::FlushEnqueuedVibrationValues (Switch 2): lote de SendVibrationValue; sin vibración
    case 1004: // nn::hid::InitializeTouchScreen(ancho, alto) del SDK de Switch 2 (sin salida); el toque viene del host
        return OK;
    case 101: nx_out32(r, 0x3F); return OK;   // GetSupportedNpadStyleSet
    case 120: { // SetNpadJoyHoldType(aruid u64, modo u64): 0 vertical, 1 horizontal
        uint64_t type = r->in_size >= 16 ? nx_in64(r, 8) : ~0ull;
        if (!set_npad_joy_hold_type(type)) {
            // Fuera del enum. No hay evidencia local del resultado que da Horizon (nn::hid::detail::SetNpadJoyHoldType,
            // 0x93ce9e0 en Mario Tennis Fever, no lo valida antes del IPC) y no es un comando desconocido: no se
            // inventa un resultado ni se da por bueno; el runtime cierra el juego a la vista, como con lo no implementado.
            fprintf(stderr, "[nxrt] hid: SetNpadJoyHoldType con modo %#llx (entrada de %u bytes) fuera de vertical/"
                            "horizontal; el resultado de Horizon no está medido y el runtime no lo emula\n",
                    (unsigned long long)type, r->in_size);
            fflush(stdout);
            exit(1);
        }
        return OK;
    }
    case 121: // GetNpadJoyHoldType(aruid) -> u64 (SDK de Switch 1; el de Switch 2 lee la NpadCondition)
        nx_out64(r, __atomic_load_n(&npad_joy_hold_type, __ATOMIC_ACQUIRE));
        return OK;
    case 106: { // AcquireNpadStyleSetUpdateEventHandle: ya avisando (hay mando conectado desde el arranque;
                // algunos juegos lo esperan antes de leer la entrada)
        KEvent *e;
        uint32_t h = nx_event_new(&e);
        nx_event_signal(e);
        nx_out_copy_handle(r, h);
        return OK;
    }
    case 107: return OK;                      // DisconnectNpad
    case 127: return OK;                      // StopLrAssignmentMode (sin salida)
    }
    return NX_R_UNKNOWN_CMD;
}
