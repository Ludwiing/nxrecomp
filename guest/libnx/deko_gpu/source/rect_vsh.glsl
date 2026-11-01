#version 460
// Un rectángulo en píxeles (rect = x0, y0, x1, y1) con 6 vértices, sin buffers de vértices.
layout(std140, binding = 0) uniform U { vec4 rect; vec4 color; };
layout(location = 0) out vec2 uv;
void main() {
    const vec2 esq[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 e = esq[gl_VertexID % 6];
    vec2 p = mix(rect.xy, rect.zw, e);
    uv = e;
    gl_Position = vec4(p.x / 640.0 - 1.0, p.y / 360.0 - 1.0, 0.0, 1.0);
}
