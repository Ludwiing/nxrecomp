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
    int uses_flow, uses_stack, uses_local, local_words, uses_discard;
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

static char *attr_load(T *t, int a, int out) {
    if (t->stage == NX_SH_FRAGMENT) {
        if (a >= 0x70 && a < 0x80) return U(t, S(t, "gl_FragCoord.%c", "xyzw"[(a >> 2) & 3]));
        if (a == 0x3FC) return (char *)"(gl_FrontFacing ? 0xffffffffu : 0u)";
        if (a == 0x60) return (char *)"uint(gl_PrimitiveID)";
        if (a == 0x64) return (char *)"uint(gl_Layer)";
        if (a == 0x2E0 || a == 0x2E4) return U(t, S(t, "gl_PointCoord.%c", "xy"[(a >> 2) & 1]));
    } else {
        if (a == 0x2FC) return (char *)"uint(gl_VertexID)";
        if (a == 0x2F8) return (char *)"uint(gl_InstanceID)";
    }
    if (a >= 0x80 && a < 0x280 && !out) {
        int i = (a - 0x80) >> 4, c = (a >> 2) & 3;
        t->info->inputs |= 1u << i;
        if (t->stage == NX_SH_VERTEX && t->key->attr_int[i]) return S(t, "uint(in_attr%d.%c)", i, "xyzw"[c]);
        return U(t, S(t, "in_attr%d.%c", i, "xyzw"[c]));
    }
    if (a >= 0x80 && a < 0x280 && out) {
        int i = (a - 0x80) >> 4, c = (a >> 2) & 3;
        t->info->outputs |= 1u << i;
        t->out_comps[i] |= 1u << c;
        return U(t, S(t, "out_attr%d.%c", i, "xyzw"[c]));
    }
    if (a >= 0x70 && a < 0x80 && out) return U(t, S(t, "gl_Position.%c", "xyzw"[(a >> 2) & 3]));
    red(t, S(t, "atributo 0x%x sin leer", a));
    return (char *)"0u";
}
static void attr_store(T *t, int a, const char *v) {
    if (a >= 0x70 && a < 0x80) {
        L(t, "gl_Position.%c = uintBitsToFloat(%s);", "xyzw"[(a >> 2) & 3], v);
    } else if (a >= 0x80 && a < 0x280) {
        int i = (a - 0x80) >> 4, c = (a >> 2) & 3;
        t->info->outputs |= 1u << i;
        t->out_comps[i] |= 1u << c;
        L(t, "out_attr%d.%c = uintBitsToFloat(%s);", i, "xyzw"[c], v);
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
            if (omap >> (rt * 4 + c) & 1) L(t, "frag%d.%c = uintBitsToFloat(%s);", rt, "xyzw"[c], R(t, base + c));
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
            case MXOP_Bra: case MXOP_Ssy: case MXOP_Pbk: case MXOP_Pcnt: case MXOP_Cal: {
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
    if (G(P) == 1 || (G(Imm11) == 0 && G(SrcA) != 255)) {
        red(t, "ALD indexado");
        return;
    }
    int n = (int)G(AlSize) + 1, d = (int)G(Dest), a = (int)G(Imm11);
    for (int k = 0; k < n && d + k < 255; k++) WR(t, d + k, attr_load(t, a + k * 4, G(O) == 1));
}
static void emit_ast(T *t) {
    if (G(P) == 1 || (G(Imm11) == 0 && G(SrcA) != 255)) {
        red(t, "AST indexado");
        return;
    }
    int n = (int)G(AlSize) + 1, s = (int)G(SrcB), a = (int)G(Imm11);
    for (int k = 0; k < n && s + k < 255; k++) attr_store(t, a + k * 4, R(t, s + k));
}
static void emit_ipa(T *t) {
    if (G(Idx) == 1) {
        red(t, "IPA indexado");
        return;
    }
    int a = (int)G(Imm10);
    char *v = F(t, attr_load(t, a, 0));
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
static void emit_f2f(T *t) {
    int64_t dst = G(DstFmt), src = G(SrcFmt), rm = G(RoundMode);
    char *f;
    if (src == 2) f = F(t, B(t, 1));
    else if (src == 1) f = S(t, "unpackHalf2x16(%s).%c", B(t, 0), G(Sh) == 1 ? 'y' : 'x');
    else {
        red(t, "F2F desde double");
        return;
    }
    f = absneg(t, f, G(AbsB), G(NegB));
    switch (rm) {
    case 4: f = S(t, "roundEven(%s)", f); break;
    case 5: f = S(t, "floor(%s)", f); break;
    case 6: f = S(t, "ceil(%s)", f); break;
    case 7: f = S(t, "trunc(%s)", f); break;
    }
    f = sat(t, f, G(Sat));
    if (dst == 2) WR(t, G(Dest), U(t, f));
    else if (dst == 1) WR(t, G(Dest), S(t, "packHalf2x16(vec2(%s, 0.0))", f));
    else red(t, "F2F a double");
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
    case 17: v = "0u"; break;                             // InvocationId
    case 18: v = "0x3f800000u"; break;                    // YDirection (1.0)
    case 33: v = "gl_GlobalInvocationID.x"; break;
    case 34: v = "gl_GlobalInvocationID.y"; break;
    case 35: v = "gl_GlobalInvocationID.z"; break;
    case 80: v = "0xffffffffu"; break;                    // EqMask etc. (un solo hilo)
    default: red(t, S(t, "S2R de SR%d", (int)G(SReg))); return;
    }
    if (t->stage != NX_SH_COMPUTE && G(SReg) >= 33 && G(SReg) <= 35) v = "0u";
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
    case MXOP_Stl: emit_local(t, 1); break;
    case MXOP_Vote: { // un solo hilo: todos/alguno/igual valen lo mismo que el predicado
        char *p = P(t, G(SrcPred), G(SrcPredInv));
        WR(t, G(Dest), S(t, "(%s ? 1u : 0u)", p));
        WP(t, G(VpDest), G(VoteMode) == 2 ? "true" : p);
        break;
    }
    case MXOP_Nop: case MXOP_Depbar: case MXOP_Membar: case MXOP_Bar: break;
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
    if (t->stage == NX_SH_VERTEX) {
        for (int i = 0; i < 32; i++)
            if (t->info->inputs >> i & 1)
                sb_add(h, "layout(location = %d) in %s in_attr%d;\n", i,
                       t->key->attr_int[i] == 1 ? "ivec4" : t->key->attr_int[i] == 2 ? "uvec4" : "vec4", i);
        int nclip = 32 - __builtin_clz(t->clip_mask | 1);
        sb_add(h, "out gl_PerVertex { vec4 gl_Position; float gl_PointSize;%s };\n",
               t->clip_mask ? S(t, " float gl_ClipDistance[%d];", nclip) : "");
        for (int i = 0; i < 32; i++)
            if (t->info->outputs >> i & 1) sb_add(h, "layout(location = %d) out vec4 out_attr%d;\n", i, i);
    } else if (t->stage == NX_SH_FRAGMENT) {
        for (int i = 0; i < 32; i++)
            if (t->info->inputs >> i & 1)
                sb_add(h, "layout(location = %d) %sin vec4 in_attr%d;\n", i,
                       t->in_flat >> i & 1 ? "flat " : t->in_linear >> i & 1 ? "noperspective " : "", i);
        uint32_t omap = t->sph[18];
        t->info->omap = omap;
        for (int rt = 0; rt < 8; rt++)
            if (omap >> (rt * 4) & 15) sb_add(h, "layout(location = %d) out vec4 frag%d;\n", rt, rt);
    }
    for (int s = 0; s < 18; s++)
        if (t->info->cbufs >> s & 1)
            sb_add(h, "layout(std140, binding = %d) uniform Cb%d { uvec4 cb%d[4096]; };\n", t->key->cb_binding + s, s, s);
    if (t->uses_local) sb_add(h, "const uint LMEM = %du;\nuint lmem[LMEM];\n", t->local_words);
    sb_add(h, "void main() {\n");
    for (int r = 0; r < 255; r++)
        if (t->reg_used[r]) sb_add(h, "    uint r%d = 0u;\n", r);
    sb_add(h, "    bool p0 = false, p1 = false, p2 = false, p3 = false, p4 = false, p5 = false, p6 = false;\n");
    sb_add(h, "    bool cc_z = false, cc_n = false, cc_c = false, cc_v = false;\n");
    if (t->stage == NX_SH_FRAGMENT)
        for (int rt = 0; rt < 8; rt++)
            if (t->sph[18] >> (rt * 4) & 15) sb_add(h, "    frag%d = vec4(0.0);\n", rt);
    if (t->stage == NX_SH_VERTEX)
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
    memcpy(t->sph, code, sizeof t->sph);
    t->code = code + 0x50;
    t->ninsn = (uint32_t)((avail - 0x50) / 8);
    if (t->ninsn > 0x40000) t->ninsn = 0x40000;
    t->visited = calloc(t->ninsn + 2, 1);
    t->leader = calloc(t->ninsn + 2, 1);
    t->local_words = (int)(((t->sph[1] & 0xFFFFFF) + (t->sph[2] & 0xFFFFFF)) / 4);
    if (t->local_words < 1) t->local_words = 1;
    if (t->local_words > 4096) t->local_words = 4096;
    discover(t);
    for (uint32_t i = t->ninsn; i-- > 0;)
        if (t->visited[i]) {
            info->code_size = 0x50 + (i + 1) * 8;
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
