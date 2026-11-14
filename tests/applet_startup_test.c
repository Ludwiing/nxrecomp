// Isolated startup-policy tests; no display, game or saved data required.
#include "../runtime/nxsvc_am.c"
#include <assert.h>

static ServiceObject objects[32];
static unsigned object_count;
static struct { int type, writable; void *object; } test_handles[32];
static uint32_t next_test_handle = 100;
static uint32_t test_syncpt_values[NX_SYNCPT_COUNT];
ServiceObject *nx_object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state) {
    assert(object_count < sizeof objects / sizeof objects[0]);
    objects[object_count] = (ServiceObject){name, fn, state};
    return &objects[object_count++];
}
uint32_t nx_handle_new(int type, void *obj, int writable) {
    assert(next_test_handle < 100 + sizeof test_handles / sizeof test_handles[0]);
    uint32_t h = next_test_handle++;
    test_handles[h - 100] = (typeof(test_handles[0])){type, writable, obj};
    return h;
}
void *nx_handle_get(uint32_t h, int type) {
    if (h < 100 || h >= next_test_handle) return NULL;
    return test_handles[h - 100].type == type ? test_handles[h - 100].object : NULL;
}
uint32_t nx_syncpt_read(uint32_t id) { return id < NX_SYNCPT_COUNT ? test_syncpt_values[id] : 0; }
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_session(NxRequest *r, ServiceObject *o) { nx_out_object(r, o); }
void nx_out_copy_session(NxRequest *r, ServiceObject *o) { (void)o; nx_out_copy_handle(r, 43); }
uint32_t nx_in32(NxRequest *r, uint32_t off) {
    uint32_t value = 0;
    if (off + sizeof value <= r->in_size) memcpy(&value, r->in + off, sizeof value);
    return value;
}
uint64_t nx_in64(NxRequest *r, uint32_t off) {
    uint64_t value = 0;
    if (off + sizeof value <= r->in_size) memcpy(&value, r->in + off, sizeof value);
    return value;
}
NxBuffer *nx_in_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
NxBuffer *nx_out_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
void nx_out_bytes(NxRequest *r, const void *data, uint32_t size) {
    assert(r->out_size + size <= sizeof r->out);
    memcpy(r->out + r->out_size, data, size);
    r->out_size += size;
}
void nx_usuario(uint8_t uid[16]) { memset(uid, 0, 16); }
uint32_t nx_nvdrv(ServiceObject *self, NxRequest *r) { (void)self; (void)r; return 0; }
uint32_t nx2_vi2(ServiceObject *self, NxRequest *r) { (void)self; (void)r; return 0; }
uint64_t nx_lang_code(const char *s) { (void)s; return 0; }
void nx_kernel_lock(void) {}
void nx_kernel_unlock(void) {}
uint32_t nx_event_new(KEvent **event) {
    *event = calloc(1, sizeof **event);
    return *event ? nx_handle_new(KO_EVENT, *event, 0) : 0;
}
void nx_event_signal(KEvent *event) {
    event->signaled = 1;
    nx2_syncpt_event_signaled(event);
}
int nx_en_base(void) { return 1; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { r->out_copy[r->nout_copy++] = h; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
void nx_out8(NxRequest *r, uint8_t v) { r->out[r->out_size++] = v; }

int main(void) {
    // Switch 2 regular apps use appletOE command 10 (OpenApplicationProxy2).
    // The command has a distinct input layout, but returns the same proxy interface.
    uint8_t proxy2_args[12] = {0};
    NxRequest r = {.cmd = 10, .in = proxy2_args, .in_size = sizeof proxy2_args,
                   .pid = 0x1234, .nin_handles = 1, .in_handles = {7}};
    assert(nx_am_oe(NULL, &r) == 0 && r.nout_objects == 1);
    assert(!strcmp(r.out_objects[0]->name, "IApplicationProxy"));
    NxRequest proxy = {.cmd = 20};
    assert(r.out_objects[0]->handle(r.out_objects[0], &proxy) == 0);
    assert(proxy.nout_objects == 1 && !strcmp(proxy.out_objects[0]->name, "IApplicationFunctions"));
    NxRequest s2_compat_probe = {.cmd = 320};
    assert(proxy.out_objects[0]->handle(proxy.out_objects[0], &s2_compat_probe) == 0);
    assert(s2_compat_probe.out_size == 0 && s2_compat_probe.nout_objects == 0);
    s2_compat_probe = (NxRequest){.cmd = 321};
    assert(proxy.out_objects[0]->handle(proxy.out_objects[0], &s2_compat_probe) == 0);
    assert(s2_compat_probe.out_size == 0 && s2_compat_probe.nout_objects == 0);
    s2_compat_probe = (NxRequest){.cmd = 210};
    assert(proxy.out_objects[0]->handle(proxy.out_objects[0], &s2_compat_probe) == 0);
    assert(s2_compat_probe.nout_copy == 1);
    assert(nx_handle_get(s2_compat_probe.out_copy[0], KO_EVENT) != NULL);
    NxRequest performance = {.cmd = 40};
    assert(r.out_objects[0]->handle(r.out_objects[0], &performance) == 0);
    assert(performance.nout_objects == 1 && !strcmp(performance.out_objects[0]->name, "IPerformanceFunctions"));
    NxRequest overclock = {.cmd = 0};
    assert(performance.out_objects[0]->handle(performance.out_objects[0], &overclock) == 0 && overclock.out_size == 1);
    assert(overclock.out[0] == 0);
    overclock = (NxRequest){.cmd = 1};
    assert(performance.out_objects[0]->handle(performance.out_objects[0], &overclock) == 0);
    NxRequest scaling = {.cmd = 60};
    assert(r.out_objects[0]->handle(r.out_objects[0], &scaling) == 0);
    assert(scaling.nout_objects == 1 && !strcmp(scaling.out_objects[0]->name,
                                                 "IGraphicsScalingParametersFunctions"));
    NxRequest scale = {.cmd = 10};
    assert(scaling.out_objects[0]->handle(scaling.out_objects[0], &scale) == 0 && scale.out_size == 16);
    uint32_t graphics_scale[2];
    uint64_t graphics_reserved;
    memcpy(graphics_scale, scale.out, sizeof graphics_scale);
    memcpy(&graphics_reserved, scale.out + 8, sizeof graphics_reserved);
    assert(graphics_scale[0] == 10 && graphics_scale[1] == 0 && graphics_reserved == 0);
    scale = (NxRequest){.cmd = 1050};
    assert(scaling.out_objects[0]->handle(scaling.out_objects[0], &scale) == 0);
    // Firmware 20.0.0+ creates library applets with command 3 (id, mode, thread id).
    NxRequest creator = {.cmd = 11};
    assert(r.out_objects[0]->handle(r.out_objects[0], &creator) == 0);
    assert(creator.nout_objects == 1 && !strcmp(creator.out_objects[0]->name, "ILibraryAppletCreator"));
    struct { uint32_t id, mode; uint64_t thread; } create3 = {0x10, 0, 0x55};
    NxRequest create = {.cmd = 3, .in = (uint8_t *)&create3, .in_size = sizeof create3};
    assert(creator.out_objects[0]->handle(creator.out_objects[0], &create) == 0);
    assert(create.nout_objects == 1 && !strcmp(create.out_objects[0]->name, "ILibraryAppletAccessor"));
    NxRequest start = {.cmd = 10};
    assert(create.out_objects[0]->handle(create.out_objects[0], &start) == 0);
    NxRequest pop = {.cmd = 101}; // PlayerSelect answers, so the id was read from offset 0
    assert(create.out_objects[0]->handle(create.out_objects[0], &pop) == 0 && pop.nout_objects == 1);
    // Preserve the legacy command used by Switch 1.
    r = (NxRequest){.cmd = 0};
    assert(nx_am_oe(NULL, &r) == 0 && r.nout_objects == 1);
    assert(!strcmp(r.out_objects[0]->name, "IApplicationProxy"));

    r = (NxRequest){.cmd = 0};
    assert(common_state(NULL, &r) == 0 && r.nout_copy == 1);
    assert(msg_event->signaled);
    r = (NxRequest){.cmd = 1};
    assert(common_state(NULL, &r) == 0 && r.out_size == 4);
    uint32_t message; memcpy(&message, r.out, 4);
    assert(message == 15 && !msg_event->signaled);
    r = (NxRequest){.cmd = 1};
    assert(common_state(NULL, &r) == R_AM_NO_MESSAGE);
    r = (NxRequest){.cmd = 900};
    assert(common_state(NULL, &r) == 0);
    r = (NxRequest){.cmd = 44};
    assert(self_controller(NULL, &r) == 0 && r.out_size == 16);
    uint64_t layers[2]; memcpy(layers, r.out, 16);
    assert(layers[0] == 1 && layers[1] == 2);
    r = (NxRequest){.cmd = 2400};
    assert(self_controller(NULL, &r) == 0 && r.out_size == 0);
    r = (NxRequest){.cmd = 2000};
    assert(self_controller(NULL, &r) == 0 && r.nout_objects == 0 && r.nout_copy == 1);
    assert(r.out_copy[0] == 43);
    r = (NxRequest){.cmd = 126};
    assert(nx2_gsv_session(NULL, &r) == 0 && r.out_size == 0);
    r = (NxRequest){.cmd = 1501};
    assert(nx2_gsv_session(NULL, &r) == 0 && r.out_size == 0);
    r = (NxRequest){.cmd = 1502};
    assert(nx2_gsv_session(NULL, &r) == 0 && r.out_size == 0);
    r = (NxRequest){.cmd = 1402};
    assert(nx2_gsv_session(NULL, &r) == 0 && r.nout_copy == 1);
    ServiceObject *gsv_service = nx_object("nx2:gsv", nx2_gsv_session, NULL);
    // GSV 991 returns the byte size of the decoder image pool, not the request
    // words. Check two independently observed sizes and the local 2560x1440
    // request captured from Mario Tennis Fever.
    const uint32_t pool_dims[][2] = {{640, 360}, {1920, 1080}, {2560, 1440}};
    const uint64_t pool_sizes[] = {0x64000, 0x348000, 0x5a0000};
    for (size_t i = 0; i < sizeof pool_dims / sizeof pool_dims[0]; i++) {
        uint32_t args[17] = {0};
        args[1] = 1; // NV12 format code from the SDK request.
        args[2] = ((pool_dims[i][1] - 1) << 16) | (pool_dims[i][0] - 1);
        args[16] = 1; // one image in this pool
        NxRequest pool = {.cmd = 991, .in = (uint8_t *)args, .in_size = sizeof args};
        assert(gsv_service->handle(gsv_service, &pool) == 0 && pool.out_size == 8);
        uint64_t size;
        memcpy(&size, pool.out, sizeof size);
        assert(size == pool_sizes[i]);
    }
    uint32_t pool_args[17] = {0};
    pool_args[1] = 1;
    pool_args[2] = ((1080u - 1) << 16) | (1920u - 1);
    pool_args[16] = 2;
    NxRequest two_images = {.cmd = 991, .in = (uint8_t *)pool_args, .in_size = sizeof pool_args};
    assert(gsv_service->handle(gsv_service, &two_images) == 0 && two_images.out_size == 8);
    uint64_t two_image_size;
    memcpy(&two_image_size, two_images.out, sizeof two_image_size);
    assert(two_image_size == 2 * pool_sizes[1]);
    NxRequest short_pool = {.cmd = 991};
    assert(gsv_service->handle(gsv_service, &short_pool) == 0xE401u && !short_pool.out_size);
    pool_args[1] = 2; // Unknown format is reported, not answered with an assumed layout.
    NxRequest unknown_format = {.cmd = 991, .in = (uint8_t *)pool_args, .in_size = sizeof pool_args};
    assert(gsv_service->handle(gsv_service, &unknown_format) == NX_R_UNKNOWN_CMD && !unknown_format.out_size);
    NxRequest boot_gsv = {.cmd = 4};
    assert(gsv_service->handle(gsv_service, &boot_gsv) == 0);
    uint64_t requested_features = 0x100000002ull, granted_features = 0;
    NxRequest features = {.cmd = 115, .in = (uint8_t *)&requested_features, .in_size = sizeof requested_features};
    assert(gsv_service->handle(gsv_service, &features) == 0);
    memcpy(&granted_features, features.out, sizeof granted_features);
    assert(features.out_size == sizeof granted_features && granted_features == requested_features);
    NxRequest bad_features = {.cmd = 115};
    assert(gsv_service->handle(gsv_service, &bad_features) != 0 && bad_features.out_size == 0);
    for (uint32_t cmd = 1101; cmd <= 1102; cmd++) {
        NxRequest permission = {.cmd = cmd};
        assert(gsv_service->handle(gsv_service, &permission) == 0 && permission.out_size == 0);
    }
    // nn::codec::video::detail::ipc::Initialize(bool): 901 and 1001 are result-only; 902, 905(u8) and
    // 1002 move the codec root session, whose command 1 moves the VideoDec/EncService session.
    for (uint32_t cmd = 901; cmd <= 1003; cmd += 100) {
        NxRequest context = {.cmd = cmd};
        assert(gsv_service->handle(gsv_service, &context) == 0);
        assert(context.out_size == 0 && context.nout_objects == 0 && context.nout_copy == 0);
        context = (NxRequest){.cmd = cmd + 2}; // 903 / 1003: finalize, result only.
        assert(gsv_service->handle(gsv_service, &context) == 0);
        assert(context.out_size == 0 && context.nout_objects == 0 && context.nout_copy == 0);
    }
    // GSV 1201 creates the context; 1203 finalizes it. 1202 returns its session.
    for (uint32_t cmd = 1201; cmd <= 1203; cmd += 2) {
        NxRequest chat = {.cmd = cmd};
        assert(gsv_service->handle(gsv_service, &chat) == 0);
        assert(chat.out_size == 0 && chat.nout_objects == 0 && chat.nout_copy == 0);
    }
    // 1202 moves the chat GSV session. Bootstrap commands preserve the ABI
    // without pretending that a host GameChat instance or media stream exists.
    NxRequest chat_root = {.cmd = 1202};
    assert(gsv_service->handle(gsv_service, &chat_root) == 0);
    assert(chat_root.nout_objects == 1 && chat_root.nout_copy == 0 && chat_root.out_size == 0);
    ServiceObject *chat_gsv = chat_root.out_objects[0];
    assert(!strcmp(chat_gsv->name, "nx2:chatgsv"));
    uint8_t chat_user_init[12] = {'c', 'h', 'a', 't', '2', 0, 0, 0, 1, 0, 0, 0};
    NxRequest chat_user = {.cmd = 103, .in = chat_user_init, .in_size = sizeof chat_user_init};
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0);
    assert(chat_user.out_size == 0 && chat_user.nout_objects == 0 && chat_user.nout_copy == 0);
    uint8_t chat7_user_init[12] = {'c', 'h', 'a', 't', '7', 0, 0, 0, 2, 0, 0, 0};
    chat_user.in = chat7_user_init;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0);
    uint8_t chat8_user_init[12] = {'c', 'h', 'a', 't', '8', 0, 0, 0, 3, 0, 0, 0};
    chat_user.in = chat8_user_init;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0);
    uint8_t chat9_user_init[12] = {'c', 'h', 'a', 't', '9', 0, 0, 0, 4, 0, 0, 0};
    chat_user.in = chat9_user_init;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0);
    chat_user.in_size = 11;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0xCA01u);
    chat_user.in_size = sizeof chat_user_init;
    chat9_user_init[0] = 'x';
    assert(chat_gsv->handle(chat_gsv, &chat_user) == NX_R_UNKNOWN_CMD);
    chat9_user_init[0] = 'c';
    chat9_user_init[8] = 0;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == NX_R_UNKNOWN_CMD);
    // Fifth call of Sports Resort: two-digit name "chat10", version 5 (aborted at boot when only one digit was taken).
    uint8_t chat10_user_init[12] = {'c', 'h', 'a', 't', '1', '0', 0, 0, 5, 0, 0, 0};
    chat_user.in = chat10_user_init;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == 0 && chat_user.nout_copy == 0);
    uint8_t chat_no_digit[12] = {'c', 'h', 'a', 't', 0, 0, 0, 0, 5, 0, 0, 0};
    chat_user.in = chat_no_digit;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == NX_R_UNKNOWN_CMD);
    uint8_t chat_gap[12] = {'c', 'h', 'a', 't', '1', 0, '0', 0, 5, 0, 0, 0};
    chat_user.in = chat_gap;
    assert(chat_gsv->handle(chat_gsv, &chat_user) == NX_R_UNKNOWN_CMD);
    NxRequest chat_id_req = {.cmd = 1};
    assert(chat_gsv->handle(chat_gsv, &chat_id_req) == 0 && chat_id_req.out_size == 4);
    uint32_t chat_id = 0;
    memcpy(&chat_id, chat_id_req.out, sizeof chat_id);
    assert(chat_id > 0 && chat_id <= 0x3ff);
    Nx2ChatGsv *chat_state = chat_gsv->state;
    assert(chat_state && chat_state->context_id == chat_id);
    NxRequest short_chat_event = {.cmd = 11};
    assert(chat_gsv->handle(chat_gsv, &short_chat_event) == 0xCA01u);
    uint32_t bad_chat_id = chat_id + 1;
    NxRequest bad_chat_event = {.cmd = 11, .in = (uint8_t *)&bad_chat_id, .in_size = 4};
    assert(chat_gsv->handle(chat_gsv, &bad_chat_event) == 0xE401u && bad_chat_event.nout_copy == 0);
    NxRequest chat_event = {.cmd = 11, .in = (uint8_t *)&chat_id, .in_size = 4};
    assert(chat_gsv->handle(chat_gsv, &chat_event) == 0 && chat_event.out_size == 0);
    assert(chat_event.nout_copy == 1);
    uint32_t chat_event_handle = chat_event.out_copy[0];
    assert(chat_event_handle != chat_state->event_handle);
    assert(chat_state->event && !chat_state->event->signaled);
    assert(nx_handle_get(chat_event_handle, KO_EVENT) == chat_state->event);
    assert(!test_handles[chat_event_handle - 100].writable);
    chat_event = (NxRequest){.cmd = 11, .in = (uint8_t *)&chat_id, .in_size = 4};
    assert(chat_gsv->handle(chat_gsv, &chat_event) == 0 && chat_event.nout_copy == 1);
    assert(chat_event.out_copy[0] != chat_event_handle);
    assert(nx_handle_get(chat_event.out_copy[0], KO_EVENT) == chat_state->event);
    NxRequest chat_unknown = {.cmd = 4};
    assert(chat_gsv->handle(chat_gsv, &chat_unknown) == NX_R_UNKNOWN_CMD);
    uint8_t unlimited = 1;
    NxRequest video_roots[3] = {{.cmd = 902}, {.cmd = 905, .in = &unlimited, .in_size = 1}, {.cmd = 1002}};
    const char *root_names[3] = {"nx2:viddec-root", "nx2:viddec-root", "nx2:videnc-root"};
    const char *service_names[3] = {"nx2:viddec", "nx2:viddec", "nx2:videnc"};
    for (int k = 0; k < 3; k++) {
        assert(gsv_service->handle(gsv_service, &video_roots[k]) == 0);
        assert(video_roots[k].nout_objects == 1 && video_roots[k].nout_copy == 0 && video_roots[k].out_size == 0);
        ServiceObject *root = video_roots[k].out_objects[0];
        assert(!strcmp(root->name, root_names[k]));
        NxRequest create_service = {.cmd = 1};
        assert(root->handle(root, &create_service) == 0 && create_service.nout_objects == 1);
        assert(create_service.nout_copy == 0 && create_service.out_size == 0);
        ServiceObject *video = create_service.out_objects[0];
        assert(!strcmp(video->name, service_names[k]));
        // No decode/encode command is emulated: each one must be reported, not answered.
        for (uint32_t cmd = 0; cmd < 4096; cmd++) {
            NxRequest work = {.cmd = cmd};
            assert(video->handle(video, &work) == NX_R_UNKNOWN_CMD && work.out_size == 0 && !work.nout_objects);
        }
        NxRequest root_unknown = {.cmd = 2};
        assert(root->handle(root, &root_unknown) == NX_R_UNKNOWN_CMD && !root_unknown.nout_objects);
    }
    NxRequest short_unlimited = {.cmd = 905};
    assert(gsv_service->handle(gsv_service, &short_unlimited) == 0xE401u && !short_unlimited.nout_objects);
    for (uint32_t cmd = 904; cmd <= 1004; cmd += 100) { // InitializeForSystem variants: system-only, not emulated.
        NxRequest system_only = {.cmd = cmd};
        assert(gsv_service->handle(gsv_service, &system_only) == NX_R_UNKNOWN_CMD && !system_only.nout_objects);
    }
    NxRequest init_syncpt = {.cmd = 202};
    uint64_t display_version = 0x300020000ull;
    NxRequest display_session = {.cmd = 305, .in = (uint8_t *)&display_version, .in_size = 8};
    assert(gsv_service->handle(gsv_service, &display_session) == 0 && display_session.nout_objects == 1);
    assert(!strcmp(display_session.out_objects[0]->name, "nx2:vi2"));
    assert(display_session.out_objects[0]->handle == nx2_vi2);
    NxRequest bad_display = {.cmd = 305};
    assert(gsv_service->handle(gsv_service, &bad_display) != 0 && !bad_display.nout_objects);
    uint64_t gpu_version = 4;
    NxRequest open_gpu = {.cmd = 497, .in = (uint8_t *)&gpu_version, .in_size = sizeof gpu_version};
    assert(gsv_service->handle(gsv_service, &open_gpu) == 0 && open_gpu.nout_objects == 1);
    ServiceObject *gpu = open_gpu.out_objects[0];
    assert(!strcmp(gpu->name, "nx2:gpu"));
    NxRequest gpu_range = {.cmd = 11};
    assert(gpu->handle(gpu, &gpu_range) == 0 && gpu_range.out_size == 8);
    uint32_t range[2]; memcpy(range, gpu_range.out, sizeof range);
    assert(range[0] == 0 && range[1] == 31);
    uint32_t slot = 3;
    NxRequest start_gpu = {.cmd = 21, .in = (uint8_t *)&slot, .in_size = sizeof slot};
    assert(gpu->handle(gpu, &start_gpu) != 0); // Must obtain the completion event first.
    NxRequest event_gpu = {.cmd = 12, .in = (uint8_t *)&slot, .in_size = sizeof slot};
    assert(gpu->handle(gpu, &event_gpu) == 0 && event_gpu.nout_copy == 1);
    Nx2GpuSession *gpu_state = gpu->state;
    assert(!gpu_state->events[slot]->signaled);
    assert(gpu->handle(gpu, &start_gpu) == 0 && gpu_state->events[slot]->signaled);
    slot = 32;
    NxRequest bad_event = {.cmd = 12, .in = (uint8_t *)&slot, .in_size = sizeof slot};
    assert(gpu->handle(gpu, &bad_event) != 0 && bad_event.nout_copy == 0);
    NxRequest worker_gpu = {.cmd = 22};
    assert(gpu->handle(gpu, &worker_gpu) == 0 && worker_gpu.nout_objects == 1);
    assert(worker_gpu.out_objects[0]->state == gpu_state);
    uint64_t device_args[3] = {0};
    NxRequest create_device = {.cmd = 41, .in = (uint8_t *)device_args, .in_size = sizeof device_args};
    assert(gpu->handle(gpu, &create_device) == 0 && create_device.out_size == 4);
    uint32_t device_id; memcpy(&device_id, create_device.out, sizeof device_id);
    assert(device_id > 0);
    NxRequest destroy_device = {.cmd = 42, .in = (uint8_t *)&device_id, .in_size = sizeof device_id};
    assert(gpu->handle(gpu, &destroy_device) == 0);
    assert(gpu->handle(gpu, &destroy_device) != 0);
    NxRequest va_gpu = {.cmd = 203};
    assert(gpu->handle(gpu, &va_gpu) == 0 && va_gpu.out_size == 24);
    assert(gsv_service->handle(gsv_service, &init_syncpt) == 0);
    assert(init_syncpt.nout_objects == 1 && !strcmp(init_syncpt.out_objects[0]->name, "nx2:syncpt"));
    uint64_t tick = 0x123456789ABCDEF0ull;
    NxRequest syncpt_ping = {.cmd = 0, .in = (uint8_t *)&tick, .in_size = sizeof tick};
    assert(init_syncpt.out_objects[0]->handle(init_syncpt.out_objects[0], &syncpt_ping) == 0);
    uint64_t echoed_tick = 0;
    memcpy(&echoed_tick, syncpt_ping.out, sizeof echoed_tick);
    assert(syncpt_ping.out_size == sizeof echoed_tick && echoed_tick == tick);
    NxRequest syncpt_initialize = {.cmd = 21};
    assert(init_syncpt.out_objects[0]->handle(init_syncpt.out_objects[0], &syncpt_initialize) == 0);
    assert(syncpt_initialize.out_size == 0 && syncpt_initialize.nout_objects == 0);
    NxRequest syncpt_unknown = {.cmd = 9999};
    assert(init_syncpt.out_objects[0]->handle(init_syncpt.out_objects[0], &syncpt_unknown) == NX_R_UNKNOWN_CMD);

    // GSV 204 maps the title's two TransferMemory pages, publishes current
    // NV syncpoints, and returns the request/notification event pair by copy.
    uint8_t rw_page[0x1000] __attribute__((aligned(8))) = {0};
    uint8_t ro_page[0x2000] __attribute__((aligned(8))) = {0};
    KTmem rw_tmem = {(uint64_t)(uintptr_t)rw_page, sizeof rw_page};
    KTmem ro_tmem = {(uint64_t)(uintptr_t)ro_page, sizeof ro_page};
    uint32_t rw_handle = nx_handle_new(KO_TMEM, &rw_tmem, 1);
    uint32_t ro_handle = nx_handle_new(KO_TMEM, &ro_tmem, 1);
    uint32_t gsv_args[2] = {1, NX_SYNCPT_COUNT + 8};
    test_syncpt_values[2] = 77;
    NxRequest attach = {.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args,
                        .nin_handles = 2, .in_handles = {rw_handle, ro_handle}};
    assert(nx2_gsv_session(NULL, &attach) == 0);
    assert(attach.nout_copy == 2 && attach.nout_move == 0);
    uint32_t syncpt_request_handle = attach.out_copy[0];
    uint32_t syncpt_notify_handle = attach.out_copy[1];
    uint32_t gsv_header[8]; memcpy(gsv_header, ro_page, sizeof gsv_header);
    assert(gsv_header[0] == 1 && gsv_header[1] == 0 && gsv_header[2] == NX_SYNCPT_COUNT);
    assert(gsv_header[3] == 0x40 && gsv_header[4] == 0 && gsv_header[5] == 0x1040);
    assert(gsv_header[6] == 0x100 && gsv_header[7] == 0x1100);
    uint64_t published = 0; memcpy(&published, ro_page + 0x40 + 2 * 8, sizeof published);
    assert(published == 77);
    KEvent *request_event = nx_handle_get(attach.out_copy[0], KO_EVENT);
    KEvent *notify_event = nx_handle_get(attach.out_copy[1], KO_EVENT);
    assert(request_event && notify_event);
    assert(test_handles[attach.out_copy[0] - 100].writable == 1);
    assert(test_handles[attach.out_copy[1] - 100].writable == 0);

    nx2_syncpt_publish(2, 78);
    memcpy(&published, ro_page + 0x40 + 2 * 8, sizeof published);
    assert(published == 78 && notify_event->signaled);
    notify_event->signaled = 0;
    uint64_t summary = 1, armed = 5;
    memcpy(rw_page, &summary, sizeof summary);
    memcpy(rw_page + 0x100, &armed, sizeof armed);
    nx_event_signal(request_event);
    uint64_t ack_summary = 0, ack_bits = 0;
    memcpy(&ack_summary, ro_page + 0x1040, sizeof ack_summary);
    memcpy(&ack_bits, ro_page + 0x1100, sizeof ack_bits);
    assert(!request_event->signaled && notify_event->signaled);
    assert(ack_summary == summary && ack_bits == armed);

    uint32_t event_handles = next_test_handle;
    attach = (NxRequest){.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args,
                         .nin_handles = 2, .in_handles = {rw_handle, ro_handle}};
    assert(nx2_gsv_session(NULL, &attach) == 0 && attach.nout_copy == 2);
    assert(attach.out_copy[0] == syncpt_request_handle && attach.out_copy[1] == syncpt_notify_handle);
    assert(next_test_handle == event_handles); // reattach reuses the persistent event pair

    attach = (NxRequest){.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args};
    assert(nx2_gsv_session(NULL, &attach) == 0xE401u);
    gsv_args[0] = 2;
    attach = (NxRequest){.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args,
                         .nin_handles = 2, .in_handles = {rw_handle, ro_handle}};
    assert(nx2_gsv_session(NULL, &attach) == 0x1001u);
    gsv_args[0] = 1;
    gsv_args[1] = 0;
    attach = (NxRequest){.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args,
                         .nin_handles = 2, .in_handles = {rw_handle, ro_handle}};
    assert(nx2_gsv_session(NULL, &attach) == 0x1001u);
    uint64_t ro_size = ro_tmem.size;
    ro_tmem.size = 0x1FFF;
    gsv_args[1] = NX_SYNCPT_COUNT;
    attach = (NxRequest){.cmd = 204, .in = (uint8_t *)gsv_args, .in_size = sizeof gsv_args,
                         .nin_handles = 2, .in_handles = {rw_handle, ro_handle}};
    assert(nx2_gsv_session(NULL, &attach) == 0xE401u && attach.nout_copy == 0);
    ro_tmem.size = ro_size;
    free(msg_event);
    puts("AppletOE startup, chat GSV id/event ABI, GSV syncpoints, and video codec session ABI: PASS");
}
