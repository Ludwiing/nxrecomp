// El renderizador de audio con las revisiones del SDK de Nintendo que libnx no habla (REV11 y REV15):
// se abre audren:u por IPC directo y los buffers de RequestUpdate se arman a mano, con el formato de
// cada revisión: voces de 0x170 o 0x188 bytes, efectos de la versión 2, splitters con destinos de
// 0x70 o 0xA8, mezclas que solo mandan las que cambiaron, cuadros hechos en la salida.
// Dos voces PCM de 16 bits: la 0 va directo a la mezcla final; la 1 pasa por un biquad (ganancia 0,5)
// y por un splitter, a una submezcla y a la final. Un efecto AUX en la mezcla final devuelve al
// programa lo que se mezcló, y se compara muestra por muestra con lo calculado aquí.
// Salida en guest/esperado/audren_rev.txt.
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#define REV(n) (0x30564552u + ((u32)(n) << 24))
#define LARGO 4800              // muestras de cada voz (20 cuadros)
#define AUXN (480 * 256)        // capacidad del buffer circular del AUX (en muestras de 32 bits)
#define SIN_MEZCLA 0x7FFFFFFF
#define printf(...) fprintf(stderr, __VA_ARGS__) // consoleDebugInit: stderr va al runtime

typedef struct {
    u32 rate, samples, mix_buffers, submixes, voices, sinks, effects, perf;
    u8 drop, pad, device, mode;
    u32 splitters, dests, ext, revision;
} Param;

static u8 entrada[0x4000] __attribute__((aligned(0x1000)));
static u8 salida[0x1000] __attribute__((aligned(0x40)));
static u8 rendimiento[0x1000] __attribute__((aligned(0x40)));
static u8 *pool;                // memoria del juego para el renderizador (una sola memoria adjunta)
static s16 *ma, *mb;            // muestras de las voces
static u8 *aux_send, *aux_ret;  // buffers del AUX: dos AuxiliaryBufferInfo (0x40 c/u) y las muestras
#define POOL_SIZE ((0x5000 + 2 * (0x80 + 4 * AUXN) + 0xFFF) & ~0xFFF)

static u8 *p8;
static void w32(u32 v) { memcpy(p8, &v, 4); p8 += 4; }
static void w64(u64 v) { memcpy(p8, &v, 8); p8 += 8; }
static void wz(u32 n) { memset(p8, 0, n); p8 += n; }
static void put32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static void putf(u8 *p, float v) { memcpy(p, &v, 4); }
static u32 get32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static u64 get64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }

