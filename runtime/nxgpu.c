// La GPU de la Switch (Tegra X1, Maxwell GM20B): el procesador de comandos.
//   - Espacio de direcciones de la GPU: direcciones de GPU → memoria del programa (nvhost-as-gpu),
//     con una tabla de páginas de 4 KiB.
//   - Syncpoints: los contadores que hacen de cercas; las esperas se bloquean hasta el valor.
//   - GPFIFO: cada entrada apunta a un pushbuffer; se ejecuta en el momento del envío.
//   - Pushbuffer: cabeceras de método (creciente, no creciente, en línea, crece una vez) hacia el
//     objeto de cada subcanal: 3D (0xB197), cómputo (0xB1C0), carga en línea (0xA140), 2D, copia.
//   - Motor de macros (MME), semáforos, reportes, motores 2D y de copia. Lo que dibuja va a nxgl.c.
// Nombres y offsets de registros: deko3d (maxwell/engine_*.def), envytools y Ryujinx.
#define _GNU_SOURCE
#include "nxgpu.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <time.h>

#include "nxgl.h"
#include "nxrt.h"

// ---------- espacio de direcciones ----------
// Cada mapeo ocupa un lugar en `maps`; la tabla de páginas (dos niveles, páginas de 4 KiB sobre 40
// bits) dice qué mapeo cubre cada página. Buscar una dirección no recorre la lista ni toma el
// candado: el motor de copia la consulta por cada fila.
typedef struct {
    uint64_t gpu, size, cpu;
} Mapping;
#define MAX_MAPS 16384
#define PAG 12           // páginas de 4 KiB
#define N2 14            // 2^14 páginas por bloque (64 MiB)
#define N1 (40 - PAG - N2)
static Mapping maps[MAX_MAPS];
static int nmaps, libres[MAX_MAPS], nlibres;
static uint16_t *tabla[1u << N1]; // índice del mapeo + 1 por página (0 = sin mapear)
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

static void paginas(uint64_t gpu, uint64_t size, uint16_t valor) {
    for (uint64_t pg = gpu >> PAG, fin = (gpu + size + (1u << PAG) - 1) >> PAG; pg < fin; pg++) {
        uint64_t i1 = pg >> N2;
        if (i1 >= (1u << N1)) break;
        if (!tabla[i1]) {
            if (!valor) continue;
            tabla[i1] = calloc(1u << N2, sizeof(uint16_t));
        }
        tabla[i1][pg & ((1u << N2) - 1)] = valor;
    }
}

static int buscar(uint64_t gpu) { // índice del mapeo que cubre `gpu`, o -1
    uint64_t pg = gpu >> PAG, i1 = pg >> N2;
    if (i1 >= (1u << N1) || !tabla[i1]) return -1;
    int k = (int)tabla[i1][pg & ((1u << N2) - 1)] - 1;
    return k >= 0 && gpu >= maps[k].gpu && gpu - maps[k].gpu < maps[k].size ? k : -1;
}

void nx_gpu_map(uint64_t gpu, uint64_t size, uint64_t cpu) {
    pthread_mutex_lock(&as_lock);
    int k = buscar(gpu);
    if (k < 0 || maps[k].gpu != gpu) k = nlibres ? libres[--nlibres] : nmaps < MAX_MAPS ? nmaps++ : -1;
    if (k >= 0) {
        maps[k] = (Mapping){gpu, size, cpu};
        paginas(gpu, size, (uint16_t)(k + 1));
    } else {
        fprintf(stderr, "[RED] GPU: más de %d mapeos\n", MAX_MAPS);
    }
    if (gpu + size > va_next) va_next = gpu + size;
    pthread_mutex_unlock(&as_lock);
}

void nx_gpu_unmap(uint64_t gpu) {
    pthread_mutex_lock(&as_lock);
    int k = buscar(gpu);
    if (k >= 0 && maps[k].gpu == gpu) {
        // Solo las páginas que siguen siendo de este mapeo (otro pudo taparlas después).
        for (uint64_t a = gpu; a < gpu + maps[k].size; a += 1u << PAG)
            if (buscar(a) == k) paginas(a, 1, 0);
        maps[k].size = 0;
        libres[nlibres++] = k;
    }
    pthread_mutex_unlock(&as_lock);
}

void *nx_gpu_ptr(uint64_t gpu) {
    int k = buscar(gpu);
    return k < 0 ? NULL : (void *)(uintptr_t)(maps[k].cpu + (gpu - maps[k].gpu));
}

