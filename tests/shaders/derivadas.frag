#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(binding = 0) uniform sampler2D t2d;
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    vec2 c = v0.xy * v1.zw;
    o = vec4(dFdx(c.x * c.y), dFdy(c.x + c.y * 3.0), fwidth(v2.x * v0.y), dFdx(v3.y) + dFdy(v3.x));
}
