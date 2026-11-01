// Runtime de nxrecomp: carga la imagen del módulo en su dirección de la Switch,
// lo que el código traducido no puede hacer solo: registros de sistema, trampas, el reloj.
#define _GNU_SOURCE
#include "nxrt.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define TICK_HZ 19200000ull            // el contador de la Switch va a 19,2 MHz

void nx_map_fixed(uint64_t addr, uint64_t size, const char *what) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "[nxrt] no pude mapear %s en 0x%llx (%llu bytes)\n", what,
                (unsigned long long)addr, (unsigned long long)size);
        exit(1);
    }
}

uint64_t nx_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t nx_ticks(void) {
    uint64_t ns = nx_now_ns();
    return ns / 1000000000ull * TICK_HZ + ns % 1000000000ull * TICK_HZ / 1000000000ull;
}

uint64_t nx_mrs(Ctx *c, int reg) {
    switch (reg) {
    case NX_TPIDR_EL0: return c->tpidr;
    case NX_TPIDRRO_EL0: return c->tpidrro;
    case NX_CNTFRQ_EL0: return TICK_HZ;
    case NX_CNTPCT_EL0:
    case NX_CNTVCT_EL0: return nx_ticks();
    case NX_DCZID_EL0: return 4; // DC ZVA borra bloques de 64 bytes
    case NX_NZCV: return (uint64_t)c->n << 31 | (uint64_t)c->z << 30 | (uint64_t)c->c << 29 | (uint64_t)c->v << 28;
    case NX_FPCR: return c->fpcr;
    case NX_FPSR: return c->fpsr;
    case NX_CTR_EL0: return 0x8444c004; // líneas de caché de 64 bytes, como el Cortex-A57
    }
    return 0;
}

void nx_msr(Ctx *c, int reg, uint64_t v) {
    switch (reg) {
    case NX_TPIDR_EL0: c->tpidr = v; break;
    case NX_NZCV: c->n = v >> 31 & 1; c->z = v >> 30 & 1; c->c = v >> 29 & 1; c->v = v >> 28 & 1; break;
    case NX_FPCR: c->fpcr = v; break;
    case NX_FPSR: c->fpsr = v; break;
    default: fprintf(stderr, "[RED] escritura a registro de sistema %d ignorada\n", reg);
    }
}

void nx_dc_zva(Ctx *c, uint64_t addr) {
    (void)c;
    memset((void *)(uintptr_t)(addr & ~63ull), 0, 64);
}

void nx_trap(Ctx *c, uint64_t pc, uint32_t word, const char *why) {
    (void)c;
    fflush(stdout);
    fprintf(stderr, "[RED] instrucción 0x%08x en 0x%llx: %s\n", word, (unsigned long long)pc, why);
    exit(1);
}

// Un salto indirecto a una dirección que el recompilador no vio como entrada. No se puede seguir,
// pero se anota en NX_PISTAS (si está definido): la próxima recompilación la toma como función
// (nxrecomp.py --pistas) y el juego pasa de ahí. NX2Linux lo hace solo.
void nx_bad_jump(Ctx *c, uint64_t from, uint64_t to) {
    (void)c;
    fflush(stdout);
    fprintf(stderr, "[RED] salto indirecto a 0x%llx (desde 0x%llx): no es una función recompilada\n",
            (unsigned long long)to, (unsigned long long)from);
    const char *pistas = getenv("NX_PISTAS");
    FILE *f = pistas && *pistas ? fopen(pistas, "a") : NULL;
    if (f) {
        fprintf(f, "0x%llx\n", (unsigned long long)to);
        fclose(f);
        fprintf(stderr, "[nxrt] anotado en %s: recompila con --pistas para seguir\n", pistas);
    }
    exit(3);
}

void nx_import_call(Ctx *c, int index) {
    (void)c;
    fflush(stdout);
    fprintf(stderr, "[RED] se llamó a %s, que ningún módulo define y el runtime todavía no implementa\n",
            nx_imports[index]);
    exit(1);
}

uint64_t nx_symbol(const char *name) {
    for (int k = 0; k < nx_nsymbols; k++)
        if (!strcmp(nx_symbols[k].name, name)) return nx_symbols[k].addr;
    return 0;
}

// ---------- diagnóstico de fallos ----------
// Si el código traducido accede a memoria inválida, se dice dónde: la dirección que falló,
// la última instrucción ARM ejecutada (con --traza) y los registros del hilo.
static __thread Ctx *crash_ctx;
void nx_set_crash_ctx(Ctx *c) { crash_ctx = c; }

static void on_crash(int sig, siginfo_t *si, void *uc) {
    (void)uc;
    fflush(stdout);
    fprintf(stderr, "\n[nxrt] %s en la dirección 0x%llx\n", sig == SIGSEGV ? "violación de segmento" : "error de bus",
            (unsigned long long)(uintptr_t)si->si_addr);
    Ctx *c = crash_ctx;
    if (c) {
        fprintf(stderr, "[nxrt] última instrucción ARM: 0x%llx (0 si no se compiló con --traza)\n", (unsigned long long)c->pc);
        for (int k = 0; k < 31; k += 4) {
            for (int j = k; j < k + 4 && j < 31; j++) fprintf(stderr, "  x%-2d %016llx", j, (unsigned long long)c->x[j]);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "  sp  %016llx\n", (unsigned long long)c->sp);
    }
    _exit(128 + sig);
}

void nx_install_crash_handler(void) {
    static char altstack[64 * 1024];
    stack_t ss = {.ss_sp = altstack, .ss_size = sizeof altstack};
    sigaltstack(&ss, NULL);
    struct sigaction sa = {0};
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

// Con nxrecomp --traza y NX_TRAZA_FUNC=1: cada función traducida avisa al entrar.
void nx_trace_func(Ctx *c, uint64_t addr) {
    static int on = -1;
    if (on < 0) on = getenv("NX_TRAZA_FUNC") != NULL;
    if (on) fprintf(stderr, "[func] 0x%llx (x30 0x%llx)\n", (unsigned long long)addr, (unsigned long long)c->x[30]);
}
