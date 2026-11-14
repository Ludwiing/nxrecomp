// am (applet manager): appletOE / appletAE. Una aplicación abre su "proxy" y de ahí saca los
// objetos que dicen si tiene el foco, si la consola está en la base, los mensajes del sistema,
// la resolución, etc. Aquí la consola está siempre en la base (1080p) y con el foco.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "nxkern.h"
#include "nxgpu.h"
#include "nxsvc.h"

#define R_AM_NO_MESSAGE 0x680u      // am, 3
#define R_AM_NO_DATA 0x480u         // am (128), 2: no hay parámetro de lanzamiento
#define OK 0

static KEvent *msg_event;
static uint32_t msg_event_h;
static int initial_focus_message = 1;

enum {
    NX2_SYNCPT_VALUES_OFFSET = 0x40,
    NX2_SYNCPT_RW_SUMMARY_OFFSET = 0x0,
    NX2_SYNCPT_RW_BITS_OFFSET = 0x100,
    NX2_SYNCPT_RO_SUMMARY_OFFSET = 0x1040,
    NX2_SYNCPT_RO_BITS_OFFSET = 0x1100,
};
static uint64_t syncpt_rw_addr, syncpt_ro_addr;
static uint32_t syncpt_count;
static KEvent *syncpt_request_event, *syncpt_notify_event;
static uint32_t syncpt_request_event_h, syncpt_notify_event_h;
static pthread_mutex_t syncpt_memory_lock = PTHREAD_MUTEX_INITIALIZER;

// Called with syncpt_memory_lock held. The RW page belongs to the title, so
// use acquire/release accesses for the protocol's shared 64-bit words.
static int syncpt_ack_armed_waits_locked(void) {
    if (!syncpt_rw_addr || !syncpt_ro_addr) return 0;
    const uint64_t *rw_summary = (const uint64_t *)(uintptr_t)(syncpt_rw_addr + NX2_SYNCPT_RW_SUMMARY_OFFSET);
    uint64_t *ro_summary = (uint64_t *)(uintptr_t)(syncpt_ro_addr + NX2_SYNCPT_RO_SUMMARY_OFFSET);
    uint64_t summary = __atomic_load_n(rw_summary, __ATOMIC_ACQUIRE);
    __atomic_store_n(ro_summary, summary, __ATOMIC_RELEASE);
    uint32_t words = (syncpt_count + 63) / 64;
    int changed = 0;
    for (uint32_t w = 0; w < words; w++) {
        const uint64_t *rw = (const uint64_t *)(uintptr_t)(syncpt_rw_addr + NX2_SYNCPT_RW_BITS_OFFSET + 8ull * w);
        uint64_t *ro = (uint64_t *)(uintptr_t)(syncpt_ro_addr + NX2_SYNCPT_RO_BITS_OFFSET + 8ull * w);
        uint64_t armed = __atomic_load_n(rw, __ATOMIC_ACQUIRE);
        uint64_t acknowledged = __atomic_load_n(ro, __ATOMIC_RELAXED);
        if (armed != acknowledged) {
            __atomic_store_n(ro, armed, __ATOMIC_RELEASE);
            changed = 1;
        }
    }
    return changed;
}

void nx2_syncpt_publish(uint32_t id, uint32_t value) {
    pthread_mutex_lock(&syncpt_memory_lock);
    int changed = 0;
    if (syncpt_ro_addr && id < syncpt_count) {
        uint64_t *slot = (uint64_t *)(uintptr_t)(syncpt_ro_addr + NX2_SYNCPT_VALUES_OFFSET + 8ull * id);
        uint64_t old = __atomic_load_n(slot, __ATOMIC_RELAXED);
        if (old != value) {
            __atomic_store_n(slot, value, __ATOMIC_RELEASE);
            changed = 1;
        }
        changed |= syncpt_ack_armed_waits_locked();
    }
    KEvent *notify = syncpt_notify_event;
    pthread_mutex_unlock(&syncpt_memory_lock);
    if (changed && notify) nx_event_signal(notify);
}

