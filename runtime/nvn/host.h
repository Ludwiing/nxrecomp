// Adaptación: la API de host.h de totk-pc-recomp que usa su capa NVN (runtime/nvn/), sobre el runtime de nxrecomp.
// Los archivos de esta carpeta que no son adaptaciones vienen de totk-pc-recomp sin cambios (LICENSE.totk-pc-recomp);
// así se pueden comparar y actualizar desde upstream.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../nxnvn.h"

typedef Ctx Cpu;

static inline void *arg_p(const Cpu *c, int i) { return (void *)(uintptr_t)arg_x(c, i); }
// Argumentos y resultado de coma flotante: s0..s7 / d0..d7 (los 32 o 64 bits bajos de v0..v7).
static inline float arg_s(const Cpu *c, int i) { float f; memcpy(&f, &c->vr[i].s[0], 4); return f; }
static inline double arg_d(const Cpu *c, int i) { double d; memcpy(&d, &c->vr[i].d[0], 8); return d; }
static inline void ret_s(Cpu *c, float f) { uint32_t b; memcpy(&b, &f, 4); wrv32(c, 0, b); }

// Registro: "[nvn] ..." a stderr. NX_TRAZA_NVN=1 activa el de cada llamada (RT_TRACE_GFX).
enum { RT_TRACE_SVC = 1, RT_TRACE_HOSTCALL = 2, RT_TRACE_IPC = 4, RT_TRACE_FS = 8, RT_TRACE_GFX = 16, RT_TRACE_PERF = 32 };
extern unsigned rt_trace;
void rt_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
__attribute__((noreturn)) void rt_fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Llama a la función del juego fn con n argumentos enteros (n <= 8) y devuelve su x0. Los registros del llamador se
// conservan: el host puede seguir leyendo sus argumentos después.
uint64_t rt_call(Cpu *c, uint64_t fn, int n, ...);

// Reemplazos y observadores de funciones del juego por símbolo: totk-pc-recomp los usa para arreglos de TotK; en un
// runtime genérico no se registran (el que haga falta va en recomp/sustituciones.txt).
#define HOST_OVERRIDE(sym, fn) __attribute__((unused)) static void (*const nx_sin_override_##fn)(Cpu *) = fn
#define HOST_OBSERVER(sym, fn) __attribute__((unused)) static void (*const nx_sin_observer_##fn)(Cpu *) = fn
uint64_t rt_original(Cpu *c);

// Tiempo: ticks de 19,2 MHz (GetSystemTick) y nanosegundos monótonos del runtime.
static inline uint64_t rt_ticks(void) { return nx_ticks(); }
static inline uint64_t kern_now_ns(void) { return nx_now_ns(); }

// Núcleos virtuales (nxkern.c): una espera del host suelta el núcleo del hilo y lo vuelve a tomar al terminar.
int nx_core_away(void);
void nx_core_back(int away);
static inline int kern_core_away(void) { return nx_core_away(); }
static inline void kern_core_back(int *away) { nx_core_back(*away); }

typedef struct { const char *gpu; } RtConfig;   // gpu: NX_GPU (null o vulkan)
const RtConfig *rt_config(void);

// Lo que totk-pc-recomp hace al presentar un cuadro y que es propio de TotK (ritmo de 60 fps, videos, cambio de
// resolución, mediciones): en el runtime genérico no hace nada.
enum { PERF_PACE, PERF_FENCE, PERF_DRAIN, PERF_PIPE, PERF_XLATE, PERF_TEX, PERF_ASTC, PERF_N };
static inline uint64_t perf_begin(void) { return 0; }
static inline void perf_end(int kind, uint64_t t0) { (void)kind, (void)t0; }
static inline void perf_replay_thread(void) {}
static inline void movie_tick(Cpu *c) { (void)c; }
static inline void res_present(Cpu *c) { (void)c; }
static inline int frame_step_realtime(void) { return 0; }
static inline uint64_t frame_step_cap_ns(void) { return 0; }
static inline void frame_step_present(uint64_t display_ns, int interval) { (void)display_ns, (void)interval; }

