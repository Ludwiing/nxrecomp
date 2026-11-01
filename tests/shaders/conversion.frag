#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    int i = int(v2.z * 3.7);
    uint u = uint(v2.x * 11.1);
    float f = float(i) * 0.5 + float(u) + float(int(ub[5].x)) * 1e-6;
    int j = int(floor(v1.x * 100.0)), k = int(round(v1.y * 50.0));
    uint w = uint(clamp(v1.z * 1000.0, 0.0, 70000.0));
    o = vec4(f, float(j + k), float(w), float(int(uint(v1.w * 1e5))));
}
