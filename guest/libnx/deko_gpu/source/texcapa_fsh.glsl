#version 460
// Lee la capa color.x de un arreglo.
layout(std140, binding = 0) uniform U { vec4 rect; vec4 color; };
layout(binding = 0) uniform sampler2DArray t;
layout(location = 0) out vec4 o;
void main() { o = texture(t, vec3(0.5, 0.5, color.x)); }
