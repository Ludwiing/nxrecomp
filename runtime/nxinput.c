// Mandos: el teclado y los mandos de SDL se escriben en la memoria compartida de hid, donde el
// juego (libnx o el SDK de Nintendo) lee el estado como en la consola: un mando Pro en el
// jugador 1 y lo mismo en modo portátil. Offsets medidos con el compilador de devkitPro sobre
// HidSharedMemory de libnx 4.12.
//
// Teclado (como el predeterminado de Ryujinx):
//   stick izquierdo WASD (F lo aprieta) · cruceta flechas · A Z · B X · X C · Y V
//   L E · ZL Q · R U · ZR O · stick derecho IJKL (H lo aprieta) · + Enter o + · − Retroceso o −
// Mando: por posición, como en la Switch (el de la derecha es A).
// Pruebas: NX_PULSAR="30:plus,45:a+b" aprieta esos botones durante 3 cuadros desde el cuadro dado.
#define _GNU_SOURCE
#include "nxinput.h"

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxrt.h"

#define NPAD_BASE 0x9A00
#define NPAD_STRIDE 0x5000
#define ST_STYLE 0x00
#define ST_FULLKEY_LIFO 0x28
#define ST_HANDHELD_LIFO 0x378
#define ST_DEVICE_TYPE 0x4188
#define ST_SYSTEM_PROPS 0x4190
#define LIFO_ENTRIES 17
#define LIFO_ENTRY 0x30

enum {
    B_A = 1 << 0, B_B = 1 << 1, B_X = 1 << 2, B_Y = 1 << 3, B_LS = 1 << 4, B_RS = 1 << 5, B_L = 1 << 6,
    B_R = 1 << 7, B_ZL = 1 << 8, B_ZR = 1 << 9, B_PLUS = 1 << 10, B_MINUS = 1 << 11, B_LEFT = 1 << 12,
    B_UP = 1 << 13, B_RIGHT = 1 << 14, B_DOWN = 1 << 15,
};

static uint8_t *shmem;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t script_buttons, sampling;
// El SDK de Switch 2 (nn::hid::detail::GetNpadStates) lee cada entrada del LIFO como un seqlock: con el bit 0 del
// número de la entrada encendido la da por escribiéndose y vuelve a empezar, y compara el número antes y después de
// copiar. Con el número de muestreo (que sube de 1 en 1) la mitad de las entradas eran impares: una lectura de una
// entrada esperaba al siguiente muestreo y una de varias no terminaba nunca. En Switch 2 la entrada lleva su propio
// número par (2 × muestreo), impar mientras se escribe; el muestreo del estado sigue subiendo de 1 en 1 (el SDK
// exige que dos estados seguidos difieran en 1). Switch 1 conserva el número de la entrada igual al del estado.
static int seqlock_par;

