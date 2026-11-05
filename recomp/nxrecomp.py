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
import multiprocessing
import os
import struct

import jumptable
from a64 import bits, decode
from program import IMPORT_BASE, load_program

LINES_PER_FILE = 150000
FUNCS_PER_BLOCK = 1000   # funciones por bloque de emisión (un archivo, o varios si pasa de LINES_PER_FILE líneas)
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
    # starts es sorted(strong) al empezar cada recorrido: se mantiene ordenada en lugar de ordenarla de nuevo en cada
    # función (con cientos de miles de funciones, eso era casi todo el tiempo del descubrimiento).
    starts = sorted(strong)
    done = set()
    while work:
        f = work.pop()
        if f in done:
            continue
        done.add(f)
        nuevas = []

        def found(t):
            if t not in strong:
                strong.add(t)
                work.append(t)
                nuevas.append(t)

        walk(mod, f, set(), starts, found)
        for t in nuevas:   # después del recorrido, como la copia que recibía walk antes
            bisect.insort(starts, t)
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
    out.append("    const uint64_t lr0 = nx_enter(c);")
    local_dispatch = any(i.kind == "br" for i in body.values())
    if local_dispatch:
        out.append("    uint64_t nx_local_target;")
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

        def tail_dispatch(reg):
            return f"{{ NxFn fn = nx_buscar({reg}); nx_tail(c, lr0); __attribute__((musttail)) return fn(c); }}"

        def jump(t):
            if t in body:
                return f"goto L_{t:x};"
            if t in strong and t != f:  # salto a otra función: llamada de cola garantizada, la pila del
                # host no crece aunque el juego encadene saltos sin volver (intérpretes, máquinas de estado)
                return f"{{ nx_tail(c, lr0); __attribute__((musttail)) return {fname(t)}(c); }}"
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
            # Algunas rutinas ensamblador reutilizan LR y vuelven con BR a una
            # copia de la dirección de retorno. Es un retorno al llamador C.
            out.append(f"    if ({i.reg} == lr0) return;")
            if i.targets:  # tabla de salto resuelta: cada destino es una etiqueta de esta función
                out.append(f"    switch ({i.reg}) {{")
                for t in i.targets:
                    out.append(f"    case 0x{t:x}ull: {jump(t)}")
                out.append(f"    default: nx_local_target = {i.reg}; goto nx_local_dispatch;")
                out.append("    }")
            else:
                out.append(f"    nx_local_target = {i.reg}; goto nx_local_dispatch;")
            continue
        elif k == "ret":  # un RET a otra dirección (el juego cambió x30): seguir allí, también de cola
            out.append(f"    if ({i.reg} != lr0) {tail_dispatch(i.reg)}")
            out.append("    return;")
            continue
        if nxt != pc + 4:  # la siguiente instrucción no está justo después: salto explícito
            out.append("    " + jump(pc + 4))
    if local_dispatch:
        out.append("nx_local_dispatch:")
        out.append("    switch (nx_local_target) {")
        for target in order:
            out.append(f"    case 0x{target:x}ull: goto L_{target:x};")
        out.append("    default: { NxFn fn = nx_buscar(nx_local_target); nx_tail(c, lr0); __attribute__((musttail)) return fn(c); }")
        out.append("    }")
    out.append("}")
    out.append("")


_EMITIR = None   # (prog, strong, sustituidas, bloques): lo heredan por fork los procesos de emitir_bloque


def emitir_bloque(k):
    """Escribe en memoria el C del bloque k: [(nombre de archivo, texto)], las estadísticas y cuántas funciones tiene."""
    prog, strong, sustituidas, bloques = _EMITIR
    stats = {"br_tabla": set(), "br_indirecto": set()}
    archivos, buf = [], []

    def flush():
        if buf:
            sufijo = f"_{len(archivos):03d}" if archivos else ""   # con ceros: el orden alfabético es el de las direcciones
            archivos.append((f"funciones_{k:04d}{sufijo}.c", '#include "nxgame.h"\n\n' + "\n".join(buf) + "\n"))
            buf.clear()

    for f in bloques[k]:
        if f in sustituidas:
            buf.append(f"void {sustituidas[f]}(Ctx *c);")
            # nx_enter consume un salto de cola pendiente: el juego suele llegar aquí con B/BR.
            buf.append(f"void {fname(f)}(Ctx *c) {{ nx_enter(c); {sustituidas[f]}(c); }}")
        else:
            emit_function(prog, f, strong, buf, stats)
        if len(buf) > LINES_PER_FILE:
            flush()
    flush()
    return archivos, stats, len(bloques[k])


