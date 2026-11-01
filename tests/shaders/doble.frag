#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(binding = 0) uniform sampler2D t2d;
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    double a = double(v2.x) * 1.000000001lf, b = double(v1.y) + 0.25lf;
    double c = a * b + double(fa[0].x);
    double d = max(c, b) - min(a, 3.0lf);
    o = vec4(float(c), float(d), float(c > b ? 1.0lf : -1.0lf), float(abs(a - b)));
}
