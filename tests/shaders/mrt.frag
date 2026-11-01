#version 460
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o0;
layout(location = 2) out vec4 o2;
void main() { o0 = v0 * 2.0; o2 = v0.wzyx; }
