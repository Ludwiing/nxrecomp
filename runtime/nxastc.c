// Decodificador ASTC (perfil LDR, bloques 2D) a RGBA8, según la especificación de Khronos.
// Las GPU de PC no leen ASTC, y casi todos los juegos de Switch lo usan: se descomprime al subir.
#include "nxastc.h"

#include <stdio.h>
#include <string.h>

int nx_astc_depurar;
int nx_astc_variante; // las pruebas lo activan para ver por qué un bloque es de error
#define DBG(...) (nx_astc_depurar ? fprintf(stderr, __VA_ARGS__) : 0)

// ---------- bits ----------

static uint32_t bits(const uint8_t *b, int pos, int n) { // n ≤ 25, lectura little-endian
    uint32_t v = 0;
    for (int i = 0; i < n; i++) v |= (uint32_t)(b[(pos + i) >> 3] >> ((pos + i) & 7) & 1) << i;
    return v;
}

// ---------- codificación entera (ISE) ----------

typedef struct {
    uint8_t trits, quints, bits;
} Rango;

// Niveles de cuantización: los de color usan todos; los de peso, los primeros 12.
static const Rango rangos[] = {
    {0, 0, 1}, {1, 0, 0}, {0, 0, 2}, {0, 1, 0}, {1, 0, 1}, {0, 0, 3}, {0, 1, 1}, {1, 0, 2}, {0, 0, 4}, {0, 1, 2}, {1, 0, 3},
    {0, 0, 5}, {0, 1, 3}, {1, 0, 4}, {0, 0, 6}, {0, 1, 4}, {1, 0, 5}, {0, 0, 7}, {0, 1, 5}, {1, 0, 6}, {0, 0, 8},
};

static int ise_bits(int n, int q) {
    const Rango *r = &rangos[q];
    return n * r->bits + (r->trits ? (8 * n + 4) / 5 : 0) + (r->quints ? (7 * n + 2) / 3 : 0);
}

static void trits(uint32_t t, int *o) {
    uint32_t c;
    int t4, t3, t2, t1, t0;
    if ((t >> 2 & 7) == 7) {
        c = (t >> 5 & 7) << 2 | (t & 3);
        t4 = t3 = 2;
    } else {
        c = t & 0x1F;
        if ((t >> 5 & 3) == 3) t4 = 2, t3 = t >> 7 & 1;
        else t4 = t >> 7 & 1, t3 = t >> 5 & 3;
    }
    if ((c & 3) == 3) t2 = 2, t1 = c >> 4 & 1, t0 = (c >> 3 & 1) << 1 | ((c >> 2 & 1) & ~(c >> 3 & 1));
    else if ((c >> 2 & 3) == 3) t2 = 2, t1 = 2, t0 = c & 3;
    else t2 = c >> 4 & 1, t1 = c >> 2 & 3, t0 = (c >> 1 & 1) << 1 | ((c & 1) & ~(c >> 1 & 1));
    o[0] = t0, o[1] = t1, o[2] = t2, o[3] = t3, o[4] = t4;
}

static void quints(uint32_t q, int *o) {
    int q2, q1, q0;
    if ((q >> 1 & 3) == 3 && (q >> 5 & 3) == 0) {
        q2 = (q & 1) << 2 | ((q >> 4 & 1) & ~(q & 1)) << 1 | ((q >> 3 & 1) & ~(q & 1));
        q1 = q0 = 4;
    } else {
        uint32_t c;
        if ((q >> 1 & 3) == 3) q2 = 4, c = (q >> 3 & 3) << 3 | (~(q >> 5) & 3) << 1 | (q & 1);
        else q2 = q >> 5 & 3, c = q & 0x1F;
        if ((c & 7) == 5) q1 = 4, q0 = c >> 3 & 3;
        else q1 = c >> 3 & 3, q0 = c & 7;
    }
    o[0] = q0, o[1] = q1, o[2] = q2;
}

