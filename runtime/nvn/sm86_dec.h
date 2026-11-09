/* Decodificador de instrucciones SM86 (Ampere, la GPU de Switch 2) para traducir shaders al vuelo.
 *
 * Es tools/sm86.py decodificar() en C, con las tablas de sm86_tablas.h (generadas con tools/sm86_c.py desde
 * build/sm86_spec.json): mismo código, nombre con modificadores, guarda, campos y forma base o variante.
 * Sin dependencias del resto del runtime, C99, sin memoria dinámica. Licencia MIT. */
#ifndef SM86_DEC_H
#define SM86_DEC_H

#include <stdint.h>

#include "sm86_tablas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cabe cualquier forma de la especificación (SM86_TABLAS_MAX_CAMPOS; sm86_dec.c lo comprueba) */
#define SM86_MAX_CAMPOS 32

/* Resultados de sm86_decodificar() */
#define SM86_OK 0
#define SM86_DESCONOCIDA (-1)   /* el código de operación (bits 0-11) no está en la especificación */
#define SM86_INVALIDA (-2)      /* combinación de modificadores que nvdisasm no acepta ("invalida" en Python) */

typedef struct Sm86Ins {
	uint16_t codigo;    /* bits 0-11 */
	uint16_t nombre;    /* nombre con modificadores (sm86_cadena()); el base si ningún grupo lo cambia */
	uint16_t forma;     /* texto de la forma base o de la variante elegida (sm86_cadena()) */
	int16_t op;         /* entrada de la especificación (interno) */
	int8_t variante;    /* -1 = forma base; si no, cuál de las variantes del código */
	uint8_t guarda;     /* bits 12-15: 0-2 predicado (7 = PT), 3 negado */
	uint8_t n_ops;      /* operandos de la forma (base o variante) */
	uint8_t n_campos;
	uint8_t campo[SM86_MAX_CAMPOS];   /* enum Sm86Campo: operando * SM86_N_TIPOS + tipo */
	uint64_t valor[SM86_MAX_CAMPOS];
} Sm86Ins;

/* Decodifica la instrucción de 128 bits (lo = bits 0-63, hi = 64-127). Devuelve SM86_OK, SM86_DESCONOCIDA (out solo
 * con codigo y guarda) o SM86_INVALIDA (out con codigo, guarda, campos y forma; nombre = el base). */
int sm86_decodificar(uint64_t lo, uint64_t hi, Sm86Ins *out);

/* Una cadena de la tabla (nombre o forma); "" si el índice no existe */
const char *sm86_cadena(unsigned indice);

/* Nombre del campo como en Python ("op3.neg") en buf; devuelve buf */
const char *sm86_campo_nombre(unsigned campo, char *buf, unsigned tam);

/* Valor del campo en *v si está presente: 1; 0 si la forma no lo tiene */
int sm86_campo(const Sm86Ins *ins, unsigned campo, uint64_t *v);

/* Nombre base del código de operación ("FADD"), o NULL si no está en la especificación */
const char *sm86_nombre_base(unsigned codigo);

#define SM86_CAMPO_OPERANDO(c) ((c) / SM86_N_TIPOS)
#define SM86_CAMPO_TIPO(c) ((c) % SM86_N_TIPOS)
#define SM86_GUARDA_PRED(g) ((g) & 7)
#define SM86_GUARDA_NEG(g) (((g) >> 3) & 1)

#ifdef __cplusplus
}
#endif

#endif /* SM86_DEC_H */
