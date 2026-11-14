// Ciclo real SDL_EVENT_KEY_DOWN/KEY_UP de la tecla Z (A) -> runtime/nxinput.c -> LIFO de npad, leído como lo lee
// el SDK de Switch 2 de Mario Tennis Fever: nn::hid::GetNpadState(FullKey) = detail::GetNpadStates con una entrada
// (sdk+0x93d0540): solo la entrada más nueva, seqlock con bit 0, número igual antes y después de copiar.
// El juego llama a GetNpadState una vez por cuadro; una pulsación que no está en la entrada más nueva en ese
// momento no la ve nunca, aunque quede en el historial del LIFO.
#include "../runtime/nxinput.c"
#include <assert.h>

#define ENTRIES 17

static int sdk_read(const uint8_t *lifo, int n, uint8_t (*out)[0x28]) {
    for (int tries = 0; tries < 100000; tries++) {
        int64_t count = (int64_t)__atomic_load_n((const uint64_t *)(lifo + 0x18), __ATOMIC_ACQUIRE);
        int64_t tail = (int64_t)__atomic_load_n((const uint64_t *)(lifo + 0x10), __ATOMIC_ACQUIRE);
        int k = count < n ? (int)count : n, ok = k > 0;
        for (int i = 0; i < k && ok; i++) {
            const uint8_t *e = lifo + 0x20 + ((tail - i + ENTRIES) % ENTRIES) * 0x30;
            uint64_t s0 = __atomic_load_n((const uint64_t *)e, __ATOMIC_ACQUIRE);
            if (s0 & 1) { ok = 0; break; }
            memcpy(out[i], e + 8, 0x28);
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            if (__atomic_load_n((const uint64_t *)e, __ATOMIC_ACQUIRE) != s0) { ok = 0; break; }
            if (i > 0) {
                uint64_t newer, older;
                memcpy(&newer, out[i - 1], 8);
                memcpy(&older, out[i], 8);
                if (newer - older != 1) ok = 0;
            }
        }
        if (ok) return k;
    }
    return 0;
}

static uint64_t state_buttons(const uint8_t *st) { uint64_t b; memcpy(&b, st + 8, 8); return b; }

// Lo que vería el juego en un cuadro: GetNpadState(FullKey, jugador 1) y GetNpadState(Handheld).
static int game_sees_a(const uint8_t *mem) {
    uint8_t out[ENTRIES][0x28];
    assert(sdk_read(mem + NPAD_BASE + ST_FULLKEY_LIFO, 1, out) == 1);
    int full = (state_buttons(out[0]) & B_A) != 0;
    assert(sdk_read(mem + NPAD_BASE + 8 * NPAD_STRIDE + ST_HANDHELD_LIFO, 1, out) == 1);
    int hand = (state_buttons(out[0]) & B_A) != 0;
    assert(full == hand);
    return full;
}

static int32_t game_left_x(const uint8_t *mem) {
    const uint8_t *lifo = mem + NPAD_BASE + ST_FULLKEY_LIFO;
    uint64_t tail = __atomic_load_n((const uint64_t *)(lifo + 0x10), __ATOMIC_ACQUIRE);
    int32_t x;
    memcpy(&x, lifo + 0x20 + tail * 0x30 + 8 + 0x10, sizeof x);
    return x;
}

static void key(SDL_EventType type, SDL_Keycode k) {
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    ev.key.key = k;
    ev.key.down = type == SDL_EVENT_KEY_DOWN;
    nx_input_event(&ev);
}

static void gamepad_button(SDL_EventType type, SDL_GamepadButton button) {
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    ev.gbutton.button = button;
    ev.gbutton.which = 1;
    nx_input_event(&ev);
}

static void sleep_ms(int ms) { nanosleep(&(struct timespec){ms / 1000, (long)(ms % 1000) * 1000000}, NULL); }

int main(void) {
    setenv("NX_PLATAFORMA", "switch2", 1);
    static uint8_t mem[0x40000];
    nx_input_attach(mem);
    sleep_ms(100);
    assert(!game_sees_a(mem));

    // 1) Pulsación breve: KEY_DOWN y KEY_UP en el mismo lote de eventos SDL (lo que entrega una pulsación
    //    sintética o una muy rápida). El juego lee en el cuadro siguiente (16,7 ms) y en el otro.
    int vistos = 0;
    for (int intento = 0; intento < 20; intento++) {
        key(SDL_EVENT_KEY_DOWN, SDLK_Z);
        key(SDL_EVENT_KEY_UP, SDLK_Z);
        sleep_ms(16);
        int a = game_sees_a(mem);
        vistos += a;
        sleep_ms(200); // vuelve a reposo
        assert(!game_sees_a(mem));
    }
    printf("pulsación breve: el juego vio A en %d de 20 cuadros siguientes\n", vistos);
    if (vistos != 20) {
        fprintf(stderr, "FALLO: una pulsación breve de Z no llega a GetNpadState del cuadro siguiente\n");
        return 1;
    }

    // 2) Pulsación sostenida 60 ms: visible mientras se mantiene y suelta después.
    key(SDL_EVENT_KEY_DOWN, SDLK_Z);
    sleep_ms(30);
    assert(game_sees_a(mem));
    sleep_ms(30);
    key(SDL_EVENT_KEY_UP, SDLK_Z);
    sleep_ms(200);
    assert(!game_sees_a(mem));

    // 3) Historial coherente para un lector de 17 estados: números de muestreo consecutivos.
    uint8_t out[ENTRIES][0x28];
    assert(sdk_read(mem + NPAD_BASE + ST_FULLKEY_LIFO, ENTRIES, out) == ENTRIES);

    // 4) Al perder foco pueden no llegar KEY_UP; la memoria HID debe volver a reposo.
    key(SDL_EVENT_KEY_DOWN, SDLK_Z);
    sleep_ms(10);
    assert(game_sees_a(mem));
    key_lx = 32767;
    SDL_Event focus_lost = {.type = SDL_EVENT_WINDOW_FOCUS_LOST};
    nx_input_event(&focus_lost);
    assert(!game_sees_a(mem));
    assert(game_left_x(mem) == 0);

    // 5) Desconectar el mando también limpia los botones que SDL ya no podrá soltar.
    gamepad_button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST);
    assert(game_sees_a(mem));
    SDL_Event axis = {.type = SDL_EVENT_GAMEPAD_AXIS_MOTION};
    axis.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axis.gaxis.value = 12345;
    nx_input_event(&axis);
    assert(game_left_x(mem) == 12345);
    SDL_Event pad_removed = {.type = SDL_EVENT_GAMEPAD_REMOVED};
    pad_removed.gdevice.which = 1;
    nx_input_event(&pad_removed);
    assert(!game_sees_a(mem));
    assert(game_left_x(mem) == 0);

    puts("HID: KEY_DOWN/KEY_UP de Z llega como B_A a GetNpadState de Switch 2 (breve y sostenida): PASS");
    puts("HID: perder foco o desconectar el mando libera botones retenidos: PASS");
    return 0;
}
