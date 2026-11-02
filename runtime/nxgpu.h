// La GPU (nxgpu.c): espacio de direcciones, syncpoints y el procesador de comandos.
#pragma once
#include <stdint.h>

uint64_t nx_gpu_alloc_va(uint64_t size, uint64_t align);
void nx_gpu_map(uint64_t gpu, uint64_t size, uint64_t cpu);
void nx_gpu_unmap(uint64_t gpu);
void *nx_gpu_ptr(uint64_t gpu);
void *nx_gpu_ptr_n(uint64_t gpu, uint64_t *avail); // y cuántos bytes siguen mapeados desde ahí

// Offset de un byte (x en bytes, fila y) dentro de una superficie "block linear": bloques de GOBs de
// 64 bytes x 8 filas, `1 << bh_log2` GOBs de alto; `width_gobs` = ancho en bytes / 64, redondeado.
static inline uint64_t nx_bl_offset(uint32_t xb, uint32_t y, uint32_t width_gobs, uint32_t bh_log2) {
    uint32_t bh = 1u << bh_log2;
    uint64_t block = (uint64_t)(y / (8 * bh)) * width_gobs * bh * 512 + (uint64_t)(xb / 64) * bh * 512 +
                     (uint64_t)((y % (8 * bh)) / 8) * 512;
    uint32_t gx = xb % 64, gy = y % 8;
    return block + (gx / 32) * 256 + (gy / 2) * 64 + ((gx % 32) / 16) * 32 + (gy % 2) * 16 + gx % 16;
}

uint32_t nx_syncpt_read(uint32_t id);
void nx_syncpt_incr(uint32_t id);
int nx_syncpt_wait(uint32_t id, uint32_t threshold, int32_t timeout_us); // 1 si llegó

void nx_gpu_unmap_rango(uint64_t gpu, uint64_t size); // deja sin mapear un rango (Remap con handle 0)
// Copia `size` bytes desde la dirección de GPU `gpu` aunque crucen varios mapeos; lo no mapeado lee
// ceros (como un pool sparse). Devuelve los bytes que sí estaban mapeados.
uint64_t nx_gpu_leer(uint64_t gpu, void *dst, uint64_t size);
void nx_gpu_bloquear(void);   // el candado de la GPU (recursivo)
void nx_gpu_desbloquear(void);
void nx_gpu_submit(uint32_t canal, const uint64_t *gpfifo_entries, uint32_t n); // canal: uno por /dev/nvhost-gpu abierto
