// runtime/nvn/nvn2_program.h: control blocks sintéticos basados en el formato NVN2 contrastado con NxNative.
//   cc -O1 -Iruntime/nvn tests/nvn2_program_test.c -o build/nvn2_program_test && build/nvn2_program_test
#include <stdio.h>

#include "nvn2_program.h"

static int fallas;
static void ver(const char *nombre, int cond) {
    printf("%s: %s\n", cond ? "OK" : "MAL", nombre);
    fallas += !cond;
}

static void le32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static void le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void control(uint8_t *p, uint32_t instrucciones, uint32_t c1_inicio, uint32_t area_tam) {
    memset(p, 0, NVN2_CONTROL_COMPUTE_TAM);
    le32(p, NVN2_CONTROL_MAGIA);
    le32(p + 4, NVN2_CONTROL_VERSION);
    le32(p + 0x30, 0x118); // observed NVN2 control-section size; includes stage byte +0x56
    le32(p + 0x38, instrucciones);
    le32(p + 0x40, c1_inicio);
    le32(p + 0x44, area_tam);
    le32(p + 0x58, 8);   // workgroup x
    le32(p + 0x5c, 4);   // workgroup y
    le32(p + 0x60, 2);   // workgroup z
    le32(p + 0x64, 96);  // shared-memory bytes
    p[0xa9] = 4;  // push-constant words
    p[0xb1] = 2;  // uniform buffers
    p[0xb2] = 1;  // storage buffers
    p[0xb3] = 3;  // textures
    p[0xb4] = 5;  // separate textures
    p[0xb5] = 6;  // separate samplers
    p[0xb6] = 4;  // images
    le16(p + 0xba, 0x80); // constant-buffer layout size
}

