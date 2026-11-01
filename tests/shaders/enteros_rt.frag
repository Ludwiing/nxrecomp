#version 460
layout(binding = 5) uniform usampler2D tu;
layout(binding = 6) uniform isampler2D ti;
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    uvec4 a = texelFetch(tu, ivec2(v0.xy * 7.0), 0);
    ivec4 b = texelFetch(ti, ivec2(v0.yx * 5.0), 0);
    o = vec4(a) * 0.001 + vec4(b) * 0.002;
}
