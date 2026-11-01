// Prueba de IPC: arma los mensajes CMIF a mano, igual que libnx (hipcMakeRequest +
// cmifMakeRequest), y habla con sm:, set y fsp-srv. Usa una sesión normal, una convertida
// en dominio, objetos nuevos devueltos como handle o como id de dominio, buffers B y un
// servicio que no existe. Se compara con guest/esperado/servicios.txt.
#include "mini.h"

#define SVC(n, ...) __asm__ volatile("svc #" #n : __VA_ARGS__ : "memory")

static u8 *tls(void) {
    u64 v;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(v));
    return (u8 *)v;
}
static u32 connect_port(u32 *h, const char *name) {
    register u64 x0 __asm__("x0");
    register u64 x1 __asm__("x1") = (u64)name;
    SVC(0x1f, "=r"(x0), "+r"(x1) :);
    *h = (u32)x1;
    return (u32)x0;
}
static u32 send_sync(u32 h) {
    register u64 x0 __asm__("x0") = h;
    SVC(0x21, "+r"(x0) :);
    return (u32)x0;
}
static void close_handle(u32 h) {
    register u64 x0 __asm__("x0") = h;
    SVC(0x16, "+r"(x0) :);
}

static void w32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static u32 r32(const u8 *p) {
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

typedef struct {
    int type;          // 4 pedido, 5 control
    u32 cmd;
    const void *in;
    u32 in_size;
    int send_pid;
    u32 domain_obj;    // 0: sesión normal
    int domain_close;
    void *buf;         // un buffer B (salida), opcional
    u64 buf_size;
} Req;

typedef struct {
    u32 result;
    u8 out[64];
    u32 move_handle;
    u32 out_object;
} Resp;

static u32 request(u32 session, const Req *q, Resp *rs) {
    u8 *m = tls();
    memset(m, 0, 0x100);
    int nb = q->buf ? 1 : 0;
    w32(m, (u32)q->type | (u32)nb << 24);
    u32 p = 8;
    if (q->send_pid) p += 4 + 8; // cabecera especial con pid
    if (nb) {
        u64 a = (u64)q->buf, s = q->buf_size;
        w32(m + p, (u32)s);
        w32(m + p + 4, (u32)a);
        w32(m + p + 8, (u32)((a >> 36 & 7) << 2 | (s >> 32 & 0xF) << 24 | (a >> 32 & 0xF) << 28));
        p += 12;
    }
    if (q->send_pid) w32(m + 8, 1);
    u32 data = p, raw = (p + 15) & ~15u, o = raw;
    if (q->domain_obj) {
        m[o] = q->domain_close ? 2 : 1;
        m[o + 1] = 0;
        m[o + 2] = (u8)(16 + q->in_size);
        w32(m + o + 4, q->domain_obj);
        o += 16;
    }
    memcpy(m + o, "SFCI", 4);
    w32(m + o + 8, q->cmd);
    if (q->in_size) memcpy(m + o + 16, q->in, q->in_size);
    o += 16 + q->in_size;
    u32 words = (o - data + 3) / 4 + 4;
    w32(m + 4, words | (q->send_pid ? 1u << 31 : 0));

    u32 rc = send_sync(session);
    if (rc) return rc;
    memset(rs, 0, sizeof *rs);
    u32 h1 = r32(m + 4);
    p = 8;
    if (h1 >> 31) {
        u32 sh = r32(m + p);
        p += 4;
        int ncopy = sh >> 1 & 0xF, nmove = sh >> 5 & 0xF;
        if (nmove) rs->move_handle = r32(m + p + 4 * ncopy);
        p += 4 * (ncopy + nmove);
    }
    raw = (p + 15) & ~15u;
    u32 nobj = 0;
    if (q->domain_obj && q->type == 4) {
        nobj = r32(m + raw);
        raw += 16;
    }
    rs->result = r32(m + raw + 8);
    memcpy(rs->out, m + raw + 16, sizeof rs->out);
    if (nobj) rs->out_object = r32(m + raw + 16); // los ids van después de los datos de salida (aquí, ninguno)
    return 0;
}

static u32 get_service(u32 sm, const char *name, u32 *out) {
    char n[8] = {0};
    for (int i = 0; i < 8 && name[i]; i++) n[i] = name[i];
    Req q = {.type = 4, .cmd = 1, .in = n, .in_size = 8};
    Resp rs;
    request(sm, &q, &rs);
    *out = rs.move_handle;
    return rs.result;
}

static void lang_str(u64 v, char *s) {
    memcpy(s, &v, 8);
    s[8] = 0;
}

static u8 romfs_buf[64];

int nx_main(void) {
    pr("IPC de Horizon: sm:, set y fsp-srv con mensajes CMIF.\n");
    u32 sm;
    pr("ConnectToNamedPort(\"sm:\"): 0x%x\n", connect_port(&sm, "sm:"));
    u64 pid = 0;
    Req reg = {.type = 4, .cmd = 0, .in = &pid, .in_size = 8, .send_pid = 1};
    Resp rs;
    request(sm, &reg, &rs);
    pr("sm RegisterClient: 0x%x\n", rs.result);

    u32 set;
    pr("GetService(\"set\"): 0x%x, handle válido: %s\n", get_service(sm, "set", &set), set ? "sí" : "no");
    Req q = {.type = 4, .cmd = 0};
    request(set, &q, &rs);
    u64 code;
    memcpy(&code, rs.out, 8);
    char s[9];
    lang_str(code, s);
    pr("idioma del sistema: %s\n", s);
    q.cmd = 3;
    request(set, &q, &rs);
    pr("idiomas disponibles: %u\n", r32(rs.out));
    u64 codes[20];
    Req ql = {.type = 4, .cmd = 1, .buf = codes, .buf_size = sizeof codes};
    request(set, &ql, &rs);
    pr("lista (buffer B):");
    for (u32 i = 0; i < r32(rs.out) && i < 20; i++) {
        lang_str(codes[i], s);
        pr(" %s", s);
    }
    pr("\n");

    // fsp-srv en una sesión convertida en dominio
    u32 fs;
    get_service(sm, "fsp-srv", &fs);
    Req conv = {.type = 5, .cmd = 0};
    request(fs, &conv, &rs);
    u32 fs_obj = r32(rs.out);
    pr("fsp-srv convertido en dominio: objeto %u\n", fs_obj);
    Req setp = {.type = 4, .cmd = 1, .in = &pid, .in_size = 8, .domain_obj = fs_obj, .send_pid = 1};
    request(fs, &setp, &rs);
    pr("SetCurrentProcess: 0x%x\n", rs.result);
    Req open = {.type = 4, .cmd = 200, .domain_obj = fs_obj};
    request(fs, &open, &rs);
    u32 storage = rs.out_object;
    pr("OpenDataStorageByCurrentProcess: 0x%x, objeto %u\n", rs.result, storage);
    Req size = {.type = 4, .cmd = 4, .domain_obj = storage};
    request(fs, &size, &rs);
    u64 sz;
    memcpy(&sz, rs.out, 8);
    pr("tamaño del RomFS: %U bytes\n", sz);
    u64 args[2] = {1000, sizeof romfs_buf};
    Req rd = {.type = 4, .cmd = 0, .in = args, .in_size = 16, .domain_obj = storage, .buf = romfs_buf, .buf_size = sizeof romfs_buf};
    request(fs, &rd, &rs);
    u32 sum = 0;
    for (u32 i = 0; i < sizeof romfs_buf; i++) sum = sum * 31 + romfs_buf[i];
    pr("Read(1000, 64): 0x%x, primeros bytes %u %u %u, hash 0x%x\n", rs.result, romfs_buf[0], romfs_buf[1], romfs_buf[2],
       sum);
    Req cls = {.type = 4, .cmd = 0, .domain_obj = storage, .domain_close = 1};
    request(fs, &cls, &rs);
    request(fs, &size, &rs);
    pr("después de cerrar el objeto %u, pedirle algo falla: %s\n", storage, rs.result ? "sí" : "no");

    // y en una sesión normal, el objeto nuevo llega como handle
    u32 fs2, st2;
    get_service(sm, "fsp-srv", &fs2);
    Req open2 = {.type = 4, .cmd = 200};
    request(fs2, &open2, &rs);
    st2 = rs.move_handle;
    Req size2 = {.type = 4, .cmd = 4};
    request(st2, &size2, &rs);
    memcpy(&sz, rs.out, 8);
    pr("sin dominio, IStorage llega como handle: %s, tamaño %U\n", st2 ? "sí" : "no", sz);
    close_handle(st2);
    close_handle(fs2);

    u32 none;
    pr("GetService(\"nada\"): 0x%x\n", get_service(sm, "nada", &none));
    pr("Fin.\n");
    flush();
    return 0;
}
