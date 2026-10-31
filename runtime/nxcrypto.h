// Extensión criptográfica de ARMv8 (AES, SHA1, SHA256), según el pseudocódigo del manual.
#pragma once
#include <stdint.h>

#include "nxtables.h"

static inline uint32_t rol32(uint32_t x, int r) { return (x << r) | (x >> (32 - r)); }
static inline uint32_t rorc32(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
static inline uint32_t sha_choose(uint32_t x, uint32_t y, uint32_t z) { return ((y ^ z) & x) ^ z; }
static inline uint32_t sha_parity(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static inline uint32_t sha_majority(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | ((x | y) & z); }

// ---------- AES ----------
static inline void aes_round(uint8_t s[16], int inverse) {
    uint8_t t[16];
    for (int i = 0; i < 16; i++) t[i] = s[(i * (inverse ? 13 : 5)) % 16];
    for (int i = 0; i < 16; i++) s[i] = inverse ? aes_isbox[t[i]] : aes_sbox[t[i]];
}
static inline uint8_t xt(uint8_t a) { return (uint8_t)((a << 1) ^ (a & 0x80 ? 0x1B : 0)); }
static inline uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    while (b) {
        if (b & 1) r ^= a;
        a = xt(a);
        b >>= 1;
    }
    return r;
}
static inline void aes_mix(uint8_t s[16], int inverse) {
    for (int c = 0; c < 4; c++) {
        uint8_t *p = s + 4 * c, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        if (!inverse) {
            p[0] = gmul(a0, 2) ^ gmul(a1, 3) ^ a2 ^ a3;
            p[1] = a0 ^ gmul(a1, 2) ^ gmul(a2, 3) ^ a3;
            p[2] = a0 ^ a1 ^ gmul(a2, 2) ^ gmul(a3, 3);
            p[3] = gmul(a0, 3) ^ a1 ^ a2 ^ gmul(a3, 2);
        } else {
            p[0] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
            p[1] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
            p[2] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
            p[3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
        }
    }
}

// ---------- SHA1 ----------
// SHA1C / SHA1P / SHA1M: f = 0 choose, 1 parity, 2 majority
static inline void sha1_hash(uint32_t x[4], uint32_t y, const uint32_t w[4], int f) {
    for (int e = 0; e < 4; e++) {
        uint32_t t = f == 0 ? sha_choose(x[1], x[2], x[3]) : f == 1 ? sha_parity(x[1], x[2], x[3])
                                                                    : sha_majority(x[1], x[2], x[3]);
        y = y + rol32(x[0], 5) + t + w[e];
        x[1] = rol32(x[1], 30);
        uint32_t top = x[3]; // <Y, X> = ROL(Y:X, 32)
        x[3] = x[2], x[2] = x[1], x[1] = x[0], x[0] = y;
        y = top;
    }
}

// ---------- SHA256 ----------
static inline uint32_t sigma0(uint32_t x) { return rorc32(x, 2) ^ rorc32(x, 13) ^ rorc32(x, 22); }
static inline uint32_t sigma1(uint32_t x) { return rorc32(x, 6) ^ rorc32(x, 11) ^ rorc32(x, 25); }
static inline void sha256_hash(uint32_t x[4], uint32_t y[4], const uint32_t w[4]) {
    for (int e = 0; e < 4; e++) {
        uint32_t chs = sha_choose(y[0], y[1], y[2]);
        uint32_t maj = sha_majority(x[0], x[1], x[2]);
        uint32_t t = y[3] + sigma1(y[0]) + chs + w[e];
        x[3] = t + x[3];
        y[3] = t + sigma0(x[0]) + maj;
        uint32_t xt3 = x[3], yt3 = y[3]; // <Y, X> = ROL(Y:X, 32), 256 bits
        x[3] = x[2], x[2] = x[1], x[1] = x[0], x[0] = yt3;
        y[3] = y[2], y[2] = y[1], y[1] = y[0], y[0] = xt3;
    }
}
