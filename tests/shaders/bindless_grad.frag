#version 460
#extension GL_ARB_bindless_texture : require
layout(std140, binding = 0) uniform A { uvec2 handles[4]; };
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    sampler2D s = sampler2D(handles[2]);
    vec2 uv = v0.xy * 2.0;
    o = textureGrad(s, uv, vec2(v0.z * 0.3, 0.01), vec2(0.02, v0.w * 0.2)) + vec4(textureQueryLod(s, uv * 8.0), 0.0, 0.0) * 0.1;
}
