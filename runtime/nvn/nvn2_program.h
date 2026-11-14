// NVN2 shader control format (Switch 2), cross-checked against seedds/nxnative,
// dm/gfx/tools/dmsh_ampere.py and dmsh_build.py (MIT). The id/version are 0x98761234 / 0xc9. Control fields:
//   +0x38 instruction length, including the 0x80-byte SPH for graphics stages
//   +0x40 start of c1 in the code area
//   +0x44 code-area end/size; c1 is code[+0x40:+0x44], not a separate size
// Compute ABI metadata: +0x58..+0x63 workgroup xyz; +0xa9 push-constant word count;
// +0xb1..+0xb6 descriptor counts; +0xba u16 constant-buffer layout size. The current NxNative
// splitter also reads +0x64 as shared bytes, but labels non-zero values unverified; keep that
// provenance explicit until checked against a real NVN2 game's shader facts and dispatches.
// The caller supplies the stage; this header does not infer it from an undocumented control field.
#pragma once
#include <stdint.h>
#include <string.h>

#define NVN2_CONTROL_MAGIA 0x98761234u
#define NVN2_CONTROL_VERSION 0xc9u
#define NVN2_CONTROL_MIN_TAM 0x48u
#define NVN2_CONTROL_COMPUTE_TAM 0xbcu
#define NVN2_CONTROL_STAGE_OFFSET 0x56u
#define NVN2_CONTROL_STAGE_MIN_TAM (NVN2_CONTROL_STAGE_OFFSET + 1u)
#define NVN2_SPH_TAM 128u
#define NVN2_SPH_STAGE_SHIFT 10u
#define NVN2_SPH_STAGE_MASK 0xfu
#define NVN2_SPH_STAGE_VERTEX_A 0u
#define NVN2_SPH_STAGE_VERTEX 1u
#define NVN2_SPH_STAGE_FRAGMENT 5u
#define NVN2_CONTROL_STAGE_TASK 6u
#define NVN2_CONTROL_STAGE_MESH 7u

typedef struct {
    uint32_t threads, payload_bytes, max_vertices, max_primitives, topology;
    uint8_t stage, has_task;
} Nvn2MeshInfo;

typedef struct {
    uint32_t largo_instrucciones; // includes SPH for graphics stages
    uint32_t c1_inicio, area_codigo_tam; // c1 = code[c1_inicio:area_codigo_tam]
    uint32_t compute_local[3], compute_shared_bytes;
    uint16_t compute_cb_layout_bytes;
    uint8_t compute_push_constant_words;
    uint8_t compute_uniform_buffers, compute_storage_buffers, compute_textures;
    uint8_t compute_images, compute_separate_textures, compute_separate_samplers;
} Nvn2Control;

static inline uint32_t nvn2_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline uint16_t nvn2_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }

static inline int nvn2_control_tiene_magia(const void *datos) {
    if (!datos) return 0;
    const uint8_t *p = (const uint8_t *)datos;
    return nvn2_u32(p) == NVN2_CONTROL_MAGIA;
}

static inline int nvn2_control_es(const void *datos) {
    return nvn2_control_tiene_magia(datos) && nvn2_u32((const uint8_t *)datos + 4) == NVN2_CONTROL_VERSION;
}

// +0x30 holds the section's own length only inside BNSH files (0xd8; 0xd0 in effects, 0x930 /
// 0x338 in sead's primitive drawer); it is 0 in agl's BAHS files and in sections the game hands
// over from memory (nxnative dm/gfx/tools/shader_split.py nvn2_ctl, which also reads +0x56
// with no length). A 0 therefore means "length not recorded": assume the smallest section seen,
// 0xd0, which covers every field read here. A non-zero length is still checked.
#define NVN2_CONTROL_TAM_SIN_REGISTRO 0xd0u
static inline uint32_t nvn2_control_tam(const uint8_t *p) {
    uint32_t declared = nvn2_u32(p + 0x30);
    return declared ? declared : NVN2_CONTROL_TAM_SIN_REGISTRO;
}

