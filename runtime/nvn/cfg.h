// Adaptación de cfg.h de totk-pc-recomp: la configuración sale del entorno (los nombres TOTK_* que lee la capa NVN).
#pragma once
#include <stdlib.h>

static inline const char *cfg_get(const char *name) {
    const char *v = getenv(name);
    return v && *v ? v : NULL;
}
static inline int cfg_bool(const char *name) {   // "0" es falso; cualquier otro valor, verdadero
    const char *v = cfg_get(name);
    return v && !(v[0] == '0' && !v[1]);
}
static inline int cfg_int(const char *name) {
    const char *v = cfg_get(name);
    return v ? atoi(v) : 0;
}
