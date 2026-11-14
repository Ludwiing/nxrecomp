// Low-overhead performance telemetry for the generic NX2Linux NVN runtime.
// TOTK_PERF_OVERLAY=fps/full or TOTK_TRACE=perf enables log summaries every 60 presents.
// This reports to the runtime log; it does not draw an on-screen overlay.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "host.h"
#include "platform/platform.h"

#define PERF_WINDOW_FRAMES 60

static volatile int64_t frame_wait_ns[PERF_N];
static volatile int64_t window_counts_atomic[PERF_CNT_N];
static volatile int64_t replay_atomic;
static volatile int64_t gpu_wait_atomic;
static volatile int64_t gpu_busy_fallback_atomic;

static uint64_t previous_present_ns, window_start_ns;
static uint64_t frame_count, frame_sum_ns, frame_max_ns;
static uint64_t wait_sum_ns[PERF_N], wait_max_ns[PERF_N];
static uint64_t count_sum[PERF_CNT_N], replay_sum_ns, gpu_wait_sum_ns;
static unsigned config_version_seen;
static int config_seen, overlay_mode, trace_perf_mode;

static void refresh_config(void) {
    unsigned version = cfg_version();
    if (config_seen && version == config_version_seen) return;

    const char *overlay = cfg_get("TOTK_PERF_OVERLAY");
    const char *trace = cfg_get("TOTK_TRACE");
    overlay_mode = !overlay || !strcmp(overlay, "off") ? 0
                 : !strcmp(overlay, "fps") ? 1 : 2;
    trace_perf_mode = trace && (!strcmp(trace, "perf") || !strcmp(trace, "all"));
    config_version_seen = version;
    config_seen = 1;
}

static int telemetry_enabled(void) {
    refresh_config();
    return overlay_mode || trace_perf_mode || (rt_trace & RT_TRACE_PERF);
}

static int detailed_enabled(void) {
    refresh_config();
    return overlay_mode == 2 || trace_perf_mode || (rt_trace & RT_TRACE_PERF);
}

uint64_t perf_begin(void) {
    return telemetry_enabled() ? kern_now_ns() : 0;
}

void perf_end(int kind, uint64_t t0) {
    if (!t0 || kind < 0 || kind >= PERF_N) return;
    uint64_t now = kern_now_ns();
    if (now >= t0) plat_add64(&frame_wait_ns[kind], (int64_t)(now - t0));
}

void perf_replay_thread(void) {}

void perf_count(int kind) {
    if (kind >= 0 && kind < PERF_CNT_N && telemetry_enabled())
        plat_inc64(&window_counts_atomic[kind]);
}

void perf_replay(uint64_t t0) {
    if (!t0 || !telemetry_enabled()) return;
    uint64_t now = kern_now_ns();
    if (now >= t0) plat_add64(&replay_atomic, (int64_t)(now - t0));
}

void perf_gpu_wait(uint64_t t0) {
    if (!t0 || !telemetry_enabled()) return;
    uint64_t now = kern_now_ns();
    if (now >= t0) plat_add64(&gpu_wait_atomic, (int64_t)(now - t0));
}

int perf_gpu_timing(void) { return detailed_enabled(); }

void perf_gpu_busy(uint64_t ns) {
    if (telemetry_enabled()) plat_add64(&gpu_busy_fallback_atomic, (int64_t)ns);
}

static void reset_window(void) {
    frame_count = frame_sum_ns = frame_max_ns = 0;
    replay_sum_ns = gpu_wait_sum_ns = 0;
    memset(wait_sum_ns, 0, sizeof wait_sum_ns);
    memset(wait_max_ns, 0, sizeof wait_max_ns);
    memset(count_sum, 0, sizeof count_sum);
}

