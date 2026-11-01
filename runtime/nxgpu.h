// La GPU (nxgpu.c): espacio de direcciones, syncpoints y el procesador de comandos.
#pragma once
#include <stdint.h>

uint64_t nx_gpu_alloc_va(uint64_t size, uint64_t align);
void nx_gpu_map(uint64_t gpu, uint64_t size, uint64_t cpu);
void nx_gpu_unmap(uint64_t gpu);
void *nx_gpu_ptr(uint64_t gpu);

uint32_t nx_syncpt_read(uint32_t id);
void nx_syncpt_incr(uint32_t id);
int nx_syncpt_wait(uint32_t id, uint32_t threshold, int32_t timeout_us); // 1 si llegó

void nx_gpu_submit(const uint64_t *gpfifo_entries, uint32_t n);
