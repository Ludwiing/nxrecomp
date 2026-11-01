// Traductor de shaders Maxwell → GLSL 4.60.
//
// El código de un shader es una cabecera (SPH, 0x50 bytes) y luego instrucciones de 64 bits; de cada
// cuatro palabras, la primera es de control del planificador y no se ejecuta. La codificación sale de
// mxisa.h (generado de las tablas de Ryujinx); la semántica de cada instrucción está aquí.
//
// Cada registro de la GPU es una variable `uint rN`, cada predicado un `bool pN`. Si el shader no
// salta, el GLSL es una secuencia directa; si salta, es un `switch` sobre el índice de la instrucción
// dentro de un bucle, con una pila para SSY/PBK/PCNT/CAL (cada invocación del GLSL es un hilo: la
// reconvergencia de la GPU no cambia lo que calcula cada hilo).
#include "nxshader.h"
#include "mxisa.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *p;
    size_t n, cap;
} Sb;

static void sb_vadd(Sb *s, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int k = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (s->n + k + 1 > s->cap) {
        s->cap = (s->n + k + 1) * 2;
        s->p = realloc(s->p, s->cap);
    }
    vsnprintf(s->p + s->n, k + 1, fmt, ap);
    s->n += k;
}
static void sb_add(Sb *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vadd(s, fmt, ap);
    va_end(ap);
}

enum { V_NONE, V_R, V_I, V_C, V_RC, V_32I };

typedef struct {
    const uint8_t *code; // instrucciones (después de la cabecera)
    uint32_t ninsn;      // palabras de 64 bits disponibles
    int stage;
    const NxShaderKey *key;
    NxShaderInfo *info;
    uint32_t sph[20];
    uint8_t *visited, *leader;
    int uses_flow, uses_stack, uses_local, local_words, uses_discard, uses_shared, uses_subgroup, tex_int;
    int64_t tld4_handle;
    int tex_slot; // constbuf del handle de la textura en curso (0xFF: el de SetBindlessTexture)
    uint8_t reg_used[256];
    uint32_t in_flat, in_linear; // atributos de entrada del fragmento por modo de interpolación
    uint32_t out_comps[32];
    uint32_t clip_mask;
    int writes_layer, writes_psize, frag_depth;
    Sb body;
    char arena[1 << 16];
    size_t arena_n;
    uint64_t o; // instrucción actual
    int op, var, idx;
} T;

// ---------- utilidades de texto ----------

static char *S(T *t, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t room = sizeof t->arena - t->arena_n;
    int k = vsnprintf(t->arena + t->arena_n, room, fmt, ap);
    va_end(ap);
    if (k < 0 || (size_t)k >= room) {
        t->arena_n = 0; // no debería pasar: una instrucción no genera tanto texto
        return (char *)"0u";
    }
    char *r = t->arena + t->arena_n;
    t->arena_n += k + 1;
    return r;
}
static void L(T *t, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_add(&t->body, "    ");
    sb_vadd(&t->body, fmt, ap);
    sb_add(&t->body, "\n");
    va_end(ap);
}
static void red(T *t, const char *why) {
    t->info->errors++;
    fprintf(stderr, "[RED] shader: %s en 0x%x (%s, %016llx)\n", why, t->idx * 8, mx_op_name[t->op],
            (unsigned long long)t->o);
    L(t, "// sin traducir: %s %016llx", mx_op_name[t->op], (unsigned long long)t->o);
}

#define G(f) mx_get(t->op, MXF_##f, t->o)
#define HAS(f) (G(f) != MX_NO)

static char *R(T *t, int64_t n) {
    if (n >= 255 || n < 0) return (char *)"0u";
    t->reg_used[n] = 1;
    return S(t, "r%d", (int)n);
}
static char *P(T *t, int64_t n, int64_t inv) {
    const char *b = n == 7 ? "true" : S(t, "p%d", (int)n);
    return inv ? S(t, "(!%s)", b) : (char *)b;
}
static void WR(T *t, int64_t d, const char *v) {
    if (d < 0 || d >= 255) return;
    t->reg_used[d] = 1;
    L(t, "r%d = %s;", (int)d, v);
}
static void WP(T *t, int64_t d, const char *v) {
    if (d >= 0 && d < 7) L(t, "p%d = %s;", (int)d, v);
}
static char *F(T *t, const char *u) { return S(t, "uintBitsToFloat(%s)", u); }
static char *U(T *t, const char *f) { return S(t, "floatBitsToUint(%s)", f); }
static char *absneg(T *t, const char *f, int64_t abs_, int64_t neg) {
    if (abs_ == 1) f = S(t, "abs(%s)", f);
    if (neg == 1) f = S(t, "(-%s)", f);
    return (char *)f;
}
static char *sat(T *t, const char *f, int64_t s) { return s == 1 ? S(t, "clamp(%s, 0.0, 1.0)", f) : (char *)f; }

static char *cbuf(T *t, int64_t slot, int64_t byte) {
    t->info->cbufs |= 1u << (slot & 31);
    return S(t, "cb%d[%d].%c", (int)slot, (int)((byte >> 4) & 0xFFF), "xyzw"[(byte >> 2) & 3]);
}
static int variant(int op) {
    const char *n = mx_op_name[op];
    size_t k = strlen(n);
    if (k > 3 && !strcmp(n + k - 3, "32i")) return V_32I;
    if (k > 2 && !strcmp(n + k - 2, "Rc")) return V_RC;
    if (n[k - 1] == 'R') return V_R;
    if (n[k - 1] == 'I') return V_I;
    if (n[k - 1] == 'C') return V_C;
    return V_NONE;
}
// Segundo operando (B) como uint. Los inmediatos de 20 bits son los bits altos de un float en las
// instrucciones de punto flotante y un entero con signo en las demás.
static char *B(T *t, int fp) {
    switch (t->var) {
    case V_R: return R(t, G(SrcB));
    case V_RC: return R(t, G(SrcC));
    case V_C: return cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    case V_32I: return S(t, "0x%08xu", (uint32_t)G(Imm32));
    case V_I: {
        int64_t v = HAS(Imm20) ? G(Imm20) : HAS(Imm16) ? G(Imm16) : G(Imm);
        if (!HAS(Imm20)) return S(t, "0x%08xu", (uint32_t)v);
        uint32_t x = fp ? (uint32_t)v << 12 : (uint32_t)((int32_t)((uint32_t)v << 12) >> 12);
        return S(t, "0x%08xu", x);
    }
    }
    return (char *)"0u";
}
static char *Cop(T *t) { // tercer operando
    if (t->var == V_RC) return cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    return R(t, G(SrcC));
}

// ---------- comparaciones ----------

static char *fcmp(T *t, int64_t c, const char *a, const char *b) {
    switch (c) {
    case 0: return (char *)"false";
    case 1: return S(t, "(%s < %s)", a, b);
    case 2: return S(t, "(%s == %s)", a, b);
    case 3: return S(t, "(%s <= %s)", a, b);
    case 4: return S(t, "(%s > %s)", a, b);
    case 5: return S(t, "(%s != %s && !isnan(%s) && !isnan(%s))", a, b, a, b);
    case 6: return S(t, "(%s >= %s)", a, b);
    case 7: return S(t, "(!isnan(%s) && !isnan(%s))", a, b);
    case 8: return S(t, "(isnan(%s) || isnan(%s))", a, b);
    case 9: return S(t, "!(%s >= %s)", a, b);
    case 10: return S(t, "!(%s != %s && !isnan(%s) && !isnan(%s))", a, b, a, b);
    case 11: return S(t, "!(%s > %s)", a, b);
    case 12: return S(t, "!(%s <= %s)", a, b);
    case 13: return S(t, "(%s != %s)", a, b);
    case 14: return S(t, "!(%s < %s)", a, b);
    default: return (char *)"true";
    }
}
static char *icmp(T *t, int64_t c, int64_t sgn, const char *a, const char *b) {
    if (sgn == 1) a = S(t, "int(%s)", a), b = S(t, "int(%s)", b);
    static const char *ops[] = {"", "<", "==", "<=", ">", "!=", ">="};
    if (c == 0) return (char *)"false";
    if (c >= 7) return (char *)"true";
    return S(t, "(%s %s %s)", a, ops[c], b);
}
static char *bop(T *t, int64_t op, const char *a, const char *b) {
    static const char *ops[] = {"&&", "||", "^^"};
    return S(t, "(%s %s %s)", a, ops[op < 0 || op > 2 ? 0 : op], b);
}
static char *ccc(T *t, int64_t c) {
    switch (c) {
    case 0: return (char *)"false";
    case 1: return (char *)"(cc_n != cc_v)";
    case 2: return (char *)"cc_z";
    case 3: return (char *)"(cc_z || cc_n != cc_v)";
    case 4: return (char *)"(!cc_z && cc_n == cc_v)";
    case 5: return (char *)"!cc_z";
    case 6: return (char *)"(cc_n == cc_v)";
    case 15: return (char *)"true";
    case 17: return (char *)"!cc_c";          // LO
    case 19: return (char *)"(!cc_c || cc_z)"; // LS
    case 20: return (char *)"(cc_c && !cc_z)"; // HI
    case 22: return (char *)"cc_c";           // HS
    case 18: return (char *)"!cc_n";          // SFF
    case 21: return (char *)"cc_n";           // SFT
    case 16: return (char *)"!cc_v";          // OFF
    case 23: return (char *)"cc_v";           // OFT
    }
    red(t, "código de condición");
    return (char *)"true";
}
static void set_zn(T *t, const char *res) {
    L(t, "cc_z = %s == 0u; cc_n = int(%s) < 0;", res, res);
}

// ---------- atributos ----------

// Primitiva de entrada de la geometría (NX_GS_*): vértices y nombre en GLSL.
static int gs_in_vertices(int p) { static const int n[] = {1, 2, 4, 3, 6}; return p >= 0 && p < 5 ? n[p] : 3; }
static const char *gs_in_name(int p) {
    static const char *n[] = {"points", "lines", "lines_adjacency", "triangles", "triangles_adjacency"};
    return p >= 0 && p < 5 ? n[p] : "triangles";
}

// Etapas cuyas entradas son arreglos de vértices (gl_in[], in_attrN[]).
static int por_vertice(int st) { return st == NX_SH_TESS_CTRL || st == NX_SH_TESS_EVAL || st == NX_SH_GEOMETRY; }

// Lee el atributo de dirección `a`. `vtx`: índice de vértice (etapas por vértice); `out`: leer una
// salida (control de teselación); `patch`: atributo por parche (niveles de teselación).
static char *attr_load(T *t, int a, int out, const char *vtx, int patch) {
    int st = t->stage;
    char c = "xyzw"[(a >> 2) & 3];
    if (patch) {
        if (a < 0x10) return U(t, S(t, "gl_TessLevelOuter[%d]", a >> 2));
        if (a < 0x18) return U(t, S(t, "gl_TessLevelInner[%d]", (a - 0x10) >> 2));
        red(t, S(t, "atributo por parche 0x%x", a));
        return (char *)"0u";
    }
    if (st == NX_SH_FRAGMENT) {
        if (a >= 0x70 && a < 0x80) return U(t, S(t, "gl_FragCoord.%c", c));
        if (a == 0x3FC) return (char *)"(gl_FrontFacing ? 0xffffffffu : 0u)";
        if (a == 0x2E0 || a == 0x2E4) return U(t, S(t, "gl_PointCoord.%c", "xy"[(a >> 2) & 1]));
    }
    if (st == NX_SH_VERTEX) {
        if (a == 0x2FC) return (char *)"uint(gl_VertexID)";
        if (a == 0x2F8) { // las instancias de varios draws se juntan en uno: nx_inst0 es la primera
            t->info->uses_instance = 1;
            return (char *)"(uint(gl_InstanceID) + nx_inst0)";
        }
    }
    if (st == NX_SH_TESS_EVAL && (a == 0x2F0 || a == 0x2F4)) return U(t, S(t, "gl_TessCoord.%c", "xy"[(a >> 2) & 1]));
    if (a == 0x60 && st != NX_SH_VERTEX) return (char *)"uint(gl_PrimitiveID)";
    if (a == 0x64 && st == NX_SH_FRAGMENT) return (char *)"uint(gl_Layer)";
    const char *idx = por_vertice(st) ? S(t, "[%s]", out && st == NX_SH_TESS_CTRL ? (vtx ? vtx : "gl_InvocationID") : vtx ? vtx : "0") : "";
    if (a >= 0x70 && a < 0x80) {
        if (out) return U(t, st == NX_SH_TESS_CTRL ? S(t, "gl_out%s.gl_Position.%c", idx, c) : S(t, "gl_Position.%c", c));
        if (por_vertice(st)) return U(t, S(t, "gl_in%s.gl_Position.%c", idx, c));
    }
    if (a >= 0x80 && a < 0x280) {
        int i = (a - 0x80) >> 4;
        if (out) {
            t->info->outputs |= 1u << i;
            t->out_comps[i] |= 1u << ((a >> 2) & 3);
            return U(t, S(t, "out_attr%d%s.%c", i, st == NX_SH_TESS_CTRL ? idx : "", c));
        }
        t->info->inputs |= 1u << i;
        if (st == NX_SH_VERTEX && t->key->attr_int[i]) return S(t, "uint(in_attr%d.%c)", i, c);
        return U(t, S(t, "in_attr%d%s.%c", i, idx, c));
    }
    red(t, S(t, "atributo 0x%x sin leer", a));
    return (char *)"0u";
}
static void attr_store(T *t, int a, const char *v, int patch) {
    int st = t->stage;
    const char *inv = st == NX_SH_TESS_CTRL ? "[gl_InvocationID]" : "";
    if (patch) {
        if (a < 0x10) L(t, "gl_TessLevelOuter[%d] = uintBitsToFloat(%s);", a >> 2, v);
        else if (a < 0x18) L(t, "gl_TessLevelInner[%d] = uintBitsToFloat(%s);", (a - 0x10) >> 2, v);
        else red(t, S(t, "atributo por parche 0x%x sin escribir", a));
        return;
    }
    if (a >= 0x70 && a < 0x80) {
        L(t, "%s.%c = uintBitsToFloat(%s);", st == NX_SH_TESS_CTRL ? "gl_out[gl_InvocationID].gl_Position" : "gl_Position",
          "xyzw"[(a >> 2) & 3], v);
    } else if (a >= 0x80 && a < 0x280) {
        int i = (a - 0x80) >> 4, c = (a >> 2) & 3;
        t->info->outputs |= 1u << i;
        t->out_comps[i] |= 1u << c;
        L(t, "out_attr%d%s.%c = uintBitsToFloat(%s);", i, inv, "xyzw"[c], v);
    } else if (a == 0x6C) {
        t->writes_psize = 1;
        L(t, "gl_PointSize = uintBitsToFloat(%s);", v);
    } else if (a >= 0x2C0 && a < 0x2E0) {
        int i = (a - 0x2C0) >> 2;
        t->clip_mask |= 1u << i;
        L(t, "gl_ClipDistance[%d] = uintBitsToFloat(%s);", i, v);
    } else if (a == 0x64) {
        t->writes_layer = 1;
        L(t, "gl_Layer = int(%s);", v);
    } else {
        red(t, S(t, "atributo 0x%x sin escribir", a));
    }
}

static void frag_outputs(T *t) {
    uint32_t omap = t->sph[18];
    int base = 0;
    for (int rt = 0; rt < 8; rt++) {
        for (int c = 0; c < 4; c++)
            if (omap >> (rt * 4 + c) & 1)
                L(t, "frag%d.%c = %s(%s);", rt, "xyzw"[c],
                  t->key->rt_int[rt] == 1 ? "int" : t->key->rt_int[rt] == 2 ? "uint" : "uintBitsToFloat", R(t, base + c));
        if (omap >> (rt * 4) & 15) base += 4;
    }
    if (t->sph[19] & 2) {
        t->frag_depth = 1;
        L(t, "gl_FragDepth = uintBitsToFloat(%s);", R(t, __builtin_popcount(omap) + 1));
    }
}

// ---------- flujo ----------

static uint32_t next_idx(uint32_t i) { return (i + 1) % 4 ? i + 1 : i + 2; }
static uint32_t target(T *t, uint32_t i, uint64_t o) {
    int32_t imm = (int32_t)((uint32_t)(o >> 20) << 8) >> 8;
    uint32_t tg = (uint32_t)(((int64_t)i * 8 + 8 + imm) / 8);
    return tg % 4 ? tg : tg + 1; // apuntar a una palabra de control es apuntar a la instrucción siguiente
}
static int decode(T *t, uint32_t i, uint64_t *o) {
    memcpy(o, t->code + (size_t)i * 8, 8);
    uint16_t top = (uint16_t)(*o >> 50);
    int best = MXOP_INVALID, bx = 99;
    for (size_t k = 0; k < sizeof mx_patterns / sizeof *mx_patterns; k++)
        if ((top & mx_patterns[k].mask) == mx_patterns[k].value && mx_patterns[k].xbits < bx)
            best = mx_patterns[k].op, bx = mx_patterns[k].xbits;
    return best;
}
static int unconditional(T *t) { // predicado siempre verdadero y código de condición T
    int64_t ccc_ = G(Ccc);
    return G(Pred) == 7 && G(PredInv) != 1 && (ccc_ == MX_NO || ccc_ == 15);
}

