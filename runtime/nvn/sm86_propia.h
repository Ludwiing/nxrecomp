/* Bajada SM86 (Ampere, Switch 2) -> Maxwell propia de nxrecomp en el runtime, opcional (NX_SM86_BAJADA=propia).
 * Licencia MIT.
 *
 * Por omisión los programas NVN2 de vértice, fragmento y cómputo van al traductor (totk_xlate) con arch=sm86 y los
 * decodifica el decodificador Ampere de nxnative (MIT) dentro del traductor de Ryujinx. Con NX_SM86_BAJADA=propia,
 * gfx_shader.c los baja antes a Maxwell con tools/sm86_programa.py (código propio, el mismo que prueba
 * tests/sm86_programa_test.py) y manda Maxwell al traductor, como un shader de Switch 1. Sirve para comparar las dos
 * vías. Task y mesh no cambian (la bajada propia no arma esas etapas).
 *
 * Una instrucción que la bajada propia no conoce hace fallar ese shader con el texto de tools/sm86_programa.py: no hay
 * vuelta silenciosa a la otra vía. */
#ifndef SM86_PROPIA_H
#define SM86_PROPIA_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t *codigo;            /* gráfico: SPH Maxwell de 0x50 bytes + instrucciones; cómputo: instrucciones */
	size_t codigo_tam;
	uint8_t *c1;                /* c1 reescrito (tablas de BRX con desplazamientos Maxwell); NULL si no había c1 */
	size_t c1_tam;
} Sm86Bajado;

/* 1 si NX_SM86_BAJADA=propia. Otro valor que no sea vacío ni "nxnative" se avisa por stderr y cuenta como 0. */
int sm86_propia_activa(void);

/* La ruta de tools/sm86_programa.py: NX_SM86_PROGRAMA, o la primera carpeta padre del ejecutable que la tenga.
 * 0 si no está (en `ruta` queda vacío). */
int sm86_propia_script(char *ruta, size_t cap);

/* Bytes que identifican la bajada (nombre y contenido de tools/sm86_programa.py, tools/sm86.py, tools/sm86_baja*.py,
 * tools/mxisa.json y build/sm86_spec.json): cambian las claves de la caché de shaders cuando cambia la bajada.
 * malloc; NULL si falta alguno. */
uint8_t *sm86_propia_huella(const char *script, size_t *tam);

/* Baja un área de programa NVN2 (gráfico: SPH de 128 bytes + instrucciones; cómputo: instrucciones) con `python3
 * <script> armar`. Los archivos de paso van a dir/<nombre>.*, y se borran. 1 bien (liberar con sm86_propia_liberar);
 * 0 mal, con el motivo en err. */
int sm86_propia_bajar(const char *script, const char *dir, const char *nombre, const char *etapa, const uint8_t *area,
                      size_t area_tam, const uint8_t *c1, size_t c1_tam, Sm86Bajado *out, char *err, size_t err_cap);

void sm86_propia_liberar(Sm86Bajado *b);

#endif
