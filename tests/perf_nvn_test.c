#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../runtime/nvn/host.h"
#include "../runtime/nvn/cfg.h"

unsigned rt_trace;
void (*perf_gpu_stats)(PerfGpu *out);
static uint64_t fake_now;
static unsigned version;
static const char *overlay = "off";
static char log_text[8192];
static int stats_calls;

uint64_t nx_now_ns(void) { return fake_now; }
unsigned cfg_version(void) { return version; }
const char *cfg_get(const char *name) {
    if (!strcmp(name, "TOTK_PERF_OVERLAY")) return overlay;
    if (!strcmp(name, "TOTK_TRACE")) return "off";
    return NULL;
}
void rt_log(const char *fmt, ...) {
    size_t used = strlen(log_text);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_text + used, sizeof(log_text) - used, fmt, ap);
    va_end(ap);
    strncat(log_text, "\n", sizeof(log_text) - strlen(log_text) - 1);
}

static void mock_gpu_stats(PerfGpu *out) {
    stats_calls++;
    out->have_gpu = 1;
    out->gpu_ns = 5 * 1000 * 1000;
    out->gpu_parts = 60;
    out->gpu_missed = 0;
}

int main(void) {
    assert(!perf_gpu_timing());
    assert(perf_begin() == 0);
    fake_now = 1;
    perf_present(0);
    for (int i = 0; i < 61; i++) {
        fake_now += 16666667;
        perf_present((uint64_t)i + 1);
    }
    assert(!strstr(log_text, "FPS"));

    overlay = "fps";
    version++;
    fake_now += 1;
    perf_present(62);
    for (int i = 0; i < 60; i++) {
        fake_now += 16666667;
        perf_present((uint64_t)i + 63);
    }
    assert(strstr(log_text, "FPS"));
    assert(strstr(log_text, "frametime"));
    assert(!perf_gpu_timing());

    overlay = "full";
    version++;
    perf_gpu_stats = mock_gpu_stats;
    assert(perf_gpu_timing());
    uint64_t t0 = perf_begin();
    fake_now = t0 + 1000000;
    perf_end(PERF_PIPE, t0);
    fake_now += 1;
    perf_present(200);
    for (int i = 0; i < 60; i++) {
        fake_now += 16666667;
        perf_present((uint64_t)i + 201);
    }
    assert(stats_calls == 1);
    assert(strstr(log_text, "GPU busy"));
    assert(strstr(log_text, "pipeline"));
    puts("perf_nvn telemetry/config tests passed");
    return 0;
}