static int discover(T *t) {
    uint32_t *work = malloc(sizeof(uint32_t) * (t->ninsn + 1));
    int nw = 0;
    work[nw++] = 1;
    t->leader[1] = 1;
    while (nw) {
        uint32_t i = work[--nw];
        while (i < t->ninsn && !t->visited[i]) {
            t->visited[i] = 1;
            t->idx = i;
            t->op = decode(t, i, &t->o);
            int fall = 1;
            switch (t->op) {
            case MXOP_Bra: case MXOP_Ssy: case MXOP_Pbk: case MXOP_Pcnt: case MXOP_Cal: case MXOP_Pret: {
                if (G(Ca) == 1) {
                    red(t, "salto a una dirección de constbuf");
                    break;
                }
                uint32_t tg = target(t, i, t->o);
                if (tg >= t->ninsn) break;
                t->leader[tg] = 1;
                work[nw++] = tg;
                t->uses_flow = 1;
                if (t->op != MXOP_Bra) t->uses_stack = 1;
                if (t->op == MXOP_Bra && unconditional(t)) fall = 0;
                if (t->op == MXOP_Cal) {
                    uint32_t r = next_idx(i);
                    t->leader[r] = 1;
                }
                break;
            }
            case MXOP_Exit: case MXOP_Kil:
                if (unconditional(t)) fall = 0;
                break;
            case MXOP_Sync: case MXOP_Brk: case MXOP_Cont: case MXOP_Ret:
                t->uses_flow = t->uses_stack = 1;
                if (unconditional(t)) fall = 0;
                break;
            case MXOP_INVALID:
                fall = 0;
                break;
            }
            if (!fall) break;
            uint32_t n = next_idx(i);
            if (t->visited[n] || n >= t->ninsn) { // cae en código ya visto: necesita etiqueta
                if (n < t->ninsn) t->leader[n] = 1, t->uses_flow = 1;
                break;
            }
            i = n;
        }
    }
    free(work);
    return 0;
}

static void jump(T *t, const char *cond, const char *to) {
    if (strcmp(cond, "true")) L(t, "if (%s) { pc = %s; continue; }", cond, to);
    else L(t, "pc = %s; continue;", to);
}

// ---------- instrucciones ----------

static void emit_ald(T *t) {
    if (G(P) != 1 && G(Imm11) == 0 && G(SrcA) != 255) {
        red(t, "ALD con dirección en registro");
        return;
    }
    int n = (int)G(AlSize) + 1, d = (int)G(Dest), a = (int)G(Imm11), patch = G(P) == 1;
    const char *vtx = NULL;
    L(t, "{");
    if (por_vertice(t->stage) && G(SrcB) != 255) L(t, "uint vi = %s;", R(t, G(SrcB))), vtx = "vi";
    for (int k = 0; k < n; k++)
        if (d + k < 255) L(t, "uint v%d = %s;", k, attr_load(t, a + k * 4, G(O) == 1, vtx, patch));
    for (int k = 0; k < n; k++)
        if (d + k < 255) WR(t, d + k, S(t, "v%d", k));
    L(t, "}");
}
static void emit_ast(T *t) {
    if (G(P) != 1 && G(Imm11) == 0 && G(SrcA) != 255) {
        red(t, "AST con dirección en registro");
        return;
    }
    int n = (int)G(AlSize) + 1, s = (int)G(SrcB), a = (int)G(Imm11);
    for (int k = 0; k < n && s + k < 255; k++) attr_store(t, a + k * 4, R(t, s + k), G(P) == 1);
}
static void emit_ipa(T *t) {
    if (G(Idx) == 1) {
        red(t, "IPA indexado");
        return;
    }
    int a = (int)G(Imm10);
    char *v = F(t, attr_load(t, a, 0, NULL, 0));
    int fixed = !(a >= 0x80 && a < 0x280);
    if (!fixed) {
        int i = (a - 0x80) >> 4;
        uint8_t imap = t->sph[6 + (i >> 2)] >> ((i & 3) * 8);
        int mode = imap & 3 ? imap & 3 : imap >> 2 & 3 ? imap >> 2 & 3 : imap >> 4 & 3 ? imap >> 4 & 3 : imap >> 6 & 3;
        if (mode == 1) t->in_flat |= 1u << i;
        else if (mode == 3) t->in_linear |= 1u << i;
        else v = S(t, "(%s * gl_FragCoord.w)", v); // el código multiplica después por w
    }
    if (G(IpaOp) == 1 && !fixed) v = S(t, "(%s * %s)", v, F(t, R(t, G(SrcB))));
    WR(t, G(Dest), U(t, sat(t, v, G(Sat))));
}
static void emit_ldc(T *t) {
    int64_t slot = G(CbufSlot), off = (int16_t)G(CbufOffset), sz = G(LsSize), d = G(Dest);
    t->info->cbufs |= 1u << (slot & 31);
    if (G(SrcA) == 255) {
        int nw = sz == 5 ? 2 : sz == 6 ? 4 : 1;
        for (int k = 0; k < nw; k++) {
            char *v = cbuf(t, slot, off + k * 4);
            if (sz < 4) {
                int sh = (int)(off & 3) * 8, bits = sz < 2 ? 8 : 16;
                v = sz & 1 ? S(t, "uint(bitfieldExtract(int(%s), %d, %d))", v, sh, bits)
                           : S(t, "bitfieldExtract(%s, %d, %d)", v, sh, bits);
            }
            WR(t, d + k, v);
        }
        return;
    }
    char *a = S(t, "(%s + %du)", R(t, G(SrcA)), (int)off);
    L(t, "{ uint a = %s;", a);
    int nw = sz == 5 ? 2 : sz == 6 ? 4 : 1;
    for (int k = 0; k < nw; k++) {
        char *v = S(t, "cb%d[((a + %du) >> 4) & 0xfffu][((a + %du) >> 2) & 3u]", (int)slot, k * 4, k * 4);
        if (sz < 4) {
            int bits = sz < 2 ? 8 : 16;
            v = sz & 1 ? S(t, "uint(bitfieldExtract(int(%s), int((a & 3u) * 8u), %d))", v, bits)
                       : S(t, "bitfieldExtract(%s, int((a & 3u) * 8u), %d)", v, bits);
        }
        if (d + k < 255) L(t, "uint v%d = %s;", k, v);
    }
    for (int k = 0; k < nw; k++)
        if (d + k < 255) WR(t, d + k, S(t, "v%d", k));
    L(t, "}");
}
static void emit_mufu(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), G(AbsA), G(NegA));
    char *r;
    switch (G(MufuOp)) {
    case 0: r = S(t, "cos(%s)", a); break;
    case 1: r = S(t, "sin(%s)", a); break;
    case 2: r = S(t, "exp2(%s)", a); break;
    case 3: r = S(t, "log2(%s)", a); break;
    case 4: r = S(t, "(1.0 / %s)", a); break;
    case 5: r = S(t, "inversesqrt(%s)", a); break;
    case 8: r = S(t, "sqrt(%s)", a); break;
    case 6: case 7: { // la mitad alta de un double
        char *dd = S(t, "packDouble2x32(uvec2(0u, %s))", R(t, G(SrcA)));
        char *res = G(MufuOp) == 6 ? S(t, "(1.0lf / %s)", dd) : S(t, "inversesqrt(%s)", dd);
        WR(t, G(Dest), S(t, "unpackDouble2x32(%s).y", res));
        return;
    }
    default: red(t, "MUFU"); return;
    }
    WR(t, G(Dest), U(t, sat(t, r, G(Sat))));
}
static void emit_fadd(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), G(AbsA), G(NegA));
    char *b = absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB));
    char *r = sat(t, S(t, "(%s + %s)", a, b), G(Sat));
    L(t, "{ uint v = %s;", U(t, r));
    if (G(WriteCC) == 1) set_zn(t, "v");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_fmul(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), 0, G(NegA));
    char *b = F(t, B(t, 1));
    char *r = S(t, "(%s * %s)", a, b);
    static const char *scale[] = {"", " * 0.5", " * 0.25", " * 0.125", " * 8.0", " * 4.0", " * 2.0"};
    int64_t sc = G(Scale);
    if (sc > 0 && sc < 7) r = S(t, "(%s%s)", r, scale[sc]);
    r = sat(t, r, G(Sat));
    L(t, "{ uint v = %s;", U(t, r));
    if (G(WriteCC) == 1) set_zn(t, "v");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_ffma(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), 0, G(NegA));
    char *b, *c;
    if (t->var == V_32I) b = F(t, S(t, "0x%08xu", (uint32_t)G(Imm32))), c = F(t, R(t, G(Dest)));
    else b = F(t, B(t, 1)), c = F(t, t->var == V_RC ? cbuf(t, G(CbufSlot), G(CbufOffset) * 4) : R(t, G(SrcC)));
    c = absneg(t, c, 0, G(NegC));
    char *r = sat(t, S(t, "fma(%s, %s, %s)", a, b, c), G(Sat));
    L(t, "{ uint v = %s;", U(t, r));
    if (G(WriteCC) == 1) set_zn(t, "v");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_fmnmx(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), G(AbsA), G(NegA));
    char *b = absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB));
    char *p = P(t, G(SrcPred), G(SrcPredInv));
    WR(t, G(Dest), U(t, S(t, "(%s ? min(%s, %s) : max(%s, %s))", p, a, b, a, b)));
}
static void emit_fsetp(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), G(AbsA), G(NegA));
    char *b = absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB));
    char *c = fcmp(t, G(FComp), a, b), *p = P(t, G(SrcPred), G(SrcPredInv));
    L(t, "{ bool c = %s;", c);
    WP(t, G(DestPred), bop(t, G(Bop), "c", p));
    WP(t, G(DestPredInv), bop(t, G(Bop), "!c", p));
    L(t, "}");
}
static void emit_fset(T *t) {
    char *a = absneg(t, F(t, R(t, G(SrcA))), G(AbsA), G(NegA));
    char *b = absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB));
    char *c = bop(t, G(Bop), fcmp(t, G(FComp), a, b), P(t, G(SrcPred), G(SrcPredInv)));
    WR(t, G(Dest), S(t, "(%s ? %s : 0u)", c, G(BVal) == 1 ? "0x3f800000u" : "0xffffffffu"));
}
static void emit_iadd(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0);
    int64_t avg = G(AvgMode);
    if (avg == 2) a = S(t, "(0u - %s)", a);
    if (avg == 1) b = S(t, "(0u - %s)", b);
    int x = G(X) == 1;
    L(t, "{ uint a = %s, b = %s; uint v = a + b%s;", a, b, x ? " + (cc_c ? 1u : 0u)" : "");
    if (avg == 3) L(t, "v += 1u;");
    if (G(WriteCC) == 1) {
        L(t, "cc_c = %s;", x ? "(v < a) || (v == a && cc_c)" : "v < a");
        L(t, "cc_v = int((v ^ a) & ~(a ^ b)) < 0;");
        set_zn(t, "v");
    }
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_iadd3(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0), *c = R(t, G(SrcC));
    if (t->var == V_R) {
        static const char *part[] = {"%s", "(%s & 0xffffu)", "(%s >> 16)"};
        int64_t pa = G(Apart), pb = G(Bpart), pc = G(Cpart);
        a = S(t, part[pa < 0 || pa > 2 ? 0 : pa], a);
        b = S(t, part[pb < 0 || pb > 2 ? 0 : pb], b);
        c = S(t, part[pc < 0 || pc > 2 ? 0 : pc], c);
    }
    if (G(NegA) == 1) a = S(t, "(0u - %s)", a);
    if (G(NegB) == 1) b = S(t, "(0u - %s)", b);
    if (G(NegC) == 1) c = S(t, "(0u - %s)", c);
    char *ab = S(t, "(%s + %s)", a, b);
    if (G(Lrs) == 1) ab = S(t, "(%s >> 16)", ab);
    if (G(Lrs) == 2) ab = S(t, "(%s << 16)", ab);
    L(t, "{ uint v = %s + %s%s;", ab, c, G(X) == 1 ? " + (cc_c ? 1u : 0u)" : "");
    if (G(WriteCC) == 1) set_zn(t, "v");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_iscadd(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0);
    if (t->var == V_32I) b = S(t, "0x%08xu", (uint32_t)G(Imm32));
    int64_t avg = G(AvgMode);
    if (avg == 2) a = S(t, "(0u - %s)", a);
    if (avg == 1) b = S(t, "(0u - %s)", b);
    L(t, "{ uint v = (%s << %du) + %s;", a, (int)G(Imm5), b);
    if (G(WriteCC) == 1) set_zn(t, "v");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_xmad(T *t) {
    char *a = R(t, G(SrcA)), *bu;
    char *c;
    if (t->var == V_RC) bu = R(t, G(SrcC)), c = cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    else if (t->var == V_I) bu = S(t, "0x%xu", (uint32_t)G(Imm16)), c = R(t, G(SrcC));
    else bu = B(t, 0), c = R(t, G(SrcC));
    int sa = G(ASigned) == 1, sb = G(BSigned) == 1, ha = G(HiloA) == 1, hb = t->var == V_I ? 0 : G(HiloB) == 1;
    L(t, "{ uint b0 = %s, c0 = %s;", bu, c);
#define EXT(dst, src, hi, sg)                                                                          \
    L(t, "uint %s = %s;", dst,                                                                         \
      sg ? (hi ? S(t, "uint(int(%s) >> 16)", src) : S(t, "uint(bitfieldExtract(int(%s), 0, 16))", src)) \
         : (hi ? S(t, "(%s >> 16)", src) : S(t, "(%s & 0xffffu)", src)))
    EXT("a1", a, ha, sa);
    EXT("b1", "b0", hb, sb);
#undef EXT
    L(t, "uint p = a1 * b1;");
    if (G(Psl) == 1) L(t, "p <<= 16;");
    int64_t mode = G(XmadCop);
    if (t->var == V_RC && mode == 4) mode = -1; // XmadCop2 no tiene Cbcc
    switch (mode) {
    case 1: L(t, "c0 &= 0xffffu;"); break;
    case 2: L(t, "c0 >>= 16;"); break;
    case 3: L(t, "c0 -= ((a1 >> 31) << 16) + ((b1 >> 31) << 16);"); break;
    case 4: L(t, "c0 += b0 << 16;"); break;
    }
    L(t, "uint v = p + c0%s;", G(X) == 1 ? " + (cc_c ? 1u : 0u)" : "");
    if (G(WriteCC) == 1) {
        L(t, "cc_c = %s;", G(X) == 1 ? "(v < p) || (v == p && cc_c)" : "v < p");
        L(t, "cc_v = int((v ^ p) & ~(p ^ c0)) < 0;");
        set_zn(t, "v");
    }
    if (G(Mrg) == 1) L(t, "v = (v & 0xffffu) | (b0 << 16);");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_imad(T *t) {
    char *a = R(t, G(SrcA)), *b = t->var == V_32I ? S(t, "0x%08xu", (uint32_t)G(Imm32)) : B(t, 0);
    char *c = t->var == V_32I ? R(t, G(Dest)) : Cop(t);
    int sg = G(ASigned) == 1 || G(BSigned) == 1;
    char *m;
    if (G(Hilo) == 1) m = sg ? S(t, "uint((int64_t(int(%s)) * int64_t(int(%s))) >> 32)", a, b)
                             : S(t, "uint((uint64_t(%s) * uint64_t(%s)) >> 32)", a, b);
    else m = S(t, "(%s * %s)", a, b);
    WR(t, G(Dest), S(t, "(%s + %s)", m, c));
}
static void emit_imul(T *t) {
    char *a = R(t, G(SrcA)), *b = t->var == V_32I ? S(t, "0x%08xu", (uint32_t)G(Imm32)) : B(t, 0);
    int sg = G(ASigned) == 1 || G(BSigned) == 1;
    if (G(Hilo) == 1)
        WR(t, G(Dest), sg ? S(t, "uint((int64_t(int(%s)) * int64_t(int(%s))) >> 32)", a, b)
                          : S(t, "uint((uint64_t(%s) * uint64_t(%s)) >> 32)", a, b));
    else WR(t, G(Dest), S(t, "(%s * %s)", a, b));
}
static void emit_imnmx(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0), *p = P(t, G(SrcPred), G(SrcPredInv));
    if (G(Signed) == 1)
        WR(t, G(Dest), S(t, "uint(%s ? min(int(%s), int(%s)) : max(int(%s), int(%s)))", p, a, b, a, b));
    else WR(t, G(Dest), S(t, "(%s ? min(%s, %s) : max(%s, %s))", p, a, b, a, b));
}
static void emit_isetp(T *t) {
    char *c = icmp(t, G(IComp), G(Signed), R(t, G(SrcA)), B(t, 0)), *p = P(t, G(SrcPred), G(SrcPredInv));
    L(t, "{ bool c = %s;", c);
    WP(t, G(DestPred), bop(t, G(Bop), "c", p));
    WP(t, G(DestPredInv), bop(t, G(Bop), "!c", p));
    L(t, "}");
}
static void emit_iset(T *t) {
    char *c = bop(t, G(Bop), icmp(t, G(IComp), G(Signed), R(t, G(SrcA)), B(t, 0)), P(t, G(SrcPred), G(SrcPredInv)));
    WR(t, G(Dest), S(t, "(%s ? %s : 0u)", c, G(BVal) == 1 ? "0x3f800000u" : "0xffffffffu"));
}
static void emit_lop(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0);
    if (G(NegA) == 1) a = S(t, "(~%s)", a);
    if (G(NegB) == 1) b = S(t, "(~%s)", b);
    int64_t lop = HAS(Lop) ? G(Lop) : G(LogicOp);
    static const char *ops[] = {"(%s & %s)", "(%s | %s)", "(%s ^ %s)"};
    char *r = lop == 3 ? b : S(t, ops[lop < 0 || lop > 2 ? 0 : lop], a, b);
    L(t, "{ uint v = %s;", r);
    if (G(WriteCC) == 1) set_zn(t, "v");
    int64_t pop = G(PredicateOp);
    if (pop != MX_NO && G(DestPred) != 7) {
        static const char *pv[] = {"false", "true", "(v == 0u)", "(v != 0u)"};
        WP(t, G(DestPred), pv[pop & 3]);
    }
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_lop3(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0), *c = R(t, G(SrcC));
    if (t->var == V_I) b = S(t, "0x%08xu", (uint32_t)((int32_t)((uint32_t)G(Imm20) << 12) >> 12));
    int lut = (int)G(Imm) & 0xFF;
    L(t, "{ uint a = %s, b = %s, c = %s; uint v = 0u;", a, b, c);
    for (int i = 0; i < 8; i++)
        if (lut >> i & 1) L(t, "v |= %sa & %sb & %sc;", i & 4 ? "" : "~", i & 2 ? "" : "~", i & 1 ? "" : "~");
    if (G(WriteCC) == 1) set_zn(t, "v");
    int64_t pop = G(PredicateOp);
    if (pop != MX_NO && G(DestPred) != 7) {
        static const char *pv[] = {"false", "true", "(v == 0u)", "(v != 0u)"};
        WP(t, G(DestPred), pv[pop & 3]);
    }
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_shift(T *t, int right) {
    char *a = R(t, G(SrcA)), *b = B(t, 0);
    int wrap = G(M) == 1;
    L(t, "{ uint a = %s, s = %s;", a, b);
    if (right && G(Brev) == 1) L(t, "a = bitfieldReverse(a);");
    if (wrap) L(t, "s &= 31u;");
    if (!right) L(t, "uint v = s < 32u ? a << s : 0u;");
    else if (G(Signed) == 1) L(t, "uint v = uint(int(a) >> min(s, 31u));");
    else L(t, "uint v = s < 32u ? a >> s : 0u;");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_shf(T *t, int right) {
    // Desplazamiento en embudo del par (alto = C, bajo = A).
    char *lo = R(t, G(SrcA)), *s = B(t, 0), *hi = R(t, G(SrcC));
    int64_t ms = G(MaxShift);
    L(t, "{ uint64_t x = (uint64_t(%s) << 32) | uint64_t(%s); uint s = %s & %s;", hi, lo, s,
      G(M) == 1 ? (ms >= 2 ? "63u" : "31u") : "0xffffffffu");
    if (G(M) != 1) L(t, "s = min(s, %s);", ms >= 2 ? "64u" : "32u");
    if (right) {
        L(t, "uint64_t y = s >= 64u ? %s : %s;", ms == 3 ? "uint64_t(int64_t(x) >> 63)" : "0ul",
          ms == 3 ? "uint64_t(int64_t(x) >> s)" : "x >> s");
        WR(t, G(Dest), "uint(y)");
    } else {
        L(t, "uint64_t y = s >= 64u ? 0ul : x << s;");
        WR(t, G(Dest), "uint(y >> 32)");
    }
    L(t, "}");
}
static void emit_bfe(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0);
    L(t, "{ uint a = %s, b = %s; uint pos = b & 0xffu, len = (b >> 8) & 0xffu;", a, b);
    if (G(Brev) == 1) L(t, "a = bitfieldReverse(a);");
    L(t, "uint v;");
    L(t, "if (len == 0u) v = 0u;");
    if (G(Signed) == 1)
        L(t, "else if (pos >= 32u) v = uint(int(a) >> 31); else v = uint(bitfieldExtract(int(a), int(pos), int(min(len, 32u - pos))));");
    else L(t, "else if (pos >= 32u) v = 0u; else v = bitfieldExtract(a, int(pos), int(min(len, 32u - pos)));");
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_bfi(T *t) {
    // BFI: inserta A en C según B (posición y largo); la variante RC toma B del registro C.
    char *a = R(t, G(SrcA)), *b, *c;
    if (t->var == V_RC) b = R(t, G(SrcC)), c = cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    else b = B(t, 0), c = R(t, G(SrcC));
    L(t, "{ uint b = %s; uint pos = b & 0xffu, len = (b >> 8) & 0xffu; uint c = %s;", b, c);
    L(t, "uint v = (len == 0u || pos >= 32u) ? c : bitfieldInsert(c, %s, int(pos), int(min(len, 32u - pos)));", a);
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_mov(T *t) {
    char *v;
    switch (t->var) {
    case V_R: v = R(t, G(SrcA)); break;
    case V_32I: v = S(t, "0x%08xu", (uint32_t)G(Imm32)); break;
    default: v = B(t, 0);
    }
    WR(t, G(Dest), v);
}
static void emit_sel(T *t) {
    WR(t, G(Dest), S(t, "(%s ? %s : %s)", P(t, G(SrcPred), G(SrcPredInv)), R(t, G(SrcA)), B(t, 0)));
}
static void emit_popc(T *t) {
    char *b = B(t, 0);
    if (G(NegB) == 1) b = S(t, "(~%s)", b);
    WR(t, G(Dest), S(t, "uint(bitCount(%s))", b));
}
static void emit_flo(T *t) {
    char *b = B(t, 0);
    if (G(NegB) == 1) b = S(t, "(~%s)", b);
    char *m = G(Signed) == 1 ? S(t, "findMSB(int(%s))", b) : S(t, "findMSB(%s)", b);
    if (G(Sh) == 1) WR(t, G(Dest), S(t, "(%s < 0 ? 0xffffffffu : uint(31 - %s))", m, m));
    else WR(t, G(Dest), S(t, "uint(%s)", m));
}
static void emit_prmt(T *t) {
    char *a = R(t, G(SrcA)), *b = B(t, 0), *c = Cop(t);
    if (t->var == V_RC) b = R(t, G(SrcC)), c = cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    int64_t mode = G(PMode);
    L(t, "{ uint a = %s, sel = %s, c = %s; uint64_t x = (uint64_t(c) << 32) | uint64_t(a); uint v = 0u;", a, b, c);
    if (mode == 0 || mode == MX_NO) {
        L(t, "for (int i = 0; i < 4; i++) { uint s = (sel >> (i * 4)) & 0xfu; uint byt = uint(x >> ((s & 7u) * 8u)) & 0xffu;");
        L(t, "if ((s & 8u) != 0u) byt = (byt & 0x80u) != 0u ? 0xffu : 0u; v |= byt << (i * 8); }");
    } else {
        red(t, "PRMT con modo especial");
    }
    WR(t, G(Dest), "v");
    L(t, "}");
}
static void emit_psetp(T *t) {
    char *a = P(t, G(Src1Pred), G(Src1PredInv)), *b = P(t, G(Src2Pred), G(Src2PredInv));
    char *c = P(t, G(SrcPred), G(SrcPredInv));
    L(t, "{ bool r = %s;", bop(t, G(BoolOpAB), a, b));
    WP(t, G(DestPred), bop(t, G(BoolOpC), "r", c));
    WP(t, G(DestPredInv), bop(t, G(BoolOpC), "!r", c));
    L(t, "}");
}
static void emit_p2r(T *t) {
    char *mask = B(t, 0);
    int64_t sh = G(ByteSel) == MX_NO ? 0 : G(ByteSel) * 8;
    if (G(Ccpr) == 1) {
        red(t, "P2R de códigos de condición");
        return;
    }
    L(t, "{ uint m = %s << %du; uint v = 0u;", mask, (int)sh);
    for (int i = 0; i < 7; i++) L(t, "if (p%d) v |= %uu;", i, 1u << (i + sh));
    WR(t, G(Dest), S(t, "((%s & ~m) | (v & m))", R(t, G(SrcA))));
    L(t, "}");
}
static void emit_r2p(T *t) {
    int64_t sh = G(ByteSel) == MX_NO ? 0 : G(ByteSel) * 8;
    if (G(Ccpr) == 1) {
        red(t, "R2P a códigos de condición");
        return;
    }
    L(t, "{ uint v = %s, m = %s << %du;", R(t, G(SrcA)), B(t, 0), (int)sh);
    for (int i = 0; i < 7; i++) L(t, "if ((m & %uu) != 0u) p%d = (v & %uu) != 0u;", 1u << (i + sh), i, 1u << (i + sh));
    L(t, "}");
}
static void emit_i2f(T *t) {
    char *b = B(t, 0);
    int64_t src = G(ISrcFmt), dst = G(DstFmt), bs = G(ByteSel);
    int sg = src >= 4, bits = (src & 3) == 0 ? 8 : (src & 3) == 1 ? 16 : 32;
    if ((src & 3) == 3 || dst != 2) {
        red(t, "I2F de 64 bits o a otro ancho");
        return;
    }
    if (bits < 32) b = sg ? S(t, "bitfieldExtract(int(%s), %d, %d)", b, (int)bs * 8, bits)
                          : S(t, "bitfieldExtract(%s, %d, %d)", b, (int)bs * 8, bits);
    else if (sg) b = S(t, "int(%s)", b);
    char *f = S(t, "float(%s)", b);
    f = absneg(t, f, G(AbsB), G(NegB));
    WR(t, G(Dest), U(t, f));
}
static void emit_f2i(T *t) {
    int64_t dst = G(IDstFmt), src = G(SrcFmt);
    if (src != 2 || (dst & 3) == 3) {
        red(t, "F2I desde otro ancho o a 64 bits");
        return;
    }
    char *f = absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB));
    static const char *rnd[] = {"roundEven", "floor", "ceil", "trunc"};
    int64_t rm = G(RoundMode);
    f = S(t, "%s(%s)", rnd[rm & 3], f);
    int sg = dst >= 4, bits = (dst & 3) == 1 ? 16 : 32;
    char *v;
    if (sg) v = bits == 16 ? S(t, "uint(int(clamp(%s, -32768.0, 32767.0)))", f)
                           : S(t, "(isnan(%s) ? 0u : uint(int(clamp(%s, -2147483648.0, 2147483520.0))))", f, f);
    else v = bits == 16 ? S(t, "uint(clamp(%s, 0.0, 65535.0))", f)
                        : S(t, "(isnan(%s) ? 0u : (%s >= 4294967296.0 ? 0xffffffffu : uint(max(%s, 0.0))))", f, f, f);
    WR(t, G(Dest), v);
}
static char *dreg(T *t, int64_t r);
static char *dsrc_b(T *t);
static void dwr(T *t, int64_t d, const char *v);

