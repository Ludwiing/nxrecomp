// La pantalla: cada cuadro que el juego entrega por QUEUE_BUFFER se pasa a RGBA lineal y se
// muestra en una ventana SDL3 (que Hyprland maneja como cualquier otra), a 60 cuadros por
// segundo como la consola.
//   NX_SIN_VENTANA=1      no abre ventana (para pruebas)
//   NX_CAPTURA=f.ppm      guarda el cuadro número NX_CAPTURA_CUADRO (30 por defecto) y termina
// El hilo principal del proceso queda para la ventana; el juego corre en su propio hilo.
#define _GNU_SOURCE
#include "nxdisplay.h"

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "nxinput.h"
#include "nxrt.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static uint8_t *frame;        // último cuadro, RGBA
static int frame_w, frame_h, frame_new, frames;
static uint64_t next_vsync_ns;

// Dirección de un byte dentro de una superficie "block linear" de la GPU de NVIDIA: GOBs de
// 64 bytes × 8 filas (512 bytes), apilados de a 2^bh en vertical para formar un bloque.
static inline uint64_t block_linear_offset(uint32_t xb, uint32_t y, uint32_t width_gobs, uint32_t bh_log2) {
    uint32_t bh = 1u << bh_log2;
    uint64_t block = (uint64_t)(y / (8 * bh)) * width_gobs * bh * 512 + (uint64_t)(xb / 64) * bh * 512 +
                     (uint64_t)((y % (8 * bh)) / 8) * 512;
    uint32_t gx = xb % 64, gy = y % 8;
    return block + (gx / 32) * 256 + (gy / 2) * 64 + ((gx % 32) / 16) * 32 + (gy % 2) * 16 + gx % 16;
}

static void to_rgba(const NxFrame *f, uint8_t *dst) {
    uint32_t bpp = f->format == NX_FMT_RGB565 || f->format == NX_FMT_RGBA4444 ? 2 : 4;
    uint32_t width_gobs = (f->pitch + 63) / 64;
    for (uint32_t y = 0; y < f->height; y++)
        for (uint32_t x = 0; x < f->width; x++) {
            uint64_t off = f->layout == 3 ? block_linear_offset(x * bpp, y, width_gobs, f->block_height_log2)
                                          : (uint64_t)y * f->pitch + (uint64_t)x * bpp;
            const uint8_t *p = f->data + off;
            uint8_t *d = dst + ((size_t)y * f->width + x) * 4;
            switch (f->format) {
            case NX_FMT_BGRA8888: d[0] = p[2], d[1] = p[1], d[2] = p[0], d[3] = p[3]; break;
            case NX_FMT_RGB565: {
                uint16_t v = (uint16_t)(p[0] | p[1] << 8);
                d[0] = (uint8_t)((v >> 11) * 255 / 31), d[1] = (uint8_t)((v >> 5 & 63) * 255 / 63);
                d[2] = (uint8_t)((v & 31) * 255 / 31), d[3] = 255;
                break;
            }
            case NX_FMT_RGBA4444: {
                uint16_t v = (uint16_t)(p[0] | p[1] << 8);
                d[0] = (uint8_t)((v >> 12) * 17), d[1] = (uint8_t)((v >> 8 & 15) * 17);
                d[2] = (uint8_t)((v >> 4 & 15) * 17), d[3] = (uint8_t)((v & 15) * 17);
                break;
            }
            default: d[0] = p[0], d[1] = p[1], d[2] = p[2], d[3] = f->format == NX_FMT_RGBX8888 ? 255 : p[3];
            }
        }
}

static void capture(const char *path) {
    FILE *o = fopen(path, "wb");
    if (!o) return;
    fprintf(o, "P6\n%d %d\n255\n", frame_w, frame_h);
    for (int k = 0; k < frame_w * frame_h; k++) fwrite(frame + 4 * k, 1, 3, o);
    fclose(o);
}

void nx_display_present(const NxFrame *f) {
    pthread_mutex_lock(&lock);
    if (!frame || frame_w != (int)f->width || frame_h != (int)f->height) {
        free(frame);
        frame = malloc((size_t)f->width * f->height * 4);
        frame_w = (int)f->width;
        frame_h = (int)f->height;
    }
    to_rgba(f, frame);
    frames++;
    nx_input_frame(frames);
    frame_new = 1;
    const char *cap = getenv("NX_CAPTURA");
    if (cap) {
        const char *n = getenv("NX_CAPTURA_CUADRO");
        if (frames >= (n ? atoi(n) : 30)) {
            capture(cap);
            fflush(stdout);
            _exit(0);
        }
    }
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    // 60 cuadros por segundo: el cuadro siguiente se acepta en el próximo vsync.
    uint64_t now = nx_now_ns(), period = 16666667;
    if (!next_vsync_ns || now > next_vsync_ns + period) next_vsync_ns = now;
    next_vsync_ns += period;
    if (next_vsync_ns > now) {
        struct timespec ts = {0, (long)(next_vsync_ns - now)};
        nanosleep(&ts, NULL);
    }
}

// El hilo principal: abre la ventana con el primer cuadro y la mantiene al día.
void nx_display_run(void) {
    int headless = getenv("NX_SIN_VENTANA") != NULL;
    pthread_mutex_lock(&lock);
    while (!frame_new) pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);
    if (headless)
        for (;;) pause();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "[nxrt] no pude abrir la ventana (%s); sigo sin pantalla\n", SDL_GetError());
        for (;;) pause();
    }
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    int tex_w = 0, tex_h = 0;
    // NX_TITULO: el nombre del juego (lo pone NX2Linux); NX_PANTALLA_COMPLETA=1 abre a pantalla
    // completa. F11 la alterna.
    const char *titulo = getenv("NX_TITULO");
    int completa = getenv("NX_PANTALLA_COMPLETA") && *getenv("NX_PANTALLA_COMPLETA") == '1';
    if (!SDL_CreateWindowAndRenderer(titulo && *titulo ? titulo : "nxrecomp", 1280, 720,
                                     SDL_WINDOW_RESIZABLE | (completa ? SDL_WINDOW_FULLSCREEN : 0), &win, &ren)) {
        fprintf(stderr, "[nxrt] no pude crear la ventana: %s\n", SDL_GetError());
        for (;;) pause();
    }
    SDL_SetRenderVSync(ren, 1);
    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                fflush(stdout);
                _exit(0);
            }
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F11 && !ev.key.repeat) {
                completa = !completa;
                SDL_SetWindowFullscreen(win, completa);
                continue;
            }
            nx_input_event(&ev);
        }
        pthread_mutex_lock(&lock);
        if (frame_new) {
            if (frame_w != tex_w || frame_h != tex_h) {
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, frame_w, frame_h);
                SDL_SetRenderLogicalPresentation(ren, frame_w, frame_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
                tex_w = frame_w, tex_h = frame_h;
            }
            SDL_UpdateTexture(tex, NULL, frame, frame_w * 4);
            frame_new = 0;
        }
        pthread_mutex_unlock(&lock);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (tex) SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
}