// The guest signals the first event after writing its RW request bitmap. Clear
// the binary event before reading the page so a concurrent subsequent request
// remains visible and can be processed on its own callback.
void nx2_syncpt_event_signaled(KEvent *event) {
    if (!event || event != syncpt_request_event) return;
    pthread_mutex_lock(&syncpt_memory_lock);
    nx_kernel_lock();
    event->signaled = 0;
    nx_kernel_unlock();
    int changed = syncpt_ack_armed_waits_locked();
    KEvent *notify = syncpt_notify_event;
    pthread_mutex_unlock(&syncpt_memory_lock);
    if (changed && notify) nx_event_signal(notify);
}

static uint32_t common_state(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // GetEventHandle: se señala cuando hay mensajes
        if (!msg_event_h) {
            msg_event_h = nx_event_new(&msg_event);
            if (initial_focus_message) nx_event_signal(msg_event);
        }
        nx_out_copy_handle(r, msg_event_h);
        return OK;
    case 1: // ReceiveMessage: la aplicación empieza con derechos de primer plano.
        nx_kernel_lock();
        if (!initial_focus_message) {
            nx_kernel_unlock();
            return R_AM_NO_MESSAGE;
        }
        initial_focus_message = 0;
        if (msg_event) msg_event->signaled = 0;
        nx_kernel_unlock();
        nx_out32(r, 15); // FocusStateChanged; GetCurrentFocusState devuelve InFocus.
        return OK;
    case 5: nx_out8(r, nx_en_base()); return OK;  // GetOperationMode: 0 portátil, 1 en la base
    case 6: nx_out32(r, nx_en_base()); return OK; // GetPerformanceMode: van juntos
    case 8: nx_out8(r, 0); return OK;         // GetBootMode
    case 9: nx_out8(r, 1); return OK;         // GetCurrentFocusState: con el foco
    case 50: nx_out8(r, 0); return OK;        // IsVrModeEnabled
    case 60: // GetDefaultDisplayResolution
        nx_out32(r, nx_en_base() ? 1920 : 1280);
        nx_out32(r, nx_en_base() ? 1080 : 720);
        return OK;
    case 61: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    case 66: return OK;                       // SetCpuBoostMode
    case 900: return OK; // SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled
    }
    return NX_R_UNKNOWN_CMD;
}