static void emit_f2f(T *t) {
    int64_t dst = G(DstFmt), src = G(SrcFmt), rm = G(RoundMode);
    char *f;
    if (src == 2) f = F(t, B(t, 1));
    else if (src == 1) f = S(t, "unpackHalf2x16(%s).%c", B(t, 0), G(Sh) == 1 ? 'y' : 'x');
    else f = dsrc_b(t); // double
    if (dst == 3 && src != 3) f = S(t, "double(%s)", f);
    f = absneg(t, f, G(AbsB), G(NegB));
    switch (rm) {
    case 4: f = S(t, "roundEven(%s)", f); break;
    case 5: f = S(t, "floor(%s)", f); break;
    case 6: f = S(t, "ceil(%s)", f); break;
    case 7: f = S(t, "trunc(%s)", f); break;
    }
    f = sat(t, f, G(Sat));
    if (dst == 2) WR(t, G(Dest), U(t, src == 3 ? S(t, "float(%s)", f) : f));
    else if (dst == 1) WR(t, G(Dest), S(t, "packHalf2x16(vec2(%s, 0.0))", src == 3 ? S(t, "float(%s)", f) : f));
    else dwr(t, G(Dest), f);
}
static void emit_i2i(T *t) {
    int64_t src = G(ISrcFmt), dst = G(IDstFmt), bs = G(ByteSel);
    char *b = B(t, 0);
    int ssg = src >= 4, sbits = (src & 3) == 0 ? 8 : (src & 3) == 1 ? 16 : 32;
    L(t, "{ int64_t v = %s;", ssg ? S(t, "int64_t(bitfieldExtract(int(%s), %d, %d))", b, (int)bs * 8, sbits < 32 ? sbits : 32 - (int)bs * 8)
                                  : S(t, "int64_t(bitfieldExtract(%s, %d, %d))", b, (int)bs * 8, sbits < 32 ? sbits : 32 - (int)bs * 8));
    if (G(AbsB) == 1) L(t, "v = abs(v);");
    if (G(NegB) == 1) L(t, "v = -v;");
    int dsg = dst >= 4, dbits = (dst & 3) == 1 ? 16 : 32;
    if (G(Sat) == 1 || 1) { // la conversión de enteros satura al rango del destino
        if (dsg) L(t, "v = clamp(v, %ldl, %ldl);", dbits == 16 ? -32768L : -2147483648L, dbits == 16 ? 32767L : 2147483647L);
        else L(t, "v = clamp(v, 0l, %ldl);", dbits == 16 ? 65535L : 4294967295L);
    }
    WR(t, G(Dest), "uint(v)");
    L(t, "}");
}
static void emit_s2r(T *t) {
    char *v;
    switch (G(SReg)) {
    case 0: v = "0u"; break;                              // LaneId
    case 17: v = t->stage == NX_SH_TESS_CTRL || t->stage == NX_SH_GEOMETRY ? "uint(gl_InvocationID)" : "0u"; break;
    case 29: // InvocationInfo: vértices por primitiva en los bits 16-23 (el índice de primitiva queda en 0)
        v = t->stage == NX_SH_TESS_CTRL || t->stage == NX_SH_TESS_EVAL ? "(uint(gl_PatchVerticesIn) << 16)"
          : t->stage == NX_SH_GEOMETRY ? S(t, "%du", gs_in_vertices(t->key->gs_input) << 16) : "0u";
        break;
    case 18: v = "0x3f800000u"; break;                    // YDirection (1.0)
    case 32: v = "(gl_LocalInvocationID.x | (gl_LocalInvocationID.y << 16) | (gl_LocalInvocationID.z << 26))"; break;
    case 33: v = "gl_LocalInvocationID.x"; break;
    case 34: v = "gl_LocalInvocationID.y"; break;
    case 35: v = "gl_LocalInvocationID.z"; break;
    case 37: v = "gl_WorkGroupID.x"; break;
    case 38: v = "gl_WorkGroupID.y"; break;
    case 39: v = "gl_WorkGroupID.z"; break;
    case 80: v = "0xffffffffu"; break;                    // EqMask etc. (un solo hilo)
    default: red(t, S(t, "S2R de SR%d", (int)G(SReg))); return;
    }
    if (t->stage != NX_SH_COMPUTE && G(SReg) >= 32 && G(SReg) <= 39) v = "0u";
    WR(t, G(Dest), v);
}
static void emit_local(T *t, int store) {
    int64_t sz = G(LsSize);
    int nw = sz == 5 ? 2 : sz == 6 || sz == 7 ? 4 : 1;
    if (sz < 4) {
        red(t, "LDL/STL de menos de 32 bits");
        return;
    }
    t->uses_local = 1;
    int64_t imm = (int32_t)((uint32_t)G(Imm24) << 8) >> 8;
    L(t, "{ uint a = (%s + %du) >> 2;", R(t, G(SrcA)), (int)imm);
    for (int k = 0; k < nw; k++) {
        if (store) L(t, "if (a + %du < LMEM) lmem[a + %du] = %s;", k, k, R(t, G(Dest) + k));
        else if (G(Dest) + k < 255) L(t, "uint v%d = a + %du < LMEM ? lmem[a + %du] : 0u;", k, k, k);
    }
    if (!store)
        for (int k = 0; k < nw; k++) WR(t, G(Dest) + k, S(t, "v%d", k));
    L(t, "}");
}


// ---------- texturas ----------
//
// Cada textura que se lee con un handle constante (palabra `TidB` del constbuf de texturas) es un
// sampler de GLSL; el host lee el handle en cada draw y enlaza la textura y el sampler que nombra.

