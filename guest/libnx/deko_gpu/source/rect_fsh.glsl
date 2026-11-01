#version 460
// color.a < 0: el alfa es la coordenada horizontal (para la prueba de alpha test).
layout(std140, binding = 0) uniform U { vec4 rect; vec4 color; };
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 o;
void main() { o = vec4(color.rgb, color.a < 0.0 ? uv.x : color.a); }
