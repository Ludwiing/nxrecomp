// Módulo "main" de prueba: importa del sdk por PLT y GOT, tiene tablas de punteros a
// funciones propias e importadas, una "vtable", constructores, un símbolo débil que nadie
// define y una importación que nadie define (y que nunca se llama).
#include <stdint.h>

extern void sdk_printf(const char *f, ...);
extern long sdk_add(long a, long b);
extern long sdk_sub(long a, long b);
extern const char *sdk_version(void);
extern int sdk_init_value(void);
extern int sdk_counter;
extern void funcion_que_no_existe(void);
extern void opcional_ausente(void) __attribute__((weak));

static int main_inits;
__attribute__((constructor)) static void init_a(void) { main_inits += 1; }
__attribute__((constructor)) static void init_b(void) { main_inits = main_inits * 10 + 2; }

static long mult(long a, long b) { return a * b; }
static long resto(long a, long b) { return b ? a % b : 0; }

typedef long (*op_fn)(long, long);
op_fn ops[] = {sdk_add, sdk_sub, mult, resto};       // ABS64 hacia el sdk y RELATIVE hacia main
const char *const op_names[] = {"suma (sdk)", "resta (sdk)", "multiplicación", "módulo"};
void (*nunca[])(void) = {funcion_que_no_existe};      // importación sin definir: queda como stub

struct Figura;
struct Vtable {
    const char *(*nombre)(void);
    long (*area)(const struct Figura *);
};
struct Figura {
    const struct Vtable *vt;
    long a, b;
};
static const char *nombre_rect(void) { return "rectángulo"; }
static long area_rect(const struct Figura *f) { return f->a * f->b; }
static const char *nombre_tri(void) { return "triángulo"; }
static long area_tri(const struct Figura *f) { return f->a * f->b / 2; }
static const struct Vtable vt_rect = {nombre_rect, area_rect};
static const struct Vtable vt_tri = {nombre_tri, area_tri};

int nnMain(void) {
    sdk_printf("Dos módulos NSO enlazados entre sí, recompilados a x86_64.\n");
    sdk_printf("versión: %s\n", sdk_version());
    sdk_printf("constructores: main %d, sdk %d\n", main_inits, sdk_init_value());
    sdk_counter += 100;
    sdk_printf("contador del sdk visto desde main: %d\n", sdk_counter);
    for (int i = 0; i < 4; i++) sdk_printf("%s(1000, 7) = %D\n", op_names[i], (int64_t)ops[i](1000, 7));
    struct Figura figs[] = {{&vt_rect, 6, 7}, {&vt_tri, 10, 5}};
    for (int i = 0; i < 2; i++) sdk_printf("%s: área %D\n", figs[i].vt->nombre(), (int64_t)figs[i].vt->area(&figs[i]));
    sdk_printf("símbolo débil ausente: %s\n", opcional_ausente ? "presente" : "vale 0, como en la consola");
    sdk_printf("importación sin definir en la tabla: %s\n", nunca[0] ? "tiene un stub" : "nula");
    sdk_printf("Fin.\n");
    return 0;
}
