#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(binding = 0) uniform sampler2D t2d;
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    vec2 c = v0.xy * 1.3;
    vec4 a = textureGrad(t2d, c, vec2(v0.z * 0.05, 0.01), vec2(0.0, v0.w * 0.08));
    vec2 l = textureQueryLod(t2d, c * 4.0);
    o = a + vec4(l, 0.0, 0.0);
}
