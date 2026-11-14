#include "nxhle_event.h"

#include "nxguest.h"

int nx_hle_event_create(Ctx *ctx, uint64_t system_event, uint32_t clear_mode,
                        int inter_process, uint32_t *write_handle,
                        uint32_t *sdk_result) {
    static const char create_name[] =
        "_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb";
    static const char writable_name[] =
        "_ZN2nn2os30GetWritableHandleOfSystemEventEPKNS0_15SystemEventTypeE";
    if (!ctx || !system_event || !write_handle) return 0;
    uint64_t create = nx_symbol(create_name);
    uint64_t writable = nx_symbol(writable_name);
    if (!create || !writable) return 0;

    uint64_t args[] = {system_event, clear_mode, inter_process != 0};
    uint64_t result = UINT32_MAX;
    if (!nx_guest_call(ctx, create, args, 3, &result)) return 0;
    if (sdk_result) *sdk_result = (uint32_t)result;
    if ((uint32_t)result != 0) return 0;

    args[0] = system_event;
    uint64_t handle = 0;
    if (!nx_guest_call(ctx, writable, args, 1, &handle) || !handle || handle > UINT32_MAX)
        return 0;
    *write_handle = (uint32_t)handle;
    return 1;
}
