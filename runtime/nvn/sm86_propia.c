/* Bajada SM86 -> Maxwell propia en el runtime (ver sm86_propia.h). Licencia MIT.
 * Solo POSIX (posix_spawnp, waitpid): el runtime de nxrecomp corre en Linux (runtime/nvn/platform/linux.c). */
#include "sm86_propia.h"

#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define SM86_ESPERA_S 120   /* un programa grande tarda segundos en Python; más que esto es un cuelgue */

int sm86_propia_activa(void)
{
	const char *m = getenv("NX_SM86_BAJADA");
	if (!m || !*m || !strcmp(m, "nxnative"))
		return 0;
	if (!strcmp(m, "propia"))
		return 1;
	fprintf(stderr, "[sm86] NX_SM86_BAJADA=%s no se conoce (propia o nxnative): se usa nxnative\n", m);
	return 0;
}

static int existe(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int sm86_propia_script(char *ruta, size_t cap)
{
	const char *e = getenv("NX_SM86_PROGRAMA");
	ruta[0] = 0;
	if (e && *e) {
		if (strlen(e) >= cap || !existe(e))
			return 0;
		strcpy(ruta, e);
		return 1;
	}
	char exe[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
	if (n <= 0)
		return 0;
	exe[n] = 0;
	for (char *corte = strrchr(exe, '/'); corte; corte = strrchr(exe, '/')) {
		*corte = 0;
		char p[PATH_MAX + 32];
		snprintf(p, sizeof p, "%s/tools/sm86_programa.py", exe);
		if (existe(p) && strlen(p) < cap) {
			strcpy(ruta, p);
			return 1;
		}
	}
	return 0;
}

static uint8_t *leer(const char *p, size_t *tam)
{
	FILE *f = fopen(p, "rb");
	if (!f)
		return NULL;
	uint8_t *b = NULL;
	long n = -1;
	if (!fseek(f, 0, SEEK_END) && (n = ftell(f)) >= 0 && !fseek(f, 0, SEEK_SET) && (b = malloc((size_t)n + 1))) {
		if (fread(b, 1, (size_t)n, f) != (size_t)n)
			free(b), b = NULL;
	}
	fclose(f);
	if (b)
		*tam = (size_t)n;
	return b;
}

static int escribir(const char *p, const uint8_t *d, size_t n)
{
	FILE *f = fopen(p, "wb");
	if (!f)
		return 0;
	int ok = fwrite(d, 1, n, f) == n;
	return fclose(f) == 0 && ok;
}

/* Agrega nombre, largo y contenido de p a *buf. */
static int sumar(uint8_t **buf, size_t *tam, const char *p)
{
	size_t n = 0;
	uint8_t *d = leer(p, &n);
	if (!d)
		return 0;
	const char *nombre = strrchr(p, '/') ? strrchr(p, '/') + 1 : p;
	size_t ln = strlen(nombre) + 1, extra = ln + sizeof(uint64_t) + n;
	uint8_t *nb = realloc(*buf, *tam + extra);
	if (!nb)
		return free(d), 0;
	uint64_t n64 = n;
	memcpy(nb + *tam, nombre, ln);
	memcpy(nb + *tam + ln, &n64, sizeof n64);
	memcpy(nb + *tam + ln + sizeof n64, d, n);
	*buf = nb, *tam += extra;
	free(d);
	return 1;
}

uint8_t *sm86_propia_huella(const char *script, size_t *tam)
{
	char tools[PATH_MAX], p[PATH_MAX + 64];
	snprintf(tools, sizeof tools, "%s", script);
	char *c = strrchr(tools, '/');
	if (!c)
		return NULL;
	*c = 0;
	uint8_t *buf = NULL;
	*tam = 0;
	int ok = sumar(&buf, tam, script);
	snprintf(p, sizeof p, "%s/sm86.py", tools), ok = ok && sumar(&buf, tam, p);
	snprintf(p, sizeof p, "%s/mxisa.json", tools), ok = ok && sumar(&buf, tam, p);
	snprintf(p, sizeof p, "%s/../build/sm86_spec.json", tools), ok = ok && sumar(&buf, tam, p);
	glob_t g;
	snprintf(p, sizeof p, "%s/sm86_baja*.py", tools);
	if (ok && glob(p, 0, NULL, &g) == 0) {   /* glob ordena los nombres: la huella no depende del orden del disco */
		for (size_t i = 0; ok && i < g.gl_pathc; i++)
			ok = sumar(&buf, tam, g.gl_pathv[i]);
		globfree(&g);
	} else {
		ok = 0;
	}
	if (!ok)
		free(buf), buf = NULL, *tam = 0;
	return buf;
}

/* python3 argv... con stdout y stderr en err; código de salida, o -1 si no arrancó / 128+señal / 124 por tiempo. */
static int correr(char *const argv[], char *err, size_t err_cap)
{
	int p[2];
	if (pipe(p))
		return -1;
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, p[1], 1);
	posix_spawn_file_actions_adddup2(&fa, p[1], 2);
	posix_spawn_file_actions_addclose(&fa, p[0]);
	pid_t pid;
	int e = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	close(p[1]);
	if (e) {
		close(p[0]);
		snprintf(err, err_cap, "no se pudo correr %s: %s", argv[0], strerror(e));
		return -1;
	}
	size_t len = 0;
	char resto[512];
	ssize_t k;
	while ((k = read(p[0], len + 1 < err_cap ? err + len : resto,
	                 len + 1 < err_cap ? err_cap - 1 - len : sizeof resto)) != 0) {
		if (k > 0 && len + 1 < err_cap)
			len += (size_t)k;
		else if (k < 0 && errno != EINTR)
			break;
	}
	if (err_cap)
		err[len] = 0;
	close(p[0]);
	int st = 0;
	time_t fin = time(NULL) + SM86_ESPERA_S;
	while (waitpid(pid, &st, WNOHANG) == 0) {
		if (time(NULL) >= fin) {
			kill(pid, SIGKILL);
			waitpid(pid, &st, 0);
			return 124;
		}
		usleep(10000);
	}
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
}

/* La última línea no vacía de s: el motivo de sm86_programa.py (sys.exit) o la excepción de una traza de Python. */
static void motivo(char *s)
{
	size_t n = strlen(s);
	while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
		s[--n] = 0;
	char *l = strrchr(s, '\n');
	if (l)
		memmove(s, l + 1, strlen(l + 1) + 1);
}

int sm86_propia_bajar(const char *script, const char *dir, const char *nombre, const char *etapa, const uint8_t *area,
                      size_t area_tam, const uint8_t *c1, size_t c1_tam, Sm86Bajado *out, char *err, size_t err_cap)
{
	memset(out, 0, sizeof *out);
	if (!script || !*script) {
		snprintf(err, err_cap, "NX_SM86_BAJADA=propia sin tools/sm86_programa.py (NX_SM86_PROGRAMA)");
		return 0;
	}
	char p_area[PATH_MAX], p_c1[PATH_MAX], p_sal[PATH_MAX], p_sal_c1[PATH_MAX + 4];
	snprintf(p_area, sizeof p_area, "%s/%s.sm86", dir, nombre);
	snprintf(p_c1, sizeof p_c1, "%s/%s.sm86c1", dir, nombre);
	snprintf(p_sal, sizeof p_sal, "%s/%s.maxwell", dir, nombre);
	snprintf(p_sal_c1, sizeof p_sal_c1, "%s.c1", p_sal);
	if (!escribir(p_area, area, area_tam) || (c1_tam && !escribir(p_c1, c1, c1_tam))) {
		snprintf(err, err_cap, "no se pudo escribir %s", p_area);
		unlink(p_area), unlink(p_c1);
		return 0;
	}
	char *argv[] = {"python3", (char *)script, "armar", p_area, (char *)etapa, p_sal, c1_tam ? "--c1" : NULL, p_c1,
	                NULL};
	int r = correr(argv, err, err_cap);
	int ok = 0;
	if (r == 0) {
		out->codigo = leer(p_sal, &out->codigo_tam);
		if (c1_tam)
			out->c1 = leer(p_sal_c1, &out->c1_tam);
		ok = out->codigo && (!c1_tam || (out->c1 && out->c1_tam == c1_tam));
		if (!ok)
			snprintf(err, err_cap, "sm86_programa.py no dejó %s%s", p_sal, c1_tam ? " o su .c1" : "");
	} else if (r == 124) {
		snprintf(err, err_cap, "sm86_programa.py tardó más de %d s", SM86_ESPERA_S);
	} else if (r > 0) {
		motivo(err);
		if (!*err)
			snprintf(err, err_cap, "sm86_programa.py salió con %d", r);
	}
	unlink(p_area), unlink(p_c1), unlink(p_sal), unlink(p_sal_c1);
	if (!ok)
		sm86_propia_liberar(out);
	return ok;
}

void sm86_propia_liberar(Sm86Bajado *b)
{
	free(b->codigo), free(b->c1);
	memset(b, 0, sizeof *b);
}
