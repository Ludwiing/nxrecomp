#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    float acc = 0.0;
    int n = int(v3.x) + 2;
    for (int i = 0; i < n; i++) {
        if (i == 5 && v0.y > 0.5) continue;
        acc += fa[i & 63].x * float(i);
        if (acc > 50.0) break;
    }
    int k = 0; uint w = uint(v2.x) + 1u;
    while (w > 1u && k < 64) { w = (w & 1u) != 0u ? 3u * w + 1u : w / 2u; k++; }
    float m = 0.0;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m += fa[i * 4 + j].y * v1[(i + j) & 3];
    o = vec4(acc, float(k), m, 1.0);
}
