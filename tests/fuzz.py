#!/usr/bin/env python3
"""Prueba diferencial del traductor contra Unicorn (una CPU ARM64 de referencia).

Genera instrucciones A64 al azar dentro de una clase, se queda con las que el
traductor acepta, y ejecuta cada una en Unicorn y en su traducción a C con el mismo
estado inicial. Compara x0-x30, sp, NZCV, v0-v31 y la memoria de prueba.
Unicorn solo se usa aquí, para verificar; los ejecutables no lo incluyen.

    .venv/bin/python tests/fuzz.py --clase simd --n 20000
"""

import argparse
import os
import random
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "recomp"))

from a64 import bits, decode  # noqa: E402
from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs  # noqa: E402
from unicorn import UC_ARCH_ARM64, UC_MODE_ARM, Uc, UcError  # noqa: E402
from unicorn import arm64_const as A  # noqa: E402

CODE = 0x01000000
LIT_BASE = CODE - 0x100000  # LDR literal alcanza ±1 MiB desde la instrucción
LIT_SIZE = 0x201000
SCRATCH = 0x10000000
SCRATCH_SIZE = 0x10000
ST = struct.Struct("<98Q")  # x0-x30, sp, nzcv, v0-v31 (2 × u64), crc de la memoria

CLASES = {
    # (máscara, valor) sobre los bits 28:25 (op0) y el bit 26 para memoria SIMD
    "int": lambda w: bits(w, 28, 26) in (0b100, 0b101) or bits(w, 27, 25) == 0b101,
    "mem": lambda w: bits(w, 27, 27) == 1 and bits(w, 25, 25) == 0 and bits(w, 26, 26) == 0,
    "memv": lambda w: bits(w, 27, 27) == 1 and bits(w, 25, 25) == 0 and bits(w, 26, 26) == 1,
    "fp": lambda w: bits(w, 28, 24) in (0b11110, 0b11111) and bits(w, 30, 30) == 0,
    "simd": lambda w: bits(w, 27, 25) == 0b111 and not (bits(w, 28, 24) in (0b11110, 0b11111) and bits(w, 30, 30) == 0),
}

GPR_SPECIAL = [0, 1, 2, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0x8000000000000000, 0x7FFFFFFFFFFFFFFF,
               0xFFFFFFFFFFFFFFFF, 0x100000000, 0xFFFFFFFF80000000]
F32_SPECIAL = [0x00000000, 0x80000000, 0x3F800000, 0xBFC00000, 0x7F800000, 0xFF800000, 0x7FC00000,
               0x7FA00000, 0xFFC00001, 0x00000001, 0x807FFFFF, 0x7F7FFFFF, 0x4F000000, 0xCF000000,
               0x4B000000, 0x3F000000, 0x40490FDB, 0x5F000000, 0x3EFFFFFF]
F64_SPECIAL = [0, 0x8000000000000000, 0x3FF0000000000000, 0xBFF8000000000000, 0x7FF0000000000000,
               0xFFF0000000000000, 0x7FF8000000000000, 0x7FF4000000000000, 0x0000000000000001,
               0x41E0000000000000, 0xC3E0000000000000, 0x43E0000000000000, 0x4330000000000000,
               0x3FE0000000000000, 0x400921FB54442D18]


def rnd_gpr(r):
    k = r.random()
    if k < 0.5:
        return r.getrandbits(64)
    if k < 0.75:
        return r.randrange(-40, 40) & (2**64 - 1)
    return r.choice(GPR_SPECIAL)


def rnd_vec(r):
    k = r.random()
    if k < 0.4:
        return r.getrandbits(128)
    if k < 0.7:
        return sum((r.choice(F32_SPECIAL) if r.random() < 0.6 else r.getrandbits(32)) << (32 * i) for i in range(4))
    if k < 0.9:
        return sum((r.choice(F64_SPECIAL) if r.random() < 0.6 else r.getrandbits(64)) << (64 * i) for i in range(2))
    return sum(r.randrange(0, 256) << (8 * i) for i in range(16)) if r.random() < 0.5 else \
        sum((r.randrange(-3, 4) & 0xFFFF) << (16 * i) for i in range(8))


