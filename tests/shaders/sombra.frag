#version 460
layout(binding = 4) uniform sampler2DShadow ts;
layout(binding = 0) uniform sampler2D t2d;
layout(location = 0) in vec4 v0;
layout(location = 0) out vec4 o;
void main() {
    float s = texture(ts, vec3(v0.xy, v0.z));
    float s2 = textureLod(ts, vec3(v0.yx, 0.5), 0.0);
    vec4 b = texture(t2d, v0.xy, v0.w * 2.0);
    o = vec4(s, s2, b.x, b.y);
}
