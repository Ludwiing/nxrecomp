/* Driver de tests/sm86_propia_test.py: runtime/nvn/sm86_propia.c sin el resto del runtime. Licencia MIT.
 *
 *   sm86_propia_test bajar <script|-> <dir> <etapa> <área> [c1]   -> <dir>/salida.bin (+ salida.c1); "error: ..." y 1
 *   sm86_propia_test activa                                       -> 0 o 1 según NX_SM86_BAJADA
 *   sm86_propia_test script                                       -> la ruta que halla (NX_SM86_PROGRAMA o el exe)
 *   sm86_propia_test huella <script>                              -> largo y FNV-1a de 64 bits de la huella */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sm86_propia.h"

static unsigned char *leer(const char *p, size_t *n)
{
	FILE *f = fopen(p, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long t = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *b = malloc((size_t)t + 1);
	*n = b ? fread(b, 1, (size_t)t, f) : 0;
	fclose(f);
	return b;
}

static void escribir(const char *p, const void *d, size_t n)
{
	FILE *f = fopen(p, "wb");
	if (f)
		fwrite(d, 1, n, f), fclose(f);
}

int main(int argc, char **argv)
{
	if (argc >= 2 && !strcmp(argv[1], "activa")) {
		printf("%d\n", sm86_propia_activa());
		return 0;
	}
	if (argc >= 2 && !strcmp(argv[1], "script")) {
		char r[4096];
		printf("%d %s\n", sm86_propia_script(r, sizeof r), r);
		return 0;
	}
	if (argc >= 3 && !strcmp(argv[1], "huella")) {
		size_t n = 0;
		unsigned char *h = sm86_propia_huella(argv[2], &n);
		if (!h) {
			printf("sin huella\n");
			return 1;
		}
		unsigned long long x = 0xcbf29ce484222325ull;
		for (size_t i = 0; i < n; i++)
			x = (x ^ h[i]) * 0x100000001b3ull;
		printf("%zu %016llx\n", n, x);
		free(h);
		return 0;
	}
	if (argc < 6 || strcmp(argv[1], "bajar")) {
		fprintf(stderr, "uso: ver la cabecera de tests/sm86_propia_test.c\n");
		return 2;
	}
	size_t area_n = 0, c1_n = 0;
	unsigned char *area = leer(argv[5], &area_n), *c1 = argc > 6 ? leer(argv[6], &c1_n) : NULL;
	if (!area) {
		fprintf(stderr, "no se pudo leer %s\n", argv[5]);
		return 2;
	}
	Sm86Bajado b;
	char err[1024];
	const char *script = strcmp(argv[2], "-") ? argv[2] : "";
	if (!sm86_propia_bajar(script, argv[3], "prueba", argv[4], area, area_n, c1, c1_n, &b, err, sizeof err)) {
		printf("error: %s\n", err);
		return 1;
	}
	char p[4096];
	snprintf(p, sizeof p, "%s/salida.bin", argv[3]);
	escribir(p, b.codigo, b.codigo_tam);
	if (b.c1) {
		snprintf(p, sizeof p, "%s/salida.c1", argv[3]);
		escribir(p, b.c1, b.c1_tam);
	}
	sm86_propia_liberar(&b);
	free(area), free(c1);
	return 0;
}
