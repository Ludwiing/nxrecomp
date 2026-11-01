// La GPU de la Switch (Tegra X1, Maxwell GM20B), primera etapa: el procesador de comandos.
//   - Espacio de direcciones de la GPU: direcciones de GPU → memoria del programa (nvhost-as-gpu).
//   - Syncpoints: los contadores que hacen de cercas; las esperas se bloquean hasta el valor.
//   - GPFIFO: cada entrada apunta a un pushbuffer; se ejecuta en el momento del envío.
//   - Pushbuffer: cabeceras de método (creciente, no creciente, en línea, crece una vez) hacia el
//     objeto de cada subcanal: 3D (0xB197), cómputo (0xB1C0), carga en línea (0xA140), 2D, copia.
//   - Motor de macros (MME): el intérprete de su conjunto de instrucciones.
//   - 3D: registros, SyncptAction, semáforos de reporte, carga de constbufs y ClearBuffers.
// Todavía no dibuja triángulos: eso pide traducir los shaders de Maxwell (siguiente etapa).
// Nombres y offsets de registros: deko3d (maxwell/engine_*.def) y envytools.
#define _GNU_SOURCE
#include "nxgpu.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxgl.h"
#include "nxrt.h"

// ---------- espacio de direcciones ----------
typedef struct {
    uint64_t gpu, size, cpu;
} Mapping;
static Mapping maps[4096];
static int nmaps;
static uint64_t va_next = 0x100000000ull; // asignación de direcciones de GPU (por encima de 4 GiB)
static pthread_mutex_t as_lock = PTHREAD_MUTEX_INITIALIZER;

uint64_t nx_gpu_alloc_va(uint64_t size, uint64_t align) {
    pthread_mutex_lock(&as_lock);
    if (align < 0x20000) align = 0x20000;
    uint64_t va = (va_next + align - 1) & ~(align - 1);
    va_next = va + ((size + 0x1FFFF) & ~0x1FFFFull);
    pthread_mutex_unlock(&as_lock);
    return va;
}

void nx_gpu_map(uint64_t gpu, uint64_t size, uint64_t cpu) {
    pthread_mutex_lock(&as_lock);
    for (int k = 0; k < nmaps; k++)
        if (maps[k].gpu == gpu) {
            maps[k] = (Mapping){gpu, size, cpu};
            pthread_mutex_unlock(&as_lock);
            return;
        }
    if (nmaps < 4096) maps[nmaps++] = (Mapping){gpu, size, cpu};
    if (gpu + size > va_next) va_next = gpu + size;
    pthread_mutex_unlock(&as_lock);
}

void nx_gpu_unmap(uint64_t gpu) {
    pthread_mutex_lock(&as_lock);
    for (int k = 0; k < nmaps; k++)
        if (maps[k].gpu == gpu) {
            maps[k] = maps[--nmaps];
            break;
        }
    pthread_mutex_unlock(&as_lock);
}

void *nx_gpu_ptr(uint64_t gpu) {
    pthread_mutex_lock(&as_lock);
    for (int k = 0; k < nmaps; k++)
        if (gpu >= maps[k].gpu && gpu < maps[k].gpu + maps[k].size) {
            void *p = (void *)(uintptr_t)(maps[k].cpu + (gpu - maps[k].gpu));
            pthread_mutex_unlock(&as_lock);
            return p;
        }
    pthread_mutex_unlock(&as_lock);
    return NULL;
}

void *nx_gpu_ptr_n(uint64_t gpu, uint64_t *avail) {
    pthread_mutex_lock(&as_lock);
    for (int k = 0; k < nmaps; k++)
        if (gpu >= maps[k].gpu && gpu < maps[k].gpu + maps[k].size) {
            void *p = (void *)(uintptr_t)(maps[k].cpu + (gpu - maps[k].gpu));
            *avail = maps[k].gpu + maps[k].size - gpu;
            pthread_mutex_unlock(&as_lock);
            return p;
        }
    pthread_mutex_unlock(&as_lock);
    *avail = 0;
    return NULL;
}

