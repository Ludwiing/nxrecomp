// acc:u0 / acc:u1: cuentas de usuario. Hay un solo usuario, ya elegido (como una consola con una
// cuenta): casi todos los juegos lo piden al arrancar (nn::account::OpenPreselectedUser o
// GetLastOpenedUser) y sin él abortan. No hay cuenta de Nintendo: lo que pide la red responde que no
// está disponible.
//   NX_USUARIO=nombre   el apodo (por defecto el usuario de Linux)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nxavatar.h"
#include "nxkern.h"
#include "nxsvc.h"

#define OK 0u
#define R_ACC_USUARIO 0x6687Cu    // 2124-0820: usuario inexistente
#define R_ACC_SIN_RED 0x3E87Cu    // 2124-0500: la cuenta de Nintendo no está disponible

static const uint8_t uid[16] = {0x6e, 0x78, 0x72, 0x65, 0x63, 0x6f, 0x6d, 0x70, // "nxrecomp" y algo fijo:
                                0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef}; // nunca cero

void nx_usuario(uint8_t out[16]) { memcpy(out, uid, 16); }

const char *nx_usuario_nombre(void) {
    const char *n = getenv("NX_USUARIO");
    if (!n || !*n) n = getenv("USER");
    return n && *n ? n : "Jugador";
}

static int es_el_usuario(NxRequest *r, uint32_t off) { return r->in_size >= off + 16 && !memcmp(r->in + off, uid, 16); }

// ProfileBase (0x38): uid, marca de tiempo de la última edición, apodo (0x21 bytes con su cero).
static void profile_base(uint8_t *p) {
    memset(p, 0, 0x38);
    memcpy(p, uid, 16);
    uint64_t ts = 1704067200; // 2024-01-01
    memcpy(p + 0x10, &ts, 8);
    snprintf((char *)p + 0x18, 0x21, "%s", nx_usuario_nombre());
}

static uint32_t lista_de_usuarios(NxRequest *r) {
    NxBuffer *b = nx_out_buffer(r, 0);
    if (!b) return OK;
    memset((void *)(uintptr_t)b->addr, 0, b->size);
    if (b->size >= 16) memcpy((void *)(uintptr_t)b->addr, uid, 16);
    return OK;
}

// IProfile
static uint32_t profile(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // Get: UserData (0x80, buffer) + ProfileBase
        NxBuffer *b = nx_out_buffer(r, 0);
        if (b) memset((void *)(uintptr_t)b->addr, 0, b->size < 0x80 ? b->size : 0x80);
        uint8_t p[0x38];
        profile_base(p);
        nx_out_bytes(r, p, sizeof p);
        return OK;
    }
    case 1: { uint8_t p[0x38]; profile_base(p); nx_out_bytes(r, p, sizeof p); return OK; } // GetBase
    case 10: nx_out32(r, sizeof nx_avatar_jpg); return OK; // GetImageSize
    case 11: { // LoadImage
        NxBuffer *b = nx_out_buffer(r, 0);
        uint32_t n = b && b->size < sizeof nx_avatar_jpg ? (uint32_t)b->size : (uint32_t)sizeof nx_avatar_jpg;
        if (b) memcpy((void *)(uintptr_t)b->addr, nx_avatar_jpg, n);
        nx_out32(r, n);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

// IAsyncContext: lo de la red termina enseguida, con error.
static uint32_t async_context(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { KEvent *e; uint32_t h = nx_event_new(&e); nx_event_signal(e); nx_out_copy_handle(r, h); return OK; } // GetSystemEvent
    case 1: return OK;                     // Cancel
    case 2: nx_out8(r, 1); return OK;      // HasDone
    case 3: return R_ACC_SIN_RED;          // GetResult
    }
    return NX_R_UNKNOWN_CMD;
}

// IManagerForApplication: la cuenta de Nintendo (sin conexión).
static uint32_t manager(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: return R_ACC_SIN_RED;          // CheckAvailability
    case 1: nx_out64(r, 0x6e78726563ull); return OK; // GetAccountId
    case 2: case 170: nx_out_object(r, nx_object("IAsyncContext", async_context, NULL)); return OK; // EnsureIdTokenCacheAsync, LoadNetworkServiceLicenseKindAsync
    case 3: case 130: case 150: return R_ACC_SIN_RED; // LoadIdTokenCache, GetNintendoAccountUserResourceCache, CreateAuthorizationRequest
    case 160: return OK;                   // StoreOpenContext
    }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_acc(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out32(r, 1); return OK;                       // GetUserCount
    case 1: nx_out8(r, es_el_usuario(r, 0)); return OK;      // GetUserExistence
    case 2: case 3: case 60: case 131: case 141: return lista_de_usuarios(r); // ListAllUsers, ListOpenUsers, ...
    case 4: case 51: nx_out_bytes(r, uid, 16); return OK;    // GetLastOpenedUser, TrySelectUserWithoutInteraction
    case 5: // GetProfile
        if (!es_el_usuario(r, 0)) return R_ACC_USUARIO;
        nx_out_object(r, nx_object("IProfile", profile, NULL));
        return OK;
    case 6: { uint8_t d[16] = {0}; memcpy(d, uid, 16); nx_out_bytes(r, d, 16); return OK; } // GetProfileDigest
    case 50: case 150: nx_out8(r, 0); return OK;              // IsUserRegistrationRequestPermitted, IsUserAccountSwitchLocked
    case 100: case 140: case 160: case 99: case 110: case 111: case 112: return OK; // InitializeApplicationInfo*, miniaturas
    case 101: // GetBaasAccountManagerForApplication
        nx_out_object(r, nx_object("IManagerForApplication", manager, NULL));
        return OK;
    case 102: case 103: // AuthenticateApplicationAsync, CheckNetworkServiceAvailabilityAsync
        nx_out_object(r, nx_object("IAsyncContext", async_context, NULL));
        return OK;
    case 120: case 130: return R_ACC_SIN_RED; // CreateGuestLoginRequest, LoadOpenContext
    }
    return NX_R_UNKNOWN_CMD;
}
