// Operaciones de NEON por elemento que C no expresa directo: saturación, desplazamientos
// por registro, multiplicaciones dobladas, estimaciones de recíproco y conversiones raras.
// Todas siguen el pseudocódigo del manual de Arm. El bit QC de FPSR no se lleva.
#pragma once
#include <stdint.h>

#include "nxfp.h"
#include "nxtables.h"

#include <float.h>
#define FLT_MIN_32 FLT_MIN
#define FLT_MIN_64 DBL_MIN

typedef __int128 i128;

// ---------- saturación ----------
static inline uint64_t ssat(i128 v, int esize) {
    i128 hi = ((i128)1 << (esize - 1)) - 1, lo = -((i128)1 << (esize - 1));
    return (uint64_t)(v > hi ? hi : v < lo ? lo : v);
}
static inline uint64_t usat(i128 v, int esize) {
    i128 hi = esize == 64 ? (i128)UINT64_MAX : ((i128)1 << esize) - 1;
    return (uint64_t)(v > hi ? hi : v < 0 ? 0 : v);
}
// valor del elemento como entero con o sin signo
static inline i128 sval(uint64_t x, int esize) { return (i128)(int64_t)(x << (64 - esize)) >> (64 - esize); }
static inline i128 uval(uint64_t x, int esize) { return (i128)(esize == 64 ? x : x & ((1ull << esize) - 1)); }
static inline i128 ival(uint64_t x, int esize, int uns) { return uns ? uval(x, esize) : sval(x, esize); }
static inline uint64_t emask(int esize) { return esize == 64 ? ~0ull : (1ull << esize) - 1; }

// SSHL/USHL/SRSHL/URSHL/SQSHL/UQSHL/SQRSHL/UQRSHL: el desplazamiento es el byte bajo con signo de b
static inline uint64_t vshl(uint64_t a, uint64_t b, int esize, int uns, int round, int sat) {
    int sh = (int8_t)(uint8_t)b;
    i128 x = ival(a, esize, uns);
    i128 r;
    if (sh >= 0) {
        if (x == 0) return 0;
        if (sh >= esize) { // todos los bits salen: 0, o saturado
            if (!sat) return 0;
            return uns ? emask(esize) : x < 0 ? (uint64_t)ssat(-((i128)1 << 100), esize) & emask(esize) : emask(esize) >> 1;
        }
        r = x * ((i128)1 << sh);
    } else {
        int s = -sh > 65 ? 65 : -sh;
        if (round) r = (x + ((i128)1 << (s - 1))) >> s;
        else r = x >> (s > 64 ? 64 : s);
    }
    if (sat) return (uns ? usat(r, esize) : ssat(r, esize)) & emask(esize);
    return (uint64_t)r & emask(esize);
}

// SQDMULH / SQRDMULH
static inline uint64_t sqdmulh(uint64_t a, uint64_t b, int esize, int round) {
    i128 p = 2 * sval(a, esize) * sval(b, esize);
    if (round) p += (i128)1 << (esize - 1);
    return ssat(p >> esize, esize) & emask(esize);
}

// SQRDMLAH / SQRDMLSH (ARMv8.1 RDM): acumula el producto doblado sobre dd ensanchado, redondea y satura
static inline uint64_t sqrdmlah(uint64_t dd, uint64_t a, uint64_t b, int esize, int sub) {
    i128 p = 2 * sval(a, esize) * sval(b, esize);
    i128 acc = sval(dd, esize) * ((i128)1 << esize) + (sub ? -p : p) + ((i128)1 << (esize - 1));
    return ssat(acc >> esize, esize) & emask(esize);
}

// SDOT / UDOT (ARMv8.2 DotProd): suma de los cuatro productos de bytes de una palabra de 32 bits
static inline uint32_t dot4(uint32_t a, uint32_t b, int uns) {
    int32_t s = 0;
    for (int k = 0; k < 32; k += 8)
        s += uns ? (int32_t)((a >> k & 0xFF) * (b >> k & 0xFF)) : (int8_t)(a >> k) * (int8_t)(b >> k);
    return (uint32_t)s;
}