// Decodifica n valores (cada uno: parte de trit/quint en `tq`, bits bajos en `m`).
static void ise(const uint8_t *bloque, int pos, int n, int q, int *tq, int *m) {
    const Rango *r = &rangos[q];
    int nb = r->bits;
    // Los bits que pasan del final de la secuencia (último grupo incompleto) valen 0.
    uint8_t b[16] = {0};
    int fin = pos + ise_bits(n, q);
    for (int i = pos; i < fin && i < 128; i++) b[i >> 3] |= (uint8_t)((bloque[i >> 3] >> (i & 7) & 1) << (i & 7));
    if (r->trits) {
        for (int i = 0; i < n; i += 5) {
            int mm[5] = {0}, t = 0, p = pos;
            static const int tb[5] = {2, 2, 1, 2, 1}; // bits de T después de cada m
            int tsh = 0;
            for (int k = 0; k < 5; k++) {
                mm[k] = (int)bits(b, p, nb), p += nb;
                t |= (int)bits(b, p, tb[k]) << tsh, p += tb[k], tsh += tb[k];
            }
            int tt[5];
            trits((uint32_t)t, tt);
            for (int k = 0; k < 5 && i + k < n; k++) tq[i + k] = tt[k], m[i + k] = mm[k];
            pos += 5 * nb + 8;
        }
    } else if (r->quints) {
        for (int i = 0; i < n; i += 3) {
            int mm[3] = {0}, qv = 0, p = pos;
            static const int qb[3] = {3, 2, 2};
            int qsh = 0;
            for (int k = 0; k < 3; k++) {
                mm[k] = (int)bits(b, p, nb), p += nb;
                qv |= (int)bits(b, p, qb[k]) << qsh, p += qb[k], qsh += qb[k];
            }
            int qq[3];
            quints((uint32_t)qv, qq);
            for (int k = 0; k < 3 && i + k < n; k++) tq[i + k] = qq[k], m[i + k] = mm[k];
            pos += 3 * nb + 7;
        }
    } else {
        for (int i = 0; i < n; i++) tq[i] = 0, m[i] = (int)bits(b, pos + i * nb, nb);
    }
}

// ---------- descuantización ----------

static int color_unq(int q, int tq, int m) {
    const Rango *r = &rangos[q];
    int nb = r->bits;
    if (!r->trits && !r->quints) { // replicar los bits hasta 8
        int v = m << (8 - nb), out = 0;
        for (int sh = 0; sh < 8; sh += nb) out |= v >> sh;
        return out & 0xFF;
    }
    int A = (m & 1) ? 0x1FF : 0, B = 0, C = 0;
    int b = m >> 1 & 1, c = m >> 2 & 1, d = m >> 3 & 1, e = m >> 4 & 1, f = m >> 5 & 1;
    if (r->trits) {
        switch (nb) {
        case 1: C = 204; break;
        case 2: C = 93, B = b * 0x116; break;
        case 3: C = 44, B = c * 0x10A + b * 0x085; break;
        case 4: C = 22, B = d * 0x104 + c * 0x082 + b * 0x041; break;
        case 5: C = 11, B = e * 0x102 + d * 0x081 + c * 0x040 + b * 0x020; break;
        case 6: C = 5, B = f * 0x101 + e * 0x080 + d * 0x040 + c * 0x020 + b * 0x010; break;
        }
    } else {
        switch (nb) {
        case 1: C = 113; break;
        case 2: C = 54, B = b * 0x10C; break;
        case 3: C = 26, B = c * 0x105 + b * 0x082; break;
        case 4: C = 13, B = d * 0x102 + c * 0x081 + b * 0x040; break;
        case 5: C = 6, B = e * 0x101 + d * 0x080 + c * 0x040 + b * 0x020; break;
        }
    }
    int T = tq * C + B;
    T ^= A;
    return (A & 0x80) | (T >> 2);
}

static int weight_unq(int q, int tq, int m) {
    const Rango *r = &rangos[q];
    int nb = r->bits, v;
    if (!r->trits && !r->quints) {
        int x = m << (6 - nb);
        v = 0;
        for (int sh = 0; sh < 6; sh += nb) v |= x >> sh;
        v &= 0x3F;
    } else if (nb == 0) {
        return r->trits ? tq * 32 : tq * 16; // ya están en 0..64
    } else {
        int A = (m & 1) ? 0x7F : 0, B = 0, C = 0, b = m >> 1 & 1, c = m >> 2 & 1;
        if (r->trits) {
            if (nb == 1) C = 50;
            else if (nb == 2) C = 23, B = b * 0x45;
            else C = 11, B = c * 0x42 + b * 0x21;
        } else {
            if (nb == 1) C = 28;
            else C = 13, B = b * 0x42;
        }
        int T = tq * C + B;
        T ^= A;
        v = (A & 0x20) | (T >> 2);
    }
    return v > 32 ? v + 1 : v;
}

