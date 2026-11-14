// fsp-srv 202 (OpenDataStorageByDataId) con un dato del sistema. Nintendo Switch Sports Resort monta
// 0x0400000000000849 al arrancar (nn::fs::QueryMountSystemDataCacheSize) y aborta con 2002-1002 si no está.
// Comprueba: sin $NX_DATOS_SISTEMA, o sin la imagen, sigue la respuesta 0x7D402; con la imagen
// $NX_DATOS_SISTEMA/<id de 16 cifras hex>.img se entrega un IStorage que lee esos bytes.
#include "../runtime/nxsvc_fs.c"
#include <assert.h>

static ServiceObject *entregado;
uint64_t nx_in64(NxRequest *r, uint32_t off) { uint64_t v; memcpy(&v, r->in + off, 8); return v; }
uint32_t nx_in32(NxRequest *r, uint32_t off) { uint32_t v; memcpy(&v, r->in + off, 4); return v; }
void nx_out64(NxRequest *r, uint64_t v) { memcpy(r->out, &v, 8); r->out_size = 8; }
void nx_out32(NxRequest *r, uint32_t v) { memcpy(r->out, &v, 4); r->out_size = 4; }
void nx_out8(NxRequest *r, uint8_t v) { r->out[0] = v; r->out_size = 1; }
NxBuffer *nx_out_buffer(NxRequest *r, int k) { (void)r; (void)k; return NULL; }
void nx_out_object(NxRequest *r, ServiceObject *o) { (void)r; entregado = o; }
ServiceObject *nx_object(const char *name, uint32_t (*fn)(ServiceObject *, NxRequest *), void *state) {
    ServiceObject *o = calloc(1, sizeof *o);
    o->name = name; o->handle = fn; o->state = state;
    return o;
}
void nx_vigia_tocar(uint64_t addr, uint64_t size) { (void)addr; (void)size; }
void nx_out_bytes(NxRequest *r, const void *p, uint32_t n) { memcpy(r->out, p, n); r->out_size = n; }
const uint64_t nx_program_id = 0, nx_romfs_size = 0;
const unsigned char nx_romfs_data[1];

static uint32_t abrir(uint64_t id) {
    uint64_t in[2] = {0, id};
    NxRequest r = {.cmd = 202, .in = (uint8_t *)in, .in_size = sizeof in};
    entregado = NULL;
    return nx_fsp(NULL, &r);
}

int main(void) {
    const uint64_t id = 0x0400000000000849ull;
    setenv("NX_TITLE_ID", "04008e001f098000", 1);
    unsetenv("NX_AOC");
    unsetenv("NX_DATOS_SISTEMA");
    assert(abrir(id) == 0x7D402u && !entregado);

    char dir[] = "/tmp/nx-datos-sistema-XXXXXX";
    assert(mkdtemp(dir));
    setenv("NX_DATOS_SISTEMA", dir, 1);
    assert(abrir(id) == 0x7D402u && !entregado);   // carpeta sin la imagen: igual que antes

    char ruta[256];
    snprintf(ruta, sizeof ruta, "%s/0400000000000849.img", dir);
    uint8_t imagen[1024];
    for (unsigned k = 0; k < sizeof imagen; k++) imagen[k] = (uint8_t)(k * 7 + 3);
    FILE *f = fopen(ruta, "wb");
    assert(f && fwrite(imagen, 1, sizeof imagen, f) == sizeof imagen);
    fclose(f);
    assert(abrir(id) == 0 && entregado && entregado->handle == storage_handle);
    Storage *s = entregado->state;
    assert(s->size == sizeof imagen);
    uint64_t in[2] = {100, 64};
    uint8_t salida[64];
    NxRequest r = {.cmd = 0, .in = (uint8_t *)in, .in_size = sizeof in, .nb = 1, .b = {{(uintptr_t)salida, sizeof salida}}};
    assert(storage_handle(entregado, &r) == 0 && !memcmp(salida, imagen + 100, sizeof salida));
    assert(abrir(0x0400000000000802ull) == 0x7D402u && !entregado); // otro dato que no está

    unlink(ruta);
    rmdir(dir);
    puts("datos del sistema por NX_DATOS_SISTEMA (fsp-srv 202): PASS");
    return 0;
}