def procesos_para_emitir(nbloques):
    """Un proceso por núcleo, mientras la memoria disponible alcance para que cada uno crezca como este."""
    cpus = os.cpu_count() or 1
    try:
        rss = int(next(l for l in open("/proc/self/status") if l.startswith("VmRSS:")).split()[1]) * 1024
        libre = int(next(l for l in open("/proc/meminfo") if l.startswith("MemAvailable:")).split()[1]) * 1024
        por_memoria = max(1, libre // max(rss // 2, 256 << 20))
    except (OSError, StopIteration, ValueError):
        por_memoria = cpus
    return max(1, min(cpus, por_memoria, nbloques))


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
    ap.add_argument("--pistas", help="archivo con direcciones (una por línea) que también son funciones: las "
                    "que el runtime anotó en NX_PISTAS al saltar a código sin recompilar")
    ap.add_argument("--sustituir", help="archivo con líneas \"símbolo funcion_c\": la función del juego con ese "
                    "símbolo no se recompila; su cuerpo llama a void funcion_c(Ctx *) del runtime (HLE, p. ej. NVN)")
    ap.add_argument("--progreso", action="store_true", help="líneas \"progreso i/n\" al escribir las funciones (para la app)")
    ap.add_argument("--hilos", type=int, default=0, help="procesos que escriben el C (por defecto, según núcleos y memoria)")
    args = ap.parse_args()
    global TRACE
    TRACE = args.traza

    prog = load_program(args.modulos, args.base)
    print("módulos:")
    print(prog.summary())
    if prog.npdm:
        print("npdm: " + ", ".join(f"{k} {v}" for k, v in prog.npdm.items()))
    boot = prog.boot()
    pistas = set()
    if args.pistas and os.path.exists(args.pistas):
        for linea in open(args.pistas):
            try:
                a = int(linea.split()[0], 0)
            except (ValueError, IndexError):
                continue
            if prog.in_text(a) and not a & 3:
                pistas.add(a)
        print(f"pistas: {len(pistas)} direcciones extra")
    sustituidas = {}   # dirección -> función C del runtime que la reemplaza
    if args.sustituir and os.path.exists(args.sustituir):
        for linea in open(args.sustituir):
            campos = linea.split("#")[0].split()
            if len(campos) != 2:
                continue
            a = prog.symbols.get(campos[0])
            if a is None or not prog.in_text(a):
                print(f"sustituir: el símbolo {campos[0]} no existe en el juego (se ignora)")
                continue
            sustituidas[a] = campos[1]
            pistas.add(a)
        print(f"sustituir: {len(sustituidas)} funciones del juego reemplazadas por el runtime")
    strong, entries = discover(prog, prog.entry_points() | set(boot["inits"]) | pistas)

    os.makedirs(args.output, exist_ok=True)
    escritos = set()

    def escribir(nombre, texto, modo="w"):
        # Solo se reescribe lo que cambió: compilar.py reutiliza el objeto de un archivo con la fecha anterior.
        escritos.add(nombre)
        ruta = os.path.join(args.output, nombre)
        try:
            with open(ruta, "rb" if modo == "wb" else "r") as f:
                if f.read() == texto:
                    return
        except OSError:
            pass
        with open(ruta, modo) as f:
            f.write(texto)

    hdr = ['#pragma once', '#include "nxrt.h"', ""]
    hdr += [f"void {fname(f)}(Ctx *c);" for f in sorted(entries)]
    escribir("nxgame.h", "\n".join(hdr) + "\n")

    # Los bloques tienen un número fijo de funciones contiguas, así la salida no depende de cuántos procesos la escriben.
    global _EMITIR
    orden = sorted(entries)
    bloques = [orden[i:i + FUNCS_PER_BLOCK] for i in range(0, len(orden), FUNCS_PER_BLOCK)]
    _EMITIR = (prog, strong, sustituidas, bloques)
    hilos = args.hilos or procesos_para_emitir(len(bloques))
    stats = {"br_tabla": set(), "br_indirecto": set()}
    hechas = nfiles = 0
    with multiprocessing.get_context("fork").Pool(hilos) as pool:
        for archivos, st, n in pool.imap_unordered(emitir_bloque, range(len(bloques))):
            for nombre, texto in archivos:
                escribir(nombre, texto)
            nfiles += len(archivos)
            for k in stats:
                stats[k] |= st[k]
            hechas += n
            if args.progreso:
                print(f"progreso {hechas}/{len(entries)}", flush=True)
    print(f"emisión: {len(bloques)} bloques en {hilos} procesos")

    out = ['#include "nxgame.h"', ""]
    for k, m in enumerate(prog.modules):
        path = os.path.abspath(os.path.join(args.output, f"modulo_{k}_{m.name}.bin"))
        # Un NSO se entrega reubicado (nxrecomp hace de rtld); un NRO se reubica solo al arrancar,
        # así que va con sus bytes originales: aplicar dos veces RELR sumaría la base dos veces.
        escribir(os.path.basename(path), m.img.original if m.img.kind == "nro" else m.img.data, "wb")
        asm = f'.section .rodata\n.balign 16\n.global nx_mod{k}\nnx_mod{k}:\n.incbin "{path}"\n.previous\n'
        out.append(f"__asm__({c_string(asm)});")
        out.append(f"extern const unsigned char nx_mod{k}[];")
    romfs = next((getattr(m.img, "romfs", b"") for m in prog.modules if getattr(m.img, "romfs", b"")), b"")
    if romfs:
        path = os.path.abspath(os.path.join(args.output, "romfs.bin"))
        escribir("romfs.bin", romfs, "wb")
        asm = f'.section .rodata\n.balign 16\n.global nx_romfs_data\nnx_romfs_data:\n.incbin "{path}"\n.previous\n'
        out.append(f"__asm__({c_string(asm)});")
        out.append("extern const unsigned char nx_romfs_data[];")
    else:
        out.append("const unsigned char nx_romfs_data[1];")
    out.append(f"const uint64_t nx_romfs_size = {len(romfs)};")
    out.append("const NxModule nx_modules[] = {")
    for k, m in enumerate(prog.modules):
        out.append(f"    {{ {c_string(m.name)}, 0x{m.base:x}ull, 0x{m.img.size:x}ull, 0x{m.img.bss:x}ull, nx_mod{k}, "
                   f"0x{m.img.ro[0]:x}ull, 0x{m.img.rw[0]:x}ull }},")
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
    np = prog.npdm or {}
    out.append(f"const uint64_t nx_system_resource = 0x{np.get('recurso del sistema', 0):x}ull;")
    out.append(f"const uint64_t nx_program_id = 0x{np.get('programa', 0):x}ull;")
    out.append(f"const int nx_main_priority = {np.get('prioridad', 44)}, nx_main_core = {np.get('núcleo', 0)};")
    out.append("")
    out.append("static __thread uint64_t destino_malo;")
    out.append("static void salto_malo(Ctx *c) { nx_bad_jump(c, 0, destino_malo); }")
    out.append("static void importacion(Ctx *c) {")
    out.append(f"    nx_import_call(c, (int)((destino_malo - 0x{IMPORT_BASE:x}ull) / 16));")
    out.append("}")
    out.append("NxFn nx_buscar(uint64_t a) {")
    out.append("    switch (a) {")
    out += [f"    case 0x{f:x}ull: return {fname(f)};" for f in sorted(entries)]
    out.append("    }")
    out.append("    destino_malo = a;")
    out.append(f"    if (a >= 0x{IMPORT_BASE:x}ull && a < 0x{IMPORT_BASE:x}ull + 16ull * nx_nimports && !(a & 15))")
    out.append("        return importacion;")
    out.append("    return salto_malo;")
    out.append("}")
    out.append("void nx_dispatch(Ctx *c, uint64_t a) { nx_buscar(a)(c); }")
    escribir("programa.c", "\n".join(out) + "\n")
    for viejo in os.listdir(args.output):   # lo de una emisión anterior que ya no corresponde
        if viejo.endswith((".c", ".h", ".bin")) and viejo not in escritos:
            os.remove(os.path.join(args.output, viejo))

    print(f"{args.output}: {len(entries)} funciones en {nfiles} archivos; "
          f"{len(stats['br_tabla'])} tablas de salto resueltas, "
          f"{len(stats['br_indirecto'])} BR sin tabla (llamadas de cola por puntero); "
          f"arranque por {'nnMain' if boot['main'] else 'la entrada del primer módulo'}")


if __name__ == "__main__":
    main()
