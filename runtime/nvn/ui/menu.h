// Adaptación de ui/menu.h de totk-pc-recomp: el menú en pantalla de TotK no existe en el runtime genérico; la
// presentación (gfx_present.c) lo consulta y aquí siempre está cerrado.
#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice device;
    uint32_t queue_family;
    VkQueue queue;
    VkFormat format;
    uint32_t width, height;
    int image_w, image_h;
    int scaler;
} MenuTarget;
static inline int menu_is_open(void) { return 0; }
static inline int menu_overlay_frame(uint64_t frame, const MenuTarget *t, const char **shot) {
    (void)frame, (void)t;
    *shot = 0;
    return 0;
}
static inline void menu_overlay_draw(VkCommandBuffer cb) { (void)cb; }
static inline void menu_vk_release(void) {}
// La ventana (nvn_wsys.c, sobre nxdisplay.c)
void gfx_window_apply_settings(void);     // TOTK_FULLSCREEN cambió: la ventana lo sigue
void gfx_window_request_close(void);      // como el botón de cerrar: el proceso termina
unsigned gfx_window_dpi(void);            // 96 a escala 100 %