def accepted(word):
    i = decode(CODE, word)
    if i.kind != "op":
        return None
    text = " ".join(i.lines)
    if "nx_" in text:  # svc, registros de sistema, DC ZVA: dependen del entorno
        return None
    return i


def lit_pattern():
    # cada palabra alineada es distinta, así un desplazamiento mal calculado no pasa por casualidad
    n = LIT_SIZE // 4
    return struct.pack(f"<{n}I", *((k * 0x9E3779B1) & 0xFFFFFFFF for k in range(n)))


def pattern(t):
    return bytes(((k * 7 + t * 13) & 0xFF) for k in range(SCRATCH_SIZE))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clase", choices=sorted(CLASES) + ["todo"], default="todo")
    ap.add_argument("--n", type=int, default=5000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--mostrar", type=int, default=25)
    ap.add_argument("--palabra", type=lambda s: int(s, 16), action="append", help="probar solo esta instrucción")
    args = ap.parse_args()
    r = random.Random(args.seed)
    clases = list(CLASES) if args.clase == "todo" else [args.clase]

    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    try:
        uc.ctl_set_cpu_model(A.UC_CPU_ARM64_A72)  # ARMv8.0 como el Cortex-A57 de la Switch
    except Exception:
        pass
    uc.mem_map(LIT_BASE, LIT_SIZE)
    uc.mem_write(LIT_BASE, lit_pattern())
    uc.mem_map(SCRATCH, SCRATCH_SIZE)
    cs = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    pats = {}

    tests, rejected, tries = [], [], 0
    while len(tests) < args.n and tries < args.n * 400:
        tries += 1
        if args.palabra:
            if tries > len(args.palabra):
                break
            w = args.palabra[tries - 1]
        else:
            w = r.getrandbits(32)
            if not any(CLASES[k](w) for k in clases):
                continue
        ins = accepted(w)
        if ins is None:
            continue
        # LDR literal: los bits 9:5 y 20:16 son parte del inmediato, no registros base/índice
        literal = bits(w, 29, 27) == 0b011 and bits(w, 25, 24) == 0
        is_mem = bits(w, 27, 27) == 1 and bits(w, 25, 25) == 0 and not literal
        x = [rnd_gpr(r) for _ in range(31)]
        sp = r.getrandbits(64) & ~0xF
        if is_mem:
            rn, rm = bits(w, 9, 5), bits(w, 20, 16)
            if rn == rm and rn != 31:
                continue
            base = SCRATCH + 0x4000 + r.randrange(0, 0x100) * 16
            if rn == 31:
                sp = base
            else:
                x[rn] = base
            if rm != 31:
                x[rm] = r.randrange(0, 0x40)
        nzcv = r.getrandbits(4) << 28
        v = [rnd_vec(r) for _ in range(32)]
        t = len(tests)

        uc.mem_write(CODE, struct.pack("<I", w))
        if is_mem:
            p = pats.get(t & 0xFF) or pats.setdefault(t & 0xFF, pattern(t))
            uc.mem_write(SCRATCH, p)
        for k in range(31):
            uc.reg_write(getattr(A, f"UC_ARM64_REG_X{k}"), x[k])
        uc.reg_write(A.UC_ARM64_REG_SP, sp)
        uc.reg_write(A.UC_ARM64_REG_NZCV, nzcv)
        uc.reg_write(A.UC_ARM64_REG_FPCR, 0)
        for k in range(32):
            uc.reg_write(getattr(A, f"UC_ARM64_REG_Q{k}"), v[k])
        try:
            uc.emu_start(CODE, CODE + 4, count=1)
        except UcError as e:
            rejected.append((w, str(e)))
            continue
        fx = [uc.reg_read(getattr(A, f"UC_ARM64_REG_X{k}")) for k in range(31)]
        fsp = uc.reg_read(A.UC_ARM64_REG_SP)
        fnzcv = uc.reg_read(A.UC_ARM64_REG_NZCV) & 0xF0000000
        fv = [uc.reg_read(getattr(A, f"UC_ARM64_REG_Q{k}")) for k in range(32)]
        crc = zlib.crc32(bytes(uc.mem_read(SCRATCH, SCRATCH_SIZE))) if is_mem else None
        tests.append((w, ins, (x, sp, nzcv, v), (fx, fsp, fnzcv, fv, crc)))

    if not tests:
        sys.exit("no se generó ninguna prueba")

    bdir = os.path.join(ROOT, "build", "fuzz")
    os.makedirs(bdir, exist_ok=True)
    src = ['#include "nxrt.h"']
    for k, (w, ins, *_ ) in enumerate(tests):
        src.append(f"static void t{k}(Ctx *c) {{ /* {w:08x} */ {' '.join(ins.lines)} }}")
    src.append("typedef void (*TestFn)(Ctx *);")
    src.append("const TestFn nx_tests[] = {" + ",".join(f"t{k}" for k in range(len(tests))) + "};")
    src.append(f"const int nx_ntests = {len(tests)};")
    src.append("const uint32_t nx_words[] = {" + ",".join(f"0x{w:08x}u" for w, *_ in tests) + "};")
    open(os.path.join(bdir, "tests.c"), "w").write("\n".join(src) + "\n")
    exe = os.path.join(bdir, "driver")
    subprocess.run(["clang", "-O1", "-march=x86-64-v3", "-w", f"-I{ROOT}/runtime", os.path.join(bdir, "tests.c"),
                    os.path.join(HERE, "driver.c"), os.path.join(ROOT, "runtime", "nxrt.c"), "-lm", "-o", exe],
                   check=True)

    def pack(x, sp, nzcv, v, mem=0):
        flat = list(x) + [sp, nzcv]
        for q in v:
            flat += [q & (2**64 - 1), q >> 64]
        return ST.pack(*flat, mem)

    with open(os.path.join(bdir, "in.bin"), "wb") as fh:
        for _, _, (x, sp, nzcv, v), _ in tests:
            fh.write(pack(x, sp, nzcv, v))
    run = subprocess.run([exe, os.path.join(bdir, "in.bin"), os.path.join(bdir, "out.bin")],
                         capture_output=True, text=True)
    if run.returncode:
        msg = run.stderr.strip()
        if msg.startswith("FALLO"):
            k = int(msg.split()[2])
            w = tests[k][0]
            dis = next(cs.disasm(struct.pack("<I", w), CODE), None)
            sys.exit(f"la traducción de {w:08x} ({dis.mnemonic + ' ' + dis.op_str if dis else '?'}) "
                     f"accedió fuera de la memoria de prueba:\n  {' '.join(tests[k][1].lines)}")
        sys.exit(f"el driver falló: {msg}")
    out = open(os.path.join(bdir, "out.bin"), "rb").read()

    fails = []
    for k, (w, ins, init, (fx, fsp, fnzcv, fv, crc)) in enumerate(tests):
        got = ST.unpack_from(out, k * ST.size)
        gx, gsp, gnzcv = got[:31], got[31], got[32]
        gv = [got[33 + 2 * q] | got[34 + 2 * q] << 64 for q in range(32)]
        diffs = [f"x{q}: {gx[q]:#x} (Unicorn {fx[q]:#x})" for q in range(31) if gx[q] != fx[q]]
        if gsp != fsp:
            diffs.append(f"sp: {gsp:#x} (Unicorn {fsp:#x})")
        if gnzcv != fnzcv:
            diffs.append(f"nzcv: {gnzcv >> 28:04b} (Unicorn {fnzcv >> 28:04b})")
        diffs += [f"v{q}: {gv[q]:#034x} (Unicorn {fv[q]:#034x})" for q in range(32) if gv[q] != fv[q]]
        if crc is not None and got[97] != crc:
            diffs.append("memoria distinta")
        if diffs:
            fails.append((w, diffs))

    print(f"{len(tests)} instrucciones probadas, {len(fails)} diferencias, "
          f"{len(rejected)} que el traductor acepta y Unicorn rechaza")
    for w, diffs in fails[: args.mostrar]:
        dis = next(cs.disasm(struct.pack("<I", w), CODE), None)
        name = f"{dis.mnemonic} {dis.op_str}" if dis else "?"
        print(f"  {w:08x}  {name:40s} {'; '.join(diffs[:3])}")
    for w, e in rejected[: min(args.mostrar, 10)]:
        dis = next(cs.disasm(struct.pack("<I", w), CODE), None)
        print(f"  rechazada {w:08x}  {(dis.mnemonic + ' ' + dis.op_str) if dis else '?'}: {e}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
