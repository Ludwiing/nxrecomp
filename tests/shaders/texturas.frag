#version 460
layout(binding = 0) uniform sampler2D t2d;
layout(binding = 1) uniform sampler2DArray tarr;
layout(binding = 2) uniform sampler3D t3d;
layout(binding = 3) uniform samplerCube tcube;
layout(location = 0) in vec4 v0; layout(location = 1) in vec4 v1;
layout(location = 0) out vec4 o;
void main() {
    vec4 a = texture(t2d, v0.xy * 1.7);
    vec4 b = textureLod(t2d, v0.yx, v0.z * 3.0);
    vec4 c = texelFetch(t2d, ivec2(v0.xy * 15.0), 1);
    vec4 d = textureGather(t2d, v0.xy, 2);
    ivec2 sz = textureSize(t2d, 1);
    vec4 e = texture(tarr, vec3(v0.xy, floor(v0.z * 4.0)));
    vec4 f = texture(t3d, vec3(v0.xy, v0.w));
    vec4 g = texture(tcube, vec3(v1.xy, 1.0));
    o = a + b * 2.0 + c * 3.0 + d * 0.5 + vec4(sz, 0.0, 0.0) + e * 5.0 + f * 7.0 + g * 11.0;
}
