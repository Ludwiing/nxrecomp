// Helpers para que un HLE invoque una función del SDK recompilado.
#pragma once

#include "nxrt.h"

#include <stddef.h>
#include <stdint.h>

// Ejecuta addr con los primeros ocho argumentos AAPCS64 y devuelve x0.
// Usa una copia de Ctx: los registros, LR y estado del llamador HLE no cambian.
// Devuelve 0 si faltan argumentos válidos o la dirección es nula.
int nx_guest_call(Ctx *caller, uint64_t addr, const uint64_t *args, size_t nargs,
                 uint64_t *result);
