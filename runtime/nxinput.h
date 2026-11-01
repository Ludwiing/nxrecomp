// Mandos (nxinput.c): teclado y mandos de SDL hacia la memoria compartida de hid.
#pragma once
#include <stdint.h>

void nx_input_attach(uint8_t *hid_shmem); // cuando el juego pide la memoria compartida de hid
void nx_input_event(const void *sdl_event); // eventos de la ventana (SDL_Event)
void nx_input_frame(int frame);             // en cada cuadro: el guion de pruebas NX_PULSAR
