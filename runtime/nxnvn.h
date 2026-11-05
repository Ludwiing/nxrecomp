// NVN en el host (HLE): el juego no ejecuta la biblioteca NVN real, sino las funciones de runtime/nxnvn*.c.
#pragma once
#include "nxrt.h"

#define NX_HLE_BASE 0x00FE00000000ull   // direcciones sintéticas de las funciones del host (fuera de todo módulo)

// Reparte una dirección sintética para una función del host; nx_buscar la resuelve con nx_hle_lookup.
uint64_t nx_hle_register(NxFn fn);
NxFn nx_hle_lookup(uint64_t a);

uint64_t nvn_proc_address(const char *name);   // 0 si NVN no tiene esa función

static inline uint64_t arg_x(const Ctx *c, int i) { return c->x[i]; }
static inline void ret_x(Ctx *c, uint64_t v) { c->x[0] = v; }
