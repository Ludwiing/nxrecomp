// Regresión de nn::mii::detail::IDatabaseService: respuestas CharInfo válidas y buffers acotados.
#include "../runtime/nxsvc_otros.c"
#include <assert.h>

ServiceObject *nx_object(const char *name, uint32_t (*handle)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    *o = (ServiceObject){name, handle, state};
    return o;
}
void nx_out_object(NxRequest *r, ServiceObject *o) { r->out_objects[r->nout_objects++] = o; }
void nx_out_copy_handle(NxRequest *r, uint32_t h) { (void)r; (void)h; abort(); }
void nx_out8(NxRequest *r, uint8_t v) { r->out[r->out_size++] = v; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out + r->out_size, &v, 4); r->out_size += 4; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out + r->out_size, &v, 8); r->out_size += 8; }
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n) { memcpy(r->out + r->out_size, p, n); r->out_size += n; }
uint32_t nx_in32(NxRequest *r, uint32_t off) { uint32_t v = 0; if (off + 4 <= r->in_size) memcpy(&v, r->in + off, 4); return v; }
NxBuffer *nx_in_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
NxBuffer *nx_out_buffer(NxRequest *r, int k) { return k < r->nc && r->c[k].size ? &r->c[k] : k < r->nb ? &r->b[k] : NULL; }
uint32_t nx_ldr_ro(ServiceObject *self, NxRequest *r) { (void)self; (void)r; abort(); }
uint32_t nx_shmem_new(uint64_t size, KShmem **out) { (void)size; (void)out; abort(); }
uint32_t nx_event_new(KEvent **out) { (void)out; abort(); }
void nx_event_signal(KEvent *e) { (void)e; abort(); }
void nx_vigia_tocar(uint64_t addr, uint64_t size) { (void)addr; (void)size; }
const uint64_t nx_program_id = 0;

static void check_charinfo(const MiiCharInfo *m) {
    assert(m->create_id[6] >> 4 == 4);
    assert((m->create_id[8] & 0xc0) == 0x80);
    assert(m->nickname[0] == 'n' && m->nickname[6] == 'e');
    assert(m->gender <= 1 && m->height == 64 && m->build == 64);
    assert(m->eye_color == 8 && m->mouth_color == 19);
}

int main(void) {
    ServiceObject *root = nx_servicio_otro("mii:u");
    assert(root);
    NxRequest r = {.cmd = 0};
    assert(root->handle(root, &r) == 0 && r.nout_objects == 1);
    ServiceObject *db = r.out_objects[0];

    uint32_t source = MII_SOURCE_DEFAULT;
    r = (NxRequest){.cmd = 2, .in = (uint8_t *)&source, .in_size = sizeof source};
    assert(db->handle(db, &r) == 0 && r.out_size == 4);
    uint32_t count = 0; memcpy(&count, r.out, 4); assert(count == 6);

    MiiCharInfo random_mii;
    uint32_t desc[3] = {1, 1, 2};
    r = (NxRequest){.cmd = 6, .in = (uint8_t *)desc, .in_size = sizeof desc};
    assert(db->handle(db, &r) == 0 && r.out_size == sizeof random_mii);
    memcpy(&random_mii, r.out, sizeof random_mii); check_charinfo(&random_mii); assert(random_mii.gender == 1);

    uint32_t index = 4;
    MiiCharInfo default_mii;
    r = (NxRequest){.cmd = 7, .in = (uint8_t *)&index, .in_size = sizeof index};
    assert(db->handle(db, &r) == 0 && r.out_size == sizeof default_mii);
    memcpy(&default_mii, r.out, sizeof default_mii); check_charinfo(&default_mii); assert(default_mii.favorite_color == 8);

    uint8_t records[6 * (sizeof(MiiCharInfo) + 4)] = {0};
    r = (NxRequest){.cmd = 3, .in = (uint8_t *)&source, .in_size = sizeof source,
                    .b = {{.addr = (uint64_t)(uintptr_t)records, .size = sizeof records}}, .nb = 1};
    assert(db->handle(db, &r) == 0 && r.out_size == 4);
    memcpy(&count, r.out, 4); assert(count == 6);
    for (uint32_t i = 0; i < count; i++) {
        MiiCharInfo m; memcpy(&m, records + i * (sizeof m + 4), sizeof m); check_charinfo(&m);
        uint32_t origin = 0; memcpy(&origin, records + i * (sizeof m + 4) + sizeof m, 4);
        assert(origin == MII_SOURCE_DEFAULT);
    }

    r = (NxRequest){.cmd = 99};
    assert(db->handle(db, &r) == R_COMANDO_DESCONOCIDO);
    r = (NxRequest){.cmd = 6, .in = (uint8_t *)desc, .in_size = 4};
    assert(db->handle(db, &r) == R_COMANDO_DESCONOCIDO);
    puts("mii:u: CharInfo Mii válida, seis defaults y límites de buffer: PASS");
}
