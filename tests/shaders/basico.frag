#version 460
layout(std140, binding = 0) uniform A { vec4 a; uvec4 b; };
layout(std140, binding = 2) uniform Bb { vec4 c[4]; };
layout(location = 0) in vec4 v; layout(location = 1) in vec4 v1;
layout(location = 0) out vec4 o;
void main() {
    int i = int(v.x * 4.0) & 3;
    o = a * v + c[i] + vec4(b & 7u) + v1;
    if (v.y > 0.5) o.x = sin(v.z);
}