// Un RequestUpdate completo. primero: voces, efecto y mezclas nuevas.
static u32 armar(int rev, int primero) {
    int v2 = rev >= 15, bq = rev >= 12;
    u32 vsize = v2 ? 0x188 : 0x170, d = v2 ? 0x18 : 0, dsize = bq ? (v2 ? 0xA8 : 0x90) : 0x70;
    u32 split = (0x20 + 0x24 + 2 * dsize + 15) & ~15u, nmix = primero ? 2 : 0;
    memset(entrada, 0, sizeof entrada);
    p8 = entrada;
    // cabecera: tamaños de cada parte
    u32 sizes[] = {0x10, 9 * 0x20, 2 * vsize, 2 * 0x70, 0xC0, 0x20 + nmix * 0x930, 0x140, 0x10, split};
    w32(REV(rev));
    for (int i = 0; i < 9; i++) w32(sizes[i]);
    wz(4 * 5);
    u32 total = 0x40;
    for (int i = 0; i < 9; i++) total += sizes[i];
    w32(total);
    // comportamiento
    w32(REV(rev)), w32(0), w64(0);
    // memorias: la 0 con todo, adjuntada en la primera actualización
    for (int i = 0; i < 9; i++) {
        u8 *m = p8;
        wz(0x20);
        if (i == 0) memcpy(m, &pool, 8), put32(m + 8, POOL_SIZE), put32(m + 0x10, primero ? 4 : 5);
    }
    // recursos de canal: el 0 (voz 0) manda 1,0 a la izquierda y 0,25 a la derecha
    for (int i = 0; i < 2; i++) {
        u8 *c = p8;
        wz(0x70);
        put32(c, i);
        if (i == 0) putf(c + 4, 1.0f), putf(c + 8, 0.25f);
        c[0x64] = 1;
    }
    // voces
    for (int i = 0; i < 2; i++) {
        u8 *v = p8;
        wz(vsize);
        put32(v, i), put32(v + 4, 0x10000 + i);
        v[8] = primero, v[9] = 1, v[0xA] = 0, v[0xB] = 2; // nueva, en uso, iniciada, PCM de 16 bits
        put32(v + 0xC, 48000), put32(v + 0x10, 0), put32(v + 0x14, i), put32(v + 0x18, 1);
        putf(v + 0x1C, 1.0f), putf(v + 0x20, i ? 1.0f : 0.5f);
        if (i == 1) { // biquad 0: y = 0,5 x (Q14 antes de REV15, float desde REV15)
            v[0x24] = 1;
            if (v2) putf(v + 0x28, 0.5f);
            else v[0x26] = 0x00, v[0x27] = 0x20;
        }
        put32(v + 0x3C + d, 1), put32(v + 0x40 + d, 0);
        put32(v + 0x58 + d, i ? SIN_MEZCLA : 0), put32(v + 0x5C + d, i ? 0 : 0xFFFFFFFF);
        u8 *w = v + 0x60 + d;
        u64 a = (u64)(i ? mb : ma);
        memcpy(w, &a, 8), put32(w + 8, LARGO * 2), put32(w + 0x14, LARGO);
        w[0x1A] = !primero;
        for (int k = 1; k < 4; k++) w[k * 0x38 + 0x1A] = 1; // los otros tres, sin usar, cuentan como ya enviados
        put32(v + 0x140 + d, i);
    }
    // efecto AUX en la mezcla final (EffectInParameterVersion2)
    u8 *e = p8;
    wz(0xC0);
    e[0] = 2, e[1] = primero, e[2] = 1;
    put32(e + 4, 0), put32(e + 0x18, 0);
    u8 *a = e + 0x20;
    a[0] = 0, a[1] = 1, a[0x18] = 0, a[0x19] = 1;
    put32(a + 0x30, 2), put32(a + 0x34, 48000), put32(a + 0x38, AUXN), put32(a + 0x3C, 2);
    u64 s = (u64)aux_send, r = (u64)aux_ret, s2 = s + 0x80, r2 = r + 0x80;
    memcpy(a + 0x40, &s, 8), memcpy(a + 0x48, &s2, 8), memcpy(a + 0x50, &r, 8), memcpy(a + 0x58, &r2, 8);
    put32(a + 0x60, 240), put32(a + 0x68, 240);
    // splitter 0 con dos destinos: el 0 a la submezcla 1 (0,5 a la izquierda), el 1 a la final (derecha)
    u8 *sp = p8;
    wz(split);
    put32(sp, 0x48444E53), put32(sp + 4, 1), put32(sp + 8, 2);
    put32(sp + 0x20, 0x49444E53), put32(sp + 0x24, 0), put32(sp + 0x28, 48000), put32(sp + 0x2C, 2);
    put32(sp + 0x30, 0), put32(sp + 0x34, 1);
    for (int k = 0; k < 2; k++) {
        u8 *q = sp + 0x44 + k * dsize;
        u32 tail = 0x6C + (bq ? (v2 ? 0x30 : 0x18) : 0);
        put32(q, 0x44444E53), put32(q + 4, k);
        putf(q + 8 + 4 * k, k ? 1.0f : 0.5f);
        put32(q + 0x68, k ? 0 : 1);
        q[tail] = 1, q[tail + 1] = primero;
    }
    // mezclas: solo las que cambiaron (desde REV7)
    w32(0x4D495844), w32(nmix), wz(0x18);
    for (u32 m = 0; m < nmix; m++) {
        u8 *x = p8;
        wz(0x930);
        putf(x, 1.0f), put32(x + 4, 48000), put32(x + 8, 2), x[0xC] = 1, x[0xD] = 1;
        put32(x + 0x10, m), put32(x + 0x14, m ? 0 : 1), put32(x + 0x18, 0x20000 + m);
        if (m) putf(x + 0x24, 1.0f), putf(x + 0x24 + 4 * 25, 1.0f);
        put32(x + 0x924, m ? 0 : SIN_MEZCLA), put32(x + 0x928, 0xFFFFFFFF);
    }
    // sink de dispositivo estéreo
    u8 *k = p8;
    wz(0x140);
    k[0] = 1, k[1] = 1, k[2] = 0xFE, k[3] = 0xCA;
    strcpy((char *)k + 0x20, "MainAudioOut");
    put32(k + 0x120, 2), k[0x124] = 0, k[0x125] = 1;
    // rendimiento
    wz(0x10);
    return (u32)(p8 - entrada);
}

