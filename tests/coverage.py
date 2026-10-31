#!/usr/bin/env python3
"""Cobertura: instrucciones válidas en un ARMv8.0 (Unicorn las ejecuta) que el traductor rechaza.

    .venv/bin/python tests/coverage.py --clase simd --n 200000
"""

import argparse
import collections
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "recomp"))
sys.path.insert(0, HERE)

from a64 import decode  # noqa: E402
from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs  # noqa: E402
from fuzz import CLASES, CODE, SCRATCH, SCRATCH_SIZE  # noqa: E402
from unicorn import UC_ARCH_ARM64, UC_MODE_ARM, Uc, UcError  # noqa: E402
from unicorn import arm64_const as A  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--clase", choices=sorted(CLASES), required=True)
ap.add_argument("--n", type=int, default=100000)
ap.add_argument("--seed", type=int, default=2)
args = ap.parse_args()

uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
uc.ctl_set_cpu_model(A.UC_CPU_ARM64_A72)
uc.mem_map(CODE, 0x1000)
uc.mem_map(SCRATCH, SCRATCH_SIZE)
cs = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
r = random.Random(args.seed)
missing = collections.Counter()
example = {}
total = 0
for _ in range(args.n):
    w = r.getrandbits(32)
    if not CLASES[args.clase](w):
        continue
    i = decode(CODE, w)
    if i.kind != "trap":
        continue
    for k in range(31):
        uc.reg_write(getattr(A, f"UC_ARM64_REG_X{k}"), SCRATCH + 0x4000)
    uc.reg_write(A.UC_ARM64_REG_SP, SCRATCH + 0x4000)
    uc.mem_write(CODE, struct.pack("<I", w))
    try:
        uc.emu_start(CODE, CODE + 4, count=1)
    except UcError:
        continue
    dis = next(cs.disasm(struct.pack("<I", w), CODE), None)
    name = dis.mnemonic if dis else "?"
    missing[name] += 1
    example.setdefault(name, (w, dis.op_str if dis else "", i.lines[0]))
    total += 1
print(f"{total} instrucciones válidas que el traductor rechaza")
for name, cnt in missing.most_common(60):
    w, ops, why = example[name]
    print(f"  {cnt:6d}  {name:10s} {w:08x} {ops:30s} {why.split(chr(34))[1] if chr(34) in why else ''}")
