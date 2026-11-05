// Adaptación: el sistema de ventanas del backend Vulkan (gfx_wsys.h) sobre la ventana de nxdisplay.c, que también
// atiende la entrada (nxinput.c). nxdisplay crea la ventana con SDL_WINDOW_VULKAN en el hilo principal cuando se pide.
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string.h>

#include "gfx_wsys.h"
#include "host.h"
#include "ui/menu.h"

SDL_Window *nx_display_vulkan_window(void);   // nxdisplay.c: la ventana Vulkan (la crea la primera vez); NULL si falla

static SDL_Window *win;

static SDL_Window *ventana(void) {
    if (!win) win = nx_display_vulkan_window();
    return win;
}

// Las extensiones de instancia que pide la superficie de la ventana (VK_KHR_surface y la de Wayland o X11).
int gfx_window_vk_extensions(const char **ext, int max) {
    if (!ventana()) return 0;
    Uint32 n = 0;
    const char *const *e = SDL_Vulkan_GetInstanceExtensions(&n);
    if (!e) rt_fatal("gfx: SDL no da las extensiones Vulkan de la ventana: %s", SDL_GetError());
    int k = 0;
    for (; k < (int)n && k < max; k++) ext[k] = e[k];
    return k;
}

int wsys_open(void) { return ventana() != NULL; }

VkSurfaceKHR wsys_surface(VkInstance inst) {
    VkSurfaceKHR s = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(ventana(), inst, NULL, &s)) rt_log("gfx: no se pudo crear la superficie: %s", SDL_GetError());
    return s;
}

void wsys_client_size(int *w, int *h) {
    *w = *h = 0;
    if (ventana()) SDL_GetWindowSizeInPixels(win, w, h);
}

int wsys_minimized(void) { return ventana() && (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED); }
void wsys_resize(int w, int h) { if (ventana()) SDL_SetWindowSize(win, w, h); }

void wsys_probe_focus(int in) { (void)in; }
void wsys_probe_escape(void) {}
int wsys_probe_click(int where) { (void)where; return 0; }

void gfx_window_apply_settings(void) { if (ventana()) SDL_SetWindowFullscreen(win, gfx_cfg_fullscreen()); }
void gfx_window_request_close(void) { gfx_window_closed(); }
unsigned gfx_window_dpi(void) { return ventana() ? (unsigned)(96.0f * SDL_GetWindowDisplayScale(win) + 0.5f) : 96; }

// La familia de colas puede presentar en la ventana.
int gfx_window_vk_presentable(VkInstance inst, VkPhysicalDevice pd, uint32_t family) {
    return ventana() && SDL_Vulkan_GetPresentationSupport(inst, pd, family);
}
