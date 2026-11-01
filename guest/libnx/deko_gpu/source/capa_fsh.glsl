#version 460
// Cada capa de otro color: 0 naranja, 1 violeta.
layout(location = 0) out vec4 o;
void main() { o = gl_Layer == 0 ? vec4(1.0, 0.6, 0.0, 1.0) : vec4(0.6, 0.0, 1.0, 1.0); }
