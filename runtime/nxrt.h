// Runtime de nxrecomp: el estado de la CPU de la Switch y las operaciones que usa
// el código recompilado. La memoria del juego se mapea en las mismas direcciones que
// en la consola (Linux x86_64 tiene 47 bits de espacio de usuario y Horizon usa 39),
// así que una dirección del juego es también un puntero válido del proceso.
#pragma once
#include <stdint.h>
#include <string.h>

#include "nxfp.h"
#include "nxsimd.h"
#include "nxcrypto.h"

// Un registro V de NEON. En x86 y en ARM el carril 0 es el byte menos significativo,
// así que las vistas por tamaño de elemento coinciden con la numeración de ARM.
typedef union V128 {
    uint8_t b[16];
    uint16_t h[8];
    uint32_t s[4];
    uint64_t d[2];
    int8_t sb[16];
    int16_t sh[8];
    int32_t ss[4];
    int64_t sd[2];
    float f[4];
    double df[2];
    unsigned __int128 q;
} V128;

typedef struct Ctx {
    V128 vr[32];  // V0-V31 (los nombres n, z, c, v son las banderas)
    uint64_t x[31];
    uint64_t sp;
    uint8_t n, z, c, v;
    uint64_t tpidr, tpidrro;
    uint64_t fpcr, fpsr;
    uint64_t pc;                   // última instrucción ejecutada (solo con nxrecomp --traza)
    uint64_t excl_addr;            // monitor exclusivo: dirección y valor que leyó LDXR/LDXP
    uint64_t excl_val, excl_val2;
    uint64_t tail_return;
    uint8_t tail_pending;
} Ctx;

// A guest tail branch preserves the native caller even when LR was used as
// scratch storage or still contains the address of an earlier BL.
static inline uint64_t nx_enter(Ctx *c) {
    uint64_t result = c->tail_pending ? c->tail_return : c->x[30];
    c->tail_pending = 0;
    return result;
}
static inline void nx_tail(Ctx *c, uint64_t caller) {
    c->tail_return = caller;
    c->tail_pending = 1;
}

// Escribir un escalar en un registro V pone en cero el resto del registro.
#define NX_WRV(bits, lane)                                                          \
    static inline void wrv##bits(Ctx *c, int d, uint64_t v) {                      \
        c->vr[d].q = 0;                                                             \
        c->vr[d].lane[0] = (uint##bits##_t)v;                                       \
    }
NX_WRV(8, b)
NX_WRV(16, h)
NX_WRV(32, s)
NX_WRV(64, d)

// Lo que genera el recompilador (programa.c)
typedef struct NxModule {
    const char *name;
    uint64_t base, size, bss;     // la imagen (text + ro + data) va en base; el bss la sigue
    const unsigned char *data;
    uint64_t ro, rw;              // dónde empiezan el rodata y los datos dentro de la imagen
} NxModule;
typedef struct NxSymbol {
    const char *name;
    uint64_t addr;
} NxSymbol;
extern const NxModule nx_modules[];
extern const int nx_nmodules;
extern const NxSymbol nx_symbols[];
extern const int nx_nsymbols;
extern const char *const nx_imports[];
extern const int nx_nimports;
extern const uint64_t nx_inits[];
extern const int nx_ninits;
extern const uint64_t nx_main_addr, nx_entry_addr, nx_main_stack;
extern const uint64_t nx_system_resource, nx_program_id; // del main.npdm (0 si no hay)
extern const int nx_main_priority, nx_main_core;
extern const unsigned char nx_romfs_data[]; // el RomFS que venía dentro del NRO (homebrew)
extern const uint64_t nx_romfs_size;
void nx_dispatch(Ctx *c, uint64_t addr);
// La función recompilada que empieza en `addr` (o una que avisa del salto inválido o de la importación
// que falta). Con ella los saltos indirectos son llamadas de cola (musttail) y la pila no crece.
typedef void (*NxFn)(Ctx *c);
NxFn nx_buscar(uint64_t addr);

enum {
    NX_TPIDR_EL0, NX_TPIDRRO_EL0, NX_CNTFRQ_EL0, NX_CNTPCT_EL0, NX_CNTVCT_EL0,
    NX_DCZID_EL0, NX_NZCV, NX_FPCR, NX_FPSR, NX_CTR_EL0,
};