// ---------- particiones ----------

static uint32_t hash52(uint32_t p) {
    p ^= p >> 15;
    p -= p << 17;
    p += p << 7;
    p += p << 4;
    p ^= p >> 5;
    p += p << 16;
    p ^= p >> 7;
    p ^= p >> 3;
    p ^= p << 6;
    p ^= p >> 17;
    return p;
}

static int particion(int seed, int x, int y, int n, int chico) {
    if (n == 1) return 0;
    if (chico) x <<= 1, y <<= 1;
    seed += (n - 1) * 1024;
    uint32_t r = hash52((uint32_t)seed);
    uint32_t s[12] = {r & 0xF, r >> 4 & 0xF, r >> 8 & 0xF, r >> 12 & 0xF, r >> 16 & 0xF, r >> 20 & 0xF, r >> 24 & 0xF, r >> 28 & 0xF,
                      r >> 18 & 0xF, r >> 22 & 0xF, r >> 26 & 0xF, ((r >> 30) | (r << 2)) & 0xF};
    for (int i = 0; i < 12; i++) s[i] *= s[i];
    int sh1, sh2;
    if (seed & 1) sh1 = seed & 2 ? 4 : 5, sh2 = n == 3 ? 6 : 5;
    else sh1 = n == 3 ? 6 : 5, sh2 = seed & 2 ? 4 : 5;
    int sh3 = seed & 0x10 ? sh1 : sh2;
    for (int i = 0; i < 8; i++) s[i] >>= (i & 1) ? sh2 : sh1;
    for (int i = 8; i < 12; i++) s[i] >>= sh3;
    int a = (int)((s[0] * x + s[1] * y + (r >> 14)) & 0x3F);
    int b = (int)((s[2] * x + s[3] * y + (r >> 10)) & 0x3F);
    int c = (int)((s[4] * x + s[5] * y + (r >> 6)) & 0x3F);
    int d = (int)((s[6] * x + s[7] * y + (r >> 2)) & 0x3F);
    if (n < 4) d = 0;
    if (n < 3) c = 0;
    if (a >= b && a >= c && a >= d) return 0;
    if (b >= c && b >= d) return 1;
    if (c >= d) return 2;
    return 3;
}

// ---------- extremos de color ----------

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static void bit_transfer(int *a, int *b) { // a: desplazamiento, b: base
    *b = (*b >> 1) | (*a & 0x80);
    *a = (*a >> 1) & 0x3F;
    if (*a & 0x20) *a -= 0x40;
}
static void azul(int *c) { // "blue contraction"
    c[0] = (c[0] + c[2]) >> 1;
    c[1] = (c[1] + c[2]) >> 1;
}