static Result actualizar(Service *ren, u32 size) {
    return serviceDispatch(ren, 10,
        .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_Out, SfBufferAttr_HipcAutoSelect | SfBufferAttr_Out,
                         SfBufferAttr_HipcAutoSelect | SfBufferAttr_In},
        .buffers = {{salida, sizeof salida}, {rendimiento, sizeof rendimiento}, {entrada, size}}, );
}

static u32 cmd_u32(Service *s, u32 id) {
    u32 v = 0;
    serviceDispatchOut(s, id, v);
    return v;
}

// Lo que tiene que salir de la mezcla final, muestra n: con la latencia de 3 muestras del remuestreador.
// Las últimas 3 quedan en su historia y no salen: sin buffers, la voz ya no genera comandos (como en Ryujinx).
static void esperado(int n, s32 *l, s32 *r) {
    int k = n - 3;
    float a = k >= 0 && n < LARGO ? ma[k] : 0, b = k >= 0 && n < LARGO ? mb[k] : 0;
    float v0 = rintf(a * 0.5f), v1 = rintf(b * 0.5f), sub = rintf(v1 * 0.5f);
    *l = (s32)(v0 + sub);
    *r = (s32)(rintf(v0 * 0.25f) + v1);
}

static void probar(Service *mgr, int rev) {
    printf("== REV%d\n", rev);
    Param p = {48000, 240, 4, 1, 2, 1, 1, 2, 0, 0, 0, 0, 1, 2, 0, REV(rev)};
    u64 size = 0;
    Result rc = serviceDispatchInOut(mgr, 1, p, size);
    printf("GetWorkBufferSize: 0x%x, 0x%lx bytes\n", rc, size);
    void *work = memalign(0x1000, size);
    TransferMemory tm;
    tmemCreateFromMemory(&tm, work, size, Perm_None);
    const struct { Param p; u32 pad; u64 size; u64 aruid; } in = {p, 0, size, appletGetAppletResourceUserId()};
    Service ren;
    rc = serviceDispatchIn(mgr, 0, in, .in_send_pid = true, .in_num_handles = 2, .in_handles = {tm.handle, CUR_PROCESS_HANDLE},
                           .out_num_objects = 1, .out_objects = &ren);
    printf("OpenAudioRenderer: 0x%x\n", rc);
    if (R_FAILED(rc)) return;
    printf("frecuencia %u, muestras %u, buffers %u, estado %u\n", cmd_u32(&ren, 0), cmd_u32(&ren, 1), cmd_u32(&ren, 2), cmd_u32(&ren, 3));
    Handle h = 0;
    serviceDispatch(&ren, 7, .out_handle_attrs = {SfOutHandleAttr_HipcCopy}, .out_handles = &h);
    Event ev;
    eventLoadRemote(&ev, h, true);

    memset(aux_send, 0, 0x80 + 4 * AUXN);
    memset(aux_ret, 0, 0x80 + 4 * AUXN);
    for (int i = 0; i < AUXN; i++) put32(aux_ret + 0x80 + 4 * i, (u32)(i % 1000 - 500)); // lo que vuelve del AUX

    // una revisión que no es la del renderizador: se rechaza
    u32 n = armar(rev, 1);
    put32(entrada + 0x40, REV(rev == 11 ? 10 : 14));
    printf("revisión equivocada: 0x%x\n", actualizar(&ren, n));

    n = armar(rev, 1);
    rc = actualizar(&ren, n);
    printf("primera actualización: 0x%x, %u bytes de entrada\n", rc, n);
    u8 *o = salida;
    printf("salida: revisión %s, comportamiento 0x%x, memorias 0x%x, voces 0x%x, efectos 0x%x, sinks 0x%x, rendimiento 0x%x, cuadros 0x%x, total 0x%x\n",
           get32(o) == REV(rev) ? "igual" : "otra", get32(o + 4), get32(o + 8), get32(o + 0xC), get32(o + 0x14), get32(o + 0x1C), get32(o + 0x20),
           get32(o + 0x28), get32(o + 0x3C));
    u8 *q = o + 0x40;
    printf("memorias: %u %u, voces nuevas: %lu/%u %lu/%u\n", get32(q), get32(q + 0x10), get64(q + 0x90), get32(q + 0x98), get64(q + 0xA0), get32(q + 0xA8));
    q += 0x90 + 0x20;
    printf("AUX antes de empezar: estado %u\n", q[0]);
    u8 *fin = o + get32(o + 0x3C) - 0x10 - 0xB0;
    printf("errores: %u\n", get32(fin + 0xA0));

    rc = serviceDispatch(&ren, 5);
    printf("Start: 0x%x, estado %u\n", rc, cmd_u32(&ren, 3));
    n = armar(rev, 0);
    u64 played[2] = {0};
    u32 consumed[2] = {0}, vueltas = 0, aux_estado = 0, hist = 0, despues = 0;
    while (vueltas++ < 400) { // hasta que las dos voces terminan, y dos cuadros más (la cola del remuestreador)
        eventWait(&ev, 1000000000ull);
        rc = actualizar(&ren, n);
        if (R_FAILED(rc)) { printf("actualización: 0x%x\n", rc); break; }
        q = salida + 0x40 + 0x90;
        for (int i = 0; i < 2; i++) played[i] = get64(q + 0x10 * i), consumed[i] = get32(q + 0x10 * i + 8);
        aux_estado = q[0x20];
        hist += get32(salida + 0x40 + 0x90 + 0x20 + 0x90 + 0x20);
        if (consumed[0] && consumed[1] && despues++ == 2) break;
    }
    serviceDispatch(&ren, 6);
    u64 cuadros = get64(salida + get32(salida + 0x3C) - 0x10);
    printf("voces: %lu muestras y %u buffer(es), %lu muestras y %u buffer(es)\n", played[0], consumed[0], played[1], consumed[1]);
    printf("AUX andando: estado %u, cuadros hechos: %s, historial de rendimiento: %s (%s)\n", aux_estado, cuadros >= 22 ? "22 o más" : "pocos",
           hist ? "sí" : "no", !memcmp(rendimiento, "PERF", 4) ? "PERF" : "?");
    // lo que mandó el AUX: cuadro tras cuadro, 240 muestras de la izquierda y 240 de la derecha
    u32 wr = get32(aux_send + 0x44), rd = get32(aux_ret + 0x40), malas = 0, primera = 0xFFFFFFFF;
    u32 nc = wr / 480;
    for (u32 f = 0; f < nc; f++)
        for (int j = 0; j < 240; j++) {
            s32 l, r, gl, gr;
            esperado((int)(f * 240 + j), &l, &r);
            memcpy(&gl, aux_send + 0x80 + 4 * (f * 480 + j), 4);
            memcpy(&gr, aux_send + 0x80 + 4 * (f * 480 + 240 + j), 4);
            if (gl != l || gr != r) {
                if (primera == 0xFFFFFFFF) primera = f * 240 + j;
                malas++;
            }
        }
    printf("AUX: %s cuadros enviados, punteros alineados: %s, %u muestras distintas", nc >= 22 ? "22 o más" : "pocos", wr % 480 == 0 && rd == wr ? "sí" : "no", malas);
    if (malas) printf(" (la primera en %u)", primera);
    printf("\n");
    tmemClose(&tm);
    serviceClose(&ren);
    free(work);
}

