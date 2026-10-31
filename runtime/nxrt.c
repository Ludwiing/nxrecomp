// Runtime de nxrecomp: carga la imagen del módulo en su dirección de la Switch,
// prepara la pila y el TLS, y responde las llamadas al kernel (SVC) de Horizon.
#define _GNU_SOURCE
#include "nxrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define STACK_SIZE 0x100000ull         // 1 MiB, como el hilo principal de un homebrew
#define STACK_TOP 0x40000000ull
#define TLS_BASE 0x3FE00000ull         // región de 0x200 bytes a la que apunta TPIDRRO_EL0
#define TICK_HZ 19200000ull            // el contador de la Switch va a 19,2 MHz

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void map_fixed(uint64_t addr, uint64_t size, const char *what) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "[nxrt] no pude mapear %s en 0x%llx (%llu bytes)\n", what,
                (unsigned long long)addr, (unsigned long long)size);
        exit(1);
    }
}

static uint64_t ticks(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * TICK_HZ + (uint64_t)ts.tv_nsec * TICK_HZ / 1000000000ull;
}

void nx_svc(Ctx *c, uint32_t imm) {
    switch (imm) {
    case 0x07: // ExitProcess
        exit(0);
    case 0x1E: // GetSystemTick
        c->x[0] = ticks();
        return;
    case 0x26: // Break
        fprintf(stderr, "[nxrt] svcBreak(0x%llx, 0x%llx, 0x%llx)\n", (unsigned long long)c->x[0],
                (unsigned long long)c->x[1], (unsigned long long)c->x[2]);
        exit(1);
    case 0x27: // OutputDebugString(ptr, len)
        fwrite((const void *)(uintptr_t)c->x[0], 1, c->x[1], stdout);
        fflush(stdout);
        c->x[0] = 0;
        return;
    default:
        fprintf(stderr, "[RED] svc 0x%x no implementado\n", imm);
        exit(1);
    }
}

uint64_t nx_mrs(Ctx *c, int reg) {
    switch (reg) {
    case NX_TPIDR_EL0: return c->tpidr;
    case NX_TPIDRRO_EL0: return c->tpidrro;
    case NX_CNTFRQ_EL0: return TICK_HZ;
    case NX_CNTPCT_EL0:
    case NX_CNTVCT_EL0: return ticks();
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

void nx_bad_jump(Ctx *c, uint64_t from, uint64_t to) {
    (void)c;
    fflush(stdout);
    fprintf(stderr, "[RED] salto indirecto a 0x%llx (desde 0x%llx): no es una función recompilada\n",
            (unsigned long long)to, (unsigned long long)from);
    exit(1);
}

int main(void) {
    const NxImage *im = &nx_image;
    uint64_t img_end = align_up(im->bss_start + im->bss_size, 0x1000);
    map_fixed(im->base, img_end - im->base, "el módulo");
    memcpy((void *)(uintptr_t)im->base, im->data, im->size); // el .bss ya viene en ceros
    map_fixed(STACK_TOP - STACK_SIZE, STACK_SIZE, "la pila");
    map_fixed(TLS_BASE, 0x1000, "el TLS");

    Ctx c = {0};
    c.sp = STACK_TOP;
    c.tpidrro = TLS_BASE;
    c.excl_addr = ~0ull;
    c.x[1] = 0xFFFF8001; // pseudo-handle del hilo actual, como lo entrega el cargador

    nx_entry(&c);
    fflush(stdout);
    fprintf(stderr, "[nxrt] el programa volvió de su entrada sin llamar a svcExitProcess\n");
    return 1;
}
