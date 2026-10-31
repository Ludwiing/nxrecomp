#!/usr/bin/env python3
"""Recompilador estático: convierte un NRO de Switch (código ARM64) en C para x86_64.

Pasos:
 1. Lee la cabecera NRO0 y sus segmentos (.text, .rodata, .data y el tamaño del .bss).
 2. Descubre las funciones: la entrada, los destinos de BL, y las direcciones de
    código que el programa guarda en datos o arma con ADR/ADRP+ADD (punteros a función).
 3. Traduce cada función a una función de C. Los saltos locales son `goto`, BL es una
    llamada de C, RET es `return` y BR/BLR pasan por nx_dispatch().
 4. Incrusta la imagen del módulo para que el runtime la cargue en la misma dirección.

El C resultante se compila con runtime/nxrt.c y queda un ejecutable nativo de Linux.
"""

import argparse
import bisect
import struct
import sys

import jumptable
from a64 import bits, decode

NRO_MAGIC = b"NRO0"


class Module:
    def __init__(self, data, base):
        if data[0x10:0x14] != NRO_MAGIC:
            sys.exit("no es un NRO (falta NRO0 en 0x10)")
        (self.size,) = struct.unpack_from("<I", data, 0x18)
        segs = struct.unpack_from("<6I", data, 0x20)
        self.text = (segs[0], segs[1])
        self.ro = (segs[2], segs[3])
        self.data_seg = (segs[4], segs[5])
        (self.bss,) = struct.unpack_from("<I", data, 0x38)
        self.image = data[: self.size]
        self.base = base
        self.text_lo = base + self.text[0]
        self.text_hi = base + self.text[0] + self.text[1]

    def in_text(self, a):
        return self.text_lo <= a < self.text_hi and a % 4 == 0

    def word(self, a):
        return struct.unpack_from("<I", self.image, a - self.base)[0]


def pointer_candidates(mod):
    """Direcciones de código guardadas en .rodata/.data (tablas de punteros a función, vtables)."""
    out = set()
    for off, size in (mod.ro, mod.data_seg):
        for p in range(off, off + size - 7, 8):
            (v,) = struct.unpack_from("<Q", mod.image, p)
            if mod.in_text(v):
                out.add(v)
    return out


def adr_candidates(mod):
    """Direcciones de código que se arman con ADR, o con ADRP seguido de ADD sobre el mismo registro."""
    out = set()
    for pc in range(mod.text_lo, mod.text_hi, 4):
        i = decode(pc, mod.word(pc))
        if i.ref is None:
            continue
        if i.adrp_reg is None:
            if mod.in_text(i.ref):
                out.add(i.ref)
            continue
        for k in range(1, 9):  # busca el ADD que completa la dirección
            q = pc + 4 * k
            if q >= mod.text_hi:
                break
            x = mod.word(q)
            if bits(x, 31, 22) == 0b1001000100 and bits(x, 9, 5) == i.adrp_reg:  # ADD Xd, Xn, #imm
                a = i.ref + bits(x, 21, 10)
                if mod.in_text(a):
                    out.add(a)
                break
    return out


def walk(mod, entry, entries, starts, on_call=None):
    """Recorre el código alcanzable desde `entry` sin salir de la función.

    `entries` son las entradas seguras (destinos de BL): un B a una de ellas es una
    llamada de cola y no se sigue. `starts` (ordenada) sirve para ubicar el inicio de
    la función que contiene un BR al buscar su tabla de salto. Devuelve {pc: Insn}.
    """
    body = {}
    stack = [entry]
    while stack:
        pc = stack.pop()
        if pc in body or not mod.in_text(pc):
            continue
        i = decode(pc, mod.word(pc))
        body[pc] = i
        k = i.kind
        if k == "bl":
            if on_call and mod.in_text(i.target):
                on_call(i.target)
            stack.append(pc + 4)
        elif k == "b":
            if i.target not in entries or i.target == entry:
                stack.append(i.target)
        elif k == "bcond":
            if i.target not in entries or i.target == entry:
                stack.append(i.target)
            stack.append(pc + 4)
        elif k in ("op", "blr"):
            stack.append(pc + 4)
        elif k == "br":
            k0 = bisect.bisect_right(starts, pc) - 1
            start = min(entry, starts[k0]) if k0 >= 0 else entry
            i.targets = jumptable.resolve(mod, pc, i.reg_num, start)
            stack.extend(t for t in i.targets if t not in entries or t == entry)
    return body


def discover(mod, entry):
    """Devuelve (entradas seguras, todas las funciones).

    Seguras: la entrada del módulo y los destinos de BL. Posibles: direcciones de código
    que aparecen en datos o con ADR (punteros a función, vtables). Las posibles también
    se compilan, para las llamadas indirectas, pero un salto directo a ellas sigue siendo
    local: puede ser solo una etiqueta, como la base de una tabla de salto.
    """
    strong = {entry}
    weak = pointer_candidates(mod) | adr_candidates(mod)
    work = [entry] + sorted(weak)
    done = set()
    while work:
        f = work.pop()
        if f in done:
            continue
        done.add(f)

        def found(t):
            if t not in strong:
                strong.add(t)
                work.append(t)

        walk(mod, f, set(), sorted(strong), found)
    return strong, strong | weak


