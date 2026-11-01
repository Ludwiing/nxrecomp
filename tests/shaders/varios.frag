#version 460
layout(std140, binding = 0) uniform A { vec4 fa[64]; };
layout(std140, binding = 1) uniform B { uvec4 ub[64]; };
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2; layout(location = 3) in vec4 v3;
layout(location = 0) out vec4 o;
void main() {
    uint a = uint(v2.x * 37.0) * 0x01000193u + ub[0].x, b = ub[0].y ^ uint(v2.y * 1000.0);
    uint hi, lo; umulExtended(a, b, hi, lo);
    int shi, slo; imulExtended(int(a), -int(b), shi, slo);
    uint carry; uint s = uaddCarry(a, b, carry);
    uint borrow; uint dd = usubBorrow(a, b, borrow);
    uint p = packUnorm4x8(v0) ^ packSnorm4x8(v1 * 0.2) ^ packHalf2x16(v0.zw);
    vec2 un = unpackSnorm2x16(a) + unpackUnorm2x16(b) + unpackHalf2x16(a & 0x3bff3bffu);
    int mx = max(int(a), int(b)) - min(int(a & 0xffffu), -5);
    bvec4 cmp = lessThan(v0, v1);
    float sel = mix(v0.x, v1.y, cmp.x) + (any(cmp) ? 1.0 : 0.0) + (all(cmp) ? 2.0 : 0.0);
    o = vec4(float((hi ^ lo ^ uint(shi) ^ uint(slo)) & 0xffffu), float((s ^ carry ^ dd ^ borrow ^ p) & 0xffffu), un.x + un.y + sel, float(mx & 0xfff));
}
