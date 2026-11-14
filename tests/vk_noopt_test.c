// Safe shader compilation (runtime/nvn/vk_noopt.h) with the real settings table (cfg.c): the two keys have rows (else
// the runtime would rt_fatal at its first pipeline), the default leaves every pipeline optimized, TOTK_VK_NOOPT_WORDS
// turns optimization off only above its size and TOTK_VK_NOOPT=1 for all. Each mode runs in its own process because
// the settings are read once.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "cfg.h"
#include "host.h"
#include "platform/platform.h"
#include "vk_noopt.h"

void rt_log(const char *fmt, ...) { (void)fmt; }
void rt_fatal(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    va_end(a);
    fputc('\n', stderr);
    exit(1);
}
int plat_exe_path(char *out, size_t cap) { (void)out; (void)cap; return 0; }
int plat_file_exists(const char *path) { (void)path; return 0; }
uint8_t *plat_file_read(const char *path, uint64_t *size) { (void)path; (void)size; return NULL; }
int plat_file_write_atomic(const char *path, const void *data, size_t n) { (void)path; (void)data; (void)n; return 0; }
int64_t plat_file_size(const char *path) { (void)path; return -1; }
void plat_mutex_lock(PlatMutex *m) { (void)m; }
void plat_mutex_unlock(PlatMutex *m) { (void)m; }

static int fallos;
static void ver(const char *que, int ok) {
    printf("%s: %s\n", ok ? "OK" : "MAL", que);
    fallos += !ok;
}

#define NOOPT VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT

static int modo(const char *m) {
    if (!strcmp(m, "defecto")) {
        ver("hay fila TOTK_VK_NOOPT", cfg_meta("TOTK_VK_NOOPT") != NULL);
        ver("hay fila TOTK_VK_NOOPT_WORDS", cfg_meta("TOTK_VK_NOOPT_WORDS") != NULL);
        ver("por defecto, ni un shader de 10 M palabras pierde la optimización", vk_noopt_flags(10000000) == 0);
    } else if (!strcmp(m, "umbral")) {
        ver("umbral 5000: 5000 palabras sigue optimizado", vk_noopt_flags(5000) == 0);
        ver("umbral 5000: 5001 palabras sin optimización", vk_noopt_flags(5001) == NOOPT);
        ver("umbral 5000: pipeline sin shaders sigue optimizada", vk_noopt_flags(0) == 0);
    } else if (!strcmp(m, "todo")) {
        ver("TOTK_VK_NOOPT=1: también la pipeline sin shaders", vk_noopt_flags(0) == NOOPT);
        ver("TOTK_VK_NOOPT=1: un shader de 1 palabra", vk_noopt_flags(1) == NOOPT);
    }
    return fallos != 0;
}

int main(int argc, char **argv) {
    if (argc > 1) return modo(argv[1]);
    static const char *const casos[][2] = {
        {"defecto", "env -u TOTK_VK_NOOPT -u TOTK_VK_NOOPT_WORDS"},
        {"umbral", "env -u TOTK_VK_NOOPT TOTK_VK_NOOPT_WORDS=5000"},
        {"todo", "env -u TOTK_VK_NOOPT_WORDS TOTK_VK_NOOPT=1"},
    };
    int mal = 0;
    for (size_t i = 0; i < sizeof casos / sizeof *casos; i++) {
        char cmd[512];
        snprintf(cmd, sizeof cmd, "%s '%s' %s", casos[i][1], argv[0], casos[i][0]);
        int r = system(cmd);
        mal |= !WIFEXITED(r) || WEXITSTATUS(r) != 0;
    }
    puts(mal ? "vk_noopt_test: MAL" : "vk_noopt_test: OK");
    return mal;
}
