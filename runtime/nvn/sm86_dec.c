/* Decodificador de instrucciones SM86 (Ampere, Switch 2): tools/sm86.py decodificar() en C. Licencia MIT.
 *
 * Por cada código de operación (bits 0-11) las tablas dicen los campos (tramos de bits seguidos de la instrucción
 * que forman el valor), los grupos de bits de modificadores (cada combinación -> nombre completo, o inválida) y
 * las variantes: modificadores que cambian la cantidad de operandos y traen su propio mapa de campos; manda la del
 * conjunto encendido más grande (la primera, si empatan), igual que el max() de Python. */
#include "sm86_dec.h"

typedef struct { uint8_t bit, k, ancho; } Sm86Tramo;
typedef struct { uint8_t campo, n_tramos; uint16_t tramo; } Sm86CampoDef;
typedef struct { uint8_t bit, ancho; uint16_t nombres; } Sm86ModDef;
typedef struct { uint8_t enc[2]; uint8_t n_enc; uint16_t campo; uint8_t n_campos; uint16_t forma; uint8_t n_ops; } Sm86VarDef;
typedef struct {
	uint16_t codigo, nombre, forma, campo;
	uint8_t n_campos, n_ops;
	uint16_t mod;
	uint8_t n_mods, var, n_var;
} Sm86OpDef;

#define SM86_TABLAS_DATOS
#include "sm86_tablas.h"

/* C99: falla al compilar si las tablas traen más campos por forma de los que caben en Sm86Ins */
typedef char sm86_caben_los_campos[(SM86_TABLAS_MAX_CAMPOS <= SM86_MAX_CAMPOS) ? 1 : -1];

/* ancho bits desde el bit b de la instrucción de 128 (b + ancho <= 128, ancho 1-64) */
static inline uint64_t bits128(uint64_t lo, uint64_t hi, unsigned b, unsigned ancho)
{
	uint64_t x;
	if (b >= 64)
		x = hi >> (b - 64);
	else if (b == 0)
		x = lo;
	else
		x = (lo >> b) | (hi << (64 - b));
	return ancho >= 64 ? x : x & ((UINT64_C(1) << ancho) - 1);
}

static inline unsigned bit128(uint64_t lo, uint64_t hi, unsigned b)
{
	return (unsigned)((b < 64 ? lo >> b : hi >> (b - 64)) & 1);
}

static unsigned poner_campos(uint64_t lo, uint64_t hi, unsigned ini, unsigned n, Sm86Ins *out)
{
	for (unsigned i = 0; i < n; i++) {
		const Sm86CampoDef *c = &sm86_campos[ini + i];
		const Sm86Tramo *t = &sm86_tramos[c->tramo];
		uint64_t v = 0;
		for (unsigned j = 0; j < c->n_tramos; j++, t++)
			v |= bits128(lo, hi, t->bit, t->ancho) << t->k;
		out->campo[i] = c->campo;
		out->valor[i] = v;
	}
	return n;
}

int sm86_decodificar(uint64_t lo, uint64_t hi, Sm86Ins *out)
{
	unsigned codigo = (unsigned)(lo & 0xFFF);
	unsigned e = sm86_indice[codigo];
	out->codigo = (uint16_t)codigo;
	out->guarda = (uint8_t)(lo >> 12 & 0xF);
	out->n_campos = 0;
	out->variante = -1;
	if (!e) {
		out->op = -1;
		out->nombre = out->forma = SM86_SIN_NOMBRE;
		out->n_ops = 0;
		return SM86_DESCONOCIDA;
	}
	const Sm86OpDef *op = &sm86_ops[e - 1];
	out->op = (int16_t)(e - 1);

	/* la variante del conjunto encendido más grande; la primera si empatan */
	int elegida = -1;
	unsigned mejor = 0;
	for (unsigned i = 0; i < op->n_var; i++) {
		const Sm86VarDef *v = &sm86_vars[op->var + i];
		if (v->n_enc <= mejor)
			continue;
		unsigned todos = 1;
		for (unsigned j = 0; j < v->n_enc; j++)
			todos &= bit128(lo, hi, v->enc[j]);
		if (todos) {
			elegida = (int)i;
			mejor = v->n_enc;
		}
	}
	if (elegida >= 0) {
		const Sm86VarDef *v = &sm86_vars[op->var + elegida];
		out->variante = (int8_t)elegida;
		out->forma = v->forma;
		out->n_ops = v->n_ops;
		out->n_campos = (uint8_t)poner_campos(lo, hi, v->campo, v->n_campos, out);
	} else {
		out->forma = op->forma;
		out->n_ops = op->n_ops;
		out->n_campos = (uint8_t)poner_campos(lo, hi, op->campo, op->n_campos, out);
	}

	/* el nombre: el del primer grupo de modificadores que lo cambia */
	unsigned nombre = op->nombre;
	out->nombre = op->nombre;
	for (unsigned i = 0; i < op->n_mods; i++) {
		const Sm86ModDef *m = &sm86_mods[op->mod + i];
		unsigned n = sm86_mod_nombres[m->nombres + bits128(lo, hi, m->bit, m->ancho)];
		if (n == SM86_SIN_NOMBRE)
			return SM86_INVALIDA;
		if (nombre == op->nombre)
			nombre = n;
	}
	out->nombre = (uint16_t)nombre;
	return SM86_OK;
}

const char *sm86_cadena(unsigned indice)
{
	return indice < SM86_N_CADENAS ? sm86_cadenas + sm86_cadena_ini[indice] : "";
}

const char *sm86_campo_nombre(unsigned campo, char *buf, unsigned tam)
{
	if (!tam)
		return buf;
	const char *t = campo < SM86_N_CAMPOS ? sm86_tipo_nombres[campo % SM86_N_TIPOS] : "?";
	unsigned op = campo / SM86_N_TIPOS, i = 0;
	char tmp[24];
	tmp[i++] = 'o';
	tmp[i++] = 'p';
	if (op >= 10)
		tmp[i++] = (char)('0' + op / 10 % 10);
	tmp[i++] = (char)('0' + op % 10);
	tmp[i++] = '.';
	while (*t && i < sizeof tmp - 1)
		tmp[i++] = *t++;
	tmp[i] = 0;
	for (i = 0; i + 1 < tam && tmp[i]; i++)
		buf[i] = tmp[i];
	buf[i] = 0;
	return buf;
}

int sm86_campo(const Sm86Ins *ins, unsigned campo, uint64_t *v)
{
	for (unsigned i = 0; i < ins->n_campos; i++)
		if (ins->campo[i] == campo) {
			if (v)
				*v = ins->valor[i];
			return 1;
		}
	return 0;
}

const char *sm86_nombre_base(unsigned codigo)
{
	unsigned e = codigo < 0x1000 ? sm86_indice[codigo] : 0;
	return e ? sm86_cadena(sm86_ops[e - 1].nombre) : 0;
}
