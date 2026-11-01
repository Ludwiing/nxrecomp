#version 460
// Pinta un patrón en la imagen: rojo = x, verde = y, azul = x xor y, alfa = 1.
layout (local_size_x = 8, local_size_y = 8) in;
layout (binding = 0, rgba8) uniform image2D img;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    imageStore(img, p, vec4(float(p.x) / 255.0, float(p.y) / 255.0, float((p.x ^ p.y) & 255) / 255.0, 1.0));
}