// The Switch 2 SDK obtains this TIPC session from ISelfController. Keep its
// protocol boundary explicit: these bootstrap calls carry no host GPU work.
static uint32_t nx2_syncpt_session(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // Ping(tick): the reply echoes the caller's tick.
        nx_out64(r, nx_in64(r, 0));
        return OK;
    case 1: // GetCount: current host-side compatibility table.
        nx_out32(r, 32);
        return OK;
    case 21: // Switch 2 calls this during syncpt initialization; accept it while its payload ABI is unknown.
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t nx2_nvdrv2_user_session(ServiceObject *self, NxRequest *r) {
    // The Switch 2 TIPC interface has the same command payloads as INvDrv2User.
    return nx_nvdrv(self, r);
}

static uint32_t attach_syncpt_memory(NxRequest *r) {
    if (r->nin_handles < 2 || r->in_size < 8) return 0xE401u;
    KTmem *rw = nx_handle_get(r->in_handles[0], KO_TMEM);
    KTmem *ro = nx_handle_get(r->in_handles[1], KO_TMEM);
    if (!rw || !ro || rw->size < 0x1000 || ro->size < 0x2000) return 0xE401u;

    uint32_t version = nx_in32(r, 0);
    uint32_t requested = nx_in32(r, 4);
    if (version != 1 || requested == 0) return 0x1001u;
    uint32_t count = requested < NX_SYNCPT_COUNT ? requested : NX_SYNCPT_COUNT;
    pthread_mutex_lock(&syncpt_memory_lock);
    if (!syncpt_request_event) syncpt_request_event = calloc(1, sizeof *syncpt_request_event);
    if (!syncpt_notify_event) syncpt_notify_event = calloc(1, sizeof *syncpt_notify_event);
    if (!syncpt_request_event || !syncpt_notify_event) {
        pthread_mutex_unlock(&syncpt_memory_lock);
        return 0xE601u;
    }
    nx_kernel_lock();
    if (!syncpt_request_event_h) syncpt_request_event_h = nx_handle_new(KO_EVENT, syncpt_request_event, 1);
    if (!syncpt_notify_event_h) syncpt_notify_event_h = nx_handle_new(KO_EVENT, syncpt_notify_event, 0);
    syncpt_request_event->signaled = 0;
    syncpt_notify_event->signaled = 0;
    nx_kernel_unlock();
    if (!syncpt_request_event_h || !syncpt_notify_event_h) {
        pthread_mutex_unlock(&syncpt_memory_lock);
        return 0xE601u;
    }

    memset((void *)(uintptr_t)rw->addr, 0, 0x1000);
    memset((void *)(uintptr_t)ro->addr, 0, 0x2000);
    uint32_t header[8] = {1, 0, count, NX2_SYNCPT_VALUES_OFFSET,
                          NX2_SYNCPT_RW_SUMMARY_OFFSET, NX2_SYNCPT_RO_SUMMARY_OFFSET,
                          NX2_SYNCPT_RW_BITS_OFFSET, NX2_SYNCPT_RO_BITS_OFFSET};
    memcpy((void *)(uintptr_t)ro->addr, header, sizeof header);
    syncpt_rw_addr = rw->addr;
    syncpt_ro_addr = ro->addr;
    syncpt_count = count;
    for (uint32_t id = 0; id < count; id++) {
        uint64_t *slot = (uint64_t *)(uintptr_t)(ro->addr + NX2_SYNCPT_VALUES_OFFSET + 8ull * id);
        __atomic_store_n(slot, nx_syncpt_read(id), __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&syncpt_memory_lock);
    nx_out_copy_handle(r, syncpt_request_event_h);
    nx_out_copy_handle(r, syncpt_notify_event_h);
    return OK;
}

typedef struct {
    pthread_mutex_t lock;
    KEvent *events[32];
    uint32_t event_handles[32];
    uint8_t devices[64];
} Nx2GpuSession;

static uint32_t nx2_gpu_session(ServiceObject *self, NxRequest *r) {
    Nx2GpuSession *s = self->state;
    switch (r->cmd) {
    case 0:
        if (r->in_size < 8) return 0xE401u;
        nx_out64(r, nx_in64(r, 0));
        return OK;
    case 11: // Inclusive range of worker completion event IDs.
        nx_out32(r, 0); nx_out32(r, 31);
        return OK;
    case 12: case 21: {
        if (r->in_size < 4) return 0xE401u;
        uint32_t id = nx_in32(r, 0);
        if (id >= 32) return 0xF001u;
        pthread_mutex_lock(&s->lock);
        if (r->cmd == 12 && !s->events[id])
            s->event_handles[id] = nx_event_new(&s->events[id]);
        KEvent *event = s->events[id];
        uint32_t handle = s->event_handles[id];
        pthread_mutex_unlock(&s->lock);
        if (!event || !handle) return 0xE401u;
        if (r->cmd == 12) nx_out_copy_handle(r, handle);
        else nx_event_signal(event); // The native worker session is immediately available.
        return OK;
    }
    case 22: // Worker sessions share the parent's event IDs and device registry.
        nx_out_session(r, nx_object("nx2:gpu", nx2_gpu_session, s));
        return OK;
    case 33: // One native GPU address space.
        nx_out32(r, 1);
        return OK;
    case 41: {
        if (r->in_size < 24) return 0xE401u;
        pthread_mutex_lock(&s->lock);
        uint32_t id;
        for (id = 1; id < 64 && s->devices[id]; id++) {}
        if (id < 64) s->devices[id] = 1;
        pthread_mutex_unlock(&s->lock);
        if (id == 64) return 0xE601u;
        nx_out32(r, id);
        return OK;
    }
    case 42: {
        if (r->in_size < 4) return 0xE401u;
        uint32_t id = nx_in32(r, 0);
        if (!id || id >= 64) return 0xE401u;
        pthread_mutex_lock(&s->lock);
        int existed = s->devices[id];
        s->devices[id] = 0;
        pthread_mutex_unlock(&s->lock);
        return existed ? OK : 0xE401u;
    }
    case 201: case 203: // User partition / legacy (kind, start, size) query.
        if (r->cmd == 203) nx_out64(r, 1);
        nx_out64(r, 0x8000000ull);
        nx_out64(r, 0xC000000000ull - 0x8000000ull);
        return OK;
    case 202: // Empty server partition, represented by min > max.
        nx_out64(r, UINT64_MAX); nx_out64(r, 0);
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

// nn::codec::video (Switch 2 SDK, nn::codec::video::detail::ipc::Initialize/Finalize/Create*Service):
// GSV 901/1001 create the decoder/encoder context (no payload, result only), GSV 902 (or 905 with a u8,
// the shim's "resolution unlimited" flag) and 1002 return one moved handle: the codec's root TIPC session.
// Its command 1 (CreateVideoDecoderService / CreateVideoEncoderService) returns one moved handle, the
// VideoDecService / VideoEncService session that later carries the decoder's own commands. Those commands
// do real decode work (configure, image pools, decode); none is implemented here, so each one reaches the
// generic unknown-command path and is reported with the session's name instead of pretending to decode.
static uint32_t nx2_video_service(ServiceObject *self, NxRequest *r) {
    (void)self;
    (void)r;
    return NX_R_UNKNOWN_CMD;
}

static uint32_t nx2_video_root(ServiceObject *self, NxRequest *r) {
    switch (r->cmd) {
    case 1: // CreateVideo{De,En}coderService -> moved TIPC handle.
        nx_out_session(r, nx_object((const char *)self->state, nx2_video_service, NULL));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

typedef struct {
    pthread_mutex_t lock;
    uint32_t context_id;
    KEvent *event;
    uint32_t event_handle;
} Nx2ChatGsv;

// Session moved by GSV 1202 to nn::chat::gsv. GameChat media is unavailable
// on this host, but these bootstrap calls still need their documented ABI:
// cmd 1 returns the context id and cmd 11(id) returns its event handle.
static uint32_t nx2_chat_gsv(ServiceObject *self, NxRequest *r) {
    Nx2ChatGsv *s = self ? self->state : NULL;
    if (!s) return 0xE401u; // InvalidHandle
    switch (r->cmd) {
    case 1: // GetChatContextId; valid ids are 1..0x3ff.
        pthread_mutex_lock(&s->lock);
        nx_out32(r, s->context_id);
        pthread_mutex_unlock(&s->lock);
        return OK;
    case 11: { // GetChatContextEvent(id) -> copied event handle.
        if (r->in_size < 4) return 0xCA01u; // InvalidSize
        uint32_t id = nx_in32(r, 0);
        pthread_mutex_lock(&s->lock);
        if (id != s->context_id) {
            pthread_mutex_unlock(&s->lock);
            return 0xE401u; // InvalidHandle
        }
        if (!s->event_handle) s->event_handle = nx_event_new(&s->event);
        uint32_t owner_handle = s->event_handle;
        pthread_mutex_unlock(&s->lock);
        if (!owner_handle || !s->event) return 0xCE01u; // OutOfResource
        // HIPC copy handles are separate guest handles even though they refer
        // to the same kernel event. Keep the service's owner handle so each
        // returned copy can be closed independently by the guest.
        nx_kernel_lock();
        uint32_t handle = nx_handle_new(KO_EVENT, s->event, 0);
        nx_kernel_unlock();
        if (!handle) return 0xCE01u;
        nx_out_copy_handle(r, handle);
        return OK;
    }
    case 103: { // InitializeUserService("chat2", 1), used by nn::chat::Initialize.
        // Sports Resort's SDK sends 8-byte zero-padded chatN names and a
        // nonzero u32 version here (observed in order: chat2/1, chat7/2, chat8/3,
        // chat9/4, chat10/5). A successful reply only initializes this local
        // context; network/GameChat features remain unavailable on this host.
        if (r->in_size < 12) return 0xCA01u; // InvalidSize
        uint32_t version = nx_in32(r, 8);
        if (memcmp(r->in, "chat", 4) != 0 || version == 0) return NX_R_UNKNOWN_CMD;
        int k = 4;
        while (k < 8 && r->in[k] >= '0' && r->in[k] <= '9') k++;
        if (k == 4) return NX_R_UNKNOWN_CMD; // "chat" sin número
        while (k < 8 && !r->in[k]) k++;
        return k == 8 ? OK : NX_R_UNKNOWN_CMD;
    }
    default:
        return NX_R_UNKNOWN_CMD;
    }
}

static uint32_t nx2_gsv_session(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 991: { // CalculateDecoderImagePoolBuffer, NV12 block-linear preference.
        // SDK 20.5 sends a 64-byte VideoDecoderImagePreference followed by a u32
        // image count. The captured preference has format 1 and packs width-1
        // in bits 0..15 and height-1 in bits 16..31. Keep unknown preference
        // variants explicit instead of returning a guessed buffer size.
        if (r->in_size < 68) return 0xE401u;
        if (nx_in32(r, 0) != 0 || nx_in32(r, 4) != 1) return NX_R_UNKNOWN_CMD;
        for (uint32_t off = 12; off < 64; off += 4)
            if (nx_in32(r, off) != 0) return NX_R_UNKNOWN_CMD;

        uint32_t packed = nx_in32(r, 8);
        uint64_t width = (packed & 0xffffu) + 1ull;
        uint64_t height = (packed >> 16) + 1ull;
        uint64_t count = nx_in32(r, 64);
        if (!count) return 0xE401u;

        // Tegra block-linear NV12 uses 64-byte GOB width and a 16-GOB
        // block height (128 rows) for each plane. The chroma plane has
        // ceil(height/2) rows. Round each plane independently, then size
        // the contiguous image pool. This matches the observed 640x360 ->
        // 0x64000 and 1920x1080 -> 0x348000 responses.
        uint64_t pitch = (width + 63u) & ~63ull;
        uint64_t y_rows = (height + 127u) & ~127ull;
        uint64_t chroma_rows = ((height + 1u) / 2u + 127u) & ~127ull;
        uint64_t rows = y_rows + chroma_rows;
        if (pitch > UINT64_MAX / rows) return 0xE401u;
        uint64_t one_image = pitch * rows;
        if (one_image && count > UINT64_MAX / one_image) return 0xE401u;
        nx_out64(r, one_image * count);
        return OK;
    }
    case 901: // Create the video decoder context (nn::codec::video::detail::ipc::Initialize).
    case 903: // Finalize it (nn::codec::video::detail::ipc::Finalize).
    case 1001: // The same pair for the video encoder.
    case 1003:
    case 1201: // CreateChatContext; the SDK initializes it separately with 1202.
    case 1203: // Its Finalize, by the same numbering as 901/903 and 1001/1003. No camera/chat video on this host.
        return OK;
    case 905: // u8 resolution-unlimited flag; same reply as 902.
        if (r->in_size < 1) return 0xE401u;
        // fall through
    case 902:
        nx_out_session(r, nx_object("nx2:viddec-root", nx2_video_root, (void *)"nx2:viddec"));
        return OK;
    case 1002:
        nx_out_session(r, nx_object("nx2:videnc-root", nx2_video_root, (void *)"nx2:videnc"));
        return OK;
    case 1202: { // InitializeChatContext -> moved chat GSV session.
        Nx2ChatGsv *chat = calloc(1, sizeof *chat);
        if (!chat) return 0xCE01u; // OutOfResource
        if (pthread_mutex_init(&chat->lock, NULL) != 0) {
            free(chat);
            return 0xCE01u;
        }
        chat->context_id = 1; // local context id; no host GameChat instance exists
        ServiceObject *session = nx_object("nx2:chatgsv", nx2_chat_gsv, chat);
        if (!session) {
            pthread_mutex_destroy(&chat->lock);
            free(chat);
            return 0xCE01u;
        }
        nx_out_session(r, session);
        return OK;
    }
    case 4: // BootWithRemoteMemory; process memory is shared by this native runtime.
    case 11: // SetCurrentProcessHandle
    case 101: // CreateContext
    case 201: // CreateSyncptContext
    case 301: // CreateDisplayContext
    case 401: // CreateGpuContext
    case 402: // InitializeGpuContext
    case 1101: // GSV feature permission checks; the native process has no applet permission restrictions.
    case 1102:
        return OK;
    case 202: // InitializeSyncptContext -> TIPC session
        nx_out_session(r, nx_object("nx2:syncpt", nx2_syncpt_session, NULL));
        return OK;
    case 497: { // CreateGpuSessionWithRemoteMemory(version).
        if (r->in_size < 8 || !nx_in64(r, 0)) return 0xE401u;
        Nx2GpuSession *gpu = calloc(1, sizeof *gpu);
        if (!gpu) return 0xE601u;
        pthread_mutex_init(&gpu->lock, NULL);
        nx_out_session(r, nx_object("nx2:gpu", nx2_gpu_session, gpu));
        return OK;
    }
    case 305: // InitializeDisplayContext(version) -> raw TIPC display session.
        if (r->in_size < 8 || !nx_in64(r, 0)) return 0xE401u;
        nx_out_session(r, nx_object("nx2:vi2", nx2_vi2, NULL));
        return OK;
    case 115: // QueryGrantedFeatures(u64 requested): session permissions, not GPU capabilities.
        if (r->in_size < 8) return 0xE401u;
        nx_out64(r, nx_in64(r, 0)); // The native process has no console feature permission restrictions.
        return OK;
    case 126: // Switch 2 SDK bootstrap query observed after CreateContext; no payload.
        return OK;
    case 1501: // CreateInterModuleImageContext (GSV protocol table).
    case 1502: // InitializeInterModuleImageContext; initialization is internal to GSV.
        return OK;
    case 204: // AttachSyncptMemory(version, max syncpoints; writable and read-only TransferMemory).
        return attach_syncpt_memory(r);
    case 1402: // CreateNvDrv2UserSessionWithRemoteMemory.
        nx_out_copy_session(r, nx_object("nx2:nvsrv", nx2_nvdrv2_user_session, NULL));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t self_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: // Exit: el sistema termina la aplicación
        fflush(stdout);
        _exit(0);
    case 1: case 2: case 10: case 11: case 12: case 13: case 14: case 16: case 19:
    case 50: case 62: case 65: case 68: case 72: case 100: case 2400: case 2401:
        return OK; // avisos y permisos: se aceptan
    case 9: { // GetLibraryAppletLaunchableEvent: ya se puede (libnx lo espera antes de abrir un applet)
        KEvent *e;
        uint32_t h = nx_event_new(&e);
        nx_event_signal(e);
        nx_out_copy_handle(r, h);
        return OK;
    }
    case 2000: // Switch 2: GetGraphicsSupervisor returns a raw TIPC session.
        {
            ServiceObject *gsv = nx_object("nx2:gsv", nx2_gsv_session, NULL);
            // GSV is returned as a copied TIPC handle by the SDK wrapper.
            nx_out_copy_session(r, gsv);
        }
        return OK;
    case 40: nx_out64(r, 1); return OK;       // CreateManagedDisplayLayer
    case 44: // CreateManagedDisplaySeparableLayer: pantalla y grabación.
        nx_out64(r, 1);
        nx_out64(r, 2);
        return OK;
    case 91: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t window_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: nx_out64(r, 0x1000); return OK;   // GetAppletResourceUserId
    case 10: return OK;                       // AcquireForegroundRights
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t audio_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 3: case 4: return OK;
    case 1: case 2: { float v = 1.0f; nx_out_bytes(r, &v, 4); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t display_controller(ServiceObject *self, NxRequest *r) {
    (void)self;
    (void)r;
    return NX_R_UNKNOWN_CMD;
}

// ---------- IStorage: los datos que van y vienen entre la aplicación, el sistema y los applets ----------
typedef struct {
    uint8_t *data;
    uint64_t size;
} Storage;

static uint32_t storage_accessor(ServiceObject *self, NxRequest *r) {
    Storage *s = self->state;
    switch (r->cmd) {
    case 0: nx_out64(r, s->size); return OK; // GetSize
    case 10: case 11: { // Write(offset, buffer de entrada) / Read(offset, buffer de salida)
        uint64_t off = nx_in64(r, 0);
        NxBuffer *b = r->cmd == 10 ? nx_in_buffer(r, 0) : nx_out_buffer(r, 0);
        if (!b || off > s->size) return 0x1A2A0u; // am 503: fuera de rango
        uint64_t n = b->size < s->size - off ? b->size : s->size - off;
        if (r->cmd == 10) memcpy(s->data + off, (void *)(uintptr_t)b->addr, n);
        else memcpy((void *)(uintptr_t)b->addr, s->data + off, n);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t storage(ServiceObject *self, NxRequest *r) {
    switch (r->cmd) {
    case 0: case 1: nx_out_object(r, nx_object("IStorageAccessor", storage_accessor, self->state)); return OK; // Open
    }
    return NX_R_UNKNOWN_CMD;
}

ServiceObject *nx_storage_new(const void *data, uint64_t size) {
    Storage *s = calloc(1, sizeof *s);
    s->data = calloc(1, size ? size : 1);
    s->size = size;
    if (data) memcpy(s->data, data, size);
    return nx_object("IStorage", storage, s);
}

// ---------- applets: el sistema los abriría encima del juego; aquí terminan al momento ----------
// con la respuesta de un usuario que acepta: el selector de usuario elige al único, el teclado
// devuelve texto vacío, el de mandos deja un jugador y el de errores solo cierra.
typedef struct {
    uint32_t id;
    ServiceObject *salida;
    int empezado;
    KEvent *estado, *datos; // se avisan al empezar (terminó) y si deja datos de salida
    uint32_t estado_h, datos_h, interactivo_h;
} Applet;

static ServiceObject *salida_de_applet(uint32_t id) {
    switch (id) {
    case 0x10: { // PlayerSelect: resultado 0 y el usuario
        uint8_t o[0x18] = {0};
        nx_usuario(o + 8);
        return nx_storage_new(o, sizeof o);
    }
    case 0x11: { // Swkbd: resultado 0 (aceptado) y texto UTF-16 vacío
        uint8_t o[0x7D8] = {0};
        return nx_storage_new(o, sizeof o);
    }
    case 0x0C: { // Controller: un jugador, el 1
        uint8_t o[0xC] = {1};
        return nx_storage_new(o, sizeof o);
    }
    }
    return NULL;
}

static uint32_t applet_accessor(ServiceObject *self, NxRequest *r) {
    Applet *a = self->state;
    switch (r->cmd) {
    case 0: nx_out_copy_handle(r, a->estado_h); return OK;       // GetAppletStateChangedEvent
    case 105: nx_out_copy_handle(r, a->datos_h); return OK;      // GetPopOutDataEvent
    case 106: nx_out_copy_handle(r, a->interactivo_h); return OK; // GetPopInteractiveOutDataEvent: nunca
    case 1: nx_out8(r, (uint8_t)a->empezado); return OK; // IsCompleted
    case 10: // Start
        a->empezado = 1;
        a->salida = salida_de_applet(a->id);
        if (a->salida) nx_event_signal(a->datos);
        nx_event_signal(a->estado);
        if (!a->salida && a->id != 0x0E)
            fprintf(stderr, "[nxrt] applet 0x%x: se cierra sin respuesta\n", a->id);
        return OK;
    case 20: case 25: case 100: case 103: case 150: return OK; // RequestExit, Terminate, PushInData, ...
    case 30: return OK;                                         // GetResult
    case 101: // PopOutData
        if (!a->salida) return R_AM_NO_DATA;
        nx_out_object(r, a->salida);
        a->salida = NULL;
        return OK;
    case 104: return R_AM_NO_DATA;                             // PopInteractiveOutData
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t application_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: { // PopLaunchParameter(tipo): 2 = el usuario preseleccionado (magic, elegido, uid)
        if (nx_in32(r, 0) != 2) return R_AM_NO_DATA;
        uint8_t p[0x88] = {0};
        uint32_t magic = 0xC79497CAu, si = 1;
        memcpy(p, &magic, 4);
        memcpy(p + 4, &si, 4);
        nx_usuario(p + 8);
        nx_out_object(r, nx_storage_new(p, sizeof p));
        return OK;
    }
    case 20: nx_out64(r, 0); return OK;       // EnsureSaveData
    case 21: nx_out64(r, nx_lang_code(getenv("NX_IDIOMA") ? getenv("NX_IDIOMA") : "es-419")); return OK; // GetDesiredLanguage
    case 25: nx_out64(r, 0); return OK;       // ExtendSaveData: no hizo falta más
    case 26: nx_out64(r, 0x4000000); nx_out64(r, 0x4000000); return OK; // QuerySaveDataSize
    case 27: nx_out64(r, 0); return OK;       // CreateCacheStorage
    case 30: case 31: case 32: return OK;     // BeginBlockingHomeButton..., EndBlocking...
    case 60: case 70: case 71: case 80: case 123: case 124: return OK;
    case 320: return OK; // Switch 2 SDK compatibility probe (no input/output payload)
    case 122: nx_out32(r, (uint32_t)-1); return OK; // GetPreviousProgramIndex: no hubo otro
    case 150: case 151: return R_AM_NO_DATA;  // notificaciones e invitaciones: no hay
    case 22: case 66: case 67: case 90: case 100: case 101: case 102: return OK;
    case 23: { char v[16] = "1.0.0"; nx_out_bytes(r, v, 16); return OK; } // GetDisplayVersion
    case 40: nx_out8(r, 1); return OK;        // NotifyRunning
    case 50: { uint8_t id[16] = {0x6e, 0x78}; nx_out_bytes(r, id, 16); return OK; } // GetPseudoDeviceId
    case 210: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; } // Switch 2 operation-mode refresh event
    case 321: return OK; // Switch 2 SDK compatibility probe (no reply payload)
    case 130: case 140: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

// Switch 2's SDK asks this interface while setting up dynamic resolution.
// Scale 10 maps to level 0 (full console quality); 20 and 30 lower the game's
// target. The trailing fields are reserved and must be present in the reply.
static uint32_t graphics_scaling_parameters(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 10: // GetGraphicsScalingParameters: scale, reserved, reserved
        nx_out32(r, 10);
        nx_out32(r, 0);
        nx_out64(r, 0);
        return OK;
    case 1050: // SetGraphicsScaleChangedNotificationEnabled
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

// Switch 2 reports CPU overclock state through IPerformanceFunctions. The
// host PC does not expose the console's applet overclock switch: report it off
// and accept requests to change it as a no-op.
static uint32_t performance_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out8(r, 0); return OK; // IsCpuOverclockEnabled
    case 1: return OK;                // SetCpuOverclockEnabled
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t library_applet_creator(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0:   // CreateLibraryAppletOld(id, modo)
    case 3: { // [20.0.0+] CreateLibraryApplet(id, modo, ThreadId): el ThreadId no hace falta aquí
        Applet *a = calloc(1, sizeof *a);
        a->id = nx_in32(r, 0);
        KEvent *e;
        a->estado_h = nx_event_new(&a->estado);
        a->datos_h = nx_event_new(&a->datos);
        a->interactivo_h = nx_event_new(&e);
        nx_out_object(r, nx_object("ILibraryAppletAccessor", applet_accessor, a));
        return OK;
    }
    case 10: case 11: case 12: // CreateStorage(tamaño), CreateTransferMemoryStorage, CreateHandleStorage
        nx_out_object(r, nx_storage_new(NULL, r->cmd == 10 ? nx_in64(r, 0) : nx_in64(r, 8)));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t debug_functions(ServiceObject *self, NxRequest *r) {
    (void)self;
    (void)r;
    return NX_R_UNKNOWN_CMD;
}

static uint32_t application_proxy(ServiceObject *self, NxRequest *r) {
    (void)self;
    static const struct {
        uint32_t cmd;
        const char *name;
        uint32_t (*fn)(ServiceObject *, NxRequest *);
    } children[] = {
        {0, "ICommonStateGetter", common_state},       {1, "ISelfController", self_controller},
        {2, "IWindowController", window_controller},   {3, "IAudioController", audio_controller},
        {4, "IDisplayController", display_controller}, {11, "ILibraryAppletCreator", library_applet_creator},
        {20, "IApplicationFunctions", application_functions},
        {40, "IPerformanceFunctions", performance_functions},
        {60, "IGraphicsScalingParametersFunctions", graphics_scaling_parameters},
        {1000, "IDebugFunctions", debug_functions},
    };
    for (size_t k = 0; k < sizeof children / sizeof *children; k++)
        if (children[k].cmd == r->cmd) {
            nx_out_object(r, nx_object(children[k].name, children[k].fn, NULL));
            return OK;
        }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_am_oe(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 100: case 200: case 201: case 300: case 350: // Open*Proxy
        nx_out_object(r, nx_object("IApplicationProxy", application_proxy, NULL));
        return OK;
    case 10: // Switch 2: OpenApplicationProxy2; devuelve el mismo IApplicationProxy.
        nx_out_object(r, nx_object("IApplicationProxy", application_proxy, NULL));
        return OK;
    }
    return NX_R_UNKNOWN_CMD;
}