// ---------- syncpoints ----------
#define NSYNCPT 192
static uint32_t syncpts[NSYNCPT];
static pthread_mutex_t sp_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sp_cond = PTHREAD_COND_INITIALIZER;

uint32_t nx_syncpt_read(uint32_t id) { return id < NSYNCPT ? __atomic_load_n(&syncpts[id], __ATOMIC_ACQUIRE) : 0; }

void nx_syncpt_incr(uint32_t id) {
    if (id >= NSYNCPT) return;
    pthread_mutex_lock(&sp_lock);
    __atomic_add_fetch(&syncpts[id], 1, __ATOMIC_RELEASE);
    pthread_cond_broadcast(&sp_cond);
    pthread_mutex_unlock(&sp_lock);
}

int nx_syncpt_wait(uint32_t id, uint32_t threshold, int32_t timeout_us) {
    if (id >= NSYNCPT) return 0;
    pthread_mutex_lock(&sp_lock);
    struct timespec dl;
    clock_gettime(CLOCK_REALTIME, &dl);
    int64_t us = timeout_us < 0 ? 2000000 : timeout_us; // "para siempre" se acota para no colgar el juego
    dl.tv_sec += us / 1000000;
    dl.tv_nsec += (us % 1000000) * 1000;
    if (dl.tv_nsec >= 1000000000) dl.tv_sec++, dl.tv_nsec -= 1000000000;
    int ok = 1;
    while ((int32_t)(syncpts[id] - threshold) < 0)
        if (pthread_cond_timedwait(&sp_cond, &sp_lock, &dl) == ETIMEDOUT) {
            ok = (int32_t)(syncpts[id] - threshold) >= 0;
            if (!ok && timeout_us < 0)
                fprintf(stderr, "[RED] GPU: el syncpoint %u no llegó a %u (está en %u)\n", id, threshold, syncpts[id]);
            break;
        }
    pthread_mutex_unlock(&sp_lock);
    return ok;
}

// ---------- estado de la GPU ----------
#define CLASS_3D 0xB197
#define CLASS_COMPUTE 0xB1C0
#define CLASS_I2M 0xA140
#define CLASS_2D 0x902D
#define CLASS_DMA 0xB0B5
#define REGS 0x1000

typedef struct {
    uint32_t regs[REGS];
    uint32_t mme_code[0x2000], mme_ptr, mme_start[0x80], mme_start_ptr;
    uint32_t macro_params[0x1000], nmacro_params;
    int macro_pending, macro_index;
    uint64_t i2m_dst;
    uint32_t i2m_len, i2m_done;
    uint64_t cb_addr[5][18]; // constbufs enlazados por etapa (Bind[etapa].Constbuf)
    uint32_t cb_size[5][18];
    uint32_t instance;       // índice de instancia del draw en curso
    int indexed;
    NxDraw pend;             // draw pendiente: las instancias seguidas del mismo draw se juntan
    int pending;
} Engine3D;

static Engine3D e3d;
static uint32_t subch_class[8];
static pthread_mutex_t gpu_lock = PTHREAD_MUTEX_INITIALIZER;

static uint64_t iova(const uint32_t *regs, uint32_t m) { return (uint64_t)regs[m] << 32 | regs[m + 1]; }

static void method_3d(uint32_t m, uint32_t v);

// ---------- motor de macros (MME) ----------
typedef struct {
    uint32_t r[8], method, inc;
    int carry;
    const uint32_t *params;
    uint32_t nparams, next;
} Mme;

