#version 460
// Lee una textura 2D en el centro.
layout(binding = 0) uniform sampler2D t;
layout(location = 0) out vec4 o;
void main() { o = texture(t, vec2(0.5)); }