// Lo que implementa nxrt.c
void nx_map_fixed(uint64_t addr, uint64_t size, const char *what);
uint64_t nx_now_ns(void);
uint64_t nx_ticks(void);
void nx_svc(Ctx *c, uint32_t imm);
uint64_t nx_mrs(Ctx *c, int reg);
void nx_msr(Ctx *c, int reg, uint64_t v);
void nx_dc_zva(Ctx *c, uint64_t addr);
__attribute__((noreturn)) void nx_trap(Ctx *c, uint64_t pc, uint32_t word, const char *why);
__attribute__((noreturn)) void nx_bad_jump(Ctx *c, uint64_t from, uint64_t to);
__attribute__((noreturn)) void nx_import_call(Ctx *c, int index);
uint64_t nx_symbol(const char *name);
void nx_install_crash_handler(void);

// Vigilancia de memoria (nxrt.c): la GPU sabe qué memoria del juego cambió desde que la copió, sin
// volver a leerla. nx_vigia_proteger deja las páginas de solo lectura y devuelve el sello actual; la
// primera escritura de la CPU en una de ellas salta a un manejador que le pone un sello nuevo y la
// vuelve a abrir. nx_vigia_cambio dice si alguna página del rango tiene un sello posterior a `desde`.
// nx_vigia_tocar abre un rango antes de que el anfitrión escriba ahí con una llamada al sistema (un
// read() en una página protegida daría EFAULT, no una señal). NX_SIN_VIGIA=1 la apaga.
int nx_vigia_activa(void);
uint32_t nx_vigia_proteger(uint64_t addr, uint64_t size);
int nx_vigia_cambio(uint64_t addr, uint64_t size, uint32_t desde);
void nx_vigia_tocar(uint64_t addr, uint64_t size);
// Páginas escondidas (sin acceso): la memoria de un render target cuyo contenido nuevo está solo en la
// GPU. El primer acceso de la CPU llama a la función registrada con nx_vigia_al_leer, que lo copia a
// memoria. nx_vigia_resolver hace lo mismo a propósito (antes de pasarle memoria del juego a OpenGL o a
// una llamada al sistema); nx_vigia_destapar las abre sin copiar (porque se va a escribir encima).
void nx_vigia_ocultar(uint64_t addr, uint64_t size);
void nx_vigia_destapar(uint64_t addr, uint64_t size);
void nx_vigia_resolver(uint64_t addr, uint64_t size);
void nx_vigia_al_leer(void (*fn)(uint64_t addr));
void nx_trace_func(Ctx *c, uint64_t addr);
void nx_set_crash_ctx(Ctx *c);

// ---------- memoria (ARM permite accesos desalineados, x86 también) ----------
#define NX_LD(bits)                                                                 \
    static inline uint##bits##_t ld##bits(uint64_t a) {                             \
        uint##bits##_t v;                                                           \
        memcpy(&v, (const void *)(uintptr_t)a, sizeof v);                           \
        return v;                                                                   \
    }                                                                               \
    static inline void st##bits(uint64_t a, uint64_t v) {                           \
        uint##bits##_t t = (uint##bits##_t)v;                                       \
        memcpy((void *)(uintptr_t)a, &t, sizeof t);                                 \
    }
NX_LD(8)
NX_LD(16)
NX_LD(32)
NX_LD(64)
// STXR: guarda solo si la memoria todavía tiene el valor que leyó LDXR.
#define NX_CAS(bits)                                                                            \
    static inline int nx_cas##bits(uint64_t a, uint64_t expected, uint64_t value) {            \
        uint##bits##_t e = (uint##bits##_t)expected;                                            \
        return __atomic_compare_exchange_n((uint##bits##_t *)(uintptr_t)a, &e, (uint##bits##_t)value, 0, \
                                           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);                \
    }
NX_CAS(8)
NX_CAS(16)
NX_CAS(32)
NX_CAS(64)
static inline int nx_cas128(uint64_t a, uint64_t lo, uint64_t hi, uint64_t nlo, uint64_t nhi) {
    unsigned __int128 e = lo | (unsigned __int128)hi << 64, n = nlo | (unsigned __int128)nhi << 64;
    return __sync_bool_compare_and_swap((unsigned __int128 *)(uintptr_t)a, e, n);
}

