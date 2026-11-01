#version 460
#extension GL_ARB_shader_viewport_layer_array : require
// Un triángulo que cubre la imagen, en la capa que dice la instancia.
void main() {
    vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID >> 1) * 4 - 1));
    gl_Position = vec4(p, 0.5, 1.0);
    gl_Layer = gl_InstanceID;
}
