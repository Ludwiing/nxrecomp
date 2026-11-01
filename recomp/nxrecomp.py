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
import os
import struct

import jumptable
from a64 import bits, decode
from program import IMPORT_BASE, load_program

LINES_PER_FILE = 150000
TRACE = False  # --traza: cada instrucción guarda su dirección en c->pc (para depurar)  # el C se reparte en varios archivos para compilarlo en paralelo


def pointer_candidates(prog):
    """Direcciones de código guardadas en .rodata/.data (vtables, tablas de punteros a función).
    Se leen después de aplicar las reubicaciones, así que valen las direcciones finales."""
    out = set()
    for lo, hi in prog.data_ranges:
        blob = prog.read(lo, hi - lo) if hi > lo else b""
        for p in range(0, len(blob) - 7, 8):
            (v,) = struct.unpack_from("<Q", blob, p)
            if prog.in_text(v):
                out.add(v)
    return out


def adr_candidates(prog):
    """Direcciones de código que se arman con ADR, o con ADRP seguido de ADD sobre el mismo registro."""
    out = set()
    for lo, hi in prog.text_ranges:
        for pc in range(lo, hi, 4):
            i = decode(pc, prog.word(pc))
            if i.ref is None:
                continue
            if i.adrp_reg is None:
                if prog.in_text(i.ref):
                    out.add(i.ref)
                continue
            for k in range(1, 9):  # busca el ADD que completa la dirección
                q = pc + 4 * k
                if q >= hi:
                    break
                x = prog.word(q)
                if bits(x, 31, 22) == 0b1001000100 and bits(x, 9, 5) == i.adrp_reg:  # ADD Xd, Xn, #imm
                    a = i.ref + bits(x, 21, 10)
                    if prog.in_text(a):
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


def discover(mod, roots):
    """Devuelve (entradas seguras, todas las funciones).

    Seguras: la entrada del módulo y los destinos de BL. Posibles: direcciones de código
    que aparecen en datos o con ADR (punteros a función, vtables). Las posibles también
    se compilan, para las llamadas indirectas, pero un salto directo a ellas sigue siendo
    local: puede ser solo una etiqueta, como la base de una tabla de salto.
    """
    strong = set(roots)
    weak = pointer_candidates(mod) | adr_candidates(mod)
    work = sorted(strong) + sorted(weak)
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
    out.append(f"void {fname(f)}(Ctx *c) {{")
    # Dirección de retorno con la que se entró: un RET con otra (código que escribe x30 a mano,
    # como el crt0 de libnx que pone exit en x30 y salta a main) continúa en esa dirección.
    out.append("    const uint64_t lr0 = c->x[30];")
    if TRACE:
        out.append(f"    nx_trace_func(c, 0x{f:x}ull);")
    order = sorted(body)
    if order[0] != f:
        out.append(f"    goto L_{f:x};")
    for idx, pc in enumerate(order):
        i = body[pc]
        nxt = order[idx + 1] if idx + 1 < len(order) else None
        out.append(f"L_{pc:x}: /* {i.word:08x} */")
        if TRACE:
            out.append(f"    c->pc = 0x{pc:x}ull;")
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
            out.append(f"    if ({i.reg} != lr0) nx_dispatch(c, {i.reg});")
            out.append("    return;")
            continue
        if nxt != pc + 4:  # la siguiente instrucción no está justo después: salto explícito
            out.append("    " + jump(pc + 4))
    out.append("}")
    out.append("")


