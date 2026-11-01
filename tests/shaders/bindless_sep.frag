#version 460
#extension GL_ARB_bindless_texture : require
// Handle separado: la palabra de la textura y la del sampler se unen con OR (Unity, Unreal, RE Engine).
layout(std140, binding = 0) uniform A { uvec2 handles[4]; };
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    sampler2D s = sampler2D(uvec2(handles[1].x | handles[3].x, handles[1].y));
    o = texture(s, v0.xy * 1.7);
}