int main(void) {
    uint8_t p[0x118];
    uint8_t sph[NVN2_SPH_TAM] = {0};
    Nvn2Control c;

    le32(sph, (4u << 5) | (NVN2_SPH_STAGE_VERTEX << NVN2_SPH_STAGE_SHIFT));
    ver("Ampere SPH v4: reconoce la etapa vertex sin offsets de NVN1",
        nvn2_sph_version(sph, sizeof sph) == 4 && nvn2_sph_stage(sph, sizeof sph) == NVN2_SPH_STAGE_VERTEX &&
            nvn2_sph_es_vertex(sph, sizeof sph));
    le32(sph, (4u << 5) | (NVN2_SPH_STAGE_VERTEX_A << NVN2_SPH_STAGE_SHIFT));
    ver("Ampere SPH v4: trata VertexA como etapa vertex",
        nvn2_sph_version(sph, sizeof sph) == 4 && nvn2_sph_es_vertex(sph, sizeof sph));
    le32(sph, (4u << 5) | (NVN2_SPH_STAGE_FRAGMENT << NVN2_SPH_STAGE_SHIFT));
    ver("Ampere SPH v4: reconoce la etapa fragment",
        nvn2_sph_version(sph, sizeof sph) == 4 && nvn2_sph_stage(sph, sizeof sph) == NVN2_SPH_STAGE_FRAGMENT);
    ver("SPH truncado: etapa no disponible", nvn2_sph_stage(sph, 0x4f) == -1);

    control(p, 0x400, 0, 0x400);
    uint8_t raw_stage = 0xff;
    p[NVN2_CONTROL_STAGE_OFFSET] = NVN2_CONTROL_STAGE_TASK;
    ver("control NVN2: reconoce task en el stage byte +0x56",
        nvn2_control_stage_leer(p, &raw_stage) && raw_stage == NVN2_CONTROL_STAGE_TASK);
    p[NVN2_CONTROL_STAGE_OFFSET] = NVN2_CONTROL_STAGE_MESH;
    ver("control NVN2: reconoce mesh en el stage byte +0x56",
        nvn2_control_stage_leer(p, &raw_stage) && raw_stage == NVN2_CONTROL_STAGE_MESH);
    le32(p + 0x30, NVN2_CONTROL_STAGE_MIN_TAM - 1);
    ver("control NVN2: no lee stage fuera del tamaño declarado", !nvn2_control_stage_leer(p, &raw_stage));
    le32(p + 0x30, 0x1001);
    ver("control NVN2: rechaza tamaño de control fuera del límite", !nvn2_control_stage_leer(p, &raw_stage));
    // En memoria (y en los BAHS de agl) +0x30 vale 0: la etapa igual se lee (nxnative shader_split.py nvn2_ctl).
    le32(p + 0x30, 0);
    p[NVN2_CONTROL_STAGE_OFFSET] = NVN2_CONTROL_STAGE_TASK;
    raw_stage = 0xff;
    ver("control NVN2 en memoria (+0x30 = 0): lee la etapa de +0x56",
        nvn2_control_stage_leer(p, &raw_stage) && raw_stage == NVN2_CONTROL_STAGE_TASK);
    ver("control NVN2 en memoria: tamaño supuesto 0xd0", nvn2_control_tam(p) == 0xd0);

    Nvn2MeshInfo mesh;
    control(p, 0x400, 0, 0x400);
    le32(p + 0x58, 64u | (2u << 24)); // 64 threads; payload occupies 512 bytes
    le32(p + 0x5c, (3u << 24) | (1u << 16)); // triangle topology; task feeds mesh
    le32(p + 0x60, 128u | (64u << 16)); // max vertices and primitives
    p[NVN2_CONTROL_STAGE_OFFSET] = NVN2_CONTROL_STAGE_MESH;
    ver("control NVN2 mesh: lee threads, payload, task, topología y límites",
        nvn2_mesh_info_leer(p, NVN2_CONTROL_STAGE_MESH, &mesh) && mesh.stage == NVN2_CONTROL_STAGE_MESH &&
            mesh.threads == 64 && mesh.payload_bytes == 512 && mesh.has_task && mesh.topology == 3 &&
            mesh.max_vertices == 128 && mesh.max_primitives == 64);
    ver("control NVN2 mesh: no acepta etapa distinta a la solicitada",
        !nvn2_mesh_info_leer(p, NVN2_CONTROL_STAGE_TASK, &mesh));
    le32(p + 0x30, 0);
    ver("control NVN2 mesh en memoria (+0x30 = 0): lee los metadatos",
        nvn2_mesh_info_leer(p, NVN2_CONTROL_STAGE_MESH, &mesh) && mesh.threads == 64 && mesh.max_primitives == 64);
    le32(p + 0x30, 0x63);
    ver("control NVN2 mesh: no lee metadatos fuera del tamaño declarado",
        !nvn2_mesh_info_leer(p, NVN2_CONTROL_STAGE_MESH, &mesh));
    control(p, 0x400, 0, 0x400);
    p[NVN2_CONTROL_STAGE_OFFSET] = NVN2_CONTROL_STAGE_TASK;
    le32(p + 0x58, 0);
    ver("control NVN2 task: rechaza un tamaño de grupo de cero",
        !nvn2_mesh_info_leer(p, NVN2_CONTROL_STAGE_TASK, &mesh));

    control(p, 0x500, 0x600, 0x640);
    ver("graphics: versión 0xC9, SPH incluido y c1 delimitado por inicio y fin",
        nvn2_control_leer(p, sizeof p, 0x1000, 1, &c) && c.largo_instrucciones == 0x500 && c.c1_inicio == 0x600 &&
            c.area_codigo_tam == 0x640 && nvn2_c1_tam(&c) == 0x40 && nvn2_codigo_inicio(1) == 128);
    ver("control gráfico mínimo de 0x48 bytes aceptado",
        nvn2_control_leer(p, NVN2_CONTROL_MIN_TAM, 0x1000, 1, &c));
    ver("magia NVN2 detectada aun si la versión no es compatible", nvn2_control_tiene_magia(p));

    control(p, 0x800, 0x800, 0x900);
    uint32_t c0_layout[7] = {0};
    int compute_control_ok = nvn2_control_leer(p, sizeof p, 0x1000, 0, &c);
    ver("compute: lee workgroup, memoria compartida y ABI de buffers, constantes y descriptores",
        compute_control_ok && c.compute_push_constant_words == 4 &&
            c.compute_local[0] == 8 && c.compute_local[1] == 4 && c.compute_local[2] == 2 &&
            c.compute_shared_bytes == 96 &&
            c.compute_uniform_buffers == 2 && c.compute_storage_buffers == 1 && c.compute_textures == 3 &&
            c.compute_images == 4 && c.compute_separate_textures == 5 && c.compute_separate_samplers == 6 &&
            c.compute_cb_layout_bytes == 0x80 && c.area_codigo_tam == 0x900 && nvn2_codigo_inicio(0) == 0);
    nvn2_compute_c0_layout(compute_control_ok ? &c : NULL, c0_layout);
    ver("translator NVN2 c[0] layout tuple uses control ABI counts, total size and push-constant bytes",
        c0_layout[0] == 2 && c0_layout[1] == 1 && c0_layout[2] == 3 && c0_layout[3] == 0x80 &&
            c0_layout[4] == 16 && c0_layout[5] == 5 && c0_layout[6] == 6);
    c.compute_shared_bytes = 97;
    ver("compute resources: accepts valid group sizes and rounds shared bytes to 16",
        nvn2_compute_resources_valid(&c) && nvn2_compute_shared_rounded(&c) == 112);
    c.compute_local[0] = 0;
    ver("compute resources: rejects a zero workgroup dimension", !nvn2_compute_resources_valid(&c));
    c.compute_local[0] = 1025;
    ver("compute resources: rejects more than 1024 invocations", !nvn2_compute_resources_valid(&c));
    c.compute_local[0] = 8;
    c.compute_shared_bytes = UINT32_MAX;
    ver("compute resources: rejects shared-memory rounding overflow", !nvn2_compute_resources_valid(&c));

    control(p, 0x800, 0x700, 0x900);
    ver("rechaza área de código menor que la longitud declarada",
        !nvn2_control_leer(p, sizeof p, 0x800, 0, &c));
    control(p, 0x1010, 0, 0x1000);
    ver("instrucciones que exceden el área disponible: rechazadas",
        !nvn2_control_leer(p, sizeof p, 0x1000, 0, &c));
    control(p, 0x801, 0, 0x900);
    ver("longitud de Ampere que no está alineada a 16 bytes: rechazada",
        !nvn2_control_leer(p, sizeof p, 0x1000, 0, &c));
    control(p, 0, 0, 0);
    ver("longitud de shader vacía: rechazada", !nvn2_control_leer(p, sizeof p, 0x1000, 0, &c));
    control(p, 0x400, 0, 0x400);

    control(p, 0x400, 0, 0x400);
    p[0] ^= 1;
    ver("sin la magia: rechazado", !nvn2_control_leer(p, sizeof p, 0x1000, 0, &c));
    control(p, 0x400, 0, 0x400);
    p[4] = 0x01;
    p[5] = 0;
    p[6] = 0;
    p[7] = 0;
    ver("versión distinta de 0xC9: rechazada", !nvn2_control_leer(p, sizeof p, 0x1000, 0, &c));
    control(p, 0x400, 0, 0x400);
    ver("control compute truncado antes del último campo ABI: rechazado",
        !nvn2_control_leer(p, NVN2_CONTROL_COMPUTE_TAM - 1, 0x1000, 0, &c));
    ver("puntero de salida nulo: rechazado", !nvn2_control_leer(p, sizeof p, 0x1000, 0, NULL));

    // Tamaño de grupo para el traductor: las respuestas u16 de cada dimensión, ampliadas una a una (MTF: 16x16x1
    // salía como 1048592 x 1 x <shared> al copiar los bytes de u16[3] en u32[3]).
    struct { uint16_t local[3]; uint32_t shared_bytes; } resp = {{16, 16, 1}, 0x400};
    uint32_t l[3];
    ver("respuestas 16x16x1: LocalSize 16 16 1",
        nvn2_cs_local(resp.local, NULL, 1, l) && l[0] == 16 && l[1] == 16 && l[2] == 1);
    resp.local[0] = 1024, resp.local[1] = 1, resp.local[2] = 1;
    ver("respuestas 1024x1x1", nvn2_cs_local(resp.local, NULL, 1, l) && l[0] == 1024 && l[1] == 1 && l[2] == 1);
    uint8_t maxwell[0x880] = {0};
    uint32_t mw[3] = {8, 8, 2};
    memcpy(maxwell + 0x748, mw, sizeof mw);
    resp.local[2] = 0;
    ver("Maxwell sin respuestas: los u32 del control en +0x748",
        nvn2_cs_local(resp.local, maxwell, 0, l) && l[0] == 8 && l[1] == 8 && l[2] == 2);
    ver("Ampere sin respuestas: ningún tamaño (el traductor no recibe uno inventado)",
        !nvn2_cs_local(resp.local, maxwell, 1, l) && !l[0] && !l[1] && !l[2]);
    ver("sin respuestas ni control: ninguno", !nvn2_cs_local(NULL, NULL, 0, l));
    return fallas ? 1 : 0;
}
