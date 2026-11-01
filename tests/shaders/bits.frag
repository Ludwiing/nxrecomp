#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
uint rotate(uint x) { return (x << 13) | (x >> 19); }
void main() {
    uint a = uint(v2.x * 31.0) * 0x9e3779b9u + ub[3].x, b = ub[3].y ^ uint(v2.y);
    uint c = bitfieldExtract(a, 3, 11) + bitfieldInsert(b, a, 5, 9);
    int d = bitfieldExtract(int(a), 7, 13);
    uint e = bitCount(a) + uint(findMSB(b)) * 64u + uint(findLSB(a | 0x80000000u)) * 4096u;
    uint f = bitfieldReverse(a) >> 16;
    uint g = (a & ~b) | (rotate(a) ^ b);
    o = vec4(float(c & 0xffffu), float(d), float(e), float((f ^ g) & 0xffffu));
}