static uint32_t mme_fetch(Mme *s) { return s->next < s->nparams ? s->params[s->next++] : 0; }
static void mme_set(Mme *s, uint32_t reg, uint32_t v) {
    if (reg) s->r[reg] = v;
}
static void mme_send(Mme *s, uint32_t v) {
    method_3d(s->method, v);
    s->method += s->inc;
}
static void mme_set_method(Mme *s, uint32_t v) {
    s->method = v & 0xFFF;
    s->inc = v >> 12 & 0x3F;
}

static void mme_result(Mme *s, uint32_t op, uint32_t dst, uint32_t res) {
    switch (op) {
    case 0: mme_set(s, dst, mme_fetch(s)); break;                              // IgnoreAndFetch
    case 1: mme_set(s, dst, res); break;                                       // Move
    case 2: mme_set(s, dst, res); mme_set_method(s, res); break;               // MoveAndSetMethod
    case 3: mme_set(s, dst, mme_fetch(s)); mme_send(s, res); break;            // FetchAndSend
    case 4: mme_set(s, dst, res); mme_send(s, res); break;                     // MoveAndSend
    case 5: mme_set(s, dst, mme_fetch(s)); mme_set_method(s, res); break;      // FetchAndSetMethod
    case 6: mme_set(s, dst, res); mme_set_method(s, res); mme_send(s, mme_fetch(s)); break; // MoveAndSetMethodFetchAndSend
    case 7: mme_set(s, dst, res); mme_set_method(s, res); mme_send(s, res >> 12 & 0x3F); break; // MoveAndSetMethodSend
    }
}

static uint32_t mme_alu(Mme *s, uint32_t op, uint32_t a, uint32_t b) {
    uint64_t t;
    switch (op) {
    case 0: t = (uint64_t)a + b; s->carry = t >> 32 != 0; return (uint32_t)t;
    case 1: t = (uint64_t)a + b + (uint32_t)s->carry; s->carry = t >> 32 != 0; return (uint32_t)t;
    case 2: t = (uint64_t)a - b; s->carry = t < 0x100000000ull; return (uint32_t)t;
    case 3: t = (uint64_t)a - b - (uint32_t)!s->carry; s->carry = t < 0x100000000ull; return (uint32_t)t;
    case 8: return a ^ b;
    case 9: return a | b;
    case 10: return a & b;
    case 11: return a & ~b;
    case 12: return ~(a & b);
    }
    fprintf(stderr, "[RED] MME: operación de ALU %u\n", op);
    return 0;
}

static void mme_run(uint32_t start, const uint32_t *params, uint32_t nparams) {
    Mme s = {0};
    s.params = params;
    s.nparams = nparams;
    s.next = 1;
    s.r[1] = nparams ? params[0] : 0;
    uint32_t pc = start, delayed = 0xFFFFFFFF;
    int exiting = 0;
    for (int steps = 0; steps < 1000000; steps++) {
        uint32_t base = pc;
        uint32_t x = e3d.mme_code[pc & 0x1FFF];
        pc++;
        int in_delay = delayed != 0xFFFFFFFF;
        if (in_delay) pc = delayed, delayed = 0xFFFFFFFF;
        uint32_t op = x & 7, rop = x >> 4 & 7, dst = x >> 8 & 7, a = s.r[x >> 11 & 7], b = s.r[x >> 14 & 7];
        int32_t imm = (int32_t)(x << 0) >> 14; // 18 bits con signo, desde el bit 14
        uint32_t sbit = x >> 17 & 31, size = x >> 22 & 31, dbit = x >> 27 & 31;
        uint32_t mask = size >= 32 ? 0xFFFFFFFFu : (1u << size) - 1;
        switch (op) {
        case 0: mme_result(&s, rop, dst, mme_alu(&s, x >> 17 & 31, a, b)); break;
        case 1: mme_result(&s, rop, dst, a + (uint32_t)imm); break;
        case 2: { // ExtractInsert: toma de B, inserta en A
            uint32_t v = (b >> sbit) & mask;
            uint32_t res = (a & ~(mask << dbit)) | (v << dbit);
            mme_result(&s, rop, dst, res);
            break;
        }
        case 3: mme_result(&s, rop, dst, ((a >> b) & mask) << dbit); break; // ExtractShiftLeftImmediate
        case 4: mme_result(&s, rop, dst, ((a >> sbit) & mask) << b); break; // ExtractShiftLeftRegister
        case 5: mme_result(&s, rop, dst, e3d.regs[(a + (uint32_t)imm) & (REGS - 1)]); break; // Read
        case 7: { // Branch, con hueco de retardo salvo "annul"
            int taken = (x >> 4 & 1) ? a != 0 : a == 0;
            if (taken) {
                uint32_t target = base + (uint32_t)imm;
                if (x >> 5 & 1) {
                    pc = target;
                    continue;
                }
                delayed = target;
                continue; // un salto tomado ignora su bit de exit
            }
            break;
        }
        default: fprintf(stderr, "[RED] MME: instrucción 0x%08x\n", x); return;
        }
        if (exiting && in_delay) return; // terminó el hueco de retardo del exit
        if ((x >> 7 & 1) && !in_delay) {
            exiting = 1;
            delayed = pc; // ejecutar la siguiente (hueco) y salir
        }
    }
    fprintf(stderr, "[RED] MME: macro sin fin (empieza en %u, pc %u, r =", start, pc);
    for (int k = 0; k < 8; k++) fprintf(stderr, " %x", s.r[k]);
    fprintf(stderr, ")\n");
}

