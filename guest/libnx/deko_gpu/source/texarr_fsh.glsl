#version 460
// Lee la misma imagen vista como arreglo de una capa.
layout(binding = 0) uniform sampler2DArray t;
layout(location = 0) out vec4 o;
void main() { o = texture(t, vec3(0.5, 0.5, 0.0)); }
