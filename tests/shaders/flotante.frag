#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    vec4 a = v1, b = fa[2], c = fa[3];
    vec4 r;
    r.x = dot(a, b) + length(a.xy) - distance(b.zw, c.xy);
    r.y = clamp(a.x * b.y, -1.0, 1.0) + min(a.z, b.w) - max(c.x, a.w) + abs(a.y) + sign(b.x);
    r.z = floor(a.x * 3.3) + ceil(b.y) + fract(c.z) + trunc(a.w * 1.7) + round(b.z) + mod(a.x, 1.3);
    r.w = exp2(a.x) + log2(abs(b.x) + 1.0) + inversesqrt(abs(c.y) + 0.5) + pow(abs(a.y) + 0.1, 1.5) + cos(a.z) + tan(b.w * 0.1);
    o = r + mix(a, b, 0.3) * step(0.0, c) + smoothstep(-1.0, 1.0, a);
}
