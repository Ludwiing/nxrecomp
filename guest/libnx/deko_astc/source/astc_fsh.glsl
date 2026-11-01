#version 460
// Cuatro texturas ASTC de 160x160 lado a lado, leídas texel a texel.
layout (binding = 0) uniform sampler2D t0;
layout (binding = 1) uniform sampler2D t1;
layout (binding = 2) uniform sampler2D t2;
layout (binding = 3) uniform sampler2D t3;
layout (location = 0) out vec4 o;
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    int i = p.x / 160;
    ivec2 q = ivec2(p.x % 160, p.y);
    if (p.y >= 160 || i >= 4) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
    if (i == 0) o = texelFetch(t0, q, 0);
    else if (i == 1) o = texelFetch(t1, q, 0);
    else if (i == 2) o = texelFetch(t2, q, 0);
    else o = texelFetch(t3, q, 0);
}
