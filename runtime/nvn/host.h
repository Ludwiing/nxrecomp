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

// El planificador de núcleos virtuales de totk-pc-recomp: aquí cada hilo del juego es un hilo del host, no hace falta.
static inline int kern_core_away(void) { return 0; }
static inline void kern_core_back(int *away) { (void)away; }

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