static const char *const tex_glsl[] = {"sampler2D", "sampler3D", "samplerCube", "sampler2DArray", "samplerCubeArray",
                                       "sampler2DMS", "samplerBuffer"};
static const char *const tex_glsl_shadow[] = {"sampler2DShadow", "sampler3D", "samplerCubeShadow", "sampler2DArrayShadow",
                                              "samplerCubeArrayShadow", "sampler2DMS", "samplerBuffer"};

static int tex_entera(T *t, int64_t handle) {
    for (int i = 0; i < 8; i++)
        if (t->key->tex_int_kind[i] && t->key->tex_int_handle[i] == handle) return t->key->tex_int_kind[i];
    return 0;
}

static char *tex_sampler(T *t, int64_t handle, int type) {
    NxShaderInfo *in = t->info;
    t->tex_int = (type & NX_TEX_SHADOW) ? 0 : tex_entera(t, handle);
    int slot = t->tex_slot;
    t->tex_slot = 0xFF;
    for (int i = 0; i < in->ntex; i++)
        if (in->tex[i].handle == handle && in->tex[i].type == type && in->tex[i].slot == slot) return S(t, "tex%d", i);
    if (in->ntex == NX_MAX_TEX) {
        red(t, "demasiadas texturas");
        return S(t, "tex0");
    }
    in->tex[in->ntex].handle = (uint16_t)handle;
    in->tex[in->ntex].type = (uint8_t)type;
    in->tex[in->ntex].entera = (uint8_t)t->tex_int;
    in->tex[in->ntex].slot = (uint8_t)slot;
    return S(t, "tex%d", in->ntex++);
}

// Tipo de TexDim: 1D se lee como 2D de una fila (como hace Ryujinx).
static int tex_type_dim(int64_t dim, int *ncoords, int *array) {
    *array = dim == 1 || dim == 3 || dim == 5 || dim == 7;
    switch (dim) {
    case 0: case 1: *ncoords = 1; return *array ? NX_TEX_2D_ARRAY : NX_TEX_2D;
    case 2: case 3: *ncoords = 2; return *array ? NX_TEX_2D_ARRAY : NX_TEX_2D;
    case 4: case 5: *ncoords = 3; return NX_TEX_3D;
    default: *ncoords = 3; return *array ? NX_TEX_CUBE_ARRAY : NX_TEX_CUBE;
    }
}

typedef struct {
    int ra, rb; // próximos registros de A y B
} Srcs;
static char *nextA(T *t, Srcs *s) { return s->ra >= 255 ? (char *)"0u" : R(t, s->ra++); }
static char *nextB(T *t, Srcs *s) { return s->rb >= 255 ? (char *)"0u" : R(t, s->rb++); }

// Escribe los componentes de `v` (vec4) elegidos por `mask` en los registros de destino.
static void tex_write(T *t, const char *v, int mask, const int *dest_of) {
    // Las texturas enteras devuelven ivec4/uvec4: se pasan por los bits de un vec4.
    if (t->tex_int) v = S(t, "uintBitsToFloat(uvec4(%s))", v);
    L(t, "{ vec4 tv = %s;", v);
    int k = 0;
    for (int c = 0; c < 4; c++)
        if (mask >> c & 1) {
            int d = dest_of[k++];
            if (d < 255) WR(t, d, U(t, S(t, "tv.%c", "xyzw"[c])));
        }
    L(t, "}");
}

// Coordenadas float de un acceso: x[, y[, z]] más la capa como último componente si es arreglo.
static char *tex_coords(T *t, int type, char **c, int nc, const char *layer, const char *ref) {
    int n2d = nc == 1 ? 2 : nc; // 1D → 2D con y = 0.5? no: y = 0 en coordenadas normalizadas
    char *xs[6];
    int k = 0;
    for (int i = 0; i < nc; i++) xs[k++] = F(t, c[i]);
    if (nc == 1) xs[k++] = (char *)"0.0";
    if (layer) xs[k++] = S(t, "float(%s)", layer);
    if (ref) xs[k++] = F(t, ref);
    (void)n2d, (void)type;
    if (k == 1) return xs[0];
    char *r = S(t, "vec%d(%s", k, xs[0]);
    for (int i = 1; i < k; i++) r = S(t, "%s, %s", r, xs[i]);
    return S(t, "%s)", r);
}
static char *tex_icoords(T *t, char **c, int nc, const char *layer) {
    char *xs[5];
    int k = 0;
    for (int i = 0; i < nc; i++) xs[k++] = S(t, "int(%s)", c[i]);
    if (nc == 1) xs[k++] = (char *)"0";
    if (layer) xs[k++] = S(t, "int(%s)", layer);
    char *r = S(t, "ivec%d(%s", k, xs[0]);
    for (int i = 1; i < k; i++) r = S(t, "%s, %s", r, xs[i]);
    return S(t, "%s)", r);
}

// Muestreo con coordenadas float. lod: 0 normal, 1 nivel 0, 2 sesgo, 3 nivel dado.
static char *tex_sample(T *t, const char *smp, int type, char **c, int nc, const char *layer, const char *ref,
                        int lod, const char *lodv) {
    char *coords = tex_coords(t, type, c, nc, layer, ref);
    int shadow = ref != NULL;
    char *r;
    if (shadow && (type == NX_TEX_2D_ARRAY || type == NX_TEX_CUBE || type == NX_TEX_CUBE_ARRAY) && (lod == 1 || lod == 3)) {
        // GLSL no tiene textureLod para estos: el nivel 0 se pide con derivadas nulas.
        const char *g = type == NX_TEX_2D_ARRAY ? "vec2(0.0)" : "vec3(0.0)";
        r = S(t, "textureGrad(%s, %s, %s, %s)", smp, coords, g, g);
    } else if (shadow && type == NX_TEX_CUBE_ARRAY) {
        r = S(t, "texture(%s, %s, %s)", smp, S(t, "vec4(%s, %s, %s, float(%s))", F(t, c[0]), F(t, c[1]), F(t, c[2]), layer), F(t, ref));
    } else if (lod == 1) r = S(t, "textureLod(%s, %s, 0.0)", smp, coords);
    else if (lod == 3) r = S(t, "textureLod(%s, %s, %s)", smp, coords, F(t, lodv));
    else if (lod == 2) r = S(t, "texture(%s, %s, %s)", smp, coords, F(t, lodv));
    else r = S(t, "texture(%s, %s)", smp, coords);
    return shadow ? S(t, "vec4(%s)", r) : r;
}

// Textura "bindless": el handle viene en un registro. Se rastrea hasta el constbuf del que se cargó
// y se trata como una textura enlazada en ese constbuf (como hace Ryujinx). Devuelve el handle (en
// palabras) o -1.
static int rastrear(T *t, uint32_t idx, int reg, int prof, int *slot, int *off);
static int64_t bindless(T *t, int reg) {
    int slot, off;
    if (reg >= 255 || !rastrear(t, t->idx, reg, 0, &slot, &off) || (off & 3)) {
        red(t, "textura bindless sin origen conocido");
        return -1;
    }
    t->tex_slot = slot;
    return off / 4;
}

static void emit_tex(T *t) {
    int b = t->op == MXOP_TexB;
    int64_t dim = G(Dim), lodm = b ? G(Lodb) : G(Lod), mask = G(WMask), d = G(Dest);
    if (d == 255) return;
    int nc, array, type = tex_type_dim(dim, &nc, &array);
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    int64_t handle = b ? bindless(t, s.rb++) : G(TidB);
    if (handle < 0) return;
    char *layer = array ? nextA(t, &s) : NULL;
    char *c[3];
    for (int i = 0; i < nc; i++) c[i] = nextA(t, &s);
    int has_lod = lodm > 1;
    char *lodv = has_lod ? nextB(t, &s) : NULL;
    if ((b ? G(Aoffib) : G(Aoffi)) == 1) nextB(t, &s), red(t, "TEX con desplazamiento"); // TODO: offsets
    char *ref = G(Dc) == 1 ? nextB(t, &s) : NULL;
    if (ref) type |= NX_TEX_SHADOW;
    char *smp = tex_sampler(t, handle, type);
    int lm = lodm == 1 ? 1 : lodm == 2 || lodm == 6 ? 2 : lodm == 3 || lodm == 7 ? 3 : 0;
    char *v = tex_sample(t, smp, type & 15, c, nc, layer, ref, lm, lodv);
    int dest_of[4];
    for (int k = 0; k < 4; k++) dest_of[k] = (int)d + k < 255 ? (int)d + k : 255;
    tex_write(t, v, (int)mask, dest_of);
}

static const int texs_mask[2][8] = {{1, 2, 4, 8, 3, 9, 10, 12}, {7, 11, 13, 14, 15, 0, 0, 0}};

static void texs_dests(T *t, int *dest_of) {
    int64_t d = G(Dest), d2 = G(Dest2);
    dest_of[0] = (int)d;
    dest_of[1] = d < 255 ? (int)d + 1 : 255;
    dest_of[2] = (int)d2;
    dest_of[3] = d2 < 255 ? (int)d2 + 1 : 255;
}

static void emit_texs(T *t) {
    if (t->op == MXOP_TexsF16) {
        red(t, "TEXS de media precisión");
        return;
    }
    int64_t tg = G(Target), d2 = G(Dest2);
    if (G(Dest) == 255 && d2 == 255) return;
    int mask = texs_mask[d2 == 255 ? 0 : 1][G(WMask) & 7];
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    char *c[3] = {0}, *layer = NULL, *ref = NULL, *lodv = NULL;
    int type = NX_TEX_2D, nc = 2, lm = 0;
    switch (tg) {
    case 0: c[0] = nextA(t, &s), nc = 1, lm = 1; break;                                        // 1D, nivel 0
    case 1: c[0] = nextA(t, &s), c[1] = nextB(t, &s); break;                                    // 2D
    case 2: c[0] = nextA(t, &s), c[1] = nextB(t, &s), lm = 1; break;                            // 2D, nivel 0
    case 3: c[0] = nextA(t, &s), c[1] = nextA(t, &s), lodv = nextB(t, &s), lm = 3; break;       // 2D, nivel dado
    case 4: c[0] = nextA(t, &s), c[1] = nextA(t, &s), ref = nextB(t, &s); break;                // 2D, comparación
    case 5: c[0] = nextA(t, &s), c[1] = nextA(t, &s), lodv = nextB(t, &s), ref = nextB(t, &s), lm = 3; break;
    case 6: c[0] = nextA(t, &s), c[1] = nextA(t, &s), ref = nextB(t, &s), lm = 1; break;
    case 7: case 8: case 9: // arreglo 2D: capa, x en A; y (y referencia) en B
        layer = nextA(t, &s), c[0] = nextA(t, &s), c[1] = nextB(t, &s), type = NX_TEX_2D_ARRAY;
        if (tg == 9) ref = nextB(t, &s);
        if (tg != 7) lm = 1;
        break;
    case 10: case 11: c[0] = nextA(t, &s), c[1] = nextA(t, &s), c[2] = nextB(t, &s), nc = 3, type = NX_TEX_3D, lm = tg == 11; break;
    case 12: c[0] = nextA(t, &s), c[1] = nextA(t, &s), c[2] = nextB(t, &s), nc = 3, type = NX_TEX_CUBE; break;
    case 13: c[0] = nextA(t, &s), c[1] = nextA(t, &s), c[2] = nextB(t, &s), nc = 3, type = NX_TEX_CUBE, lodv = nextB(t, &s), lm = 3; break;
    default: red(t, "TEXS con destino desconocido"); return;
    }
    if (ref) type |= NX_TEX_SHADOW;
    char *smp = tex_sampler(t, G(TidB), type);
    char *v = tex_sample(t, smp, type & 15, c, nc, layer, ref, lm, lodv);
    int dest_of[4], seq[4], k = 0;
    texs_dests(t, dest_of);
    for (int i = 0; i < 4; i++) seq[i] = 255;
    for (int i = 0; i < 4 && k < 4; i++)
        if (mask >> i & 1) seq[k] = dest_of[k], k++;
    tex_write(t, v, mask, seq);
}

static void emit_tlds(T *t) {
    if (t->op == MXOP_TldsF16) {
        red(t, "TLDS de media precisión");
        return;
    }
    int64_t tg = G(Target), d2 = G(Dest2);
    if (G(Dest) == 255 && d2 == 255) return;
    int mask = texs_mask[d2 == 255 ? 0 : 1][G(WMask) & 7];
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    char *c[3] = {0}, *layer = NULL, *lod = (char *)"0u", *off = NULL;
    int type = NX_TEX_2D, nc = 2;
    switch (tg) {
    case 0: c[0] = nextA(t, &s), nc = 1; break;
    case 1: c[0] = nextA(t, &s), nc = 1, lod = nextB(t, &s); break;
    case 2: c[0] = nextA(t, &s), c[1] = nextB(t, &s); break;
    case 4: c[0] = nextA(t, &s), c[1] = nextA(t, &s), off = nextB(t, &s); break;
    case 5: c[0] = nextA(t, &s), c[1] = nextA(t, &s), lod = nextB(t, &s); break;
    case 6: c[0] = nextA(t, &s), c[1] = nextA(t, &s), lod = nextB(t, &s), type = NX_TEX_2D_MS; break;
    case 7: c[0] = nextA(t, &s), c[1] = nextA(t, &s), c[2] = nextB(t, &s), nc = 3, type = NX_TEX_3D; break;
    case 8: c[0] = nextB(t, &s), c[1] = nextB(t, &s), layer = nextA(t, &s), type = NX_TEX_2D_ARRAY; break;
    case 12: c[0] = nextA(t, &s), c[1] = nextA(t, &s), lod = nextB(t, &s), off = nextB(t, &s); break;
    default: red(t, "TLDS con destino desconocido"); return;
    }
    char *smp = tex_sampler(t, G(TidB), type);
    char *ic = tex_icoords(t, c, nc, layer);
    if (off) ic = S(t, "(%s + ivec2(bitfieldExtract(int(%s), 0, 4), bitfieldExtract(int(%s), 4, 4)))", ic, off, off);
    char *v = S(t, "texelFetch(%s, %s, int(%s))", smp, ic, lod);
    int dest_of[4], seq[4], k = 0;
    texs_dests(t, dest_of);
    for (int i = 0; i < 4; i++) seq[i] = 255;
    for (int i = 0; i < 4 && k < 4; i++)
        if (mask >> i & 1) seq[k] = dest_of[k], k++;
    tex_write(t, v, mask, seq);
}

static void emit_tld(T *t) {
    int64_t d = G(Dest);
    if (d == 255) return;
    int nc, array, type = tex_type_dim(G(Dim), &nc, &array);
    if (G(Ms) == 1) type = NX_TEX_2D_MS;
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    int64_t handle = t->op == MXOP_TldB ? bindless(t, s.rb++) : G(TidB);
    if (handle < 0) return;
    int slot = t->tex_slot;
    char *layer = array ? nextA(t, &s) : NULL;
    char *c[3];
    for (int i = 0; i < nc; i++) c[i] = nextA(t, &s);
    char *lod = G(Lod) == 1 ? nextB(t, &s) : (char *)"0u";
    char *off = G(Toff) == 1 ? nextB(t, &s) : NULL;
    char *ms = type == NX_TEX_2D_MS ? nextB(t, &s) : NULL;
    t->tex_slot = slot;
    char *smp = tex_sampler(t, handle, type);
    char *ic = tex_icoords(t, c, nc, layer);
    if (off && nc >= 2) ic = S(t, "(%s + ivec%d(bitfieldExtract(int(%s), 0, 4), bitfieldExtract(int(%s), 4, 4)%s))", ic,
                               layer || nc == 3 ? 3 : 2, off, off, layer || nc == 3 ? ", 0" : "");
    char *v = S(t, "texelFetch(%s, %s, int(%s))", smp, ic, ms ? ms : lod);
    int dest_of[4];
    for (int k = 0; k < 4; k++) dest_of[k] = (int)d + k < 255 ? (int)d + k : 255;
    tex_write(t, v, (int)G(WMask), dest_of);
}