// Extremos de 8 bits de un modo de color. Devuelve 0 si el modo es HDR: en el perfil LDR esos texels
// son de error (como hace Mesa: solo los de esa partición).
static int extremos(int cem, const int *v, int e0[4], int e1[4]) {
    int a[8];
    memcpy(a, v, sizeof a);
    switch (cem) {
    case 0:
        for (int i = 0; i < 3; i++) e0[i] = a[0], e1[i] = a[1];
        e0[3] = e1[3] = 255;
        return 1;
    case 1: {
        int l0 = (a[0] >> 2) | (a[1] & 0xC0), l1 = l0 + (a[1] & 0x3F);
        if (l1 > 255) l1 = 255;
        for (int i = 0; i < 3; i++) e0[i] = l0, e1[i] = l1;
        e0[3] = e1[3] = 255;
        return 1;
    }
    case 4:
        for (int i = 0; i < 3; i++) e0[i] = a[0], e1[i] = a[1];
        e0[3] = a[2], e1[3] = a[3];
        return 1;
    case 5:
        bit_transfer(&a[1], &a[0]);
        bit_transfer(&a[3], &a[2]);
        for (int i = 0; i < 3; i++) e0[i] = a[0], e1[i] = clamp255(a[0] + a[1]);
        e0[3] = a[2], e1[3] = clamp255(a[2] + a[3]);
        return 1;
    case 6:
        for (int i = 0; i < 3; i++) e0[i] = (a[i] * a[3]) >> 8, e1[i] = a[i];
        e0[3] = e1[3] = 255;
        return 1;
    case 8: case 12: {
        int al0 = cem == 12 ? a[6] : 255, al1 = cem == 12 ? a[7] : 255;
        if (a[1] + a[3] + a[5] >= a[0] + a[2] + a[4]) {
            e0[0] = a[0], e0[1] = a[2], e0[2] = a[4], e0[3] = al0;
            e1[0] = a[1], e1[1] = a[3], e1[2] = a[5], e1[3] = al1;
        } else {
            e0[0] = a[1], e0[1] = a[3], e0[2] = a[5], e0[3] = al1;
            e1[0] = a[0], e1[1] = a[2], e1[2] = a[4], e1[3] = al0;
            azul(e0), azul(e1);
        }
        return 1;
    }
    case 9: case 13: {
        bit_transfer(&a[1], &a[0]);
        bit_transfer(&a[3], &a[2]);
        bit_transfer(&a[5], &a[4]);
        if (cem == 13) bit_transfer(&a[7], &a[6]);
        int al0 = cem == 13 ? a[6] : 255, al1 = cem == 13 ? a[6] + a[7] : 255;
        if (a[1] + a[3] + a[5] >= 0) {
            e0[0] = a[0], e0[1] = a[2], e0[2] = a[4], e0[3] = al0;
            e1[0] = a[0] + a[1], e1[1] = a[2] + a[3], e1[2] = a[4] + a[5], e1[3] = al1;
        } else {
            e0[0] = a[0] + a[1], e0[1] = a[2] + a[3], e0[2] = a[4] + a[5], e0[3] = al1;
            e1[0] = a[0], e1[1] = a[2], e1[2] = a[4], e1[3] = al0;
            azul(e0), azul(e1);
        }
        for (int i = 0; i < 4; i++) e0[i] = clamp255(e0[i]), e1[i] = clamp255(e1[i]);
        return 1;
    }
    case 10:
        for (int i = 0; i < 3; i++) e0[i] = (a[i] * a[3]) >> 8, e1[i] = a[i];
        e0[3] = a[4], e1[3] = a[5];
        return 1;
    }
    return 0; // 2, 3, 7, 11, 14, 15: HDR
}

// ---------- bloque ----------

static void error_bloque(uint8_t *out, int bw, int bh, int stride) {
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            uint8_t *p = out + y * stride + x * 4;
            p[0] = 255, p[1] = 0, p[2] = 255, p[3] = 255;
        }
}

