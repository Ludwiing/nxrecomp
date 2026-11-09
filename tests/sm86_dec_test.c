/* Prueba del decodificador SM86 en C contra el de Python: lee los casos que arma tests/sm86_dec_test.py (una
 * instrucción por línea con lo que dio tools/sm86.py decodificar()) y compara código de retorno, nombre con
 * modificadores, guarda, forma base o variante y cada campo con su valor. Después mide la velocidad.
 * Sale con 0 si no hay diferencias, 1 si las hay, 77 si no hay archivo de casos. Licencia MIT. */
#define _POSIX_C_SOURCE 199309L   /* clock_gettime con -std=c99 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sm86_dec.h"

#define MAX_CASOS 1000000

static uint64_t los[MAX_CASOS], his[MAX_CASOS];

static void guarda_txt(unsigned g, char *buf)
{
	char *p = buf;
	if (g & 8)
		*p++ = '!';
	if ((g & 7) == 7) {
		*p++ = 'P';
		*p++ = 'T';
	} else {
		*p++ = 'P';
		*p++ = (char)('0' + (g & 7));
	}
	*p = 0;
}

/* corta s en el siguiente tabulador; devuelve el resto o NULL */
static char *cortar(char *s)
{
	char *t = s ? strchr(s, '\t') : NULL;
	if (!t)
		return NULL;
	*t = 0;
	return t + 1;
}

int main(int argc, char **argv)
{
	FILE *f = fopen(argc > 1 ? argv[1] : "build/sm86_dec_casos.txt", "r");
	if (!f) {
		fprintf(stderr, "sin archivo de casos (lo arma tests/sm86_dec_test.py)\n");
		return 77;
	}
	static char l[1 << 14];
	unsigned n = 0, mal = 0, por_ret[3] = {0, 0, 0}, variantes = 0, campos = 0;
	while (fgets(l, sizeof l, f) && n < MAX_CASOS) {
		l[strcspn(l, "\n")] = 0;
		char *s_hi = cortar(l), *s_ret = cortar(s_hi), *s_nombre = cortar(s_ret);
		char *s_guarda = cortar(s_nombre), *s_forma = cortar(s_guarda), *s_campos = cortar(s_forma);
		if (!s_ret)
			continue;
		uint64_t lo = strtoull(l, NULL, 16), hi = strtoull(s_hi, NULL, 16);
		int ret_py = atoi(s_ret);
		los[n] = lo;
		his[n] = hi;
		n++;
		Sm86Ins ins;
		int r = sm86_decodificar(lo, hi, &ins);
		char err[512] = "";
		if (r != ret_py) {
			snprintf(err, sizeof err, "resultado %d, Python %d", r, ret_py);
		} else if (r == SM86_OK) {
			por_ret[0]++;
			char g[8];
			guarda_txt(ins.guarda, g);
			variantes += ins.variante >= 0;
			if (!s_campos)
				snprintf(err, sizeof err, "línea mal formada");
			else if (strcmp(sm86_cadena(ins.nombre), s_nombre))
				snprintf(err, sizeof err, "nombre %s, Python %s", sm86_cadena(ins.nombre), s_nombre);
			else if (strcmp(g, s_guarda))
				snprintf(err, sizeof err, "guarda %s, Python %s", g, s_guarda);
			else if (strcmp(sm86_cadena(ins.forma), s_forma))
				snprintf(err, sizeof err, "forma %s, Python %s", sm86_cadena(ins.forma), s_forma);
			else {
				/* cada campo de Python, presente en C con el mismo valor; y la misma cantidad */
				unsigned cuantos = 0;
				char *c = s_campos;
				while (c && *c && !err[0]) {
					char *fin = strchr(c, ' ');
					if (fin)
						*fin = 0;
					char *igual = strchr(c, '=');
					if (!igual) {
						snprintf(err, sizeof err, "campo mal formado: %s", c);
						break;
					}
					*igual = 0;
					uint64_t v_py = strtoull(igual + 1, NULL, 16), v;
					unsigned id = 0;
					char nom[32];
					while (id < SM86_N_CAMPOS && strcmp(sm86_campo_nombre(id, nom, sizeof nom), c))
						id++;
					if (id == SM86_N_CAMPOS)
						snprintf(err, sizeof err, "campo %s de Python sin enum", c);
					else if (!sm86_campo(&ins, id, &v))
						snprintf(err, sizeof err, "falta el campo %s", c);
					else if (v != v_py)
						snprintf(err, sizeof err, "%s = 0x%llx, Python 0x%llx", c, (unsigned long long)v,
							 (unsigned long long)v_py);
					cuantos++;
					c = fin ? fin + 1 : NULL;
				}
				if (!err[0] && cuantos != ins.n_campos)
					snprintf(err, sizeof err, "%u campos, Python %u", ins.n_campos, cuantos);
				campos += cuantos;
			}
		} else {
			por_ret[r == SM86_DESCONOCIDA ? 1 : 2]++;
		}
		if (err[0]) {
			if (mal < 20)
				fprintf(stderr, "MAL %016llx %016llx (%s): %s\n", (unsigned long long)lo,
					(unsigned long long)hi, sm86_nombre_base(lo & 0xFFF) ? sm86_nombre_base(lo & 0xFFF) : "?",
					err);
			mal++;
		}
	}
	fclose(f);
	if (!n) {
		fprintf(stderr, "archivo de casos vacío\n");
		return 1;
	}

	/* velocidad: todas las instrucciones una y otra vez durante ~0,3 s */
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	unsigned long long hechas = 0, suma = 0;
	double seg = 0;
	do {
		for (unsigned i = 0; i < n; i++) {
			Sm86Ins ins;
			suma += (unsigned)sm86_decodificar(los[i], his[i], &ins) + ins.nombre + ins.n_campos;
		}
		hechas += n;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		seg = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
	} while (seg < 0.3);

	printf("%u instrucciones (%u decodificadas, %u con variante, %u campos; %u códigos desconocidos; "
	       "%u combinaciones inválidas): %u diferencias con Python\n",
	       n, por_ret[0], variantes, campos, por_ret[1], por_ret[2], mal);
	printf("velocidad: %.1f millones de instrucciones/s (suma %llu)\n", (double)hechas / seg / 1e6, suma & 0xFF);
	return mal ? 1 : 0;
}
