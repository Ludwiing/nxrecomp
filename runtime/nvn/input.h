// Adaptación de input.h de totk-pc-recomp: la entrada la maneja nxinput.c (con la ventana de nxdisplay.c).
#pragma once
static inline int input_flight_key(void) { return 0; }   // flight.c: la tecla de su grabador de vuelo (no hay)
static inline void input_cursor_suspend(int on) { (void)on; }