// El juego lee GetNpadState una vez por cuadro y solo ve la entrada más nueva del LIFO. Si KEY_DOWN y KEY_UP llegan
// en el mismo lote de eventos SDL (pulsación sintética o muy rápida), el botón solo estaba publicado entre dos
// escrituras y ningún cuadro lo veía. Un botón soltado antes de MIN_HOLD_NS queda apretado hasta cumplirlo; el
// hilo de muestreo lo suelta. Cubre dos cuadros a 30 fps.
#define MIN_HOLD_NS 70000000ull
typedef struct {
    uint64_t buttons, pending_release;
    uint64_t down_ns[16];
} Source;
static Source keys, pads;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// Traza opt-in (NX_TRAZA_ENTRADA=1): adjuntar memoria, foco y transiciones de botones con lo publicado; tope de líneas.
static int traza = -1;
static unsigned traza_lineas;
static void trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void trace(const char *fmt, ...) {
    if (traza < 0) traza = getenv("NX_TRAZA_ENTRADA") && *getenv("NX_TRAZA_ENTRADA") == '1';
    if (!traza || traza_lineas >= 400) return;
    if (++traza_lineas == 400) { fputs("[entrada] traza: tope de 400 líneas alcanzado\n", stderr); return; }
    va_list ap;
    va_start(ap, fmt);
    fputs("[entrada] ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void source_set(Source *s, uint64_t bits, int down) {
    uint64_t t = now_ns();
    for (int i = 0; i < 16; i++) {
        uint64_t bit = 1ull << i;
        if (!(bits & bit)) continue;
        if (down) {
            if (!(s->buttons & bit) || (s->pending_release & bit)) s->down_ns[i] = t;
            s->buttons |= bit;
            s->pending_release &= ~bit;
        } else if (s->buttons & bit) {
            if (t - s->down_ns[i] < MIN_HOLD_NS) s->pending_release |= bit;
            else s->buttons &= ~bit;
        }
    }
}

// Lo llama el hilo de muestreo con el candado tomado: suelta los botones retenidos que ya cumplieron.
static void source_expire(Source *s, uint64_t t) {
    for (int i = 0; i < 16; i++) {
        uint64_t bit = 1ull << i;
        if ((s->pending_release & bit) && t - s->down_ns[i] >= MIN_HOLD_NS) {
            s->pending_release &= ~bit;
            s->buttons &= ~bit;
            trace("botón %#llx soltado tras la retención mínima", (unsigned long long)bit);
        }
    }
}
static int key_lx, key_ly, key_rx, key_ry;   // ejes del teclado: -1, 0, 1
static int pad_lx, pad_ly, pad_rx, pad_ry;   // ejes del mando: −32767 … 32767

static void u32w(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void u64w(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static uint64_t u64r(const uint8_t *p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

// Agrega un estado al LIFO: avanza la cola, escribe la entrada y después el contador.
static void lifo_push(uint8_t *lifo, uint64_t buttons, int lx, int ly, int rx, int ry) {
    uint64_t tail = (u64r(lifo + 0x10) + 1) % LIFO_ENTRIES, count = u64r(lifo + 0x18);
    uint8_t *e = lifo + 0x20 + tail * LIFO_ENTRY;
    uint8_t st[0x28] = {0};
    u64w(st, sampling);
    u64w(st + 0x08, buttons);
    int32_t sticks[4] = {lx, ly, rx, ry};
    memcpy(st + 0x10, sticks, 16);
    u32w(st + 0x20, 1); // conectado
    uint64_t seq = seqlock_par ? sampling * 2 : sampling;
    __atomic_store_n((uint64_t *)e, seqlock_par ? seq - 1 : seq, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    memcpy(e + 8, st, sizeof st);
    __atomic_store_n((uint64_t *)e, seq, __ATOMIC_RELEASE);
    u64w(lifo + 0x08, LIFO_ENTRIES);
    __atomic_store_n((uint64_t *)(lifo + 0x10), tail, __ATOMIC_RELEASE);
    __atomic_store_n((uint64_t *)(lifo + 0x18), count < LIFO_ENTRIES ? count + 1 : LIFO_ENTRIES, __ATOMIC_RELEASE);
}

static int clamp_axis(int v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }

static void publish(void) {
    pthread_mutex_lock(&lock);
    uint64_t ahora = now_ns();
    source_expire(&keys, ahora);
    source_expire(&pads, ahora);
    uint64_t b = keys.buttons | pads.buttons | script_buttons;
    int lx = clamp_axis(pad_lx + key_lx * 32767), ly = clamp_axis(pad_ly + key_ly * 32767);
    int rx = clamp_axis(pad_rx + key_rx * 32767), ry = clamp_axis(pad_ry + key_ry * 32767);
    // botones virtuales de los sticks, como los pone el sistema
    const int t = 16384;
    if (lx < -t) b |= 1ull << 16;
    if (ly > t) b |= 1ull << 17;
    if (lx > t) b |= 1ull << 18;
    if (ly < -t) b |= 1ull << 19;
    if (rx < -t) b |= 1ull << 20;
    if (ry > t) b |= 1ull << 21;
    if (rx > t) b |= 1ull << 22;
    if (ry < -t) b |= 1ull << 23;
    sampling++;
    lifo_push(shmem + NPAD_BASE + ST_FULLKEY_LIFO, b, lx, ly, rx, ry);                       // jugador 1
    lifo_push(shmem + NPAD_BASE + 8 * NPAD_STRIDE + ST_HANDHELD_LIFO, b, lx, ly, rx, ry);    // portátil
    pthread_mutex_unlock(&lock);
}

static void *sampler(void *arg) {
    (void)arg;
    for (;;) { // el sistema muestrea los mandos cada ~5 ms
        publish();
        struct timespec ts = {0, 5000000};
        nanosleep(&ts, NULL);
    }
    return NULL;
}

void nx_input_attach(uint8_t *hid_shmem) {
    if (shmem) return;
    const char *plataforma = getenv("NX_PLATAFORMA"); // lo pone NX2Linux según el NPDM/ExeFS
    seqlock_par = plataforma && !strcmp(plataforma, "switch2");
    shmem = hid_shmem;
    trace("memoria compartida de hid adjunta en %p (seqlock Switch 2: %s); npad 0 en +0x9a00, portátil en +0x31a00",
          (void *)hid_shmem, seqlock_par ? "sí" : "no");
    uint8_t *p1 = shmem + NPAD_BASE;
    u32w(p1 + ST_STYLE, 1);                        // mando Pro (FullKey)
    u32w(p1 + ST_DEVICE_TYPE, 1);                  // HidDeviceTypeBits_FullKey
    u64w(p1 + ST_SYSTEM_PROPS, 0xE800);            // ABXY orientados; +, − y cruceta disponibles
    uint8_t *hh = shmem + NPAD_BASE + 8 * NPAD_STRIDE;
    u32w(hh + ST_STYLE, 2);                        // portátil
    publish();
    pthread_t t;
    pthread_create(&t, NULL, sampler, NULL);
}

// ---------- teclado y mando ----------
static uint64_t key_button(SDL_Keycode k) {
    switch (k) {
    case SDLK_Z: return B_A;
    case SDLK_X: return B_B;
    case SDLK_C: return B_X;
    case SDLK_V: return B_Y;
    case SDLK_E: return B_L;
    case SDLK_Q: return B_ZL;
    case SDLK_U: return B_R;
    case SDLK_O: return B_ZR;
    case SDLK_F: return B_LS;
    case SDLK_H: return B_RS;
    case SDLK_RETURN: case SDLK_PLUS: case SDLK_KP_PLUS: return B_PLUS;
    case SDLK_BACKSPACE: case SDLK_MINUS: case SDLK_KP_MINUS: return B_MINUS;
    case SDLK_UP: return B_UP;
    case SDLK_DOWN: return B_DOWN;
    case SDLK_LEFT: return B_LEFT;
    case SDLK_RIGHT: return B_RIGHT;
    }
    return 0;
}

static uint64_t pad_button(int b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_EAST: return B_A;   // posición Nintendo: A a la derecha
    case SDL_GAMEPAD_BUTTON_SOUTH: return B_B;
    case SDL_GAMEPAD_BUTTON_NORTH: return B_X;
    case SDL_GAMEPAD_BUTTON_WEST: return B_Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return B_L;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return B_R;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return B_LS;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return B_RS;
    case SDL_GAMEPAD_BUTTON_START: return B_PLUS;
    case SDL_GAMEPAD_BUTTON_BACK: return B_MINUS;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return B_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return B_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return B_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return B_RIGHT;
    }
    return 0;
}

static void key_axes(void) {
    const bool *k = SDL_GetKeyboardState(NULL);
    key_lx = k[SDL_SCANCODE_D] - k[SDL_SCANCODE_A];
    key_ly = k[SDL_SCANCODE_W] - k[SDL_SCANCODE_S]; // en la Switch, arriba es positivo
    key_rx = k[SDL_SCANCODE_L] - k[SDL_SCANCODE_J];
    key_ry = k[SDL_SCANCODE_I] - k[SDL_SCANCODE_K];
}

void nx_input_event(const void *event) {
    const SDL_Event *ev = event;
    pthread_mutex_lock(&lock);
    uint64_t before = keys.buttons | pads.buttons;
    switch (ev->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        trace("ventana: foco %s", ev->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "ganado" : "perdido");
        if (ev->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            // SDL puede no enviar KEY_UP al perder el foco. No dejes botones ni ejes pegados en
            // la memoria HID del juego; también cancela las liberaciones diferidas pendientes.
            memset(&keys, 0, sizeof keys);
            memset(&pads, 0, sizeof pads);
            key_lx = key_ly = key_rx = key_ry = 0;
            pad_lx = pad_ly = pad_rx = pad_ry = 0;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (!ev->key.repeat)
            trace("tecla %s %s -> botón %#llx", SDL_GetKeyName(ev->key.key),
                  ev->type == SDL_EVENT_KEY_DOWN ? "abajo" : "arriba", (unsigned long long)key_button(ev->key.key));
        source_set(&keys, key_button(ev->key.key), ev->type == SDL_EVENT_KEY_DOWN);
        key_axes();
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        trace("mando SDL %u conectado", (unsigned)ev->gdevice.which);
        SDL_OpenGamepad(ev->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        // Este HLE expone un mando virtual agregado, no un LIFO por cada dispositivo SDL.
        // Al desconectarse uno, vuelve a reposo para evitar botones/ejes retenidos.
        trace("mando SDL %u desconectado; limpiando estado HID", (unsigned)ev->gdevice.which);
        memset(&pads, 0, sizeof pads);
        pad_lx = pad_ly = pad_rx = pad_ry = 0;
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        trace("mando botón %d %s -> botón %#llx", ev->gbutton.button,
              ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "abajo" : "arriba",
              (unsigned long long)pad_button(ev->gbutton.button));
        source_set(&pads, pad_button(ev->gbutton.button), ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        int v = ev->gaxis.value;
        switch (ev->gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: pad_lx = v; break;
        case SDL_GAMEPAD_AXIS_LEFTY: pad_ly = -v; break;
        case SDL_GAMEPAD_AXIS_RIGHTX: pad_rx = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: pad_ry = -v; break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: source_set(&pads, B_ZL, v > 8000); break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: source_set(&pads, B_ZR, v > 8000); break;
        }
        break;
    }
    }
    uint64_t after = keys.buttons | pads.buttons;
    pthread_mutex_unlock(&lock);
    if (shmem) publish();
    if (after != before && shmem) {
        const uint8_t *lifo = shmem + NPAD_BASE + ST_FULLKEY_LIFO;
        uint64_t tail = __atomic_load_n((const uint64_t *)(lifo + 0x10), __ATOMIC_ACQUIRE);
        const uint8_t *e = lifo + 0x20 + tail * LIFO_ENTRY;
        trace("publicado: botones %#llx -> %#llx; FullKey cola %llu número %llu muestreo %llu botones %#llx",
              (unsigned long long)before, (unsigned long long)after, (unsigned long long)tail,
              (unsigned long long)u64r(e), (unsigned long long)u64r(e + 8), (unsigned long long)u64r(e + 16));
    } else if (after != before) {
        trace("botones %#llx -> %#llx sin memoria de hid adjunta: el juego aún no la pidió",
              (unsigned long long)before, (unsigned long long)after);
    }
}

// ---------- guion de pruebas (NX_PULSAR) ----------
static uint64_t button_by_name(const char *s, size_t n) {
    static const struct {
        const char *name;
        uint64_t bit;
    } names[] = {{"a", B_A}, {"b", B_B}, {"x", B_X}, {"y", B_Y}, {"l", B_L}, {"r", B_R}, {"zl", B_ZL}, {"zr", B_ZR},
                 {"plus", B_PLUS}, {"minus", B_MINUS}, {"up", B_UP}, {"down", B_DOWN}, {"left", B_LEFT}, {"right", B_RIGHT}};
    for (size_t k = 0; k < sizeof names / sizeof *names; k++)
        if (strlen(names[k].name) == n && !strncmp(names[k].name, s, n)) return names[k].bit;
    return 0;
}

void nx_input_frame(int frame) {
    const char *s = getenv("NX_PULSAR");
    if (!s) return;
    uint64_t b = 0;
    while (*s) {
        int at = atoi(s);
        const char *c = strchr(s, ':');
        if (!c) break;
        const char *end = strchr(c, ',');
        if (!end) end = c + strlen(c);
        if (frame >= at && frame < at + 3)
            for (const char *p = c + 1; p < end;) {
                const char *q = p;
                while (q < end && *q != '+') q++;
                b |= button_by_name(p, (size_t)(q - p));
                p = q < end ? q + 1 : q;
            }
        s = *end ? end + 1 : end;
    }
    pthread_mutex_lock(&lock);
    script_buttons = b;
    pthread_mutex_unlock(&lock);
    if (shmem) publish();
}
