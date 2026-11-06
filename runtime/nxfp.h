// Punto flotante con la semántica exacta de ARM (FPCR por defecto: redondeo al par más
// cercano, sin flush-to-zero, NaN por defecto apagado). Las operaciones trabajan con los
// bits del valor para que ningún NaN se pierda en registros de x86.
//
// Donde x86 y ARM coinciden (todo resultado que no es NaN) se usa la instrucción de
// x86 directamente; el camino lento solo corre cuando el resultado es NaN.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline float f32of(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t u32of(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline double f64of(uint64_t u) { double f; memcpy(&f, &u, 8); return f; }
static inline uint64_t u64of(double f) { uint64_t u; memcpy(&u, &f, 8); return u; }

// ---------- NaN (FPProcessNaNs del manual) ----------
#define NX_FP_COMMON(W, U, F, SFX, SIGN, QBIT, DNAN, EXPM)                                              \
    static inline int isnan##W(U x) { return (x & ~SIGN) > EXPM; }                              \
    static inline int issnan##W(U x) { return isnan##W(x) && !(x & QBIT); }                     \
    static inline int isinf##W(U x) { return (x & ~SIGN) == EXPM; }                             \
    static inline int iszero##W(U x) { return (x & ~SIGN) == 0; }                               \
    static inline U nan2_##W(U a, U b) {                                                        \
        if (issnan##W(a)) return a | QBIT;                                                      \
        if (issnan##W(b)) return b | QBIT;                                                      \
        if (isnan##W(a)) return a;                                                              \
        if (isnan##W(b)) return b;                                                              \
        return DNAN;                                                                            \
    }                                                                                           \
    static inline U nan3_##W(U a, U b, U c) {                                                   \
        if (issnan##W(a)) return a | QBIT;                                                      \
        if (issnan##W(b)) return b | QBIT;                                                      \
        if (issnan##W(c)) return c | QBIT;                                                      \
        if (isnan##W(a)) return a;                                                              \
        if (isnan##W(b)) return b;                                                              \
        if (isnan##W(c)) return c;                                                              \
        return DNAN;                                                                            \
    }                                                                                           \
    static inline U fadd##W(U a, U b) { U r = u##W##of(f##W##of(a) + f##W##of(b)); return isnan##W(r) ? nan2_##W(a, b) : r; } \
    static inline U fsub##W(U a, U b) { U r = u##W##of(f##W##of(a) - f##W##of(b)); return isnan##W(r) ? nan2_##W(a, b) : r; } \
    static inline U fmul##W(U a, U b) { U r = u##W##of(f##W##of(a) * f##W##of(b)); return isnan##W(r) ? nan2_##W(a, b) : r; } \
    static inline U fdiv##W(U a, U b) { U r = u##W##of(f##W##of(a) / f##W##of(b)); return isnan##W(r) ? nan2_##W(a, b) : r; } \
    static inline U fabd##W(U a, U b) { return fsub##W(a, b) & ~SIGN; }                         \
    static inline U fsqrt##W(U a) {                                                             \
        if (isnan##W(a)) return a | QBIT;                                                       \
        if ((a & SIGN) && !iszero##W(a)) return DNAN;                                           \
        return u##W##of(sqrt((double)f##W##of(a)));                                                    \
    }                                                                                           \
    /* FPMulAdd(addend, op1, op2): addend + op1 * op2 con un solo redondeo */                   \
    static inline U fmuladd##W(U a, U n, U m) {                                                 \
        int bad = (isinf##W(n) && iszero##W(m)) || (iszero##W(n) && isinf##W(m));              \
        if (isnan##W(a) || isnan##W(n) || isnan##W(m)) {                                        \
            U r = nan3_##W(a, n, m);                                                            \
            return (!issnan##W(a) && isnan##W(a) && bad && !issnan##W(n) && !issnan##W(m)) ? DNAN : r; \
        }                                                                                       \
        U r = u##W##of(fma##SFX(f##W##of(n), f##W##of(m), f##W##of(a)));                          \
        return isnan##W(r) ? DNAN : r;                                                          \
    }                                                                                           \
    static inline U fmax##W(U a, U b) {                                                         \
        if (isnan##W(a) || isnan##W(b)) return nan2_##W(a, b);                                  \
        if (iszero##W(a) && iszero##W(b)) return a & b;                                         \
        return f##W##of(a) > f##W##of(b) ? a : b;                                               \
    }                                                                                           \
    static inline U fmin##W(U a, U b) {                                                         \
        if (isnan##W(a) || isnan##W(b)) return nan2_##W(a, b);                                  \
        if (iszero##W(a) && iszero##W(b)) return a | b;                                         \
        return f##W##of(a) < f##W##of(b) ? a : b;                                               \
    }                                                                                           \
    /* FMAXNM/FMINNM: un NaN silencioso solo pierde contra un número */                        \
    static inline U fmaxnm##W(U a, U b) {                                                       \
        int qa = isnan##W(a) && !issnan##W(a), qb = isnan##W(b) && !issnan##W(b);              \
        if (qa && !qb) a = SIGN | EXPM;                                                         \
        else if (!qa && qb) b = SIGN | EXPM;                                                    \
        return fmax##W(a, b);                                                                   \
    }                                                                                           \
    static inline U fminnm##W(U a, U b) {                                                       \
        int qa = isnan##W(a) && !issnan##W(a), qb = isnan##W(b) && !issnan##W(b);              \
        if (qa && !qb) a = EXPM;                                                                \
        else if (!qa && qb) b = EXPM;                                                           \
        return fmin##W(a, b);                                                                   \
    }                                                                                           \
    /* NZCV de FCMP: desordenado 0011, igual 0110, menor 1000, mayor 0010 */                    \
    static inline unsigned fcmp##W(U a, U b) {                                                  \
        if (isnan##W(a) || isnan##W(b)) return 0x3;                                             \
        F x = f##W##of(a), y = f##W##of(b);                                                     \
        return x == y ? 0x6 : x < y ? 0x8 : 0x2;                                                \
    }                                                                                           \
    /* redondeo a entero en formato float: 0 al par, 1 hacia +inf, 2 hacia -inf, 3 hacia 0, 4 alejándose del 0 */ \
    static inline U frint##W(U a, int mode) {                                                   \
        if (isnan##W(a)) return a | QBIT;                                                       \
        F x = f##W##of(a), r;                                                                   \
        switch (mode) {                                                                         \
        case 0: r = __builtin_roundeven((double)x); break;                                              \
        case 1: r = ceil((double)x); break;                                                          \
        case 2: r = floor((double)x); break;                                                          \
        case 3: r = trunc((double)x); break;                                                          \
        default: r = round((double)x); break;                                                          \
        }                                                                                       \
        return u##W##of(r) | (a & SIGN); /* conserva el signo de -0,4 → -0 */                   \
    }

NX_FP_COMMON(32, uint32_t, float, f, 0x80000000u, 0x00400000u, 0x7FC00000u, 0x7F800000u)
NX_FP_COMMON(64, uint64_t, double, , 0x8000000000000000ull, 0x0008000000000000ull, 0x7FF8000000000000ull,
             0x7FF0000000000000ull)

// Media precisión (ARMv8.2 FP16, Switch 2). _Float16 de clang calcula en float y redondea una vez al
// guardar: suma, resta, producto y cociente salen exactos así (float tiene más de 2 × 11 + 2 bits).
// La raíz y el FMA van por double, que también deja un solo redondeo efectivo a media precisión.
static inline _Float16 f16of(uint16_t u) { _Float16 f; memcpy(&f, &u, 2); return f; }
static inline uint16_t u16of(_Float16 f) { uint16_t u; memcpy(&u, &f, 2); return u; }
static inline _Float16 fmah16(_Float16 n, _Float16 m, _Float16 a) { return (_Float16)fma((double)n, (double)m, (double)a); }
NX_FP_COMMON(16, uint16_t, _Float16, h16, 0x8000, 0x0200, 0x7E00, 0x7C00)

static inline double fval16(uint16_t a) { return (double)f16of(a); }
static inline double fval32(uint32_t a) { return (double)f32of(a); }
static inline double fval64(uint64_t a) { return f64of(a); }

// ---------- float a entero (FPToFixed): satura y un NaN da 0 ----------
static inline double nx_round(double x, int mode) {
    switch (mode) {
    case 0: return __builtin_roundeven(x);
    case 1: return ceil(x);
    case 2: return floor(x);
    case 3: return trunc(x);
    default: return round(x);
    }
}
// Las entradas de 32 bits se pasan a double, que es exacto; `scale` es 2^fbits.
static inline int64_t f2s64(double x, int mode, double scale) {
    if (x != x) return 0;
    double r = nx_round(x * scale, mode);
    if (r >= 9223372036854775808.0) return INT64_MAX;
    if (r < -9223372036854775808.0) return INT64_MIN;
    return (int64_t)r;
}
static inline uint64_t f2u64(double x, int mode, double scale) {
    if (x != x) return 0;
    double r = nx_round(x * scale, mode);
    if (r >= 18446744073709551616.0) return UINT64_MAX;
    if (r <= 0) return 0;
    return (uint64_t)r;
}
static inline int32_t f2s32(double x, int mode, double scale) {
    if (x != x) return 0;
    double r = nx_round(x * scale, mode);
    if (r >= 2147483648.0) return INT32_MAX;
    if (r < -2147483648.0) return INT32_MIN;
    return (int32_t)r;
}
static inline uint32_t f2u32(double x, int mode, double scale) {
    if (x != x) return 0;
    double r = nx_round(x * scale, mode);
    if (r >= 4294967296.0) return UINT32_MAX;
    if (r <= 0) return 0;
    return (uint32_t)r;
}

// ---------- conversiones entre precisiones (FPConvert) ----------
static inline uint64_t cvt_d_s(uint32_t a) {
    if (isnan32(a)) // el NaN conserva su carga útil y queda silencioso
        return (uint64_t)(a >> 31) << 63 | 0x7FF8000000000000ull | (uint64_t)(a & 0x003FFFFF) << 29;
    return u64of((double)f32of(a));
}
static inline uint32_t cvt_s_d(uint64_t a) {
    if (isnan64(a))
        return (uint32_t)(a >> 63) << 31 | 0x7FC00000u | (uint32_t)((a >> 29) & 0x003FFFFF);
    return u32of((float)f64of(a));
}

// Media precisión IEEE (FPCR.AHP = 0). De un formato más ancho se redondea una sola vez.
static inline uint16_t half_from_parts(int sign, int exp, uint64_t mant, int mbits) {
    // valor = (-1)^sign × 1.mant × 2^exp (exp sin sesgo), mant con mbits bits
    uint16_t s = (uint16_t)sign << 15;
    uint64_t full = (1ull << mbits) | mant;   // 1.mant con mbits bits fraccionarios
    int e = exp + 15;
    int shift;                                // bits a descartar para dejar 10 fraccionarios
    if (e >= 1) shift = mbits - 10;
    else shift = mbits - 10 + (1 - e), e = 0; // subnormal
    uint64_t q, rem, half;
    if (shift >= 64) q = 0, rem = full ? 1 : 0, half = ~0ull; // todo se pierde (sticky)
    else if (shift <= 0) q = full << -shift, rem = 0, half = 1;
    else q = full >> shift, rem = full & ((1ull << shift) - 1), half = 1ull << (shift - 1);
    if (shift < 64 && (rem > half || (rem == half && (q & 1)))) q++;
    if (e == 0) { // subnormal: q ya incluye el bit implícito si redondeó hasta el normal
        return s | (uint16_t)q;
    }
    if (q >= (1ull << 11)) { q >>= 1; e++; }
    if (e >= 31) return s | 0x7C00;           // desborda a infinito
    return s | (uint16_t)(e << 10) | (uint16_t)(q & 0x3FF);
}
static inline uint16_t cvt_h_d(uint64_t a) {
    int sign = a >> 63, exp = (a >> 52) & 0x7FF;
    uint64_t m = a & 0xFFFFFFFFFFFFFull;
    if (exp == 0x7FF) return m ? (uint16_t)(sign << 15 | 0x7E00 | (m >> 42 & 0x1FF)) : (uint16_t)(sign << 15 | 0x7C00);
    if (exp == 0 && m == 0) return (uint16_t)(sign << 15);
    if (exp == 0) { // subnormal de double: muy lejos del rango de half
        return (uint16_t)(sign << 15);
    }
    return half_from_parts(sign, exp - 1023, m, 52);
}
static inline uint16_t cvt_h_s(uint32_t a) {
    int sign = a >> 31, exp = (a >> 23) & 0xFF;
    uint32_t m = a & 0x7FFFFF;
    if (exp == 0xFF) return m ? (uint16_t)(sign << 15 | 0x7E00 | (m >> 13 & 0x1FF)) : (uint16_t)(sign << 15 | 0x7C00);
    if (exp == 0 && m == 0) return (uint16_t)(sign << 15);
    if (exp == 0) return (uint16_t)(sign << 15);
    return half_from_parts(sign, exp - 127, m, 23);
}
static inline uint32_t cvt_s_h(uint16_t h) {
    uint32_t sign = (uint32_t)(h >> 15) << 31, exp = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (exp == 0x1F) return sign | 0x7F800000u | (m ? 0x00400000u | m << 13 : 0);
    if (exp == 0) {
        if (!m) return sign;
        int k = 0;
        while (!(m & 0x400)) m <<= 1, k++;
        return sign | (uint32_t)(113 - k) << 23 | (m & 0x3FF) << 13;
    }
    return sign | (exp + 112) << 23 | m << 13;
}
static inline uint64_t cvt_d_h(uint16_t h) { return cvt_d_s(cvt_s_h(h)); }

// SCVTF/UCVTF a media precisión: un solo redondeo desde el entero, con `fbits` bits fraccionarios
// (pasar por float o double redondearía dos veces).
static inline uint16_t cvt_h_u64(uint64_t u, int sign, int fbits) {
    if (!u) return 0;
    int p = 63 - __builtin_clzll(u);
    return half_from_parts(sign, p - fbits, u & ~(1ull << p), p);
}
static inline uint16_t scvtf_h(int64_t v, int fbits) {
    return v < 0 ? cvt_h_u64(-(uint64_t)v, 1, fbits) : cvt_h_u64((uint64_t)v, 0, fbits);
}
static inline uint16_t ucvtf_h(uint64_t v, int fbits) { return cvt_h_u64(v, 0, fbits); }
