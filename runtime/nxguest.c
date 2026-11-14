#include "nxguest.h"

int nx_guest_call(Ctx *caller, uint64_t addr, const uint64_t *args, size_t nargs,
                  uint64_t *result) {
    if (!caller || !addr || nargs > 8 || (nargs && !args)) return 0;
    Ctx nested = *caller;
    for (size_t i = 0; i < nargs; i++) nested.x[i] = args[i];
    nested.x[30] = 0;
    nested.tail_return = 0;
    nested.tail_pending = 0;
    nx_dispatch(&nested, addr);
    if (result) *result = nested.x[0];
    return 1;
}
