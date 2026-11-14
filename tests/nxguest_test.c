#include "nxguest.h"

#include <assert.h>
#include <stdio.h>

static uint64_t seen[31];
static uint64_t seen_addr;

void nx_dispatch(Ctx *c, uint64_t addr) {
    seen_addr = addr;
    for (int i = 0; i < 31; i++) seen[i] = c->x[i];
    c->x[0] = 0x12345678;
}

int main(void) {
    Ctx caller = {0};
    caller.x[0] = 0xdead;
    caller.x[30] = 0xfeed;
    caller.sp = 0x40001000;
    caller.tail_return = 0xbeef;
    caller.tail_pending = 1;
    uint64_t args[] = {0x11, 0x22, 0x33};
    uint64_t result = 0;

    assert(nx_guest_call(&caller, 0x123400, args, 3, &result));
    assert(seen_addr == 0x123400);
    assert(seen[0] == 0x11 && seen[1] == 0x22 && seen[2] == 0x33);
    assert(seen[30] == 0);
    assert(result == 0x12345678);
    assert(caller.x[0] == 0xdead && caller.x[30] == 0xfeed && caller.sp == 0x40001000);
    assert(caller.tail_pending == 1 && caller.tail_return == 0xbeef);
    assert(!nx_guest_call(&caller, 0, args, 3, &result));
    assert(!nx_guest_call(&caller, 0x123400, args, 9, &result));
    puts("OK: llamada anidada al SDK conserva el contexto del HLE");
    return 0;
}
