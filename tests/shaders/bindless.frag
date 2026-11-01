#version 460
#extension GL_ARB_bindless_texture : require
layout(std140, binding = 0) uniform A { uvec2 handles[4]; };
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    sampler2D s = sampler2D(handles[1]);
    o = texture(s, v0.xy * 1.5) + texelFetch(s, ivec2(v0.xy * 7.0), 0) * 0.5 + vec4(textureSize(s, 0), 0.0, 0.0) * 0.01;
}