static void dispositivo(Service *mgr) {
    u64 aruid = appletGetAppletResourceUserId();
    const struct { u32 rev, pad; u64 aruid; } in = {REV(11), 0, aruid};
    Service dev = {0};
    Result rc = serviceDispatchIn(mgr, 4, in, .out_num_objects = 1, .out_objects = &dev);
    printf("== IAudioDevice: 0x%x\n", rc);
    static char nombres[8][0x100];
    u32 n = 0;
    serviceDispatchOut(&dev, 6, n, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_Out}, .buffers = {{nombres, sizeof nombres}});
    printf("%u dispositivos:", n);
    for (u32 i = 0; i < n; i++) printf(" %s", nombres[i]);
    char activo[0x100] = {0};
    serviceDispatch(&dev, 10, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_Out}, .buffers = {{activo, sizeof activo}});
    printf("\nactivo: %s, canales: %u\n", activo, cmd_u32(&dev, 5));
    float vol = 0.25f, leido = 0;
    rc = serviceDispatchIn(&dev, 7, vol, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_In}, .buffers = {{activo, sizeof activo}});
    Result rc2 = serviceDispatchOut(&dev, 8, leido, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_In}, .buffers = {{activo, sizeof activo}});
    printf("volumen: 0x%x 0x%x, %.2f\n", rc, rc2, leido);
    vol = 1.0f;
    serviceDispatchIn(&dev, 7, vol, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_In}, .buffers = {{activo, sizeof activo}});
    rc = serviceDispatchOut(&dev, 8, leido, .buffer_attrs = {SfBufferAttr_HipcAutoSelect | SfBufferAttr_In}, .buffers = {{"NoExiste", 9}});
    printf("dispositivo desconocido: 0x%x\n", rc);
    Handle h = 0;
    serviceDispatch(&dev, 4, .out_handle_attrs = {SfOutHandleAttr_HipcCopy}, .out_handles = &h);
    Event e;
    eventLoadRemote(&e, h, false);
    printf("evento del sistema señalado: %s\n", R_SUCCEEDED(eventWait(&e, 0)) ? "sí" : "no");
    serviceClose(&dev);
}

int main(int argc, char **argv) {
    consoleDebugInit(debugDevice_SVC);
    pool = memalign(0x1000, POOL_SIZE);
    memset(pool, 0, POOL_SIZE);
    ma = (s16 *)pool, mb = (s16 *)(pool + 0x2800);
    aux_send = pool + 0x5000, aux_ret = aux_send + 0x80 + 4 * AUXN;
    for (int i = 0; i < LARGO; i++) ma[i] = (s16)((i * 37) % 2001 - 1000), mb[i] = (s16)((i * 53) % 4001 - 2000);
    Service mgr;
    Result rc = smGetService(&mgr, "audren:u");
    if (R_FAILED(rc)) {
        printf("audren:u: 0x%x\n", rc);
        return 0;
    }
    Param malo = {48000, 240, 4, 1, 2, 1, 1, 2, 0, 0, 0, 0, 1, 2, 0, 0x12345678};
    u64 size = 0;
    printf("GetWorkBufferSize con una revisión inválida: 0x%x\n", serviceDispatchInOut(&mgr, 1, malo, size));
    probar(&mgr, 11);
    probar(&mgr, 15);
    dispositivo(&mgr);
    serviceClose(&mgr);
    return 0;
}