// PMUL de 8 bits y PMULL (polinomios sobre GF(2))
static inline uint64_t pmul(uint64_t a, uint64_t b, int esize) {
    unsigned __int128 r = 0;
    for (int k = 0; k < esize; k++)
        if (b >> k & 1) r ^= (unsigned __int128)a << k;
    return (uint64_t)r;
}
static inline unsigned __int128 pmul128(uint64_t a, uint64_t b) {
    unsigned __int128 r = 0;
    for (int k = 0; k < 64; k++)
        if (b >> k & 1) r ^= (unsigned __int128)a << k;
    return r;
}

// ---------- punto flotante por elemento ----------
#define NX_SIMD_FP(W, U, T, SIGN, EXPM, ONE, TWO, THREEHALF, THREE, SFX)                     \
    /* FMULX: como FMUL, pero 0 × inf da ±2 */                                                \
    static inline U fmulx##W(U a, U b) {                                                      \
        if (isnan##W(a) || isnan##W(b)) return nan2_##W(a, b);                                \
        if ((isinf##W(a) && iszero##W(b)) || (iszero##W(a) && isinf##W(b)))                  \
            return ((a ^ b) & SIGN) | TWO;                                                    \
        return fmul##W(a, b);                                                                 \
    }                                                                                         \
    /* FRECPS: 2 - a × b con un redondeo */                                                   \
    static inline U frecps##W(U a, U b) {                                                     \
        U na = a ^ SIGN;                                                                      \
        if (isnan##W(na) || isnan##W(b)) return nan2_##W(na, b);                              \
        if ((isinf##W(na) && iszero##W(b)) || (iszero##W(na) && isinf##W(b))) return TWO;     \
        return fmuladd##W(TWO, na, b);                                                        \
    }                                                                                         \
    /* FRSQRTS: (3 - a × b) / 2 con un redondeo */                                            \
    static inline U frsqrts##W(U a, U b) {                                                    \
        U na = a ^ SIGN;                                                                      \
        if (isnan##W(na) || isnan##W(b)) return nan2_##W(na, b);                              \
        if ((isinf##W(na) && iszero##W(b)) || (iszero##W(na) && isinf##W(b))) return THREEHALF; \
        if (isinf##W(na) || isinf##W(b)) return ((na ^ b) & SIGN) | EXPM;                     \
        /* (3 + na·b) / 2 = 1,5 + na·(b/2): un solo redondeo y sin desbordar antes de dividir */ \
        T x = f##W##of(na), y = f##W##of(b), r;                                               \
        if (fabs((double)y) >= 4 * (double)FLT_MIN_##W) r = fma##SFX(x, y / 2, (T)1.5);           \
        else if (fabs((double)x) >= 4 * (double)FLT_MIN_##W) r = fma##SFX(x / 2, y, (T)1.5);     \
        else r = fma##SFX(x, y, (T)3.0) / (T)2.0;                                              \
        return isnan##W(u##W##of(r)) ? nan2_##W(na, b) : u##W##of(r);                         \
    }

NX_SIMD_FP(32, uint32_t, float, 0x80000000u, 0x7F800000u, 0x3F800000u, 0x40000000u, 0x3FC00000u, 0x40400000u, f)
NX_SIMD_FP(64, uint64_t, double, 0x8000000000000000ull, 0x7FF0000000000000ull, 0x3FF0000000000000ull,
           0x4000000000000000ull, 0x3FF8000000000000ull, 0x4008000000000000ull, )

// FRECPE (FPRecipEstimate). `w` es 32 o 64; los bits llegan en un uint64_t.
static inline uint64_t frecpe(uint64_t op, int w) {
    int fbits = w == 32 ? 23 : 52, ebits = w == 32 ? 8 : 11;
    uint64_t sign = op >> (w - 1) & 1;
    int exp = (int)(op >> fbits & ((1u << ebits) - 1));
    uint64_t frac = op & ((1ull << fbits) - 1);
    uint64_t expmax = (1ull << ebits) - 1;
    if (exp == (int)expmax) {
        if (frac) return op | (1ull << (fbits - 1));         // NaN: silencioso
        return sign << (w - 1);                               // 1/inf = 0
    }
    if (exp == 0 && frac == 0) return sign << (w - 1) | expmax << fbits; // 1/0 = inf
    uint64_t f52 = frac << (52 - fbits);                      // fracción alineada a 52 bits
    if (exp == 0) {
        // |valor| < 2^-128 (o 2^-1024): el resultado desborda a infinito
        if (!(f52 >> 51) && !(f52 >> 50 & 1)) return sign << (w - 1) | expmax << fbits;
        if (!(f52 >> 51)) exp = -1, f52 = (f52 << 2) & ((1ull << 52) - 1);
        else f52 = (f52 << 1) & ((1ull << 52) - 1);
    }
    uint64_t scaled = (1u << 8) | (f52 >> 44 & 0xFF);
    int rexp = (w == 32 ? 253 : 2045) - exp;
    uint64_t est = recip_est[scaled - 256];
    uint64_t rf = (est & 0xFF) << 44;
    if (rexp == 0) rf = (1ull << 51) | rf >> 1;
    else if (rexp == -1) rf = (1ull << 50) | rf >> 2, rexp = 0;
    return sign << (w - 1) | (uint64_t)rexp << fbits | rf >> (52 - fbits);
}

// FRSQRTE (FPRSqrtEstimate)
static inline uint64_t frsqrte(uint64_t op, int w) {
    int fbits = w == 32 ? 23 : 52, ebits = w == 32 ? 8 : 11;
    uint64_t sign = op >> (w - 1) & 1;
    int exp = (int)(op >> fbits & ((1u << ebits) - 1));
    uint64_t frac = op & ((1ull << fbits) - 1);
    uint64_t expmax = (1ull << ebits) - 1;
    if (exp == (int)expmax && frac) return op | (1ull << (fbits - 1));
    if (exp == 0 && frac == 0) return sign << (w - 1) | expmax << fbits;
    if (sign) return w == 32 ? 0x7FC00000u : 0x7FF8000000000000ull;
    if (exp == (int)expmax) return 0;
    uint64_t f52 = frac << (52 - fbits);
    if (exp == 0) {
        while (!(f52 >> 51)) f52 = (f52 << 1) & ((1ull << 52) - 1), exp--;
        f52 = (f52 << 1) & ((1ull << 52) - 1);
    }
    uint64_t scaled = !(exp & 1) ? (1u << 8) | (f52 >> 44 & 0xFF) : (1u << 7) | (f52 >> 45 & 0x7F);
    int num = (w == 32 ? 380 : 3068) - exp;
    int rexp = num >= 0 ? num / 2 : -((-num + 1) / 2);
    uint64_t est = rsqrt_est[scaled - 128];
    return (uint64_t)rexp << fbits | (est & 0xFF) << (fbits - 8);
}

static inline uint32_t urecpe(uint32_t op) {
    if (!(op >> 31)) return 0xFFFFFFFFu;
    return (uint32_t)recip_est[(op >> 23) - 256] << 23;
}
static inline uint32_t ursqrte(uint32_t op) {
    if (!(op >> 30)) return 0xFFFFFFFFu;
    return (uint32_t)rsqrt_est[(op >> 23) - 128] << 23;
}

// FRECPX
static inline uint64_t frecpx(uint64_t op, int w) {
    int fbits = w == 32 ? 23 : 52, ebits = w == 32 ? 8 : 11;
    uint64_t expmax = (1ull << ebits) - 1;
    uint64_t exp = op >> fbits & expmax, frac = op & ((1ull << fbits) - 1), sign = op >> (w - 1) & 1;
    if (exp == expmax && frac) return op | (1ull << (fbits - 1));
    if (exp == 0) return sign << (w - 1) | (expmax - 1) << fbits;
    return sign << (w - 1) | (~exp & expmax) << fbits;
}

// FCVTXN: de double a float redondeando a impar
static inline uint32_t cvt_s_d_odd(uint64_t a) {
    if (isnan64(a)) return cvt_s_d(a);
    double x = f64of(a);
    float f = (float)x;
    if ((double)f == x) return u32of(f);
    if (fabs((double)f) > fabs(x)) f = nextafterf(f, 0.0f);
    return u32of(f) | 1;
}

static inline uint64_t popcnt8(uint64_t x) { return (uint64_t)__builtin_popcount((unsigned)(x & 0xFF)); }

// CRC32 / CRC32C de ARM: actualiza el acumulador con `bytes` bytes de `data`, sin invertir.
static inline uint32_t crc32_arm(uint32_t acc, uint64_t data, int bytes, uint32_t poly) {
    for (int k = 0; k < bytes; k++) {
        acc ^= (uint8_t)(data >> (8 * k));
        for (int j = 0; j < 8; j++) acc = (acc >> 1) ^ (poly & (0u - (acc & 1)));
    }
    return acc;
}
