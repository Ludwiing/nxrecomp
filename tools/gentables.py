#!/usr/bin/env python3
"""Genera runtime/nxtables.h: las tablas de RecipEstimate y RecipSqrtEstimate del manual de Arm."""


def recip_estimate(a):  # a en [256, 511]
    a = a * 2 + 1
    b = (1 << 19) // a
    return (b + 1) // 2


def rsqrt_estimate(a):  # a en [128, 511]
    if a < 256:
        a = a * 2 + 1
    else:
        a = (a >> 1) << 1
        a = (a + 1) * 2
    b = 512
    while a * (b + 1) * (b + 1) < (1 << 28):
        b += 1
    return (b + 1) // 2


def table(name, values):
    rows = [", ".join(str(v) for v in values[k:k + 16]) for k in range(0, len(values), 16)]
    return f"static const uint16_t {name}[{len(values)}] = {{\n    " + ",\n    ".join(rows) + "\n};\n"


out = ["// Generado por tools/gentables.py; no editar a mano.", "#pragma once", "#include <stdint.h>", ""]
out.append("// recip_est[a - 256] = RecipEstimate(a), a en [256, 511]")
out.append(table("recip_est", [recip_estimate(a) for a in range(256, 512)]))
out.append("// rsqrt_est[a - 128] = RecipSqrtEstimate(a), a en [128, 511]")
out.append(table("rsqrt_est", [rsqrt_estimate(a) for a in range(128, 512)]))
def aes_sbox():
    def mul(a, b):
        r = 0
        while b:
            if b & 1:
                r ^= a
            a = ((a << 1) ^ 0x1B) & 0xFF if a & 0x80 else a << 1
            b >>= 1
        return r
    inv = [0] * 256
    for x in range(1, 256):
        for y in range(1, 256):
            if mul(x, y) == 1:
                inv[x] = y
                break
    sbox = []
    for x in range(256):
        b = inv[x]
        s = b
        for k in range(1, 5):
            s ^= ((b << k) | (b >> (8 - k))) & 0xFF
        sbox.append(s ^ 0x63)
    isbox = [0] * 256
    for x, s in enumerate(sbox):
        isbox[s] = x
    return sbox, isbox


sbox, isbox = aes_sbox()
assert sbox[0] == 0x63 and sbox[0x53] == 0xED
out.append("// S-box de AES y su inversa")
out.append(table("aes_sbox", sbox).replace("uint16_t", "uint8_t"))
out.append(table("aes_isbox", isbox).replace("uint16_t", "uint8_t"))
open("runtime/nxtables.h", "w").write("\n".join(out))
print("runtime/nxtables.h generado")