static void macro_flush(void) {
    if (!e3d.macro_pending) return;
    e3d.macro_pending = 0;
    mme_run(e3d.mme_start[e3d.macro_index & 0x7F], e3d.macro_params, e3d.nmacro_params);
}

// ---------- 3D: acciones ----------
static uint8_t unorm8(float f) { return (uint8_t)lrintf((f < 0 ? 0 : f > 1 ? 1 : f) * 255.0f); }

static void clear_buffers(uint32_t v) {
    uint32_t *R = e3d.regs;
    if (!(v & 0x3C)) return; // solo profundidad/stencil: todavía sin superficies de profundidad
    uint32_t t = v >> 6 & 15;
    uint32_t base = 0x200 + t * 0x10;
    uint64_t addr = iova(R, base);
    uint32_t w = R[base + 2], h = R[base + 3], fmt = R[base + 4], tile = R[base + 5];
    uint8_t *mem = nx_gpu_ptr(addr);
    if (!mem || !w || !h) {
        fprintf(stderr, "[RED] GPU: ClearBuffers en un render target sin memoria (0x%llx)\n", (unsigned long long)addr);
        return;
    }
    nx_gl_cpu_write(addr, (uint64_t)w * 4 * (h + 128)); // lo que la GPU haya dibujado ahí va primero
    float col[4];
    memcpy(col, &R[0x360], 16);
    uint8_t px[4];
    switch (fmt) {
    case 0xD5: case 0xD6: // A8B8G8R8 (RGBA8 en memoria)
        px[0] = unorm8(col[0]), px[1] = unorm8(col[1]), px[2] = unorm8(col[2]), px[3] = unorm8(col[3]);
        break;
    case 0xCF: case 0xD0: // A8R8G8B8 (BGRA8 en memoria)
        px[0] = unorm8(col[2]), px[1] = unorm8(col[1]), px[2] = unorm8(col[0]), px[3] = unorm8(col[3]);
        break;
    default:
        fprintf(stderr, "[RED] GPU: ClearBuffers con formato de render target 0x%x\n", fmt);
        return;
    }
    uint8_t keep[4] = {!(v & 4), !(v & 8), !(v & 16), !(v & 32)};
    uint32_t x0 = 0, y0 = 0, x1 = w, y1 = h;
    if ((R[0x43E] & 0x100) && R[0x380]) { // las ClearBufferFlags piden respetar el scissor 0
        x0 = R[0x381] & 0xFFFF, x1 = R[0x381] >> 16, y0 = R[0x382] & 0xFFFF, y1 = R[0x382] >> 16;
        if (x1 > w) x1 = w;
        if (y1 > h) y1 = h;
    }
    int linear = tile >> 12 & 1;
    uint32_t bh = tile >> 4 & 15, width_gobs = (w * 4 + 63) / 64;
    for (uint32_t y = y0; y < y1; y++)
        for (uint32_t x = x0; x < x1; x++) {
            uint8_t *p = mem + (linear ? (uint64_t)y * w * 4 + x * 4 : nx_bl_offset(x * 4, y, width_gobs, bh));
            for (int c = 0; c < 4; c++)
                if (!keep[c]) p[c] = px[c];
        }
}