// ---- lo que pide el backend Vulkan (vk_*.c, gfx_shader.c, gfx_present.c); implementación en nvn_host.c
enum { RT_TRACE_SCHED = 64, RT_TRACE_AUDIO = 128, RT_TRACE_PRESENT = 256 };
#define RT_ADDRESS_LIMIT (1ull << 39)   // espacio de direcciones del juego (39 bits)
extern const int rt_game_version;       // TotK: 110/121/143; 0 en el runtime genérico
void rt_sha256(const void *p, size_t n, uint8_t out[32]);
// Interruptores TOTK_<name>: 0 apagado, 2 "ab" (medición A/B, aquí siempre apagada), 1 cualquier otro valor.
int rt_ab(void);
int rt_knob(const char *name, const char *def);
#define RT_KNOB_ON(mode) ((mode) == 2 ? rt_ab() : (mode))
// Estadísticas de rendimiento de totk-pc-recomp: no se recogen en esta adaptación.
enum { PERF_CNT_PIPE_ASYNC, PERF_CNT_PIPE_SKIP, PERF_CNT_TEX_UP, PERF_CNT_TEX_REUP, PERF_CNT_SHADER_SKIP,
       PERF_CNT_PIPE_WAIT, PERF_CNT_ONESHOT_WAIT, PERF_CNT_PIPE_BUILT, PERF_CNT_TEX_NEW, PERF_CNT_TEX_ALIAS,
       PERF_CNT_WB, PERF_CNT_WB_SKIP, PERF_CNT_N };
typedef struct {
    int have_gpu, have_vram, vram_exact;
    uint64_t gpu_ns, gpu_parts, gpu_missed;
    uint64_t vram_used, vram_budget;
    int have_idle;
    uint64_t idle_ns, starved_ns, queued_ns;
    uint64_t idle_gaps, max_gap_ns;
} PerfGpu;
extern void (*perf_gpu_stats)(PerfGpu *out);
static inline void perf_count(int kind) { (void)kind; }
static inline void perf_present(uint64_t frame) { (void)frame; }
static inline void perf_replay(uint64_t t0) { (void)t0; }
static inline void perf_gpu_wait(uint64_t t0) { (void)t0; }
static inline int perf_gpu_timing(void) { return 0; }
static inline void perf_gpu_busy(uint64_t ns) { (void)ns; }
static inline void prof_write(void) {}
// Memoria del juego: totk-pc-recomp vigila qué páginas escribe la CPU (memory.c). Aquí todavía no: mem_written_pages
// devuelve UINT64_MAX ("sin vigilancia") y el backend copia el rango entero antes de cada envío. Correcto, más lento.
uint64_t mem_written_pages(uint64_t addr, uint64_t size, uint64_t *pages, uint64_t max);
uint64_t mem_written_peek(uint64_t addr, uint64_t size, uint64_t *pages, uint64_t max);
static inline int mem_freeze(uint64_t a) { (void)a; return 0; }
static inline int mem_frozen(uint64_t a) { (void)a; return 0; }
static inline void mem_thaw(uint64_t a, uint64_t n) { (void)a, (void)n; }
static inline void mem_freeze_stats(uint64_t *f, uint64_t *fz, uint64_t *ft, uint64_t *ot) { *f = *fz = *ft = *ot = 0; }
uint32_t mem_set_heap_size(uint64_t size, uint64_t *addr);
// Resolución y DLSS de TotK: sin cambios de tamaño de escena.
static inline float res_dlss_lod_bias(void) { return 0; }
static inline int res_dlss_mode(const char *mode) { (void)mode; return 0; }
void res_sizes(unsigned out[2], unsigned scene[2]);
static inline void res_fov_live(void) {}
static inline int probe_census_dir(char *dir, size_t n) { (void)dir, (void)n; return 0; }
// Vsync del juego: el servicio vi (nxsvc_vi.c) ya la emite; gfx_present.c no tiene eventos registrados.
static inline void kern_signal_system_event(uint64_t ev) { (void)ev; }
