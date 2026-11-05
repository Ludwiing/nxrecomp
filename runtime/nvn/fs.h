// Adaptación de fs.h de totk-pc-recomp: las partidas las guarda nxsvc_fs.c, sin escrituras pendientes en segundo plano.
#pragma once
static inline void fs_save_flush(void) {}
