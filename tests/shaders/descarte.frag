#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    if (v0.x * v0.y > 0.3) discard;
    o = vec4(v0.xy, fa[0].x, 1.0);
}