// Switch 2 records the NVN stage in control[0x56] (NVN stages 0..5, then task=6 and mesh=7).
// The control length (nvn2_control_tam) must cover that byte before it is read; observed sizes
// are 0xd0 to 0x930. Reject implausible lengths rather than trusting a corrupted size.
static inline int nvn2_control_stage_leer(const void *datos, uint8_t *stage) {
    if (!datos || !stage) return 0;
    const uint8_t *p = (const uint8_t *)datos;
    if (nvn2_u32(p) != NVN2_CONTROL_MAGIA || nvn2_u32(p + 4) != NVN2_CONTROL_VERSION) return 0;
    uint32_t declared = nvn2_control_tam(p);
    if (declared < NVN2_CONTROL_STAGE_MIN_TAM || declared > 0x1000u) return 0;
    *stage = p[NVN2_CONTROL_STAGE_OFFSET];
    return 1;
}

// NVN2 task/mesh metadata is stored after the common graphics prefix. Validate the
// declared control size before reading +0x58..+0x63; these values feed the SM86
// lowering and must also participate in the shader cache key.
static inline int nvn2_mesh_info_leer(const void *datos, uint8_t expected_stage, Nvn2MeshInfo *out) {
    if (!datos || !out || (expected_stage != NVN2_CONTROL_STAGE_TASK && expected_stage != NVN2_CONTROL_STAGE_MESH))
        return 0;
    const uint8_t *p = (const uint8_t *)datos;
    if (nvn2_u32(p) != NVN2_CONTROL_MAGIA || nvn2_u32(p + 4) != NVN2_CONTROL_VERSION) return 0;
    uint32_t declared = nvn2_control_tam(p);
    if (declared < 0x64u || declared > 0x1000u || p[NVN2_CONTROL_STAGE_OFFSET] != expected_stage) return 0;
    uint32_t a = nvn2_u32(p + 0x58), b = nvn2_u32(p + 0x5c), c = nvn2_u32(p + 0x60);
    Nvn2MeshInfo m = {
        .threads = a & 0xffffu,
        .payload_bytes = (a >> 24) * 256u,
        .max_vertices = c & 0xffffu,
        .max_primitives = c >> 16,
        .topology = b >> 24,
        .stage = expected_stage,
        .has_task = (uint8_t)((b >> 16) & 0xffu),
    };
    if (!m.threads || m.threads > 1024u) return 0;
    *out = m;
    return 1;
}

// The first 0x50 bytes of the Ampere SPH retain the v3 shader-stage field.
// Return the raw SPH stage (not an NVN API enum); compute NVN2 records have no
// SPH stage and must be identified by their call context.
static inline int nvn2_sph_stage(const void *codigo, uint32_t n) {
    if (!codigo || n < 0x50u) return -1;
    uint32_t word0 = nvn2_u32((const uint8_t *)codigo);
    return (int)((word0 >> NVN2_SPH_STAGE_SHIFT) & NVN2_SPH_STAGE_MASK);
}
static inline int nvn2_sph_version(const void *codigo, uint32_t n) {
    if (!codigo || n < 0x50u) return -1;
    uint32_t word0 = nvn2_u32((const uint8_t *)codigo);
    return (int)((word0 >> 5) & 0x1fu);
}
static inline int nvn2_sph_es_vertex(const void *codigo, uint32_t n) {
    int stage = nvn2_sph_stage(codigo, n);
    return stage == NVN2_SPH_STAGE_VERTEX_A || stage == NVN2_SPH_STAGE_VERTEX;
}

