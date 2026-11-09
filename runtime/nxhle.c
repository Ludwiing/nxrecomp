// Funciones del juego sustituidas con "símbolo !" (recomp/sustituciones*.txt): devuelven 0 y anotan en stderr la
// primera llamada a cada una, para saber qué parte del SDK usa el juego y todavía no tiene implementación propia.
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "nxrt.h"

#define MAX_AVISOS 4096

static const char *avisados[MAX_AVISOS];
static int n_avisados;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void nxhle_aviso(Ctx *c, const char *simbolo) {
    c->x[0] = 0;
    pthread_mutex_lock(&lock);
    int visto = 0;
    for (int i = 0; i < n_avisados && !visto; i++) visto = avisados[i] == simbolo || !strcmp(avisados[i], simbolo);
    if (!visto && n_avisados < MAX_AVISOS) avisados[n_avisados++] = simbolo;
    pthread_mutex_unlock(&lock);
    if (!visto) fprintf(stderr, "[hle] %s: sin implementar, devuelve 0\n", simbolo);
}
