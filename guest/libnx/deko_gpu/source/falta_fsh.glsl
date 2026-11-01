#version 460
// Lee un atributo que el shader de vértices no escribe: el hardware da (0, 0, 0, 1).
layout(location = 1) in vec4 v;
layout(location = 0) out vec4 o;
void main() { o = vec4(v.w, 1.0 - v.x, v.y + 0.5, 1.0); }