static void report_semaphore(uint32_t v) {
    uint32_t *R = e3d.regs;
    uint8_t *p = nx_gpu_ptr(iova(R, 0x6C0));
    if (!p) return;
    uint32_t op = v & 3;
    if (op == 0 || op == 2) { // Release / ReportOnly
        if (v >> 28 & 1) { // forma corta: solo el valor
            memcpy(p, &R[0x6C2], 4);
        } else {
            uint64_t payload = R[0x6C2], ts = nx_now_ns();
            memcpy(p, &payload, 8);
            memcpy(p + 8, &ts, 8);
        }
    }
}

static int traza_gpu = -1; // NX_TRAZA_GPU: cada método del motor 3D

// Los drivers dibujan N instancias como N pares VertexBeginGl(InstanceNext)/VertexEndGl sin nada en
// medio; se juntan en un draw instanciado, que sale cuando llega cualquier otro método.
static void draw_flush(void) {
    if (!e3d.pending) return;
    e3d.pending = 0;
    nx_gl_draw(&e3d.pend);
}
static void method_3d(uint32_t m, uint32_t v) {
    m &= 0xFFF;
    if (m >= 0xE00) { // llamada a macro: la par empieza, la impar agrega parámetros
        if (!(m & 1)) {
            macro_flush();
            e3d.macro_pending = 1;
            e3d.macro_index = (int)((m - 0xE00) / 2);
            e3d.nmacro_params = 0;
        }
        if (e3d.nmacro_params < 0x1000) e3d.macro_params[e3d.nmacro_params++] = v;
        return;
    }
    if (traza_gpu) fprintf(stderr, "[gpu] 3d %03x = %08x\n", m, v);
    if (e3d.pending && m != 0x585 && m != 0x586 && m != 0x35D && m != 0x35E && m != 0x5F7 && m != 0x5F8) draw_flush();
    e3d.regs[m] = v;
    switch (m) {
    case 0x046: e3d.mme_code[e3d.mme_ptr++ & 0x1FFF] = v; break;       // MmeInstructionRamLoad
    case 0x045: e3d.mme_ptr = v; break;                                 // MmeInstructionRamPointer
    case 0x048: e3d.mme_start[e3d.mme_start_ptr++ & 0x7F] = v; break;   // MmeStartAddressRamLoad
    case 0x047: e3d.mme_start_ptr = v; break;                           // MmeStartAddressRamPointer
    case 0x0B2: // SyncptAction: lo dibujado tiene que estar en memoria antes de avisar
        if (v >> 20 & 1) draw_flush(), nx_gl_flush(), nx_syncpt_incr(v & 0xFFF);
        break;
    case 0x586: // VertexBeginGl: primitiva; InstanceNext suma una instancia, si no (y no sigue) vuelve a 0
        if (v >> 26 & 1) e3d.instance++;
        else if (!(v >> 27 & 1)) e3d.instance = 0;
        break;
    case 0x35E: e3d.indexed = 0; break; // DrawArraysCount
    case 0x5F8: e3d.indexed = 1; break; // DrawElementsCount
    case 0x585: { // VertexEndGl: dibujar
        uint32_t *R = e3d.regs;
        NxDraw d = {R, (const uint64_t (*)[18])e3d.cb_addr, (const uint32_t (*)[18])e3d.cb_size, R[0x586] & 0xFFFF,
                    e3d.indexed ? R[0x5F7] : R[0x35D], e3d.indexed ? R[0x5F8] : R[0x35E], e3d.instance, 1, e3d.indexed};
        NxDraw *p = &e3d.pend;
        if (e3d.pending && p->topology == d.topology && p->first == d.first && p->count == d.count && p->indexed == d.indexed &&
            p->instance + p->ninstances == d.instance) {
            p->ninstances++;
        } else {
            draw_flush();
            e3d.pend = d;
            e3d.pending = 1;
        }
        break;
    }
    case 0x8C4: e3d.regs[0xD00] = 1; break; // FirmwareCall4: el firmware (falcon) toca registros
                                            // que no nos importan y avisa en MmeFirmwareArgs[0]
    case 0x674: clear_buffers(v); break;                                // ClearBuffers
    case 0x6C3: draw_flush(), nx_gl_flush(), report_semaphore(v); break; // SetReportSemaphore
    case 0x06C: // LaunchDma (carga en línea): destino y largo
        e3d.i2m_dst = iova(e3d.regs, 0x62);
        e3d.i2m_len = e3d.regs[0x60] * (e3d.regs[0x61] ? e3d.regs[0x61] : 1);
        e3d.i2m_done = 0;
        break;
    case 0x06D: { // LoadInlineData
        if (e3d.i2m_done == 0) nx_gl_cpu_write(e3d.i2m_dst, e3d.i2m_len);
        uint8_t *p = nx_gpu_ptr(e3d.i2m_dst + e3d.i2m_done);
        if (p && e3d.i2m_done < e3d.i2m_len) memcpy(p, &v, e3d.i2m_len - e3d.i2m_done < 4 ? e3d.i2m_len - e3d.i2m_done : 4);
        e3d.i2m_done += 4;
        break;
    }
    case 0x904: case 0x90C: case 0x914: case 0x91C: case 0x924: { // Bind[etapa].Constbuf
        int s = (m - 0x904) / 8, slot = v >> 4 & 31;
        if (slot < 18) {
            e3d.cb_addr[s][slot] = v & 1 ? iova(e3d.regs, 0x8E1) : 0;
            e3d.cb_size[s][slot] = e3d.regs[0x8E0] & 0x1FFFF;
        }
        break;
    }
    default:
        if (m >= 0x8E4 && m < 0x8F4) { // LoadConstbufData: al constbuf elegido, en el offset que avanza
            uint8_t *p = nx_gpu_ptr(iova(e3d.regs, 0x8E1) + e3d.regs[0x8E3]);
            if (p) memcpy(p, &v, 4);
            e3d.regs[0x8E3] += 4;
        }
    }
}