// `area_tam` is the actual available code area; zero skips only the external-size check.
// `grafica` comes from the NVN caller's stage, not from a guessed control field.
static inline int nvn2_control_leer(const void *datos, uint32_t n, uint32_t area_tam, int grafica, Nvn2Control *out) {
    if (!datos || !out || n < NVN2_CONTROL_MIN_TAM || (!grafica && n < NVN2_CONTROL_COMPUTE_TAM)) return 0;
    const uint8_t *p = (const uint8_t *)datos;
    if (nvn2_u32(p) != NVN2_CONTROL_MAGIA || nvn2_u32(p + 4) != NVN2_CONTROL_VERSION) return 0;

    Nvn2Control c = {.largo_instrucciones = nvn2_u32(p + 0x38), .c1_inicio = nvn2_u32(p + 0x40),
                     .area_codigo_tam = nvn2_u32(p + 0x44)};
    if (!grafica) {
        c.compute_local[0] = nvn2_u32(p + 0x58);
        c.compute_local[1] = nvn2_u32(p + 0x5c);
        c.compute_local[2] = nvn2_u32(p + 0x60);
        c.compute_shared_bytes = nvn2_u32(p + 0x64);
        c.compute_push_constant_words = p[0xa9];
        c.compute_uniform_buffers = p[0xb1];
        c.compute_storage_buffers = p[0xb2];
        c.compute_textures = p[0xb3];
        c.compute_separate_textures = p[0xb4];
        c.compute_separate_samplers = p[0xb5];
        c.compute_images = p[0xb6];
        c.compute_cb_layout_bytes = nvn2_u16(p + 0xba);
    }

    if (!c.largo_instrucciones || (c.largo_instrucciones & 0x0fu) || c.c1_inicio > c.area_codigo_tam ||
        (area_tam && (c.area_codigo_tam > area_tam || c.largo_instrucciones > area_tam)))
        return 0;
    *out = c;
    return 1;
}

static inline uint32_t nvn2_c1_tam(const Nvn2Control *c) { return c->area_codigo_tam - c->c1_inicio; }
static inline uint32_t nvn2_codigo_inicio(int grafica) { return grafica ? NVN2_SPH_TAM : 0; }

// Ryujinx's Ampere compute translator expects this c[0] layout tuple:
// NxNative's `nvn2_cs` contract is [uniform buffers, storage buffers, textures, c[0] size,
// push-constant bytes, separate textures, separate samplers]. Images occupy the remaining c[0]
// layout after combined texture handles and are bounded by c[0] size in the translator.
static inline void nvn2_compute_c0_layout(const Nvn2Control *c, uint32_t out[7]) {
    if (!c || !out) return;
    out[0] = c->compute_uniform_buffers;
    out[1] = c->compute_storage_buffers;
    out[2] = c->compute_textures;
    out[3] = c->compute_cb_layout_bytes;
    out[4] = (uint32_t)c->compute_push_constant_words * 4u;
    out[5] = c->compute_separate_textures;
    out[6] = c->compute_separate_samplers;
}

// NVN2 workgroups target Ampere (at most 1024 invocations); GfxShaderAnswers stores
// each dimension in u16, and the translator rounds shared memory up to 16 bytes.
static inline int nvn2_compute_resources_valid(const Nvn2Control *c) {
    if (!c || !c->compute_local[0] || !c->compute_local[1] || !c->compute_local[2] ||
        c->compute_local[0] > UINT16_MAX || c->compute_local[1] > UINT16_MAX ||
        c->compute_local[2] > UINT16_MAX || c->compute_shared_bytes > UINT32_MAX - 15u)
        return 0;
    uint64_t xy = (uint64_t)c->compute_local[0] * c->compute_local[1];
    return xy <= 1024 && c->compute_local[2] <= 1024 / xy;
}

// Workgroup size the translator receives for a compute shader: the answers (u16 per dimension, set from the NVN2
// control for Switch 2 programs) when all three are present; otherwise, for a Maxwell program, the three u32 of its
// control at +0x748; otherwise none (0). Each answer is widened on its own: copying the u16[3] into u32[3] as bytes
// made X = x | y << 16, Y = z and Z = the low half of shared_bytes (Mario Tennis Fever: LocalSize 1048592 1 1024
// for a 16x16x1 shader, and the GPU lost the device on its dispatch).
static inline int nvn2_cs_local(const uint16_t answers[3], const uint8_t *maxwell_control, int ampere,
                                uint32_t out[3]) {
    if (answers && answers[0] && answers[1] && answers[2]) {
        for (int i = 0; i < 3; i++) out[i] = answers[i];
        return 1;
    }
    if (!ampere && maxwell_control) {
        memcpy(out, maxwell_control + 0x748, 3 * sizeof(uint32_t));
        return 1;
    }
    out[0] = out[1] = out[2] = 0;
    return 0;
}

static inline uint32_t nvn2_compute_shared_rounded(const Nvn2Control *c) {
    return c && c->compute_shared_bytes <= UINT32_MAX - 15u ? (c->compute_shared_bytes + 15u) & ~15u : 0;
}
