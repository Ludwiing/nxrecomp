#version 460
// Un triángulo que cubre la pantalla.
void main() {
    vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID >> 1) * 4 - 1));
    gl_Position = vec4(p, 0.5, 1.0);
}
