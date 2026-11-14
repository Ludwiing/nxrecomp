// LIFO de npad frente al lector del SDK de Switch 2 (nn::hid::detail::GetNpadStates, SDK 20.5 de Mario Tennis
// Fever, sdk+0x93d0540 en la build local): entrada = [número u64][estado 0x28], LIFO en npad+0x28 con la cola en
// +0x10 y la cuenta en +0x18. El lector reintenta si el número de la entrada es impar o cambia mientras copia, y
// exige que dos estados seguidos difieran en 1 en su número de muestreo. Antes del arreglo la mitad de las entradas
// tenían número impar: leer 17 estados no terminaba nunca.
#include "../runtime/nxinput.c"
#include <assert.h>

#define ENTRIES 17

// Devuelve cuántas vueltas necesitó, o 0 si no lo logró en max_tries.
static int sdk_read(const uint8_t *lifo, int n, uint8_t (*out)[0x28], int max_tries) {
    for (int tries = 1; tries <= max_tries; tries++) {
        int64_t count = (int64_t)__atomic_load_n((const uint64_t *)(lifo + 0x18), __ATOMIC_ACQUIRE);
        int64_t tail = (int64_t)__atomic_load_n((const uint64_t *)(lifo + 0x10), __ATOMIC_ACQUIRE);
        int k = count < n ? (int)count : n;
        if (k <= 0) return 0;
        int ok = 1;
        for (int i = 0; i < k && ok; i++) { // i = 0 es el más nuevo
            const uint8_t *e = lifo + 0x20 + ((tail - i + ENTRIES) % ENTRIES) * 0x30;
            uint64_t s0 = __atomic_load_n((const uint64_t *)e, __ATOMIC_ACQUIRE);
            if (s0 & 1) { ok = 0; break; }
            memcpy(out[i], e + 8, 0x28);
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            uint64_t s1 = __atomic_load_n((const uint64_t *)e, __ATOMIC_ACQUIRE);
            if (s0 != s1) { ok = 0; break; }
            if (i > 0) {
                uint64_t newer, older;
                memcpy(&newer, out[i - 1], 8);
                memcpy(&older, out[i], 8);
                if (newer - older != 1) ok = 0;
            }
        }
        if (ok) return tries;
    }
    return 0;
}

int main(void) {
    setenv("NX_PLATAFORMA", "switch2", 1);
    static uint8_t mem[0x40000];
    nx_input_attach(mem); // arranca el hilo que muestrea cada 5 ms
    struct timespec ts = {0, 150 * 1000000}; // > 17 muestreos: el LIFO ya dio la vuelta
    nanosleep(&ts, NULL);

    const uint8_t *fullkey = mem + NPAD_BASE + ST_FULLKEY_LIFO;
    const uint8_t *handheld = mem + NPAD_BASE + 8 * NPAD_STRIDE + ST_HANDHELD_LIFO;
    uint8_t out[ENTRIES][0x28];
    for (int round = 0; round < 200; round++) {
        // GetNpadState (una entrada, lo que usa el juego) y GetNpadStates de 16/17 entradas.
        int a = sdk_read(fullkey, 1, out, 1000), b = sdk_read(fullkey, ENTRIES, out, 1000);
        int c = sdk_read(handheld, 16, out, 1000);
        if (!a || !b || !c) {
            fprintf(stderr, "ronda %d: lectura del SDK de Switch 2 sin terminar (1:%d 17:%d portátil:%d)\n",
                    round, a, b, c);
            return 1;
        }
        // Todas las entradas en reposo llevan número par y el estado conserva su muestreo.
        uint64_t seq, st;
        uint64_t tail = __atomic_load_n((const uint64_t *)(fullkey + 0x10), __ATOMIC_ACQUIRE);
        const uint8_t *e = fullkey + 0x20 + tail * 0x30;
        memcpy(&seq, e, 8);
        memcpy(&st, e + 8, 8);
        assert(!(seq & 1) ? seq == st * 2 : 1);
        assert(*(const uint32_t *)(mem + NPAD_BASE + ST_STYLE) == 1);
        nanosleep(&(struct timespec){0, 1000000}, NULL);
    }
    puts("HID npad LIFO: the Switch 2 SDK seqlock reader finishes for 1, 16 and 17 states: PASS");
    return 0;
}