static void emit_tld4(T *t, int scalar) {
    int64_t d = G(Dest);
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    char *c[3], *layer = NULL, *ref = NULL;
    int type = NX_TEX_2D, nc = 2, mask = 15;
    if (scalar) {
        mask = texs_mask[G(Dest2) == 255 ? 0 : 1][4];
        if (t->op == MXOP_Tld4sF16) {
            red(t, "TLD4S de media precisión");
            return;
        }
        int dc = G(Dc) == 1, off = G(Aoffi) == 1;
        c[0] = nextA(t, &s);
        c[1] = dc || off ? nextA(t, &s) : nextB(t, &s);
        if (off) nextB(t, &s), red(t, "TLD4S con desplazamiento");
        if (dc) ref = nextB(t, &s);
    } else {
        if (t->op == MXOP_Tld4B) {
            int64_t h = bindless(t, s.rb++);
            if (h < 0) return;
            t->tld4_handle = h;
        }
        int array;
        type = tex_type_dim(G(Dim), &nc, &array);
        if (array) layer = nextA(t, &s);
        for (int i = 0; i < nc; i++) c[i] = nextA(t, &s);
        if (G(Toff) != 0) nextB(t, &s), red(t, "TLD4 con desplazamiento");
        if (G(Dc) == 1) ref = nextB(t, &s);
        mask = (int)G(WMask);
    }
    if (ref) type |= NX_TEX_SHADOW;
    char *smp = tex_sampler(t, t->op == MXOP_Tld4B ? t->tld4_handle : G(TidB), type);
    char *coords = tex_coords(t, type & 15, c, nc, layer, NULL);
    char *v = ref ? S(t, "textureGather(%s, %s, %s)", smp, coords, F(t, ref))
                  : S(t, "textureGather(%s, %s, %d)", smp, coords, (int)(G(TexComp) & 3));
    int dest_of[4];
    if (scalar) {
        int dd[4], seq[4];
        texs_dests(t, dd);
        for (int k = 0; k < 4; k++) seq[k] = dd[k];
        tex_write(t, v, mask, seq);
        return;
    }
    if (d == 255) return;
    for (int k = 0; k < 4; k++) dest_of[k] = (int)d + k < 255 ? (int)d + k : 255;
    tex_write(t, v, mask, dest_of);
}

static void emit_txq(T *t) {
    int64_t d = G(Dest), q = G(TexQuery), mask = G(WMask), handle = G(TidB);
    int ra = (int)G(SrcA);
    if (t->op == MXOP_TxqB) { // el handle va primero en A
        if ((handle = bindless(t, ra++)) < 0) return;
    }
    if (d == 255) return;
    if (q != 1) {
        red(t, "TXQ de otra consulta");
        return;
    }
    // TXQ no dice el tipo: si el shader ya leyó esa textura se usa ese tipo, si no 2D.
    int type = NX_TEX_2D, slot = t->tex_slot;
    for (int i = 0; i < t->info->ntex; i++)
        if (t->info->tex[i].handle == handle && t->info->tex[i].slot == slot) type = t->info->tex[i].type;
    char *smp = tex_sampler(t, handle, type);
    char *lod = R(t, ra);
    const char *sz;
    switch (type & 15) {
    case NX_TEX_3D: case NX_TEX_2D_ARRAY: case NX_TEX_CUBE_ARRAY: sz = S(t, "ivec4(textureSize(%s, int(%s)), 0)", smp, lod); break;
    case NX_TEX_2D_MS: sz = S(t, "ivec4(textureSize(%s), 1, 0)", smp); break;
    default: sz = S(t, "ivec4(textureSize(%s, int(%s)), 1, 0)", smp, lod);
    }
    L(t, "{ ivec4 ts = %s; ts.w = %s;", sz, (type & 15) == NX_TEX_2D_MS ? "1" : S(t, "textureQueryLevels(%s)", smp));
    int k = 0;
    for (int c = 0; c < 4; c++)
        if (mask >> c & 1) {
            if (d + k < 255) WR(t, d + k, S(t, "uint(ts.%c)", "xyzw"[c]));
            k++;
        }
    L(t, "}");
}


// ---------- memoria global, compartida y atómicos ----------
//
// La GPU accede a la memoria global con direcciones de 64 bits. Como hace Ryujinx, se busca de dónde
// sale la base de la dirección: casi siempre es un descriptor de storage buffer que el driver dejó
// en un constbuf (c[slot][offset]). Cada base distinta es un SSBO; el índice se calcula restando esa
// base, leída del mismo constbuf en el shader.

static int no_escribe(int op) {
    switch (op) {
    case MXOP_Stg: case MXOP_Sts: case MXOP_Stl: case MXOP_St: case MXOP_Red: case MXOP_Ast: case MXOP_Exit: case MXOP_Bra:
    case MXOP_Sync: case MXOP_Brk: case MXOP_Kil: case MXOP_Ssy: case MXOP_Pbk: case MXOP_Nop: return 1;
    }
    return 0;
}