// Métodos del "puller" (subcanal cualquiera, método < 0x40): semáforos y syncpoints.
static void method_gpfifo(uint32_t m, uint32_t v, uint32_t *regs) {
    regs[m] = v;
    if (m == 0x07) { // Semaphore
        uint32_t op = v & 31;
        if (op == 2) draw_flush(), nx_gl_flush();
        uint8_t *p = nx_gpu_ptr(iova(regs, 0x04));
        if (op == 2 && p) { // Release
            if (v >> 24 & 1) memcpy(p, &regs[0x06], 4);
            else {
                uint64_t payload = regs[0x06], ts = nx_now_ns();
                memcpy(p, &payload, 8);
                memcpy(p + 8, &ts, 8);
            }
        }
    } else if (m == 0x1D && (v & 1)) { // Syncpoint: incrementar (esperar no hace falta: todo es en orden)
        draw_flush();
        nx_gl_flush();
        nx_syncpt_incr(v >> 8 & 0xFF);
    }
}

static uint32_t puller_regs[0x40];

// ---------- motor de copia (DMA, 0xB0B5) ----------
static uint32_t dma[0x200];

// Dirección de un byte de una superficie del motor de copia (lineal con pitch o "block linear").
static uint64_t dma_addr(int bl, uint64_t base, uint32_t pitch, uint32_t blk, uint32_t w_bytes, uint32_t h, uint32_t z,
                         uint32_t xb, uint32_t y) {
    if (!bl) return base + (uint64_t)y * pitch + xb;
    uint32_t bhl = blk >> 4 & 15, bdl = blk >> 8 & 15, gh = 1u << bhl, gd = 1u << bdl;
    uint32_t wgobs = (w_bytes + 63) / 64, bpc = (h + 8 * gh - 1) / (8 * gh);
    uint64_t block = ((uint64_t)(z / gd) * bpc + y / (8 * gh)) * wgobs + xb / 64;
    uint64_t gob = (z % gd) * gh + (y / 8) % gh;
    uint32_t gx = xb % 64, gy = y % 8;
    return base + block * 512 * gh * gd + gob * 512 + (gx / 32) * 256 + (gy / 2) * 64 + ((gx % 32) / 16) * 32 +
           (gy % 2) * 16 + gx % 16;
}

