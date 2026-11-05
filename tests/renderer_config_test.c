#define main nx_game_main
#include "../runtime/nxmain.c"
#undef main
#include <assert.h>
int main(void) {
    unsetenv("NX_RENDERER");unsetenv("GALLIUM_DRIVER");
    assert(configurar_renderer());assert(!getenv("GALLIUM_DRIVER"));
    setenv("NX_RENDERER","opengl",1);assert(configurar_renderer());assert(!getenv("GALLIUM_DRIVER"));
    setenv("NX_RENDERER","invalid",1);assert(!configurar_renderer());
    setenv("NX_RENDERER","vulkan",1);assert(configurar_renderer());
    assert(!strcmp(getenv("GALLIUM_DRIVER"),"zink"));
    assert(!strcmp(getenv("SDL_RENDER_DRIVER"),"vulkan"));
    assert(strstr(getenv("__EGL_VENDOR_LIBRARY_FILENAMES"),"mesa"));
    puts("Renderer selection: OpenGL preserved, Vulkan/Zink explicit, invalid rejected: PASS");
}
