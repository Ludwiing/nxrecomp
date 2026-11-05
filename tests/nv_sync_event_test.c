#include "../runtime/nxsvc_vi.c"
#include <assert.h>

static KEvent *test_handles[256];
static unsigned next_test_handle = 1;
static pthread_mutex_t kernel_mutex = PTHREAD_MUTEX_INITIALIZER;
void nx_kernel_lock(void) { pthread_mutex_lock(&kernel_mutex); }
void nx_kernel_unlock(void) { pthread_mutex_unlock(&kernel_mutex); }
uint32_t nx_handle_new(int type, void *obj, int writable) {
    (void)type; (void)writable;
    assert(next_test_handle < 256);
    test_handles[next_test_handle] = obj;
    return next_test_handle++;
}
uint32_t nx_event_new(KEvent **event) {
    *event = calloc(1, sizeof **event);
    return nx_handle_new(KO_EVENT, *event, 0);
}
void nx_event_signal(KEvent *event) {
    nx_kernel_lock(); event->signaled = 1; nx_kernel_unlock();
}

static void *increment_concurrently(void *arg) {
    nx_syncpt_incr(*(uint32_t *)arg);
    return NULL;
}

int main(void) {
    uint8_t io[16] = {0};
    io32(io, 0, 3);
    assert(ctrl_ioctl(0x1F, io) == 0);
    uint32_t handle = sync_event_query(3);
    KEvent *event = test_handles[handle];
    assert(event && !event->signaled);
    io32(io, 0, 18); io32(io, 4, 2); io32(io, 8, UINT32_MAX); io32(io, 12, 3);
    assert(ctrl_ioctl(0x1E, io) == 5);
    assert(u32at(io + 12) == ((18 << 4) | 3));
    assert(test_handles[sync_event_query(u32at(io + 12))] == event);
    nx_syncpt_incr(17); assert(!event->signaled);
    nx_syncpt_incr(18); assert(!event->signaled);
    nx_syncpt_incr(18); assert(event->signaled);
    io32(io, 0, 0x10000003); assert(ctrl_ioctl(0x1C, io) == 0);
    assert(!event->signaled && sync_events[3].allocated);
    io32(io, 0, 18); io32(io, 4, 3); io32(io, 12, 3);
    assert(ctrl_ioctl(0x1E, io) == 5);
    io32(io, 0, 3); assert(ctrl_ioctl(0x20, io) == 0);
    nx_syncpt_incr(18); assert(!event->signaled);
    io32(io, 0, 18); io32(io, 4, 4); io32(io, 8, 0);
    assert(ctrl_ioctl(0x1D, io) == 5);
    uint32_t encoded = u32at(io + 12);
    assert((encoded & 0x10000000) && (encoded >> 16 & 0xFFF) == 18);
    KEvent *other = test_handles[sync_event_query(encoded)];
    assert(other && !other->signaled);
    nx_syncpt_incr(18); assert(other->signaled);
    io32(io, 0, 18); io32(io, 4, 4);
    assert(ctrl_ioctl(0x1D, io) == 0 && u32at(io + 12) == 4);
    io32(io, 12, 64); assert(ctrl_ioctl(0x1E, io) == 4);
    // Completion racing event registration must either take the fast path or
    // signal the event; neither ordering may lose the notification.
    uint32_t race_id = 22;
    for (unsigned iteration = 0; iteration < 128; iteration++) {
        uint32_t target = nx_syncpt_read(race_id) + 1;
        io32(io, 0, race_id); io32(io, 4, target); io32(io, 12, 1);
        pthread_t worker;
        assert(!pthread_create(&worker, NULL, increment_concurrently, &race_id));
        uint32_t result = ctrl_ioctl(0x1E, io);
        pthread_join(worker, NULL);
        assert(result == 0 || result == 5);
        if (result == 5)
            assert(test_handles[sync_event_query(u32at(io + 12))]->signaled);
    }
    puts("GPU syncpoint events: pending, exact threshold, query identity, clear, unregister and fast completion: PASS");
}