static void report_window(const char *label, uint64_t span_ns) {
    if (!frame_count || !frame_sum_ns) return;
    double average_ms = (double)frame_sum_ns / (double)frame_count / 1e6;
    double fps = (double)frame_count * 1e9 / (double)frame_sum_ns;
    rt_log("perf: %s, %llu frames: %.2f FPS, frametime %.2f ms avg / %.2f ms max",
           label, (unsigned long long)frame_count, fps, average_ms, (double)frame_max_ns / 1e6);

    if (!detailed_enabled()) return;
    PerfGpu gpu = {0};
    void (*stats)(PerfGpu *) = perf_gpu_stats;
    if (stats) stats(&gpu);
    uint64_t busy_ns = gpu.have_gpu ? gpu.gpu_ns
                                   : (uint64_t)plat_xchg64(&gpu_busy_fallback_atomic, 0);
    rt_log("perf: CPU spans ms/frame pace %.2f fence %.2f drain %.2f pipeline %.2f shader-xlate %.2f texture %.2f ASTC %.2f; replay %.2f; fence-wait %.2f",
           wait_sum_ns[PERF_PACE] / 1e6 / frame_count,
           wait_sum_ns[PERF_FENCE] / 1e6 / frame_count,
           wait_sum_ns[PERF_DRAIN] / 1e6 / frame_count,
           wait_sum_ns[PERF_PIPE] / 1e6 / frame_count,
           wait_sum_ns[PERF_XLATE] / 1e6 / frame_count,
           wait_sum_ns[PERF_TEX] / 1e6 / frame_count,
           wait_sum_ns[PERF_ASTC] / 1e6 / frame_count,
           replay_sum_ns / 1e6 / frame_count, gpu_wait_sum_ns / 1e6 / frame_count);
    if (gpu.have_gpu) {
        rt_log("perf: GPU busy %.2f ms/frame (%.0f%% of %.2f ms frametime), timestamp parts %llu, missed %llu; idle/starved/queued %.2f/%.2f/%.2f ms/frame",
               busy_ns / 1e6 / frame_count,
               span_ns ? 100.0 * (double)busy_ns / (double)span_ns : 0.0,
               span_ns / 1e6 / frame_count,
               (unsigned long long)gpu.gpu_parts, (unsigned long long)gpu.gpu_missed,
               gpu.have_idle ? gpu.idle_ns / 1e6 / frame_count : 0.0,
               gpu.have_idle ? gpu.starved_ns / 1e6 / frame_count : 0.0,
               gpu.have_idle ? gpu.queued_ns / 1e6 / frame_count : 0.0);
    } else {
        rt_log("perf: GPU timestamps unavailable; CPU spans above do not establish GPU utilization");
    }
    if (gpu.have_vram)
        rt_log("perf: VRAM %llu / %llu MiB%s",
               (unsigned long long)(gpu.vram_used >> 20),
               (unsigned long long)(gpu.vram_budget >> 20),
               gpu.vram_exact ? "" : " (estimated)");
    rt_log("perf: max single-frame spans ms pace %.2f fence %.2f drain %.2f pipeline %.2f shader-xlate %.2f texture %.2f ASTC %.2f; pipelines async/skip %llu/%llu, texture uploads/reuploads %llu/%llu",
           wait_max_ns[PERF_PACE] / 1e6, wait_max_ns[PERF_FENCE] / 1e6,
           wait_max_ns[PERF_DRAIN] / 1e6, wait_max_ns[PERF_PIPE] / 1e6,
           wait_max_ns[PERF_XLATE] / 1e6, wait_max_ns[PERF_TEX] / 1e6,
           wait_max_ns[PERF_ASTC] / 1e6,
           (unsigned long long)count_sum[PERF_CNT_PIPE_ASYNC],
           (unsigned long long)count_sum[PERF_CNT_PIPE_SKIP],
           (unsigned long long)count_sum[PERF_CNT_TEX_UP],
           (unsigned long long)count_sum[PERF_CNT_TEX_REUP]);
}

void perf_present(uint64_t frame) {
    uint64_t now = kern_now_ns();
    if (!telemetry_enabled()) {
        previous_present_ns = window_start_ns = 0;
        return;
    }
    if (!previous_present_ns) {
        previous_present_ns = window_start_ns = now;
        return;
    }

    uint64_t interval = now >= previous_present_ns ? now - previous_present_ns : 0;
    previous_present_ns = now;
    frame_count++;
    frame_sum_ns += interval;
    if (interval > frame_max_ns) frame_max_ns = interval;

    for (int i = 0; i < PERF_N; i++) {
        uint64_t elapsed = (uint64_t)plat_xchg64(&frame_wait_ns[i], 0);
        wait_sum_ns[i] += elapsed;
        if (elapsed > wait_max_ns[i]) wait_max_ns[i] = elapsed;
    }
    for (int i = 0; i < PERF_CNT_N; i++)
        count_sum[i] += (uint64_t)plat_xchg64(&window_counts_atomic[i], 0);
    replay_sum_ns += (uint64_t)plat_xchg64(&replay_atomic, 0);
    gpu_wait_sum_ns += (uint64_t)plat_xchg64(&gpu_wait_atomic, 0);

    if (frame_count >= PERF_WINDOW_FRAMES) {
        uint64_t span_ns = now >= window_start_ns ? now - window_start_ns : frame_sum_ns;
        report_window("60-present window", span_ns);
        reset_window();
        window_start_ns = now;
    }
    (void)frame;
}

void prof_write(void) {
    if (frame_count) report_window("partial window", frame_sum_ns);
}