static inline V128 ldq(uint64_t a) {
    V128 v;
    memcpy(&v, (const void *)(uintptr_t)a, 16);
    return v;
}
static inline void stq(uint64_t a, V128 v) { memcpy((void *)(uintptr_t)a, &v, 16); }

// ---------- aritmética con banderas (AddWithCarry del manual de Arm) ----------
static inline uint64_t awc64(Ctx *c, uint64_t x, uint64_t y, unsigned cin, int setf) {
    unsigned __int128 us = (unsigned __int128)x + y + cin;
    uint64_t r = (uint64_t)us;
    if (setf) {
        c->n = r >> 63;
        c->z = r == 0;
        c->c = (us >> 64) != 0;
        c->v = (((x ^ r) & (y ^ r)) >> 63) & 1;
    }
    return r;
}
static inline uint32_t awc32(Ctx *c, uint32_t x, uint32_t y, unsigned cin, int setf) {
    uint64_t us = (uint64_t)x + y + cin;
    uint32_t r = (uint32_t)us;
    if (setf) {
        c->n = r >> 31;
        c->z = r == 0;
        c->c = (us >> 32) != 0;
        c->v = (((x ^ r) & (y ^ r)) >> 31) & 1;
    }
    return r;
}
static inline void nz64(Ctx *c, uint64_t r) { c->n = r >> 63; c->z = r == 0; c->c = 0; c->v = 0; }
static inline void nz32(Ctx *c, uint32_t r) { c->n = r >> 31; c->z = r == 0; c->c = 0; c->v = 0; }

// ---------- operaciones con la semántica de ARM ----------
static inline uint64_t ror64(uint64_t x, unsigned r) { r &= 63; return r ? (x >> r) | (x << (64 - r)) : x; }
static inline uint32_t ror32(uint32_t x, unsigned r) { r &= 31; return r ? (x >> r) | (x << (32 - r)) : x; }

// ARM devuelve 0 al dividir entre 0 y no falla con INT_MIN / -1; x86 lanzaría una excepción.
static inline uint64_t udiv64(uint64_t a, uint64_t b) { return b ? a / b : 0; }
static inline uint32_t udiv32(uint32_t a, uint32_t b) { return b ? a / b : 0; }
static inline int64_t sdiv64(int64_t a, int64_t b) { return !b ? 0 : (a == INT64_MIN && b == -1) ? a : a / b; }
static inline int32_t sdiv32(int32_t a, int32_t b) { return !b ? 0 : (a == INT32_MIN && b == -1) ? a : a / b; }

static inline uint64_t clz64(uint64_t x) { return x ? __builtin_clzll(x) : 64; }
static inline uint32_t clz32(uint32_t x) { return x ? __builtin_clz(x) : 32; }
// Bits que siguen al bit de signo y valen lo mismo que él.
static inline uint64_t cls64(uint64_t x) { return clz64((x ^ (x << 1)) | 1); }
static inline uint32_t cls32(uint32_t x) { return clz32((x ^ (x << 1)) | 1); }

static inline uint64_t rbit64(uint64_t x) {
    x = ((x >> 1) & 0x5555555555555555ull) | ((x & 0x5555555555555555ull) << 1);
    x = ((x >> 2) & 0x3333333333333333ull) | ((x & 0x3333333333333333ull) << 2);
    x = ((x >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((x & 0x0F0F0F0F0F0F0F0Full) << 4);
    return __builtin_bswap64(x);
}
static inline uint32_t rbit32(uint32_t x) { return (uint32_t)(rbit64(x) >> 32); }
static inline uint64_t rev16_64(uint64_t x) { return ((x & 0x00FF00FF00FF00FFull) << 8) | ((x >> 8) & 0x00FF00FF00FF00FFull); }
static inline uint32_t rev16_32(uint32_t x) { return ((x & 0x00FF00FFu) << 8) | ((x >> 8) & 0x00FF00FFu); }
static inline uint64_t rev32_64(uint64_t x) {
    return (uint64_t)__builtin_bswap32((uint32_t)x) | ((uint64_t)__builtin_bswap32((uint32_t)(x >> 32)) << 32);
}

static inline uint64_t smulh(uint64_t a, uint64_t b) { return (uint64_t)(((__int128)(int64_t)a * (int64_t)b) >> 64); }
static inline uint64_t umulh(uint64_t a, uint64_t b) { return (uint64_t)(((unsigned __int128)a * b) >> 64); }
