#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
float f(float x) { if (x < 0.0) return -x * 2.0; else if (x < 1.0) return sqrt(x); return x * x; }
void main() {
    vec4 r = vec4(0.0);
    int sel = int(v3.y) & 7;
    switch (sel) {
    case 0: r.x = 1.0; break;
    case 1: r.x = v0.x; r.y = 2.0; break;
    case 3: case 4: r.x = fa[sel].z; break;
    default: r.x = -1.0;
    }
    r.y = f(v1.x) + f(v1.y);
    r.z = v1.z > 1.5 ? (v1.w < 2.0 ? 1.0 : 2.0) : 3.0;
    r.w = (v0.x > 0.25 && v0.y < 0.75) || v0.z > 0.5 ? 10.0 : 20.0;
    o = r;
}