void nx_astc_bloque(const uint8_t *b, int bw, int bh, int srgb, uint8_t *out, int stride) {
    uint32_t modo = bits(b, 0, 11);
    // Bloque de color constante ("void extent").
    if ((modo & 0x1FF) == 0x1FC) {
        if (modo >> 9 & 1) { // HDR: error en el perfil LDR (los bits reservados 10 y 11 no se miran)
            DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
            return;
        }
        uint32_t s0 = bits(b, 12, 13), s1 = bits(b, 25, 13), t0 = bits(b, 38, 13), t1 = bits(b, 51, 13);
        int todos = s0 == 0x1FFF && s1 == 0x1FFF && t0 == 0x1FFF && t1 == 0x1FFF;
        if (!todos && (s0 >= s1 || t0 >= t1)) {
            DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
            return;
        }
        uint8_t c[4];
        for (int i = 0; i < 4; i++) {
            uint32_t v = bits(b, 64 + i * 16, 16);
            c[i] = (uint8_t)(v >> 8);
        }
        for (int y = 0; y < bh; y++)
            for (int x = 0; x < bw; x++) memcpy(out + y * stride + x * 4, c, 4);
        return;
    }
    // Modo de bloque: tamaño de la grilla de pesos, rango y plano doble.
    int gw, gh, r, alta, doble;
    if (modo & 3) {
        r = (int)((modo >> 4 & 1) | (modo & 3) << 1);
        alta = modo >> 9 & 1, doble = modo >> 10 & 1;
        int A = modo >> 5 & 3, B = modo >> 7 & 3;
        switch (modo >> 2 & 3) {
        case 0: gw = B + 4, gh = A + 2; break;
        case 1: gw = B + 8, gh = A + 2; break;
        case 2: gw = A + 2, gh = B + 8; break;
        default:
            if (modo >> 8 & 1) gw = (modo >> 7 & 1) + 2, gh = A + 2;
            else gw = A + 2, gh = (modo >> 7 & 1) + 6;
        }
    } else {
        r = (int)((modo >> 4 & 1) | (modo >> 2 & 3) << 1);
        alta = modo >> 9 & 1, doble = modo >> 10 & 1;
        int A = modo >> 5 & 3, B = modo >> 9 & 3;
        if (r == 0) {
            DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
            return;
        }
        switch (modo >> 7 & 3) {
        case 0: gw = 12, gh = A + 2; break;
        case 1: gw = A + 2, gh = 12; break;
        case 2: gw = A + 6, gh = B + 6, alta = 0, doble = 0; break;
        default:
            if ((modo >> 5 & 3) == 0) gw = 6, gh = 10;
            else if ((modo >> 5 & 3) == 1) gw = 10, gh = 6;
            else {
                DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
                return;
            }
        }
    }
    if (r < 2) {
        DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
        return;
    }
    int qw = (r - 2) + (alta ? 6 : 0); // índice en `rangos` del peso: 2,3,4,5,6,8 y 10,12,16,20,24,32 valores
    static const int qpeso[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    qw = qpeso[qw];
    int npesos = gw * gh * (doble ? 2 : 1);
    int wbits = ise_bits(npesos, qw);
    if (gw > bw || gh > bh || npesos > 64 || wbits < 24 || wbits > 96) {
        DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
        return;
    }
    DBG("modo 0x%x: grilla %dx%d, rango %d (alta %d), doble %d\n", modo, gw, gh, r, alta, doble);
    int nparts = (int)bits(b, 11, 2) + 1;
    if (doble && nparts == 4) {
        DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
        return;
    }
    // Modos de color de cada partición.
    int cem[4], semilla = 0, pos_color, extra = 0;
    if (nparts == 1) {
        cem[0] = (int)bits(b, 13, 4);
        pos_color = 17;
    } else {
        semilla = (int)bits(b, 13, 10);
        uint32_t campo = bits(b, 23, 6), sel = campo & 3;
        if (sel == 0) {
            for (int i = 0; i < nparts; i++) cem[i] = (int)(campo >> 2);
        } else {
            extra = 3 * nparts - 4;
            uint32_t todo = (campo >> 2) | bits(b, 128 - wbits - extra, extra) << 4;
            for (int i = 0; i < nparts; i++) {
                int c = todo >> i & 1, m = todo >> (nparts + 2 * i) & 3;
                cem[i] = (int)((sel - 1 + c) * 4 + m);
            }
        }
        pos_color = 29;
    }
    int ccs = doble ? (int)bits(b, 128 - wbits - extra - 2, 2) : -1;
    DBG("particiones %d, semilla %d, cem %d %d %d %d, color desde %d, extra %d, ccs %d\n", nparts, semilla, cem[0], cem[1], cem[2], cem[3], pos_color, extra, ccs);
    int nvalores = 0;
    for (int i = 0; i < nparts; i++) nvalores += (cem[i] / 4 + 1) * 2;
    int disponibles = 128 - wbits - extra - (doble ? 2 : 0) - pos_color;
    if (nvalores > 18 || disponibles < (13 * nvalores + 4) / 5) {
        DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
        return;
    }
    int qc = -1;
    for (int q = 20; q >= 0; q--)
        if (ise_bits(nvalores, q) <= disponibles) {
            qc = q;
            break;
        }
    if (qc < 0) {
        DBG("error en la línea %d\n", __LINE__), error_bloque(out, bw, bh, stride);
        return;
    }
    int tq[64], m[64], v[18];
    ise(b, pos_color, nvalores, qc, tq, m);
    for (int i = 0; i < nvalores; i++) v[i] = color_unq(qc, tq[i], m[i]);
    DBG("rango de color %d (%d trits %d quints %d bits):", qc, rangos[qc].trits, rangos[qc].quints, rangos[qc].bits);
    for (int i = 0; i < nvalores; i++) DBG(" %d[%d,%d]", v[i], tq[i], m[i]);
    DBG("\n");
    int e0[4][4], e1[4][4], valido[4], k = 0;
    for (int i = 0; i < nparts; i++) {
        valido[i] = extremos(cem[i], v + k, e0[i], e1[i]);
        k += (cem[i] / 4 + 1) * 2;
    }
    // Pesos: se leen del final del bloque hacia atrás (el bloque invertido bit a bit).
    uint8_t inv[16];
    for (int i = 0; i < 16; i++) {
        uint8_t x = b[15 - i], y = 0;
        for (int j = 0; j < 8; j++) y |= (uint8_t)((x >> j & 1) << (7 - j));
        inv[i] = y;
    }
    int w[64];
    ise(inv, 0, npesos, qw, tq, m);
    for (int i = 0; i < npesos; i++) w[i] = weight_unq(qw, tq[i], m[i]);
    DBG("pesos (rango %d):", qw);
    for (int i = 0; i < npesos; i++) DBG(" %d", w[i]);
    DBG("\n");
    int ds = (1024 + bw / 2) / (bw - 1 ? bw - 1 : 1), dt = (1024 + bh / 2) / (bh - 1 ? bh - 1 : 1);
    int planos = doble ? 2 : 1, chico = bw * bh < 31;
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            // Peso de cada plano en este texel (interpolación bilineal de la grilla).
            int pw[2];
            int gs = (ds * x * (gw - 1) + 32) >> 6, gt = (dt * y * (gh - 1) + 32) >> 6;
            int js = gs >> 4, fs = gs & 15, jt = gt >> 4, ft = gt & 15;
            int w11 = (fs * ft + 8) >> 4, w10 = ft - w11, w01 = fs - w11, w00 = 16 - fs - ft + w11;
            for (int p = 0; p < planos; p++) {
                int i00 = js + jt * gw;
                int p00 = w[i00 * planos + p];
                int p01 = js + 1 < gw ? w[(i00 + 1) * planos + p] : 0;
                int p10 = jt + 1 < gh ? w[(i00 + gw) * planos + p] : 0;
                int p11 = js + 1 < gw && jt + 1 < gh ? w[(i00 + gw + 1) * planos + p] : 0;
                pw[p] = (p00 * w00 + p01 * w01 + p10 * w10 + p11 * w11 + 8) >> 4;
            }
            int part = particion(semilla, x, y, nparts, chico);
            uint8_t *o = out + y * stride + x * 4;
            if (!valido[part]) {
                o[0] = 255, o[1] = 0, o[2] = 255, o[3] = 255;
                continue;
            }
            for (int c = 0; c < 4; c++) {
                int peso = c == ccs ? pw[1] : pw[0];
                // En sRGB los extremos se expanden con 0x80 abajo y el resultado son los 8 bits altos; en
                // lineal se replican y el valor de 16 bits se redondea a 8 (como al convertir un UNORM16).
                int c0, c1;
                if (srgb) c0 = e0[part][c] << 8 | 0x80, c1 = e1[part][c] << 8 | 0x80;
                else c0 = e0[part][c] * 257, c1 = e1[part][c] * 257;
                int val = (c0 * (64 - peso) + c1 * peso + 32) >> 6;
                o[c] = (uint8_t)(val >> 8);
            }
        }
}

void nx_astc_imagen(const uint8_t *src, int w, int h, int bw, int bh, int srgb, uint8_t *dst) {
    int bx = (w + bw - 1) / bw, by = (h + bh - 1) / bh;
    uint8_t tmp[12 * 12 * 4];
    for (int j = 0; j < by; j++)
        for (int i = 0; i < bx; i++) {
            nx_astc_bloque(src + ((size_t)j * bx + i) * 16, bw, bh, srgb, tmp, bw * 4);
            for (int y = 0; y < bh && j * bh + y < h; y++) {
                int n = bw;
                if (i * bw + n > w) n = w - i * bw;
                memcpy(dst + ((size_t)(j * bh + y) * w + i * bw) * 4, tmp + y * bw * 4, (size_t)n * 4);
            }
        }
}
