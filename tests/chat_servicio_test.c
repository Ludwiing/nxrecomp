// chat:u (GameChat de Switch 2): sm debe encontrarlo. nn::chat::Initialize lo abre al arrancar Nintendo Switch
// Sports Resort y aborta (2001-0114) si no existe. El comando 0 entrega la interfaz IChatUserService; sin red,
// sus comandos responden bien con ceros.
#include "../runtime/nxsvc_otros.c"
#include <assert.h>

ServiceObject *nx_object(const char *name, uint32_t (*handle)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    *o = (ServiceObject){name, handle, state};
    return o;
}

// Lo que el resto de nxsvc_otros.c enlaza; chat:u no lo usa.
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { (void)r; (void)h; abort(); }
void nx_out8(NxRequest *r, uint8_t v) { r->out[r->out_size++] = v; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n) { memcpy(r->out + r->out_size, p, n); r->out_size += n; }
uint32_t nx_in32(NxRequest *r, uint32_t off) { uint32_t v; memcpy(&v, r->in + off, 4); return v; }
NxBuffer *nx_in_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
NxBuffer *nx_out_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
uint32_t nx_ldr_ro(ServiceObject *self, NxRequest *r) { (void)self; (void)r; abort(); }
uint32_t nx_shmem_new(uint64_t size, KShmem **out) { (void)size; (void)out; abort(); }
uint32_t nx_event_new(KEvent **out) { (void)out; abort(); }
void nx_event_signal(KEvent *e) { (void)e; abort(); }
void nx_vigia_tocar(uint64_t addr, uint64_t size) { (void)addr; (void)size; }
const uint64_t nx_program_id = 0;

int main(void) {
    ServiceObject *chat = nx_servicio_otro("chat:u");
    assert(chat && !strcmp(chat->name, "chat:u"));
    NxRequest r = {.cmd = 0};
    assert(chat->handle(chat, &r) == 0 && r.nout_objects == 1);
    ServiceObject *usuario = r.out_objects[0];
    assert(usuario && !strcmp(usuario->name, "IChatUserService"));
    r = (NxRequest){.cmd = 1};
    assert(usuario->handle(usuario, &r) == 0 && r.nout_objects == 0);
    for (uint32_t k = 0; k < r.out_size; k++) assert(r.out[k] == 0);  // sin red: bien, con ceros
    assert(!nx_servicio_otro("chat:x"));
    puts("chat:u: sm lo encuentra y responde bien sin red: PASS");
    return 0;
}