static void dma_launch(uint32_t v) {
    uint32_t *R = dma;
    int src_bl = !(v >> 7 & 1), dst_bl = !(v >> 8 & 1), multi = v >> 9 & 1, remap = v >> 10 & 1;
    uint64_t src = (uint64_t)R[0x100] << 32 | R[0x101], dst = (uint64_t)R[0x102] << 32 | R[0x103];
    uint32_t len = R[0x106], lines = multi ? R[0x107] : 1;
    uint32_t rc = R[0x1C2], csz = remap ? (rc >> 16 & 3) + 1 : 1;
    uint32_t sbpp = remap ? csz * ((rc >> 20 & 3) + 1) : 1, dbpp = remap ? csz * ((rc >> 24 & 3) + 1) : 1;
    // Origen y tamaño de las superficies "block linear" vienen en elementos (bytes sin remap).
    uint32_t sx0 = R[0x1CF] & 0xFFFF, sy0 = R[0x1CF] >> 16, dx0 = R[0x1C8] & 0xFFFF, dy0 = R[0x1C8] >> 16;
    uint32_t sw = R[0x1CB] * sbpp, sh = R[0x1CC], sz = R[0x1CE], dw = R[0x1C4] * dbpp, dh = R[0x1C5], dz = R[0x1C7];
    if (!src_bl) sx0 = sy0 = 0;
    if (!dst_bl) dx0 = dy0 = 0;
    // Lo que la GPU tenga pendiente en esa memoria va antes; lo copiado invalida sus copias.
    uint64_t sspan = src_bl ? (uint64_t)((sw + 63) / 64) * 64 * (sh + 256) * (sz + 1) : (uint64_t)R[0x104] * lines + len * sbpp;
    uint64_t dspan = dst_bl ? (uint64_t)((dw + 63) / 64) * 64 * (dh + 256) * (dz + 1) : (uint64_t)R[0x105] * lines + len * dbpp;
    nx_gl_cpu_write(src, sspan);
    nx_gl_cpu_write(dst, dspan);
    for (uint32_t y = 0; y < lines; y++) {
        if (!remap && !src_bl && !dst_bl) { // fila contigua
            uint8_t *s = nx_gpu_ptr(src + (uint64_t)y * R[0x104]), *d = nx_gpu_ptr(dst + (uint64_t)y * R[0x105]);
            if (s && d) memmove(d, s, len);
            continue;
        }
        for (uint32_t x = 0; x < len; x++) {
            uint64_t sa = dma_addr(src_bl, src, R[0x104], R[0x1CA], sw, sh, sz, (sx0 + x) * sbpp, sy0 + y);
            uint64_t da = dma_addr(dst_bl, dst, R[0x105], R[0x1C3], dw, dh, dz, (dx0 + x) * dbpp, dy0 + y);
            uint8_t *s = nx_gpu_ptr(sa), *d = nx_gpu_ptr(da);
            if (!d) continue;
            if (!remap) {
                if (s) *d = *s;
                continue;
            }
            for (uint32_t c = 0; c * csz < dbpp; c++) {
                uint32_t sel = rc >> (c * 4) & 7;
                if (sel == 6) continue; // no escribir
                if (sel >= 4) memcpy(d + c * csz, &R[0x1C0 + (sel - 4)], csz);
                else if (s && sel * csz < sbpp) memcpy(d + c * csz, s + sel * csz, csz);
            }
        }
    }
}

