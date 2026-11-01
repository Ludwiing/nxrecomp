#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    int x = int(v2.x) - 50, y = int(v2.w), z = int(ub[1].y & 0xffffu);
    uint u = uint(v2.y) * 2654435761u + ub[0].x;
    int m = x * y + z;
    uint s = (u >> 7) ^ (u << 3) ^ uint(m);
    int q = (x + 64) / ((-y) | 1);
    int r = (x + 64) % 7;
    uint uq = u / (ub[2].z | 1u);
    o = vec4(float(m & 0xfffff), float(s & 0xffffu), float(q * 1000 + r), float(uq & 0xfffu));
}
