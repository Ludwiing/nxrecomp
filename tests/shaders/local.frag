#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    float arr[16];
    for (int i = 0; i < 16; i++) arr[i] = fa[i].x * v0.x + float(i);
    int idx = int(v3.x) & 15;
    arr[(idx + 3) & 15] += 100.0;
    float s = 0.0;
    for (int i = 0; i < 16; i += 3) s += arr[(idx + i) & 15];
    o = vec4(s, arr[idx], arr[15 - idx], 0.0);
}
