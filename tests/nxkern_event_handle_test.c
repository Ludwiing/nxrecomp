#include "nxkern.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    KEvent *event = NULL;
    uint32_t read_handle = nx_event_new(&event);
    assert(event != NULL && read_handle != 0);

    nx_kernel_lock();
    uint32_t write_handle = nx_handle_new(KO_EVENT, event, 1);
    nx_kernel_unlock();
    assert(write_handle != 0 && write_handle != read_handle);

    assert(!event->signaled);
    assert(!nx_event_signal_handle(read_handle));
    assert(!nx_event_clear_handle(read_handle));
    assert(!event->signaled);

    assert(nx_event_signal_handle(write_handle));
    assert(event->signaled);
    assert(nx_event_clear_handle(write_handle));
    assert(!event->signaled);

    assert(!nx_event_signal_handle(0));
    assert(!nx_event_clear_handle(0));
    puts("OK: handles de SystemEvent respetan lectura/escritura y despiertan el kernel");
    return 0;
}
