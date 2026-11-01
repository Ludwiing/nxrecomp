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
static uint64_t key_buttons, pad_buttons, script_buttons, sampling;
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
    __atomic_store_n((uint64_t *)e, sampling, __ATOMIC_RELEASE);
    memcpy(e + 8, st, sizeof st);
    __atomic_store_n((uint64_t *)e, sampling, __ATOMIC_RELEASE);
    u64w(lifo + 0x08, LIFO_ENTRIES);
    __atomic_store_n((uint64_t *)(lifo + 0x10), tail, __ATOMIC_RELEASE);
    __atomic_store_n((uint64_t *)(lifo + 0x18), count < LIFO_ENTRIES ? count + 1 : LIFO_ENTRIES, __ATOMIC_RELEASE);
}

static int clamp_axis(int v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }

static void publish(void) {
    pthread_mutex_lock(&lock);
    uint64_t b = key_buttons | pad_buttons | script_buttons;
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
    shmem = hid_shmem;
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
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (ev->type == SDL_EVENT_KEY_DOWN) key_buttons |= key_button(ev->key.key);
        else key_buttons &= ~key_button(ev->key.key);
        key_axes();
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(ev->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        pad_buttons |= pad_button(ev->gbutton.button);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        pad_buttons &= ~pad_button(ev->gbutton.button);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        int v = ev->gaxis.value;
        switch (ev->gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: pad_lx = v; break;
        case SDL_GAMEPAD_AXIS_LEFTY: pad_ly = -v; break;
        case SDL_GAMEPAD_AXIS_RIGHTX: pad_rx = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: pad_ry = -v; break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: pad_buttons = v > 8000 ? pad_buttons | B_ZL : pad_buttons & ~(uint64_t)B_ZL; break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: pad_buttons = v > 8000 ? pad_buttons | B_ZR : pad_buttons & ~(uint64_t)B_ZR; break;
        }
        break;
    }
    }
    pthread_mutex_unlock(&lock);
    if (shmem) publish();
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
