#version 460
// Muestra la imagen de 160x160 texel a texel en la esquina; el resto negro.
layout (binding = 0) uniform sampler2D t;
layout (location = 0) out vec4 o;
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    o = p.x < 160 && p.y < 160 ? texelFetch(t, p, 0) : vec4(0.0, 0.0, 0.0, 1.0);
}