static int rastrear(T *t, uint32_t idx, int reg, int prof, int *slot, int *off) {
    if (reg >= 255 || prof > 6) return 0;
    uint32_t lim = idx > 400 ? idx - 400 : 1;
    for (uint32_t i = idx; i-- > lim;) {
        if (i % 4 == 0 || !t->visited[i]) continue;
        uint64_t o;
        int op = decode(t, i, &o);
        if (no_escribe(op)) continue;
        int64_t d = mx_get(op, MXF_Dest, o);
        if (d == MX_NO) continue;
        int64_t sz = mx_get(op, MXF_LsSize, o);
        int ancho = op == MXOP_Ldc && sz == 5 ? 2 : 1;
        if (!(reg >= d && reg < d + ancho)) continue;
        switch (op) {
        case MXOP_MovC:
            *slot = (int)mx_get(op, MXF_CbufSlot, o), *off = (int)mx_get(op, MXF_CbufOffset, o) * 4;
            return 1;
        case MXOP_Ldc:
            if (mx_get(op, MXF_SrcA, o) != 255) return 0;
            *slot = (int)mx_get(op, MXF_CbufSlot, o), *off = (int)(int16_t)mx_get(op, MXF_CbufOffset, o) + (reg - (int)d) * 4;
            return 1;
        case MXOP_IaddC: case MXOP_Iadd3C: case MXOP_IscaddC: case MXOP_LopC:
            *slot = (int)mx_get(op, MXF_CbufSlot, o), *off = (int)mx_get(op, MXF_CbufOffset, o) * 4;
            return 1;
        case MXOP_MovR:
            return rastrear(t, i, (int)mx_get(op, MXF_SrcA, o), prof + 1, slot, off);
        case MXOP_IaddR: case MXOP_Iadd3R: case MXOP_IscaddR: case MXOP_IaddI: case MXOP_Iadd32i: case MXOP_Iadd3I:
        case MXOP_IscaddI: case MXOP_Iscadd32i: case MXOP_XmadR: case MXOP_XmadI: case MXOP_LopR: case MXOP_Lop3R:
            if (rastrear(t, i, (int)mx_get(op, MXF_SrcA, o), prof + 1, slot, off)) return 1;
            if (mx_get(op, MXF_SrcB, o) != MX_NO && rastrear(t, i, (int)mx_get(op, MXF_SrcB, o), prof + 1, slot, off)) return 1;
            if (mx_get(op, MXF_SrcC, o) != MX_NO && rastrear(t, i, (int)mx_get(op, MXF_SrcC, o), prof + 1, slot, off)) return 1;
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

// Prepara `a` (dirección), `i` (índice de palabra) y devuelve el nombre del arreglo del SSBO.
static char *global_addr(T *t, int64_t ra, int64_t imm, int e, int write) {
    int slot, off;
    if (!rastrear(t, t->idx, (int)ra, 0, &slot, &off)) {
        red(t, "acceso a memoria global sin base conocida");
        return NULL;
    }
    NxShaderInfo *in = t->info;
    int k;
    for (k = 0; k < in->nssbo; k++)
        if (in->ssbo[k].slot == slot && in->ssbo[k].offset == off) break;
    if (k == in->nssbo) {
        if (k == NX_MAX_SSBO) {
            red(t, "demasiados storage buffers");
            return NULL;
        }
        in->ssbo[k].slot = (uint8_t)slot, in->ssbo[k].offset = (uint16_t)off, in->nssbo++;
    }
    if (write) in->ssbo[k].written = 1;
    t->info->cbufs |= 1u << slot;
    char *lo = R(t, ra), *hi = e ? R(t, ra + 1) : (char *)"0u";
    L(t, "uint64_t a = ((uint64_t(%s) << 32) | uint64_t(%s)) + uint64_t(%d);", hi, lo, (int)imm);
    L(t, "uint64_t b = uint64_t(cb%d[%d].%c) | (uint64_t(cb%d[%d].%c) << 32);", slot, off >> 4, "xyzw"[(off >> 2) & 3], slot,
      (off + 4) >> 4, "xyzw"[((off + 4) >> 2) & 3]);
    L(t, "uint i = uint((a - b) >> 2);");
    return S(t, "sb%d", k);
}

static void emit_ldst_global(T *t, int store) {
    int64_t sz = G(LsSize), d = G(Dest);
    int64_t imm = (int32_t)((uint32_t)G(Imm24) << 8) >> 8;
    int nw = sz == 5 ? 2 : sz >= 6 ? 4 : 1;
    L(t, "{");
    char *sb = global_addr(t, G(SrcA), imm, G(E) == 1, store);
    if (!sb) {
        L(t, "}");
        return;
    }
    if (sz < 4) { // 8 o 16 bits
        int bits = sz < 2 ? 8 : 16;
        if (store) {
            L(t, "if (i < %s.length()) { uint sh = uint(a & 3u) * 8u; %s[i] = bitfieldInsert(%s[i], %s, int(sh), %d); }", sb, sb, sb,
              R(t, d), bits);
        } else {
            L(t, "uint w = i < %s.length() ? %s[i] : 0u; int sh = int(a & 3u) * 8;", sb, sb);
            WR(t, d, sz & 1 ? S(t, "uint(bitfieldExtract(int(w), sh, %d))", bits) : S(t, "bitfieldExtract(w, sh, %d)", bits));
        }
    } else if (store) {
        for (int k = 0; k < nw; k++) L(t, "if (i + %du < %s.length()) %s[i + %du] = %s;", k, sb, sb, k, R(t, d + k));
    } else {
        for (int k = 0; k < nw; k++) L(t, "uint v%d = i + %du < %s.length() ? %s[i + %du] : 0u;", k, k, sb, sb, k);
        for (int k = 0; k < nw; k++) WR(t, d + k, S(t, "v%d", k));
    }
    L(t, "}");
}

static void emit_ldst_shared(T *t, int store) {
    int64_t sz = G(LsSize), d = G(Dest);
    int64_t imm = (int32_t)((uint32_t)G(Imm24) << 8) >> 8;
    int nw = sz == 5 ? 2 : sz >= 6 ? 4 : 1;
    t->uses_shared = 1;
    L(t, "{ uint a = %s + %du; uint i = a >> 2;", R(t, G(SrcA)), (int)imm);
    if (sz < 4) {
        int bits = sz < 2 ? 8 : 16;
        if (store) L(t, "if (i < SMEM) smem[i] = bitfieldInsert(smem[i], %s, int((a & 3u) * 8u), %d);", R(t, d), bits);
        else {
            L(t, "uint w = i < SMEM ? smem[i] : 0u; int sh = int((a & 3u) * 8u);");
            WR(t, d, sz & 1 ? S(t, "uint(bitfieldExtract(int(w), sh, %d))", bits) : S(t, "bitfieldExtract(w, sh, %d)", bits));
        }
    } else if (store) {
        for (int k = 0; k < nw; k++) L(t, "if (i + %du < SMEM) smem[i + %du] = %s;", k, k, R(t, d + k));
    } else {
        for (int k = 0; k < nw; k++) L(t, "uint v%d = i + %du < SMEM ? smem[i + %du] : 0u;", k, k, k);
        for (int k = 0; k < nw; k++) WR(t, d + k, S(t, "v%d", k));
    }
    L(t, "}");
}

// Operación atómica sobre `ref` (un uint de memoria) con el valor `v`; deja el valor viejo en `old`.
static void atomico(T *t, const char *ref, int op, int sgn, int flt, const char *v) {
    if (flt && op == 0) { // suma de floats: CAS en bucle
        L(t, "uint old = %s; for (;;) { uint nv = floatBitsToUint(uintBitsToFloat(old) + uintBitsToFloat(%s)); uint got = atomicCompSwap(%s, old, nv); if (got == old) break; old = got; }", ref, v, ref);
        return;
    }
    switch (op) {
    case 0: L(t, "uint old = atomicAdd(%s, %s);", ref, v); return;
    case 5: L(t, "uint old = atomicAnd(%s, %s);", ref, v); return;
    case 6: L(t, "uint old = atomicOr(%s, %s);", ref, v); return;
    case 7: L(t, "uint old = atomicXor(%s, %s);", ref, v); return;
    case 8: L(t, "uint old = atomicExchange(%s, %s);", ref, v); return;
    case 10: L(t, "uint old = atomicAdd(%s, %s);", ref, v); return;
    case 1: case 2:
        if (!sgn) {
            L(t, "uint old = atomic%s(%s, %s);", op == 1 ? "Min" : "Max", ref, v);
            return;
        }
        L(t, "uint old = %s; for (;;) { uint nv = uint(%s(int(old), int(%s))); uint got = atomicCompSwap(%s, old, nv); if (got == old) break; old = got; }",
          ref, op == 1 ? "min" : "max", v, ref);
        return;
    case 3: case 4: // INC / DEC con tope
        L(t, "uint old = %s; for (;;) { uint nv = %s; uint got = atomicCompSwap(%s, old, nv); if (got == old) break; old = got; }", ref,
          op == 3 ? S(t, "old >= %s ? 0u : old + 1u", v) : S(t, "(old == 0u || old > %s) ? %s : old - 1u", v, v), ref);
        return;
    }
    red(t, "operación atómica");
    L(t, "uint old = 0u;");
}

static void emit_atom_global(T *t, int red_) {
    int64_t op = red_ ? G(RedOp) : G(Op), size = red_ ? G(RedSize) : G(Size);
    if (size == 2 || size == 5 || size == 4) {
        red(t, "atómico de 64 bits o de media precisión");
        return;
    }
    int64_t imm = (int32_t)((uint32_t)G(Imm20) << 12) >> 12;
    L(t, "{");
    char *sb = global_addr(t, G(SrcA), imm, G(E) == 1, 1);
    if (!sb) {
        L(t, "}");
        return;
    }
    atomico(t, S(t, "%s[min(i, uint(%s.length()) - 1u)]", sb, sb), (int)op, size == 1, size == 3, R(t, G(SrcB)));
    if (!red_) WR(t, G(Dest), "old");
    L(t, "}");
}

static void emit_atom_shared(T *t) {
    t->uses_shared = 1;
    int64_t imm = G(Imm22);
    L(t, "{ uint i = min((%s + %du) >> 2, SMEM - 1u);", R(t, G(SrcA)), (int)(imm == MX_NO ? 0 : imm * 4));
    atomico(t, "smem[i]", (int)G(AtomOp), G(AtomsSize) == 1, 0, R(t, G(SrcB)));
    WR(t, G(Dest), "old");
    L(t, "}");
}

static void emit_atom_cas(T *t, int shared) {
    // CAS: compara con B y escribe B+1 (registros consecutivos).
    int64_t b = G(SrcB);
    L(t, "{");
    char *ref;
    if (shared) {
        t->uses_shared = 1;
        L(t, "uint i = min(%s >> 2, SMEM - 1u);", R(t, G(SrcA)));
        ref = "smem[i]";
    } else {
        char *sb = global_addr(t, G(SrcA), 0, G(E) == 1, 1);
        if (!sb) {
            L(t, "}");
            return;
        }
        ref = S(t, "%s[min(i, uint(%s.length()) - 1u)]", sb, sb);
    }
    L(t, "uint old = atomicCompSwap(%s, %s, %s);", ref, R(t, b), R(t, b + 1));
    WR(t, G(Dest), "old");
    L(t, "}");
}


// ---------- media precisión (dos half en un registro) ----------

// Operando de dos componentes (vec2) según el swizzle: F16 los dos half, F32 el registro como float
// en los dos, H0H0 / H1H1 uno de los half repetido.
static char *half_src(T *t, int64_t sw, const char *u, int64_t neg, int64_t abs_) {
    char *v;
    switch (sw) {
    case 1: v = S(t, "vec2(uintBitsToFloat(%s))", u); break;
    case 2: v = S(t, "unpackHalf2x16(%s).xx", u); break;
    case 3: v = S(t, "unpackHalf2x16(%s).yy", u); break;
    default: v = S(t, "unpackHalf2x16(%s)", u);
    }
    if (abs_ == 1) v = S(t, "abs(%s)", v);
    if (neg == 1) v = S(t, "(-%s)", v);
    return v;
}
static char *half_imm(T *t, uint32_t h0, uint32_t h1) { // inmediatos de 10 bits: los bits altos de cada half
    return S(t, "unpackHalf2x16(%uu)", (h0 << 6 & 0xFFFF) | (h1 << 6 & 0xFFFF) << 16);
}
// Empaqueta el resultado (vec2 en `r`) según OFmt: F16, F32 (solo x), o mezclado con el destino.
static char *half_out(T *t, int64_t ofmt, const char *r, int64_t d) {
    switch (ofmt) {
    case 1: return S(t, "floatBitsToUint(%s.x)", r);
    case 2: return S(t, "packHalf2x16(vec2(%s.x, unpackHalf2x16(%s).y))", r, R(t, d));
    case 3: return S(t, "packHalf2x16(vec2(unpackHalf2x16(%s).x, %s.y))", R(t, d), r);
    default: return S(t, "packHalf2x16(%s)", r);
    }
}

static void emit_half_arith(T *t) {
    int op = t->op, mul = op >= MXOP_Hmul232i && op <= MXOP_Hmul2R, fma = op >= MXOP_Hfma232i && op <= MXOP_Hfma2Rc;
    int64_t d = G(Dest), ofmt = HAS(OFmt) ? G(OFmt) : 0;
    char *a, *b, *c = NULL;
    int v = t->var;
    if (fma) {
        a = half_src(t, G(ASwizzle), R(t, G(SrcA)), 0, 0);
        if (v == V_R) b = half_src(t, G(BSwizzle), R(t, G(SrcB)), G(NegA), 0), c = half_src(t, G(CSwizzle), R(t, G(SrcC)), G(NegC), 0);
        else if (v == V_I) b = half_imm(t, (uint32_t)G(BimmH0), (uint32_t)G(BimmH1)), c = half_src(t, G(CSwizzle), R(t, G(SrcC)), G(NegC), 0);
        else if (v == V_C) b = half_src(t, 1, cbuf(t, G(CbufSlot), G(CbufOffset) * 4), G(NegA), 0), c = half_src(t, G(CSwizzle), R(t, G(SrcC)), G(NegC), 0);
        else if (v == V_RC) b = half_src(t, G(CSwizzle), R(t, G(SrcC)), G(NegA), 0), c = half_src(t, 1, cbuf(t, G(CbufSlot), G(CbufOffset) * 4), G(NegC), 0);
        else b = S(t, "unpackHalf2x16(0x%08xu)", (uint32_t)G(Imm)), c = half_src(t, 0, R(t, d), G(NegC), 0), ofmt = 0;
    } else {
        int sinA = mul && (v == V_R || v == V_C || v == V_32I); // en HMUL2 el NegA va al operando B
        a = half_src(t, G(ASwizzle), R(t, G(SrcA)), sinA ? 0 : G(NegA), v == V_32I ? 0 : G(AbsA));
        if (v == V_R) b = half_src(t, G(BSwizzle), R(t, G(SrcB)), mul ? G(NegA) : G(NegB), G(AbsB));
        else if (v == V_I) b = half_imm(t, (uint32_t)G(BimmH0), (uint32_t)G(BimmH1));
        else if (v == V_C) b = half_src(t, 1, cbuf(t, G(CbufSlot), G(CbufOffset) * 4), mul ? G(NegA) : G(NegB), G(AbsB));
        else b = S(t, "unpackHalf2x16(0x%08xu)", (uint32_t)(HAS(Imm32) ? G(Imm32) : G(Imm))), ofmt = 0;
    }
    char *r = fma ? S(t, "fma(%s, %s, %s)", a, b, c) : S(t, "(%s %s %s)", a, mul ? "*" : "+", b);
    if (G(Sat) == 1) r = S(t, "clamp(%s, 0.0, 1.0)", r);
    L(t, "{ vec2 hr = %s;", r);
    WR(t, d, half_out(t, ofmt, "hr", d));
    L(t, "}");
}

static void emit_half_cmp(T *t, int pred) {
    int v = t->var;
    char *a = half_src(t, G(ASwizzle), R(t, G(SrcA)), G(NegA), G(AbsA)), *b;
    if (v == V_R) b = half_src(t, G(BSwizzle), R(t, G(SrcB)), G(NegB), G(AbsB));
    else if (v == V_I) b = half_imm(t, (uint32_t)G(BimmH0), (uint32_t)G(BimmH1));
    else b = half_src(t, 1, cbuf(t, G(CbufSlot), G(CbufOffset) * 4), G(NegB), pred ? G(AbsB) : 0);
    int64_t cmp = pred ? (HAS(FComp2) ? G(FComp2) : G(FComp)) : G(Cmp);
    char *p = P(t, G(SrcPred), G(SrcPredInv));
    L(t, "{ vec2 ha = %s, hb = %s;", a, b);
    L(t, "bool c0 = %s, c1 = %s;", fcmp(t, cmp, "ha.x", "hb.x"), fcmp(t, cmp, "ha.y", "hb.y"));
    if (pred) {
        if (G(HAnd) == 1) L(t, "c0 = c0 && c1; c1 = !c0;");
        WP(t, G(DestPred), bop(t, G(Bop), "c0", p));
        WP(t, G(DestPredInv), bop(t, G(Bop), "c1", p));
    } else {
        L(t, "c0 = %s; c1 = %s;", bop(t, G(Bop), "c0", p), bop(t, G(Bop), "c1", p));
        if (G(Bval) == 1) WR(t, G(Dest), "packHalf2x16(vec2(c0 ? 1.0 : 0.0, c1 ? 1.0 : 0.0))");
        else WR(t, G(Dest), "((c0 ? 0xffffu : 0u) | (c1 ? 0xffff0000u : 0u))");
    }
    L(t, "}");
}

// ---------- doble precisión (pares de registros) ----------

static char *dreg(T *t, int64_t r) { return S(t, "packDouble2x32(uvec2(%s, %s))", R(t, r), R(t, r < 255 ? r + 1 : 255)); }
static char *dsrc_b(T *t) {
    switch (t->var) {
    case V_R: return dreg(t, G(SrcB));
    case V_I: return S(t, "packDouble2x32(uvec2(0u, 0x%08xu))", (uint32_t)G(Imm20) << 12);
    case V_C: return S(t, "packDouble2x32(uvec2(%s, %s))", cbuf(t, G(CbufSlot), G(CbufOffset) * 4), cbuf(t, G(CbufSlot), G(CbufOffset) * 4 + 4));
    case V_RC: return dreg(t, G(SrcC));
    }
    return (char *)"0.0lf";
}
static char *dabsneg(T *t, const char *v, int64_t abs_, int64_t neg) {
    if (abs_ == 1) v = S(t, "abs(%s)", v);
    if (neg == 1) v = S(t, "(-%s)", v);
    return (char *)v;
}
static void dwr(T *t, int64_t d, const char *v) {
    L(t, "{ uvec2 dv = unpackDouble2x32(%s);", v);
    WR(t, d, "dv.x");
    if (d < 254) WR(t, d + 1, "dv.y");
    L(t, "}");
}

static void emit_double(T *t) {
    int op = t->op;
    char *a = dreg(t, G(SrcA));
    if (op >= MXOP_DaddC && op <= MXOP_DaddR) {
        dwr(t, G(Dest), S(t, "(%s + %s)", dabsneg(t, a, G(AbsA), G(NegA)), dabsneg(t, dsrc_b(t), G(AbsB), G(NegB))));
    } else if (op >= MXOP_DmulC && op <= MXOP_DmulR) {
        dwr(t, G(Dest), S(t, "(%s * %s)", dabsneg(t, a, 0, G(NegA)), dsrc_b(t)));
    } else if (op >= MXOP_DfmaC && op <= MXOP_DfmaRc) {
        char *b = dsrc_b(t), *c = t->var == V_RC ? S(t, "packDouble2x32(uvec2(%s, %s))", cbuf(t, G(CbufSlot), G(CbufOffset) * 4),
                                                    cbuf(t, G(CbufSlot), G(CbufOffset) * 4 + 4))
                                                 : dreg(t, G(SrcC));
        dwr(t, G(Dest), S(t, "fma(%s, %s, %s)", dabsneg(t, a, 0, G(NegA)), b, dabsneg(t, c, 0, G(NegC))));
    } else if (op >= MXOP_DmnmxC && op <= MXOP_DmnmxR) {
        char *x = dabsneg(t, a, G(AbsA), G(NegA)), *y = dabsneg(t, dsrc_b(t), G(AbsB), G(NegB));
        dwr(t, G(Dest), S(t, "(%s ? min(%s, %s) : max(%s, %s))", P(t, G(SrcPred), G(SrcPredInv)), x, y, x, y));
    } else { // DSET / DSETP
        char *x = dabsneg(t, a, G(AbsA), G(NegA)), *y = dabsneg(t, dsrc_b(t), G(AbsB), G(NegB));
        L(t, "{ double da = %s, db = %s; bool c = %s;", x, y, fcmp(t, G(FComp), "da", "db"));
        char *p = P(t, G(SrcPred), G(SrcPredInv));
        if (op >= MXOP_DsetpC && op <= MXOP_DsetpR) {
            WP(t, G(DestPred), bop(t, G(Bop), "c", p));
            WP(t, G(DestPredInv), bop(t, G(Bop), "!c", p));
        } else {
            WR(t, G(Dest), S(t, "(%s ? %s : 0u)", bop(t, G(Bop), "c", p), G(BVal) == 1 ? "0x3f800000u" : "0xffffffffu"));
        }
        L(t, "}");
    }
}

// ---------- hilos del warp: SHFL y FSWZADD (derivadas) ----------
// Cada invocación del GLSL es un hilo; los subgrupos de NVIDIA son warps de 32 hilos con los quads
// en grupos de 4, como en la GPU de la Switch.

// Hilos del quad: Maxwell los numera al revés que las GPU de PC en x y en y (comprobado con las
// derivadas que genera uam contra dFdx/dFdy de OpenGL): su número es el del subgrupo con los dos
// bits bajos invertidos.
#define XSH 3u
#define XSW 3u
static void emit_shfl(T *t) {
    t->uses_subgroup = 1;
    char *b = G(BFixShfl) == 1 ? S(t, "%uu", (uint32_t)G(SrcBImm)) : R(t, G(SrcB));
    char *c = G(CFixShfl) == 1 ? S(t, "%uu", (uint32_t)G(SrcCImm)) : R(t, G(SrcC));
    L(t, "{ uint lane = gl_SubgroupInvocationID ^ %uu, b = %s, c = %s, seg = (c >> 8) & 31u;", XSH, b, c);
    L(t, "uint lo = lane & seg, hi = lo | (c & 31u & ~seg); uint j; bool ok;");
    switch (G(ShflMode)) {
    case 0: L(t, "j = lo | (b & ~seg); ok = j <= hi;"); break;
    case 1: L(t, "j = lane - b; ok = int(j) >= int(hi);"); break;
    case 2: L(t, "j = lane + b; ok = j <= hi;"); break;
    default: L(t, "j = lane ^ b; ok = j <= hi;");
    }
    L(t, "uint v = subgroupShuffle(%s, (ok ? j : lane) ^ %uu);", R(t, G(SrcA)), XSH);
    WR(t, G(Dest), "v");
    WP(t, G(DestPred), "ok");
    L(t, "}");
}

static void emit_fswzadd(T *t) {
    t->uses_subgroup = 1;
    L(t, "{ int m = (%d >> (int((gl_SubgroupInvocationID ^ %uu) & 3u) * 2)) & 3;", (int)G(PnWord), XSW);
    L(t, "const float lx[4] = float[4](1.0, -1.0, 1.0, 0.0), ly[4] = float[4](1.0, 1.0, -1.0, 1.0);");
    WR(t, G(Dest), U(t, S(t, "(%s * lx[m] + %s * ly[m])", F(t, R(t, G(SrcA))), F(t, R(t, G(SrcB))))));
    L(t, "}");
}

// TXD: textura con derivadas explícitas (dPdx, dPdy en los registros B).
static void emit_txd(T *t) {
    int64_t d = G(Dest);
    if (d == 255) return;
    int nc, array, type = tex_type_dim(G(Dim), &nc, &array);
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    int64_t handle = t->op == MXOP_TxdB ? bindless(t, s.ra++) : G(TidB); // el handle va primero en A
    if (handle < 0) return;
    int slot = t->tex_slot;
    char *c[3];
    for (int i = 0; i < nc; i++) c[i] = nextA(t, &s);
    char *packed = nextA(t, &s);
    char *layer = array ? S(t, "(%s & 0xffffu)", packed) : NULL;
    char *g[6];
    for (int i = 0; i < 2 * nc; i++) g[i] = F(t, nextB(t, &s));
    t->tex_slot = slot;
    char *smp = tex_sampler(t, handle, type);
    char *coords = tex_coords(t, type, c, nc, layer, NULL);
    char *dx, *dy;
    // Las derivadas van intercaladas: dPdx.x, dPdy.x, dPdx.y, dPdy.y...
    if (nc == 1) dx = S(t, "vec2(%s, 0.0)", g[0]), dy = S(t, "vec2(%s, 0.0)", g[1]);
    else if (nc == 2) dx = S(t, "vec2(%s, %s)", g[0], g[2]), dy = S(t, "vec2(%s, %s)", g[1], g[3]);
    else dx = S(t, "vec3(%s, %s, %s)", g[0], g[2], g[4]), dy = S(t, "vec3(%s, %s, %s)", g[1], g[3], g[5]);
    int dest_of[4];
    for (int k = 0; k < 4; k++) dest_of[k] = (int)d + k < 255 ? (int)d + k : 255;
    tex_write(t, S(t, "textureGrad(%s, %s, %s, %s)", smp, coords, dx, dy), (int)G(WMask), dest_of);
}

// TMML: nivel de mipmap que se usaría, en punto fijo 8.8 (y al revés que GLSL: x es el calculado).
static void emit_tmml(T *t) {
    int64_t d = G(Dest), mask = G(WMask);
    if (d == 255) return;
    int nc, array, type = tex_type_dim(G(Dim), &nc, &array);
    Srcs s = {(int)G(SrcA), (int)G(SrcB)};
    int64_t handle = t->op == MXOP_TmmlB ? bindless(t, s.rb++) : G(TidB);
    if (handle < 0) return;
    int slot = t->tex_slot;
    char *layer = array ? nextA(t, &s) : NULL;
    (void)layer;
    char *c[3];
    for (int i = 0; i < nc; i++) c[i] = nextA(t, &s);
    t->tex_slot = slot;
    char *smp = tex_sampler(t, handle, type);
    char *coords = tex_coords(t, type, c, nc, NULL, NULL);
    if (array && nc == 2) coords = S(t, "vec2(%s, %s)", F(t, c[0]), F(t, c[1]));
    L(t, "{ vec2 lod = textureQueryLod(%s, %s);", smp, coords);
    int k = 0;
    for (int comp = 0; comp < 4; comp++)
        if (mask >> comp & 1) {
            if (d + k < 255) WR(t, d + k, comp < 2 ? S(t, "uint(int(lod.%c * 256.0))", comp == 0 ? 'y' : 'x') : "0u");
            k++;
        }
    L(t, "}");
}


// ---------- comparaciones y selección con códigos de condición; LEA; video ----------

static void emit_icmp(T *t) { // d = (C cmp 0) ? A : B
    char *a = R(t, G(SrcA)), *b, *c;
    if (t->var == V_RC) b = R(t, G(SrcC)), c = cbuf(t, G(CbufSlot), G(CbufOffset) * 4);
    else b = B(t, 0), c = R(t, G(SrcC));
    WR(t, G(Dest), S(t, "(%s ? %s : %s)", icmp(t, G(IComp), G(Signed), c, "0u"), a, b));
}

static void emit_lea(T *t, int hi) {
    char *a = R(t, G(SrcA)), *b = t->var == V_I ? B(t, 0) : t->var == V_C ? cbuf(t, G(CbufSlot), G(CbufOffset) * 4) : R(t, G(SrcB));
    int sh = (int)G(ImmU5);
    if (!hi) {
        char *x = S(t, "(%s << %du)", a, sh);
        if (G(NegA) == 1) x = S(t, "(0u - %s)", x);
        WR(t, G(Dest), S(t, "(%s + %s)", x, b));
        return;
    }
    char *c = R(t, G(SrcC)); // parte alta de A (A:C es un valor de 64 bits)
    L(t, "{ uint64_t x = ((uint64_t(%s) << 32) | uint64_t(%s)) << %d;", c, a, sh);
    if (G(NegA) == 1) L(t, "x = 0ul - x;");
    WR(t, G(Dest), S(t, "(uint(x >> 32) + %s)", b));
    L(t, "}");
}

static void emit_cset(T *t, int pred) {
    char *c = ccc(t, G(Ccc)), *p = P(t, G(SrcPred), G(SrcPredInv));
    if (pred) {
        L(t, "{ bool c = %s;", c);
        WP(t, G(DestPred), bop(t, G(Bop), "c", p));
        WP(t, G(DestPredInv), bop(t, G(Bop), "!c", p));
        L(t, "}");
    } else {
        WR(t, G(Dest), S(t, "(%s ? %s : 0u)", bop(t, G(Bop), c, p), G(BVal) == 1 ? "0x3f800000u" : "0xffffffffu"));
    }
}

static void emit_pset(T *t) {
    char *a = P(t, G(Src2Pred), G(Src2PredInv)), *b = P(t, G(Src1Pred), G(Src1PredInv)), *c = P(t, G(SrcPred), G(SrcPredInv));
    char *r = bop(t, G(BoolOpC), bop(t, G(BoolOpAB), a, b), c);
    WR(t, G(Dest), S(t, "(%s ? %s : 0u)", r, G(BVal) == 1 ? "0x3f800000u" : "0xffffffffu"));
}

// Selector de vídeo: un byte, una mitad o los 32 bits, con o sin signo.
static char *video_sel(T *t, const char *v, int64_t sel) {
    int sg = sel >= 8, s = (int)(sel & 7);
    if (s == 6) return (char *)v;
    int pos = s < 4 ? s * 8 : (s - 4) * 16, bits = s < 4 ? 8 : 16;
    return sg ? S(t, "uint(bitfieldExtract(int(%s), %d, %d))", v, pos, bits) : S(t, "bitfieldExtract(%s, %d, %d)", v, pos, bits);
}
static char *video_b(T *t) {
    if (G(BVideo) == 1) return video_sel(t, R(t, G(SrcB)), G(BSelect));
    int64_t imm = G(Imm16);
    if (G(BSelect) >= 8) imm = (int16_t)imm;
    return S(t, "%uu", (uint32_t)imm);
}

static void emit_vmad(T *t) {
    int64_t as = G(ASelect), bs = G(BSelect), avg = G(AvgMode);
    int sa = as >= 8, sb = bs >= 8;
    L(t, "{ int64_t a = %s, b = %s;", sa ? S(t, "int64_t(int(%s))", video_sel(t, R(t, G(SrcA)), as)) : S(t, "int64_t(%s)", video_sel(t, R(t, G(SrcA)), as)),
      sb ? S(t, "int64_t(int(%s))", video_b(t)) : S(t, "int64_t(%s)", video_b(t)));
    L(t, "int64_t r = a * b;");
    if (avg == 2) L(t, "r = -r;");
    L(t, "r += %s;", avg == 1 ? S(t, "-int64_t(int(%s))", R(t, G(SrcC))) : S(t, "int64_t(%s)", R(t, G(SrcC))));
    if (avg == 3) L(t, "r += 1;");
    int64_t vs = G(VideoScale);
    if (vs == 1) L(t, "r >>= 7;");
    if (vs == 2) L(t, "r >>= 15;");
    int sg = (as & 7) == 6 && as >= 8 ? 1 : (bs & 7) == 6 && bs >= 8 ? 1 : avg == 1 || avg == 2;
    if (G(Sat) == 1) L(t, sg ? "r = clamp(r, -2147483648l, 2147483647l);" : "r = clamp(r, 0l, 4294967295l);");
    WR(t, G(Dest), "uint(r)");
    L(t, "}");
}

static void emit_vmnmx(T *t) {
    int64_t as = G(ASelect), bs = G(BSelect);
    char *a = video_sel(t, R(t, G(SrcA)), as), *b = video_b(t), *c = R(t, G(SrcC));
    int sa = as >= 8, sb = bs >= 8, mx = G(Mn) == 1, rs;
    L(t, "{ uint a = %s, b = %s, c = %s, r;", a, b, c);
    if (sa != sb) { // un operando con signo y otro sin: el máximo cabe en U32 y el mínimo en S32
        rs = !mx;
        L(t, mx ? "r = max(a, b);" : "r = uint(min(int(a), int(b)));");
        L(t, "if (int(%s) < 0) r = %s;", sa ? "b" : "a", sa ? "b" : "a");
    } else {
        rs = sa;
        if (mx) L(t, rs ? "r = uint(max(int(a), int(b)));" : "r = max(a, b);");
        else L(t, rs ? "r = uint(min(int(a), int(b)));" : "r = min(a, b);");
    }
    int df = G(DFormat) == 1;
    if (G(Sat) == 1) {
        if (df && !rs) L(t, "r = min(r, 0x7fffffffu);");
        else if (!df && rs) L(t, "r = uint(max(int(r), 0));");
    }
    switch (G(VideoOp)) {
    case 4: L(t, "r += c;"); break;
    case 5: L(t, df ? "r = uint(min(int(r), int(c)));" : "r = min(r, c);"); break;
    case 6: L(t, df ? "r = uint(max(int(r), int(c)));" : "r = max(r, c);"); break;
    case 0: L(t, "r = bitfieldInsert(c, r, 16, 16);"); break;
    case 1: L(t, "r = bitfieldInsert(c, r, 0, 16);"); break;
    case 2: L(t, "r = bitfieldInsert(c, r, 0, 8);"); break;
    case 3: L(t, "r = bitfieldInsert(c, r, 16, 8);"); break;
    }
    WR(t, G(Dest), "r");
    L(t, "}");
}

static void emit_vsetp(T *t) {
    int64_t as = G(ASelect), bs = G(BSelect);
    int sa = as >= 8, sb = bs >= 8;
    char *a = video_sel(t, R(t, G(SrcA)), as), *b = video_b(t), *c;
    if (sa != sb && ((as & 7) == 6 || (bs & 7) == 6)) c = (char *)"false"; // como Ryujinx: caso sin resolver
    else c = icmp(t, G(VComp), sa || sb, a, b);
    char *p = P(t, G(SrcPred), G(SrcPredInv));
    L(t, "{ bool c = %s;", c);
    WP(t, G(DestPred), bop(t, G(BoolOp), "c", p));
    WP(t, G(DestPredInv), bop(t, G(BoolOp), "!c", p));
    L(t, "}");
}


// ---------- imágenes (SULD / SUST) ----------
// Las versiones "D" leen o escriben los bits crudos (la imagen se ve con un formato entero del mismo
// tamaño de texel: r32ui, rg32ui...); las "P" usan el formato real, que el host pone en la clave.

static const char *const img_qual[NX_IMG_COUNT] = {"", "r8ui", "r8i", "r16ui", "r16i", "r32ui", "rg32ui", "rgba32ui",
    "rgba8", "rgba8_snorm", "rgba16f", "rgba32f", "r32f", "rg16f", "r11f_g11f_b10f", "rgb10_a2", "rgba8ui",
    "rgba16ui", "r32i", "rg32f", "r16f", "rg8", "r8"};
static int img_entero(int f) { // 0 float, 1 con signo, 2 sin signo
    return f == NX_IMG_R8I || f == NX_IMG_R16I || f == NX_IMG_R32I ? 1
         : (f >= NX_IMG_R8UI && f <= NX_IMG_RGBA32UI) || f == NX_IMG_RGBA8UI || f == NX_IMG_RGBA16UI ? 2 : 0;
}

static int img_tipo(T *t, int64_t dim, int *nc) {
    switch (dim) {
    case 0: *nc = 1; return NX_TEX_2D;          // 1D: como 2D de una fila
    case 2: *nc = 1; return NX_TEX_2D_ARRAY;    // arreglo 1D: x y la capa como y
    case 3: *nc = 2; return NX_TEX_2D;
    case 4: *nc = 2; return NX_TEX_2D_ARRAY;
    case 5: *nc = 3; return NX_TEX_3D;
    }
    red(t, "imagen de buffer");
    *nc = 1;
    return -1;
}

static char *img_nombre(T *t, int64_t handle, int type, int fmt, int write) {
    NxShaderInfo *in = t->info;
    for (int i = 0; i < in->nimg; i++)
        if (in->img[i].handle == handle && in->img[i].type == type && in->img[i].fmt == fmt) {
            in->img[i].written |= write;
            return S(t, "img%d", i);
        }
    if (in->nimg == NX_MAX_IMG) {
        red(t, "demasiadas imágenes");
        return S(t, "img0");
    }
    in->img[in->nimg] = (typeof(in->img[0])){(uint16_t)handle, (uint8_t)type, (uint8_t)fmt, (uint8_t)write};
    return S(t, "img%d", in->nimg++);
}

static int img_fmt_clave(T *t, int64_t handle) {
    for (int i = 0; i < 8; i++)
        if (t->key->img_fmt[i] && t->key->img_handle[i] == handle) return t->key->img_fmt[i];
    return NX_IMG_RGBA8;
}

static char *img_coord(T *t, int type, int nc, Srcs *s, int shift) {
    char *c[3];
    for (int i = 0; i < nc; i++) c[i] = nextA(t, s);
    char *x = shift ? S(t, "(int(%s) >> %d)", c[0], shift) : S(t, "int(%s)", c[0]);
    if (type == NX_TEX_3D) return S(t, "ivec3(%s, int(%s), int(%s))", x, c[1], c[2]);
    if (type == NX_TEX_2D_ARRAY) { // la capa viene después de las coordenadas
        char *l = nextA(t, s);
        return nc == 1 ? S(t, "ivec3(%s, 0, int(%s))", x, l) : S(t, "ivec3(%s, int(%s), int(%s))", x, c[1], l);
    }
    return nc == 1 ? S(t, "ivec2(%s, 0)", x) : S(t, "ivec2(%s, int(%s))", x, c[1]);
}

static void emit_suld_sust(T *t, int store) {
    int op = t->op, crudo = op == MXOP_SuldD || op == MXOP_SustD;
    if (op == MXOP_SuldDB || op == MXOP_SustDB || op == MXOP_SustB || op == MXOP_SuldB) {
        red(t, "imagen sin handle constante");
        return;
    }
    int nc, type = img_tipo(t, G(Dim), &nc);
    if (type < 0) return;
    int64_t d = G(Dest);
    Srcs s = {(int)G(SrcA), 255};
    if (crudo) {
        int64_t size = G(Size);
        static const int fmts[] = {NX_IMG_R8UI, NX_IMG_R8I, NX_IMG_R16UI, NX_IMG_R16I, NX_IMG_R32UI, NX_IMG_RG32UI, NX_IMG_RGBA32UI, NX_IMG_RGBA32UI};
        static const int shifts[] = {0, 0, 1, 1, 2, 3, 4, 4};
        int fmt = fmts[size & 7], nw = size == 5 ? 2 : size >= 6 ? 4 : 1;
        char *img = img_nombre(t, G(TidB), type, fmt, store);
        char *co = img_coord(t, type, nc, &s, G(Ba) == 1 ? shifts[size & 7] : 0);
        if (store) {
            char *v[4];
            for (int k = 0; k < 4; k++) v[k] = k < nw ? R(t, d + k) : (char *)"0u";
            L(t, "imageStore(%s, %s, %s(%s, %s, %s, %s));", img, co, fmt == NX_IMG_R8I || fmt == NX_IMG_R16I ? "ivec4" : "uvec4",
              v[0], v[1], v[2], v[3]);
        } else {
            L(t, "{ uvec4 iv = uvec4(imageLoad(%s, %s));", img, co);
            for (int k = 0; k < nw; k++) WR(t, d + k, S(t, "iv.%c", "xyzw"[k]));
            L(t, "}");
        }
        return;
    }
    // Con formato: las componentes que pide la máscara (RGBA), en el formato real de la imagen.
    int fmt = img_fmt_clave(t, G(TidB)), ent = img_entero(fmt), mask = (int)G(Rgba);
    char *img = img_nombre(t, G(TidB), type, fmt, store);
    char *co = img_coord(t, type, nc, &s, 0);
    if (store) {
        char *v[4];
        int k = 0;
        for (int c = 0; c < 4; c++) v[c] = mask >> c & 1 ? R(t, d + k++) : (char *)"0u";
        if (ent) L(t, "imageStore(%s, %s, %s(%s, %s, %s, %s));", img, co, ent == 1 ? "ivec4" : "uvec4", v[0], v[1], v[2], v[3]);
        else L(t, "imageStore(%s, %s, uintBitsToFloat(uvec4(%s, %s, %s, %s)));", img, co, v[0], v[1], v[2], v[3]);
    } else {
        L(t, "{ uvec4 iv = %s;", ent ? S(t, "uvec4(imageLoad(%s, %s))", img, co) : S(t, "floatBitsToUint(imageLoad(%s, %s))", img, co));
        int k = 0;
        for (int c = 0; c < 4; c++)
            if (mask >> c & 1) WR(t, d + k++, S(t, "iv.%c", "xyzw"[c]));
        L(t, "}");
    }
}

static void emit_insn(T *t) {
    switch (t->op) {
    case MXOP_Ald: emit_ald(t); break;
    case MXOP_Ast: emit_ast(t); break;
    case MXOP_Ipa: emit_ipa(t); break;
    case MXOP_Ldc: emit_ldc(t); break;
    case MXOP_Al2p: WR(t, G(Dest), S(t, "(%s + %du)", R(t, G(SrcA)), (int)G(Imm11))); break;
    case MXOP_Isberd: WR(t, G(Dest), R(t, G(SrcA))); break;
    case MXOP_MovR: case MXOP_MovI: case MXOP_MovC: case MXOP_Mov32i: emit_mov(t); break;
    case MXOP_SelR: case MXOP_SelI: case MXOP_SelC: emit_sel(t); break;
    case MXOP_Mufu: emit_mufu(t); break;
    case MXOP_RroR: case MXOP_RroI: case MXOP_RroC:
        WR(t, G(Dest), U(t, absneg(t, F(t, B(t, 1)), G(AbsB), G(NegB))));
        break;
    case MXOP_FaddR: case MXOP_FaddI: case MXOP_FaddC: case MXOP_Fadd32i: emit_fadd(t); break;
    case MXOP_FmulR: case MXOP_FmulI: case MXOP_FmulC: case MXOP_Fmul32i: emit_fmul(t); break;
    case MXOP_FfmaR: case MXOP_FfmaI: case MXOP_FfmaC: case MXOP_FfmaRc: case MXOP_Ffma32i: emit_ffma(t); break;
    case MXOP_FmnmxR: case MXOP_FmnmxI: case MXOP_FmnmxC: emit_fmnmx(t); break;
    case MXOP_FsetpR: case MXOP_FsetpI: case MXOP_FsetpC: emit_fsetp(t); break;
    case MXOP_FsetR: case MXOP_FsetI: case MXOP_FsetC: emit_fset(t); break;
    case MXOP_IaddR: case MXOP_IaddI: case MXOP_IaddC: case MXOP_Iadd32i: emit_iadd(t); break;
    case MXOP_Iadd3R: case MXOP_Iadd3I: case MXOP_Iadd3C: emit_iadd3(t); break;
    case MXOP_IscaddR: case MXOP_IscaddI: case MXOP_IscaddC: case MXOP_Iscadd32i: emit_iscadd(t); break;
    case MXOP_XmadR: case MXOP_XmadI: case MXOP_XmadC: case MXOP_XmadRc: emit_xmad(t); break;
    case MXOP_ImadR: case MXOP_ImadI: case MXOP_ImadC: case MXOP_ImadRc: case MXOP_Imad32i: emit_imad(t); break;
    case MXOP_ImulR: case MXOP_ImulI: case MXOP_ImulC: case MXOP_Imul32i: emit_imul(t); break;
    case MXOP_ImnmxR: case MXOP_ImnmxI: case MXOP_ImnmxC: emit_imnmx(t); break;
    case MXOP_IsetpR: case MXOP_IsetpI: case MXOP_IsetpC: emit_isetp(t); break;
    case MXOP_IsetR: case MXOP_IsetI: case MXOP_IsetC: emit_iset(t); break;
    case MXOP_LopR: case MXOP_LopI: case MXOP_LopC: case MXOP_Lop32i: emit_lop(t); break;
    case MXOP_Lop3R: case MXOP_Lop3I: case MXOP_Lop3C: emit_lop3(t); break;
    case MXOP_ShlR: case MXOP_ShlI: case MXOP_ShlC: emit_shift(t, 0); break;
    case MXOP_ShrR: case MXOP_ShrI: case MXOP_ShrC: emit_shift(t, 1); break;
    case MXOP_ShfLR: case MXOP_ShfLI: emit_shf(t, 0); break;
    case MXOP_ShfRR: case MXOP_ShfRI: emit_shf(t, 1); break;
    case MXOP_BfeR: case MXOP_BfeI: case MXOP_BfeC: emit_bfe(t); break;
    case MXOP_BfiR: case MXOP_BfiI: case MXOP_BfiC: case MXOP_BfiRc: emit_bfi(t); break;
    case MXOP_PopcR: case MXOP_PopcI: case MXOP_PopcC: emit_popc(t); break;
    case MXOP_FloR: case MXOP_FloI: case MXOP_FloC: emit_flo(t); break;
    case MXOP_PrmtR: case MXOP_PrmtI: case MXOP_PrmtC: case MXOP_PrmtRc: emit_prmt(t); break;
    case MXOP_Psetp: emit_psetp(t); break;
    case MXOP_P2rR: case MXOP_P2rI: case MXOP_P2rC: emit_p2r(t); break;
    case MXOP_R2pR: case MXOP_R2pI: case MXOP_R2pC: emit_r2p(t); break;
    case MXOP_I2fR: case MXOP_I2fI: case MXOP_I2fC: emit_i2f(t); break;
    case MXOP_F2iR: case MXOP_F2iI: case MXOP_F2iC: emit_f2i(t); break;
    case MXOP_F2fR: case MXOP_F2fI: case MXOP_F2fC: emit_f2f(t); break;
    case MXOP_I2iR: case MXOP_I2iI: case MXOP_I2iC: emit_i2i(t); break;
    case MXOP_S2r: emit_s2r(t); break;
    case MXOP_Ldl: emit_local(t, 0); break;
    case MXOP_Ldg: emit_ldst_global(t, 0); break;
    case MXOP_Stg: emit_ldst_global(t, 1); break;
    case MXOP_Lds: emit_ldst_shared(t, 0); break;
    case MXOP_Sts: emit_ldst_shared(t, 1); break;
    case MXOP_Atom: emit_atom_global(t, 0); break;
    case MXOP_Red: emit_atom_global(t, 1); break;
    case MXOP_Atoms: emit_atom_shared(t); break;
    case MXOP_AtomCas: emit_atom_cas(t, 0); break;
    case MXOP_AtomsCas: emit_atom_cas(t, 1); break;
    case MXOP_OutR: case MXOP_OutI: case MXOP_OutC: { // geometría: emitir el vértice y/o cortar la tira
        int64_t ty = G(OutType);
        if (ty & 1) L(t, "EmitVertex();");
        if (ty & 2) L(t, "EndPrimitive();");
        WR(t, G(Dest), "0u");
        break;
    }
    case MXOP_Tex: case MXOP_TexB: emit_tex(t); break;
    case MXOP_Txd: case MXOP_TxdB: emit_txd(t); break;
    case MXOP_Tmml: case MXOP_TmmlB: emit_tmml(t); break;
    case MXOP_Shfl: emit_shfl(t); break;
    case MXOP_SuldD: case MXOP_Suld: case MXOP_SuldDB: emit_suld_sust(t, 0); break;
    case MXOP_SustD: case MXOP_Sust: case MXOP_SustDB: case MXOP_SustB: emit_suld_sust(t, 1); break;
    case MXOP_IcmpR: case MXOP_IcmpI: case MXOP_IcmpC: case MXOP_IcmpRc: emit_icmp(t); break;
    case MXOP_LeaR: case MXOP_LeaI: case MXOP_LeaC: emit_lea(t, 0); break;
    case MXOP_LeaHiR: case MXOP_LeaHiC: emit_lea(t, 1); break;
    case MXOP_Cset: emit_cset(t, 0); break;
    case MXOP_Csetp: emit_cset(t, 1); break;
    case MXOP_Pset: emit_pset(t); break;
    case MXOP_Vmad: emit_vmad(t); break;
    case MXOP_Vmnmx: emit_vmnmx(t); break;
    case MXOP_Vsetp: emit_vsetp(t); break;
    case MXOP_Fswzadd: emit_fswzadd(t); break;
    case MXOP_Hadd232i: case MXOP_Hadd2C: case MXOP_Hadd2I: case MXOP_Hadd2R:
    case MXOP_Hmul232i: case MXOP_Hmul2C: case MXOP_Hmul2I: case MXOP_Hmul2R:
    case MXOP_Hfma232i: case MXOP_Hfma2C: case MXOP_Hfma2I: case MXOP_Hfma2R: case MXOP_Hfma2Rc:
        emit_half_arith(t);
        break;
    case MXOP_Hset2C: case MXOP_Hset2I: case MXOP_Hset2R: emit_half_cmp(t, 0); break;
    case MXOP_Hsetp2C: case MXOP_Hsetp2I: case MXOP_Hsetp2R: emit_half_cmp(t, 1); break;
    case MXOP_DaddC: case MXOP_DaddI: case MXOP_DaddR: case MXOP_DmulC: case MXOP_DmulI: case MXOP_DmulR:
    case MXOP_DfmaC: case MXOP_DfmaI: case MXOP_DfmaR: case MXOP_DfmaRc: case MXOP_DmnmxC: case MXOP_DmnmxI: case MXOP_DmnmxR:
    case MXOP_DsetC: case MXOP_DsetI: case MXOP_DsetR: case MXOP_DsetpC: case MXOP_DsetpI: case MXOP_DsetpR:
        emit_double(t);
        break;
    case MXOP_Texs: case MXOP_TexsF16: emit_texs(t); break;
    case MXOP_Tlds: case MXOP_TldsF16: emit_tlds(t); break;
    case MXOP_Tld: case MXOP_TldB: emit_tld(t); break;
    case MXOP_Tld4: case MXOP_Tld4B: emit_tld4(t, 0); break;
    case MXOP_Tld4s: case MXOP_Tld4sF16: emit_tld4(t, 1); break;
    case MXOP_Txq: case MXOP_TxqB: emit_txq(t); break;
    case MXOP_Stl: emit_local(t, 1); break;
    case MXOP_Vote: { // un solo hilo: todos/alguno/igual valen lo mismo que el predicado
        char *p = P(t, G(SrcPred), G(SrcPredInv));
        WR(t, G(Dest), S(t, "(%s ? 1u : 0u)", p));
        WP(t, G(VpDest), G(VoteMode) == 2 ? "true" : p);
        break;
    }
    case MXOP_Nop: case MXOP_Depbar: case MXOP_Cctl: case MXOP_Cctll: break;
    case MXOP_Membar: L(t, "memoryBarrier();"); break;
    case MXOP_Bar:
        if (t->stage == NX_SH_COMPUTE) L(t, "memoryBarrierShared(); barrier();");
        break;
    case MXOP_Kil:
        t->uses_discard = 1;
        if (unconditional(t)) L(t, "discard;");
        else L(t, "if (%s) discard;", ccc(t, G(Ccc)));
        break;
    case MXOP_Exit: {
        char *c = ccc(t, G(Ccc));
        if (strcmp(c, "true")) L(t, "if (%s) {", c);
        if (t->stage == NX_SH_FRAGMENT) frag_outputs(t);
        L(t, "return;");
        if (strcmp(c, "true")) L(t, "}");
        break;
    }
    case MXOP_Bra:
        if (G(Ca) == 1) break;
        jump(t, ccc(t, G(Ccc)), S(t, "%uu", target(t, t->idx, t->o)));
        break;
    // Pila de reconvergencia: cada entrada es el destino con su tipo en los bits altos. SYNC, BRK y
    // RET desapilan hasta su entrada (incluida); CONT vuelve a la suya y la deja para la siguiente vuelta.
    case MXOP_Ssy: case MXOP_Pbk: case MXOP_Pcnt:
        if (G(Ca) == 1) break;
        L(t, "if (sp < 16) stk[sp++] = %uu | %uu;", target(t, t->idx, t->o),
          (t->op == MXOP_Ssy ? 1u : t->op == MXOP_Pbk ? 2u : 3u) << 28);
        break;
    case MXOP_Pret: // apila un retorno (para RET) sin saltar
        if (G(Ca) == 1) break;
        L(t, "if (sp < 16) stk[sp++] = %uu | %uu;", target(t, t->idx, t->o), 4u << 28);
        break;
    case MXOP_Cal:
        L(t, "if (sp < 16) stk[sp++] = %uu | %uu;", next_idx(t->idx), 4u << 28);
        jump(t, "true", S(t, "%uu", target(t, t->idx, t->o)));
        break;
    case MXOP_Sync: case MXOP_Brk: case MXOP_Cont: case MXOP_Ret: {
        char *c = ccc(t, G(Ccc));
        unsigned ty = t->op == MXOP_Sync ? 1 : t->op == MXOP_Brk ? 2 : t->op == MXOP_Cont ? 3 : 4;
        if (strcmp(c, "true")) L(t, "if (%s) {", c);
        L(t, "while (sp > 0 && (stk[sp - 1] >> 28) != %uu) sp--;", ty);
        L(t, "if (sp == 0) return;");
        L(t, "pc = stk[sp - 1] & 0xfffffffu;%s continue;", ty == 3 ? "" : " sp--;");
        if (strcmp(c, "true")) L(t, "}");
        break;
    }
    default: red(t, "instrucción sin traducir");
    }
}

// ---------- programa ----------

static void header(T *t, Sb *h) {
    t->arena_n = 0;
    sb_add(h, "#version 460\n#extension GL_ARB_gpu_shader_int64 : enable\n");
    if (t->uses_subgroup) sb_add(h, "#extension GL_KHR_shader_subgroup_basic : enable\n#extension GL_KHR_shader_subgroup_shuffle : enable\n");
    if (t->stage == NX_SH_VERTEX || por_vertice(t->stage)) {
        int st = t->stage;
        if (st == NX_SH_TESS_CTRL) sb_add(h, "layout(vertices = %u) out;\n", t->sph[2] >> 24 ? t->sph[2] >> 24 : 3);
        if (st == NX_SH_TESS_EVAL) {
            uint32_t m = t->key->tess_mode;
            static const char *prim[] = {"isolines", "triangles", "quads", "triangles"};
            static const char *esp[] = {"equal_spacing", "fractional_odd_spacing", "fractional_even_spacing", "equal_spacing"};
            sb_add(h, "layout(%s, %s, %s) in;\n", prim[m & 3], esp[m >> 4 & 3], (m >> 8 & 3) == 1 && (m & 3) != 0 ? "cw" : "ccw");
        }
        if (st == NX_SH_GEOMETRY) {
            uint32_t topo = t->sph[3] >> 24 & 15, maxv = t->sph[4] & 0xFFF;
            sb_add(h, "layout(%s) in;\nlayout(%s, max_vertices = %u) out;\n", gs_in_name(t->key->gs_input),
                   topo == 1 ? "points" : topo == 6 ? "line_strip" : "triangle_strip", maxv ? maxv : 1);
        }
        const char *arr = por_vertice(st) ? "[]" : "";
        if (por_vertice(st)) sb_add(h, "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[];\n");
        for (int i = 0; i < 32; i++)
            if (t->info->inputs >> i & 1)
                sb_add(h, "layout(location = %d) in %s in_attr%d%s;\n", i,
                       st != NX_SH_VERTEX ? "vec4" : t->key->attr_int[i] == 1 ? "ivec4" : t->key->attr_int[i] == 2 ? "uvec4" : "vec4", i, arr);
        int nclip = 32 - __builtin_clz(t->clip_mask | 1);
        const char *oarr = st == NX_SH_TESS_CTRL ? "[]" : "";
        sb_add(h, "out gl_PerVertex { vec4 gl_Position; float gl_PointSize;%s } %s;\n",
               t->clip_mask ? S(t, " float gl_ClipDistance[%d];", nclip) : "", st == NX_SH_TESS_CTRL ? "gl_out[]" : "");
        for (int i = 0; i < 32; i++)
            if (t->info->outputs >> i & 1) sb_add(h, "layout(location = %d) out vec4 out_attr%d%s;\n", i, i, oarr);
    } else if (t->stage == NX_SH_FRAGMENT) {
        for (int i = 0; i < 32; i++)
            if (t->info->inputs >> i & 1)
                sb_add(h, "layout(location = %d) %sin vec4 in_attr%d;\n", i,
                       t->in_flat >> i & 1 ? "flat " : t->in_linear >> i & 1 ? "noperspective " : "", i);
        uint32_t omap = t->sph[18];
        t->info->omap = omap;
        for (int rt = 0; rt < 8; rt++)
            if (omap >> (rt * 4) & 15)
                sb_add(h, "layout(location = %d) out %s frag%d;\n", rt, t->key->rt_int[rt] == 1 ? "ivec4" : t->key->rt_int[rt] == 2 ? "uvec4" : "vec4", rt);
    }
    if (t->stage == NX_SH_COMPUTE)
        sb_add(h, "layout(local_size_x = %d, local_size_y = %d, local_size_z = %d) in;\n", t->key->local[0] ? t->key->local[0] : 1,
               t->key->local[1] ? t->key->local[1] : 1, t->key->local[2] ? t->key->local[2] : 1);
    if (t->uses_shared) {
        uint32_t n = (t->key->shared_bytes + 3) / 4;
        sb_add(h, "const uint SMEM = %uu;\nshared uint smem[SMEM];\n", n ? n : 1);
    }
    static const char *const img_glsl[] = {"image2D", "image3D", "imageCube", "image2DArray"};
    for (int i = 0; i < t->info->nimg; i++) {
        int f = t->info->img[i].fmt, ent = img_entero(f);
        sb_add(h, "layout(binding = %d, %s) uniform coherent %s%s img%d;\n", t->key->img_binding + i, img_qual[f],
               ent == 1 ? "i" : ent == 2 ? "u" : "", img_glsl[t->info->img[i].type & 3], i);
    }
    for (int k = 0; k < t->info->nssbo; k++)
        sb_add(h, "layout(std430, binding = %d) buffer Sb%d { uint sb%d[]; };\n", t->key->sb_binding + k, k, k);
    for (int s = 0; s < 18; s++)
        if (t->info->cbufs >> s & 1)
            sb_add(h, "layout(std140, binding = %d) uniform Cb%d { uvec4 cb%d[4096]; };\n", t->key->cb_binding + s, s, s);
    for (int i = 0; i < t->info->ntex; i++) {
        int ty = t->info->tex[i].type;
        sb_add(h, "layout(binding = %d) uniform %s%s tex%d;\n", t->key->tex_binding + i,
               t->info->tex[i].entera == 1 ? "i" : t->info->tex[i].entera == 2 ? "u" : "",
               ty & NX_TEX_SHADOW ? tex_glsl_shadow[ty & 15] : tex_glsl[ty & 15], i);
    }
    if (t->info->uses_instance) sb_add(h, "layout(location = 0) uniform uint nx_inst0;\n");
    if (t->uses_local) sb_add(h, "const uint LMEM = %du;\nuint lmem[LMEM];\n", t->local_words);
    sb_add(h, "void main() {\n");
    for (int r = 0; r < 255; r++)
        if (t->reg_used[r]) sb_add(h, "    uint r%d = 0u;\n", r);
    sb_add(h, "    bool p0 = false, p1 = false, p2 = false, p3 = false, p4 = false, p5 = false, p6 = false;\n");
    sb_add(h, "    bool cc_z = false, cc_n = false, cc_c = false, cc_v = false;\n");
    if (t->stage == NX_SH_FRAGMENT)
        for (int rt = 0; rt < 8; rt++)
            if (t->sph[18] >> (rt * 4) & 15)
                sb_add(h, "    frag%d = %s(0);\n", rt, t->key->rt_int[rt] == 1 ? "ivec4" : t->key->rt_int[rt] == 2 ? "uvec4" : "vec4");
    if (t->stage == NX_SH_VERTEX || t->stage == NX_SH_TESS_EVAL)
        for (int i = 0; i < 32; i++)
            if (t->info->outputs >> i & 1) sb_add(h, "    out_attr%d = vec4(0.0, 0.0, 0.0, 1.0);\n", i);
}

char *nx_shader_glsl(const uint8_t *code, size_t avail, int stage, const NxShaderKey *key, NxShaderInfo *info) {
    if (avail < 0x60) return NULL;
    T *t = calloc(1, sizeof *t);
    memset(info, 0, sizeof *info);
    t->info = info;
    t->key = key;
    t->stage = stage;
    size_t hdr = stage == NX_SH_COMPUTE ? 0 : 0x50; // los de cómputo no tienen cabecera (la QMD hace de cabecera)
    if (hdr) memcpy(t->sph, code, sizeof t->sph);
    t->code = code + hdr;
    t->ninsn = (uint32_t)((avail - hdr) / 8);
    if (t->ninsn > 0x40000) t->ninsn = 0x40000;
    t->visited = calloc(t->ninsn + 2, 1);
    t->leader = calloc(t->ninsn + 2, 1);
    t->tex_slot = 0xFF;
    t->local_words = (int)(((t->sph[1] & 0xFFFFFF) + (t->sph[2] & 0xFFFFFF)) / 4);
    if (stage == NX_SH_COMPUTE) t->local_words = (int)(key->local_bytes / 4);
    if (t->local_words < 1) t->local_words = 1;
    if (t->local_words > 4096) t->local_words = 4096;
    discover(t);
    for (uint32_t i = t->ninsn; i-- > 0;)
        if (t->visited[i]) {
            info->code_size = (uint32_t)hdr + (i + 1) * 8;
            break;
        }

    if (t->uses_flow) {
        L(t, "uint pc = 1u; uint stk[16]; int sp = 0;");
        L(t, "while (true) { switch (pc) {");
    }
    for (uint32_t i = 0; i < t->ninsn; i++) {
        if (!t->visited[i]) continue;
        if (t->uses_flow && t->leader[i]) sb_add(&t->body, "case %uu:\n", i);
        t->idx = i;
        t->arena_n = 0;
        t->op = decode(t, i, &t->o);
        t->var = variant(t->op);
        int64_t pred = G(Pred), inv = G(PredInv);
        if (pred == 7 && inv == 1) continue; // nunca se ejecuta
        int guarded = pred != MX_NO && pred != 7;
        if (guarded) L(t, "if (%s) {", P(t, pred, inv));
        emit_insn(t);
        if (guarded) L(t, "}");
    }
    if (t->uses_flow) {
        L(t, "return;");
        L(t, "} }");
    }
    sb_add(&t->body, "}\n");

    Sb h = {0};
    header(t, &h);
    sb_add(&h, "%s", t->body.p ? t->body.p : "}\n");
    free(t->body.p);
    free(t->visited);
    free(t->leader);
    free(t);
    return h.p;
}