// Además dice cuántos bytes siguen contiguos (también a través de mapeos vecinos que siguen en la
// misma memoria: un juego puede mapear un buffer en varios trozos).
void *nx_gpu_ptr_n(uint64_t gpu, uint64_t *avail) {
    int k = buscar(gpu);
    if (k < 0) {
        *avail = 0;
        return NULL;
    }
    uint64_t cpu = maps[k].cpu + (gpu - maps[k].gpu), fin = maps[k].gpu + maps[k].size;
    for (int j; (j = buscar(fin)) >= 0 && maps[j].gpu == fin && maps[j].cpu == cpu + (fin - gpu);)
        fin += maps[j].size;
    *avail = fin - gpu;
    return (void *)(uintptr_t)cpu;
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
    uint32_t shadow[REGS];   // "shadow RAM": copia de los registros que MmeShadowRamControl graba o repone
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
    if (nx_gl_clear(R, v)) return;
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

// Marcas de tiempo de la GPU: ticks de 614,4 MHz (384 por cada 625 ns), no los de 19,2 MHz de la CPU.
// Algunos juegos restan dos marcas y convierten la diferencia ellos mismos (NieR, BotW).
static uint64_t gpu_ticks(void) {
    uint64_t ns = nx_now_ns();
    return ns / 625 * 384 + ns % 625 * 384 / 625;
}

// Escribe un reporte: corto (4 bytes) o largo (valor de 64 bits y marca de tiempo).
static void reporte(uint8_t *p, uint64_t valor, int corto) {
    if (corto) {
        uint32_t v32 = (uint32_t)valor;
        memcpy(p, &v32, 4);
    } else {
        uint64_t ts = gpu_ticks();
        memcpy(p, &valor, 8);
        memcpy(p + 8, &ts, 8);
    }
}

// Espera (acquire) a que la palabra en `p` cumpla la condición. Lo que la cumple puede ser la CPU u
// otro canal; si no llega en 2 s se avisa y se sigue, para no colgar el juego entero.
static void esperar_semaforo(volatile uint32_t *p, uint32_t valor, int modo) {
    uint64_t limite = nx_now_ns() + 2000000000ull;
    for (;;) {
        uint32_t x = *p;
        if (modo == 0 ? x == valor : modo == 1 ? x >= valor : (x & valor) != 0) return;
        if (nx_now_ns() > limite) {
            fprintf(stderr, "[RED] GPU: semáforo sin cumplirse (vale %u, espera %u, modo %d)\n", x, valor, modo);
            return;
        }
        sched_yield();
    }
}

// Reducciones atómicas de los semáforos (Min, Max, Xor, And, Or, Add, Inc, Dec).
static void reducir(uint32_t *p, uint32_t valor, uint32_t op, int con_signo) {
    uint32_t viejo = __atomic_load_n(p, __ATOMIC_SEQ_CST), nuevo;
    do {
        switch (op) {
        case 0: nuevo = con_signo ? ((int32_t)viejo < (int32_t)valor ? viejo : valor) : viejo < valor ? viejo : valor; break;
        case 1: nuevo = con_signo ? ((int32_t)viejo > (int32_t)valor ? viejo : valor) : viejo > valor ? viejo : valor; break;
        case 2: nuevo = viejo ^ valor; break;
        case 3: nuevo = viejo & valor; break;
        case 4: nuevo = viejo | valor; break;
        case 5: nuevo = viejo + valor; break;
        case 6: nuevo = viejo >= valor ? 0 : viejo + 1; break;
        case 7: nuevo = viejo == 0 || viejo > valor ? valor : viejo - 1; break;
        default: return;
        }
    } while (!__atomic_compare_exchange_n(p, &viejo, nuevo, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
}

// Renderizado condicional (SetRenderEnableA/B/C, 0x554-0x556): los draws y las limpiezas solo
// ocurren si se cumple la condición sobre lo que dejó un reporte en memoria (C: 0 nunca, 1 siempre,
// 2 si el valor de 64 bits no es 0, 3 si los valores en +0 y +16 son iguales, 4 si son distintos).
static int render_cond_escrito; // hasta que el driver lo escribe, se dibuja siempre
static int render_enabled(void) {
    uint32_t *R = e3d.regs, modo = R[0x556] & 7;
    if (!render_cond_escrito) return 1;
    if (modo == 1 || modo > 4) return 1;
    if (modo == 0) return 0;
    uint64_t avail, a, b;
    const uint8_t *p = nx_gpu_ptr_n((uint64_t)R[0x554] << 32 | R[0x555], &avail);
    if (!p || avail < 24) return 1;
    memcpy(&a, p, 8);
    memcpy(&b, p + 16, 8);
    return modo == 2 ? a != 0 : modo == 3 ? a == b : a != b;
}

// SetReportSemaphore (0x6C3): bits 0-1 operación (0 Release, 1 Acquire, 2 contador), 23-27 tipo de
// contador, 28 reporte corto.
static void report_semaphore(uint32_t v) {
    uint32_t *R = e3d.regs;
    uint8_t *p = nx_gpu_ptr(iova(R, 0x6C0));
    if (!p) return;
    int corto = v >> 28 & 1;
    switch (v & 3) {
    case 0: reporte(p, R[0x6C2], corto); break;
    case 1: esperar_semaforo((volatile uint32_t *)p, R[0x6C2], 0); break;
    case 2: {
        uint32_t tipo = v >> 23 & 31;
        reporte(p, tipo == 0 ? R[0x6C2] : nx_gl_contador(tipo), corto);
        break;
    }
    }
}

static int traza_gpu; // NX_TRAZA_GPU: cada método del motor 3D y del puller

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
    // MmeShadowRamControl (0x049): 0 y 1 graban cada escritura en la shadow RAM, 2 no la toca y
    // 3 (Replay) ignora el valor recibido y repone el grabado.
    if (m != 0x049) {
        uint32_t modo = e3d.regs[0x049];
        if (modo == 0 || modo == 1) e3d.shadow[m] = v;
        else if (modo == 3) v = e3d.shadow[m];
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
        if (!render_enabled()) break;
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
    case 0x556: render_cond_escrito = 1; break;
    case 0x8C4: e3d.regs[0xD00] = 1; break; // FirmwareCall4: el firmware (falcon) toca registros
                                            // que no nos importan y avisa en MmeFirmwareArgs[0]
    case 0x674: if (render_enabled()) clear_buffers(v); break;          // ClearBuffers
    case 0x6C3: draw_flush(), nx_gl_flush(), report_semaphore(v); break; // SetReportSemaphore
    case 0x54C: draw_flush(), nx_gl_contador_reiniciar(v); break;         // CounterReset
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
    if (traza_gpu) fprintf(stderr, "[gpu] fifo %02x = %08x\n", m, v);
    regs[m] = v;
    if (m == 0x07) { // SemaphoreD: bits 0-4 operación, 24 tamaño (4 bytes), 27-30 reducción, 31 sin signo
        uint32_t op = v & 31;
        uint8_t *p = nx_gpu_ptr(iova(regs, 0x04));
        if (!p) return;
        if (op == 2 || op == 16) draw_flush(), nx_gl_flush(); // lo anterior queda escrito antes
        if (op == 1) esperar_semaforo((volatile uint32_t *)p, regs[0x06], 0);      // Acquire
        else if (op == 4) esperar_semaforo((volatile uint32_t *)p, regs[0x06], 1); // AcqGeq
        else if (op == 8) esperar_semaforo((volatile uint32_t *)p, regs[0x06], 2); // AcqAnd
        else if (op == 2) reporte(p, regs[0x06], v >> 24 & 1);                     // Release
        else if (op == 16) reducir((uint32_t *)p, regs[0x06], v >> 27 & 15, !(v >> 31 & 1)); // Reduction
    } else if (m == 0x1D && (v & 1)) { // Syncpoint: incrementar (esperar no hace falta: todo es en orden)
        draw_flush();
        nx_gl_flush();
        nx_syncpt_incr(v >> 8 & 0xFF);
    }
}

static uint32_t puller_regs[0x40];
static uint32_t comp[REGS]; // registros del motor de cómputo

// ---------- motor 2D (0x902D): blits con escala ----------
static uint32_t r2d[0x300];

static uint32_t fmt_bpp(uint32_t f) { // bytes por píxel de un formato de superficie de color
    if (f >= 0xC0 && f <= 0xC5) return 16;
    if (f >= 0xC6 && f <= 0xCE) return 8;
    if (f >= 0xCF && f <= 0xE7) return 4;
    if (f >= 0xE8 && f <= 0xF2) return 2;
    if (f >= 0xF3 && f <= 0xF7) return 1;
    return 4;
}
static int fmt_bgra(uint32_t f) { return f == 0xCF || f == 0xD0 || f == 0xE6 || f == 0xE7 || f == 0xDF; }

static uint8_t *px2d(int base, int bl, uint32_t x, uint32_t y, uint32_t bpp) {
    uint32_t *R = r2d;
    uint64_t addr = (uint64_t)R[base + 8] << 32 | R[base + 9];
    uint32_t w = R[base + 6], h = R[base + 7];
    if (x >= w || y >= h) return NULL;
    uint64_t off;
    if (!bl) off = (uint64_t)y * R[base + 5] + (uint64_t)x * bpp;
    else off = nx_bl_offset(x * bpp, y, (w * bpp + 63) / 64, R[base + 2] >> 4 & 7);
    return nx_gpu_ptr(addr + off);
}

static void blit2d(void) {
    uint32_t *R = r2d;
    // Destino en 0x080-0x089, origen en 0x08C-0x095 con el mismo orden (formato, layout, bloque, ...).
    uint32_t df = R[0x80], sf = R[0x8C], db = fmt_bpp(df), sb = fmt_bpp(sf);
    int dbl = !(R[0x81] & 1), sbl = !(R[0x8D] & 1);
    int64_t dudx = (int64_t)((uint64_t)R[0x231] << 32 | R[0x230]), dvdy = (int64_t)((uint64_t)R[0x233] << 32 | R[0x232]);
    int64_t sx0 = (int64_t)((uint64_t)R[0x235] << 32 | R[0x234]), sy0 = (int64_t)((uint64_t)R[0x237] << 32 | R[0x236]);
    uint32_t dx0 = R[0x22C], dy0 = R[0x22D], dw = R[0x22E], dh = R[0x22F];
    int bilineal = R[0x223] >> 4 & 1, centro = !(R[0x223] & 1);
    if (R[0xAB] != 3 && R[0xAB] != 0) {
        static int avisado;
        if (!avisado++) fprintf(stderr, "[RED] GPU: operación del motor 2D %u (se copia)\n", R[0xAB]);
    }
    uint64_t dsp = (uint64_t)R[0x86] * R[0x87] * db + 0x10000, ssp = (uint64_t)R[0x92] * R[0x93] * sb + 0x10000;
    nx_gl_cpu_write((uint64_t)R[0x94] << 32 | R[0x95], ssp);
    nx_gl_cpu_write((uint64_t)R[0x88] << 32 | R[0x89], dsp);
    int swap = fmt_bgra(df) != fmt_bgra(sf) && db == 4 && sb == 4;
    for (uint32_t y = 0; y < dh; y++)
        for (uint32_t x = 0; x < dw; x++) {
            uint8_t *d = px2d(0x80, dbl, dx0 + x, dy0 + y, db);
            if (!d) continue;
            // Posición en el origen, en 32.32: la esquina del píxel (más medio paso si el origen es el centro).
            int64_t u = sx0 + (int64_t)x * dudx + (centro ? dudx / 2 : 0), v = sy0 + (int64_t)y * dvdy + (centro ? dvdy / 2 : 0);
            uint8_t out[16];
            if (bilineal && sb == 4 && db == 4) {
                int64_t uf = u - (1ll << 31), vf = v - (1ll << 31); // centros de texel en +0.5
                int32_t ui = (int32_t)(uf >> 32), vi = (int32_t)(vf >> 32);
                uint32_t fu = (uint32_t)(uf >> 24) & 0xFF, fv = (uint32_t)(vf >> 24) & 0xFF;
                uint8_t *p[4];
                for (int k = 0; k < 4; k++) {
                    int32_t xx = ui + (k & 1), yy = vi + (k >> 1);
                    p[k] = px2d(0x8C, sbl, xx < 0 ? 0 : xx, yy < 0 ? 0 : yy, sb);
                    if (!p[k]) p[k] = px2d(0x8C, sbl, ui < 0 ? 0 : ui, vi < 0 ? 0 : vi, sb);
                }
                if (!p[0]) continue;
                for (int c = 0; c < 4; c++) {
                    uint32_t a = p[0][c] * (256 - fu) + (p[1] ? p[1][c] : p[0][c]) * fu;
                    uint32_t b = (p[2] ? p[2][c] : p[0][c]) * (256 - fu) + (p[3] ? p[3][c] : p[0][c]) * fu;
                    out[c] = (uint8_t)((a * (256 - fv) + b * fv + 32768) >> 16);
                }
            } else {
                uint8_t *s = px2d(0x8C, sbl, (uint32_t)(u >> 32), (uint32_t)(v >> 32), sb);
                if (!s) continue;
                memcpy(out, s, sb < 16 ? sb : 16);
            }
            if (swap) {
                uint8_t tmp = out[0];
                out[0] = out[2], out[2] = tmp;
            }
            memcpy(d, out, db <= sb ? db : sb);
        }
}

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
        if (!remap) { // en "block linear" cada tramo de 16 bytes alineado es contiguo: se copia por tramos
            for (uint32_t x = 0, n; x < len; x += n) {
                n = len - x;
                if (src_bl && n > 16 - ((sx0 + x) & 15)) n = 16 - ((sx0 + x) & 15);
                if (dst_bl && n > 16 - ((dx0 + x) & 15)) n = 16 - ((dx0 + x) & 15);
                uint64_t sa = dma_addr(src_bl, src, R[0x104], R[0x1CA], sw, sh, sz, sx0 + x, sy0 + y);
                uint64_t da = dma_addr(dst_bl, dst, R[0x105], R[0x1C3], dw, dh, dz, dx0 + x, dy0 + y);
                uint64_t sav, dav;
                uint8_t *s = nx_gpu_ptr_n(sa, &sav), *d = nx_gpu_ptr_n(da, &dav);
                if (s && d && sav >= n && dav >= n) memmove(d, s, n);
            }
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

// Semáforo al terminar la copia (LaunchDma bits 3-4: 1 una palabra, 2 cuatro palabras con marca de tiempo).
static void dma_semaforo(uint32_t v) {
    uint32_t tipo = v >> 3 & 3;
    if (!tipo) return;
    uint8_t *p = nx_gpu_ptr((uint64_t)dma[0x090] << 32 | dma[0x091]);
    if (p) reporte(p, dma[0x092], tipo == 1);
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
    else if (cls == CLASS_2D) {
        if (m < 0x300) r2d[m] = v;
        if (m == 0x237) blit2d();
    } else if (cls == CLASS_COMPUTE) {
        if (m < REGS) comp[m] = v;
        if (m >= 0x60 && m <= 0x6D) method_3d(m, v); // la carga en línea es la misma que la del 3D
        else if (m == 0x0AF) { // SendSignalingPcasB: lanzar la QMD que nombró SendPcasA
            uint64_t avail;
            const uint32_t *q = nx_gpu_ptr_n((uint64_t)comp[0x0AD] << 8, &avail);
            if (q && avail >= 256) nx_gl_dispatch(comp, q);
        }
    } else if (cls == CLASS_DMA) {
        if (m < 0x200) dma[m] = v;
        if (m == 0x0C0) dma_launch(v), dma_semaforo(v);
    }
    else if (cls == CLASS_I2M) { // la carga en línea usa los mismos métodos 0x60-0x6D
        if (m >= 0x60 && m <= 0x6D) method_3d(m, v);
    }
    // cómputo, 2D y copia: todavía no
}

// Estado del lector de pushbuffers. Sobrevive entre entradas del GPFIFO y entre envíos, como en el
// hardware: NVN arma los draws indirectos con la llamada a la macro en una entrada y sus argumentos
// (escritos por la GPU) en la siguiente. La macro corre cuando llega su último argumento.
static struct {
    uint32_t method, sub, count, mode; // count: datos que faltan del comando en curso
} pbs;

static void run_pushbuffer(uint64_t gpu, uint32_t words) {
    uint64_t avail;
    const uint32_t *pb = nx_gpu_ptr_n(gpu, &avail);
    if (!pb || avail < (uint64_t)words * 4) {
        fprintf(stderr, "[RED] GPU: pushbuffer en 0x%llx (%u palabras) sin memoria\n", (unsigned long long)gpu, words);
        return;
    }
    uint32_t i = 0;
    while (i < words) {
        if (pbs.count) { // datos de un comando que empezó antes (quizá en otra entrada)
            call_method(pbs.sub, pbs.method, pb[i++]);
            if (pbs.mode == 1) pbs.method++;
            else if (pbs.mode == 5) pbs.mode = 3, pbs.method++;
            if (!--pbs.count) macro_flush();
            continue;
        }
        uint32_t h = pb[i++];
        uint32_t m = h & 0x1FFF, sub = h >> 13 & 7, arg = h >> 16 & 0x1FFF, mode = h >> 29;
        switch (mode) {
        case 1: case 3: case 5: // creciente, no creciente, crece una vez
            pbs.method = m, pbs.sub = sub, pbs.count = arg, pbs.mode = mode;
            break;
        case 4: // en línea: el dato va en la cabecera
            call_method(sub, m, arg);
            macro_flush();
            break;
        case 0: case 2: break; // SetSubDeviceMask y compañía: una sola GPU
        default:
            if (h) fprintf(stderr, "[RED] GPU: cabecera de pushbuffer 0x%08x\n", h);
        }
    }
}

void nx_gpu_submit(const uint64_t *entries, uint32_t n) {
    pthread_mutex_lock(&gpu_lock);
    static int iniciada;
    if (!iniciada++) traza_gpu = getenv("NX_TRAZA_GPU") != NULL;
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