def fname(a):
    return f"f_{a:x}"


def emit_function(mod, f, strong, out, stats):
    body = walk(mod, f, strong, sorted(strong))
    for i in body.values():
        if i.kind == "br":
            stats["br_tabla" if i.targets else "br_indirecto"].add(i.pc)
    out.append(f"static void {fname(f)}(Ctx *c) {{")
    order = sorted(body)
    if order[0] != f:
        out.append(f"    goto L_{f:x};")
    for idx, pc in enumerate(order):
        i = body[pc]
        nxt = order[idx + 1] if idx + 1 < len(order) else None
        out.append(f"L_{pc:x}: /* {i.word:08x} */")
        k = i.kind

        def jump(t):
            if t in strong and t != f:
                return f"{{ {fname(t)}(c); return; }}"
            if t in body:
                return f"goto L_{t:x};"
            return f'{{ nx_bad_jump(c, 0x{pc:x}ull, 0x{t:x}ull); return; }}'

        if k == "op":
            out.extend("    " + line for line in i.lines)
        elif k == "trap":
            out.extend("    " + line for line in i.lines)
            out.append("    return;")
            continue
        elif k == "b":
            out.append("    " + jump(i.target))
            continue
        elif k == "bl":
            out.append(f"    c->x[30] = 0x{pc + 4:x}ull;")
            if mod.in_text(i.target):
                out.append(f"    {fname(i.target)}(c);")
            else:
                out.append(f"    nx_dispatch(c, 0x{i.target:x}ull);")
        elif k == "bcond":
            out.append(f"    if ({i.cond}) {jump(i.target)}")
        elif k == "blr":
            out.append(f"    {{ uint64_t t = {i.reg}; c->x[30] = 0x{pc + 4:x}ull; nx_dispatch(c, t); }}")
        elif k == "br":
            if i.targets:  # tabla de salto resuelta: cada destino es una etiqueta de esta función
                out.append(f"    switch ({i.reg}) {{")
                for t in i.targets:
                    out.append(f"    case 0x{t:x}ull: {jump(t)}")
                out.append(f"    default: nx_dispatch(c, {i.reg}); return;")
                out.append("    }")
            else:
                out.append(f"    nx_dispatch(c, {i.reg}); return;")
            continue
        elif k == "ret":
            out.append("    return;")
            continue
        if nxt != pc + 4:  # la siguiente instrucción no está justo después: salto explícito
            out.append("    " + jump(pc + 4))
    out.append("}")
    out.append("")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("nro")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x08000000,
                    help="dirección donde se carga el módulo (por defecto 0x08000000)")
    args = ap.parse_args()

    mod = Module(open(args.nro, "rb").read(), args.base)
    entry = mod.text_lo
    strong, entries = discover(mod, entry)

    out = ['#include "nxrt.h"', ""]
    out.append(f"/* {args.nro}: {len(entries)} funciones, .text {mod.text[1]:#x} bytes */")
    out.append("static const unsigned char image[] = {")
    img = mod.image
    for p in range(0, len(img), 24):
        out.append("    " + ",".join(f"0x{b:02x}" for b in img[p:p + 24]) + ",")
    out.append("};")
    out.append(f"const NxImage nx_image = {{ 0x{mod.base:x}ull, sizeof image, image, 0x{mod.bss:x}ull, "
               f"0x{mod.base + mod.data_seg[0] + mod.data_seg[1]:x}ull }};")
    out.append("")
    for f in sorted(entries):
        out.append(f"static void {fname(f)}(Ctx *c);")
    out.append("")
    stats = {"br_tabla": set(), "br_indirecto": set()}
    for f in sorted(entries):
        emit_function(mod, f, strong, out, stats)

    out.append("void nx_dispatch(Ctx *c, uint64_t a) {")
    out.append("    switch (a) {")
    for f in sorted(entries):
        out.append(f"    case 0x{f:x}ull: {fname(f)}(c); return;")
    out.append("    default: nx_bad_jump(c, 0, a);")
    out.append("    }")
    out.append("}")
    out.append("")
    out.append(f"void nx_entry(Ctx *c) {{ {fname(entry)}(c); }}")

    with open(args.output, "w") as fh:
        fh.write("\n".join(out) + "\n")
    print(f"{args.output}: {len(entries)} funciones recompiladas desde {args.nro}; "
          f"{len(stats['br_tabla'])} tablas de salto resueltas, "
          f"{len(stats['br_indirecto'])} BR sin tabla (llamadas de cola por puntero)")


if __name__ == "__main__":
    main()
