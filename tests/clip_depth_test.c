// A negative-Z triangle must survive when DepthMode selects [-1,1], even
// when ViewVolumeClipControl has its independent range-control bit set.
#include "../runtime/nxgl.c"
#include <assert.h>

static GLuint stage(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    assert(ok);
    return shader;
}

int main(void) {
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    assert(eglInitialize(display, NULL, NULL));
    assert(eglBindAPI(EGL_OPENGL_API));
    EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config; EGLint count;
    assert(eglChooseConfig(display, attrs, &config, 1, &count) && count);
    EGLint surface_attrs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attrs);
    EGLint context_attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 6, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
    assert(eglMakeCurrent(display, surface, surface, context));
    GLuint program = glCreateProgram();
    glAttachShader(program, stage(GL_VERTEX_SHADER, "#version 460\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],-0.5,1);}"));
    glAttachShader(program, stage(GL_FRAGMENT_SHADER, "#version 460\nlayout(location=0) out vec4 color;void main(){color=vec4(1,0,0,1);}"));
    glLinkProgram(program); glUseProgram(program);
    GLuint array; glCreateVertexArrays(1, &array); glBindVertexArray(array);
    uint32_t regs[0x1000] = {0};
    float sixteen = 16, half = .5f;
    memcpy(&regs[0x280], &sixteen, 4); memcpy(&regs[0x281], &sixteen, 4);
    memcpy(&regs[0x283], &sixteen, 4); memcpy(&regs[0x284], &sixteen, 4);
    memcpy(&regs[0x282], &half, 4); memcpy(&regs[0x285], &half, 4);
    regs[0x64B] = 1; regs[0x64F] = 0x181d; regs[0x680] = 0x1111;
    for (int mode = 0; mode < 2; mode++) {
        regs[0x35F] = mode;
        estado_raster(regs);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        unsigned char pixel[4]; glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        if (mode == 0) {
            if (pixel[0] < 250) { fprintf(stderr, "FAIL: negative-Z triangle clipped in [-1,1] mode\n"); return 1; }
        } else if (pixel[0] != 0) { fprintf(stderr, "FAIL: negative-Z triangle visible in [0,1] mode\n"); return 1; }
    }
    puts("DepthMode clips negative Z independently of ViewVolumeClipControl: PASS");
    return 0;
}
