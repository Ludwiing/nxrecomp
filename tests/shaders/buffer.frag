#version 460
layout(binding = 7) uniform samplerBuffer luces;
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    int i = int(v0.x * 63.0);
    o = texelFetch(luces, i) * 0.5 + texelFetch(luces, (i * 7) & 63) * v0.y;
}