static void call_method(uint32_t subch, uint32_t m, uint32_t v) {
    if (m == 0) { // SetObject: qué clase atiende este subcanal
        subch_class[subch] = v & 0xFFFF;
        return;
    }
    if (m < 0x40) {
        method_gpfifo(m, v, puller_regs);
        return;
    }
    uint32_t cls = subch_class[subch];
    if (cls != CLASS_3D) draw_flush(); // otro motor: el draw pendiente del 3D va antes
    if (cls == CLASS_3D) method_3d(m, v);
    else if (cls == CLASS_DMA) {
        if (m < 0x200) dma[m] = v;
        if (m == 0x0C0) dma_launch(v);
    }
    else if (cls == CLASS_I2M) { // la carga en línea usa los mismos métodos 0x60-0x6D
        if (m >= 0x60 && m <= 0x6D) method_3d(m, v);
    }
    // cómputo, 2D y copia: todavía no
}

// Ejecuta un pushbuffer de `words` palabras en la dirección de GPU `gpu`.
static void run_pushbuffer(uint64_t gpu, uint32_t words) {
    const uint32_t *pb = nx_gpu_ptr(gpu);
    if (!pb) {
        fprintf(stderr, "[RED] GPU: pushbuffer en 0x%llx sin memoria\n", (unsigned long long)gpu);
        return;
    }
    uint32_t i = 0;
    while (i < words) {
        uint32_t h = pb[i++];
        uint32_t m = h & 0x1FFF, sub = h >> 13 & 7, arg = h >> 16 & 0x1FFF, mode = h >> 29;
        switch (mode) {
        case 1: // creciente
            for (uint32_t k = 0; k < arg && i < words; k++) call_method(sub, m + k, pb[i++]);
            break;
        case 3: // no creciente
            for (uint32_t k = 0; k < arg && i < words; k++) call_method(sub, m, pb[i++]);
            break;
        case 4: // en línea: el dato va en la cabecera
            call_method(sub, m, arg);
            break;
        case 5: // crece una vez
            for (uint32_t k = 0; k < arg && i < words; k++) call_method(sub, k ? m + 1 : m, pb[i++]);
            break;
        default:
            if (h) fprintf(stderr, "[RED] GPU: cabecera de pushbuffer 0x%08x\n", h);
        }
        macro_flush(); // una macro corre cuando termina el comando que la llamó
    }
}

void nx_gpu_submit(const uint64_t *entries, uint32_t n) {
    pthread_mutex_lock(&gpu_lock);
    if (traza_gpu < 0) traza_gpu = getenv("NX_TRAZA_GPU") != NULL;
    nx_gl_invalidate_all();
    for (uint32_t k = 0; k < n; k++) {
        uint64_t e = entries[k];
        uint64_t gpu = e & 0xFFFFFFFFFCull;
        uint32_t words = (uint32_t)(e >> 42 & 0x1FFFFF);
        if (words) run_pushbuffer(gpu, words);
    }
    draw_flush();
    nx_gl_release();
    pthread_mutex_unlock(&gpu_lock);
}
