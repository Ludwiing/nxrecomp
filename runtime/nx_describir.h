// Dirección del invitado -> "módulo+0xoff (símbolo+0xoff)" con las tablas que genera el recompilador (nx_modules,
// nx_symbols ordenado por dirección). Encabezado para que tests/describir_test.c lo pruebe con tablas propias.
#pragma once
#include <stdio.h>
#include "nxrt.h"

static const char *nx_describir(uint64_t a, char *buf, size_t n) {
    const NxModule *m = NULL;
    for (int k = 0; k < nx_nmodules; k++)
        if (a >= nx_modules[k].base && a < nx_modules[k].base + nx_modules[k].size) m = &nx_modules[k];
    if (!m) {
        snprintf(buf, n, "?");
        return buf;
    }
    int lo = 0, hi = nx_nsymbols - 1, s = -1;   // nx_symbols va ordenado por dirección: el último <= a
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (nx_symbols[mid].addr <= a) s = mid, lo = mid + 1;
        else hi = mid - 1;
    }
    if (s >= 0 && nx_symbols[s].name && nx_symbols[s].addr >= m->base)
        snprintf(buf, n, "%s+0x%llx (%s+0x%llx)", m->name, (unsigned long long)(a - m->base), nx_symbols[s].name,
                 (unsigned long long)(a - nx_symbols[s].addr));
    else
        snprintf(buf, n, "%s+0x%llx", m->name, (unsigned long long)(a - m->base));
    return buf;
}