def c_string(s):
    """Literal de C para una cadena cualquiera."""
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("modulos", nargs="+", help="un NRO, NSO sueltos, o la carpeta ExeFS de un juego")
    ap.add_argument("-o", "--output", required=True, help="carpeta donde se escribe el C")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x08000000,
                    help="dirección del primer módulo (por defecto 0x08000000)")
    ap.add_argument("--traza", action="store_true", help="guardar en c->pc la dirección de cada instrucción")
    args = ap.parse_args()
    global TRACE
    TRACE = args.traza

    prog = load_program(args.modulos, args.base)
    print("módulos:")
    print(prog.summary())
    if prog.npdm:
        print("npdm: " + ", ".join(f"{k} {v}" for k, v in prog.npdm.items()))
    boot = prog.boot()
    strong, entries = discover(prog, prog.entry_points() | set(boot["inits"]))

    os.makedirs(args.output, exist_ok=True)
    for old in os.listdir(args.output):
        if old.endswith(".c") or old.endswith(".h") or old.endswith(".bin"):
            os.remove(os.path.join(args.output, old))

    hdr = ['#pragma once', '#include "nxrt.h"', ""]
    hdr += [f"void {fname(f)}(Ctx *c);" for f in sorted(entries)]
    open(os.path.join(args.output, "nxgame.h"), "w").write("\n".join(hdr) + "\n")

    stats = {"br_tabla": set(), "br_indirecto": set()}
    part, buf, nfiles = 0, [], 0

    def flush():
        nonlocal part, buf, nfiles
        if buf:
            open(os.path.join(args.output, f"funciones_{part:03d}.c"), "w").write(
                '#include "nxgame.h"\n\n' + "\n".join(buf) + "\n")
            part += 1
            nfiles += 1
            buf = []

    for f in sorted(entries):
        emit_function(prog, f, strong, buf, stats)
        if len(buf) > LINES_PER_FILE:
            flush()
    flush()

    out = ['#include "nxgame.h"', ""]
    for k, m in enumerate(prog.modules):
        path = os.path.abspath(os.path.join(args.output, f"modulo_{k}_{m.name}.bin"))
        # Un NSO se entrega reubicado (nxrecomp hace de rtld); un NRO se reubica solo al arrancar,
        # así que va con sus bytes originales: aplicar dos veces RELR sumaría la base dos veces.
        open(path, "wb").write(m.img.original if m.img.kind == "nro" else m.img.data)
        asm = f'.section .rodata\n.balign 16\n.global nx_mod{k}\nnx_mod{k}:\n.incbin "{path}"\n.previous\n'
        out.append(f"__asm__({c_string(asm)});")
        out.append(f"extern const unsigned char nx_mod{k}[];")
    out.append("const NxModule nx_modules[] = {")
    for k, m in enumerate(prog.modules):
        out.append(f"    {{ {c_string(m.name)}, 0x{m.base:x}ull, 0x{m.img.size:x}ull, 0x{m.img.bss:x}ull, nx_mod{k} }},")
    out.append("};")
    out.append(f"const int nx_nmodules = {len(prog.modules)};")
    syms = sorted((a, n) for n, a in prog.symbols.items() if prog.in_text(a))
    out.append("const NxSymbol nx_symbols[] = {")
    out += [f"    {{ {c_string(n)}, 0x{a:x}ull }}," for a, n in syms] or ["    { 0, 0 },"]
    out.append("};")
    out.append(f"const int nx_nsymbols = {len(syms)};")
    out.append("const char *const nx_imports[] = {")
    out += [f"    {c_string(n)}," for n in prog.imports] or ["    0,"]
    out.append("};")
    out.append(f"const int nx_nimports = {len(prog.imports)};")
    out.append("const uint64_t nx_inits[] = {" + ", ".join(f"0x{a:x}ull" for a in boot["inits"]) + (" 0 " if not boot["inits"] else "") + "};")
    out.append(f"const int nx_ninits = {len(boot['inits'])};")
    out.append(f"const uint64_t nx_main_addr = 0x{boot['main'] or 0:x}ull;")
    out.append(f"const uint64_t nx_entry_addr = 0x{boot['entry']:x}ull;")
    stack = prog.npdm["pila"] if prog.npdm else 0x100000
    out.append(f"const uint64_t nx_main_stack = 0x{stack:x}ull;")
    out.append("")
    out.append("void nx_dispatch(Ctx *c, uint64_t a) {")
    out.append("    switch (a) {")
    out += [f"    case 0x{f:x}ull: {fname(f)}(c); return;" for f in sorted(entries)]
    out.append("    }")
    out.append(f"    if (a >= 0x{IMPORT_BASE:x}ull && a < 0x{IMPORT_BASE:x}ull + 16ull * nx_nimports && !(a & 15))")
    out.append(f"        nx_import_call(c, (int)((a - 0x{IMPORT_BASE:x}ull) / 16));")
    out.append("    else")
    out.append("        nx_bad_jump(c, 0, a);")
    out.append("}")
    open(os.path.join(args.output, "programa.c"), "w").write("\n".join(out) + "\n")

    print(f"{args.output}: {len(entries)} funciones en {nfiles} archivos; "
          f"{len(stats['br_tabla'])} tablas de salto resueltas, "
          f"{len(stats['br_indirecto'])} BR sin tabla (llamadas de cola por puntero); "
          f"arranque por {'nnMain' if boot['main'] else 'la entrada del primer módulo'}")


if __name__ == "__main__":
    main()
