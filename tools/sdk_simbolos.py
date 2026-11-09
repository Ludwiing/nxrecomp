"""Las funciones del SDK (nn::, nvn) que un juego trae en su ExeFS, agrupadas, y cuáles ya tienen sustitución.

    python3 tools/sdk_simbolos.py <carpeta ExeFS> [--sustituir archivo ...] [--todas]

Sirve para la fase HLE de Switch 2: muestra qué espacios de nombres usa el juego (nn::gs, nn::vi, nn::hid...) y qué
falta escribir en recomp/sustituciones-switch2.txt. Sin --todas lista solo las que no tienen sustitución.
"""
import argparse
import collections
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "recomp"))
from program import load_program  # noqa: E402
from nxrecomp import leer_sustituciones  # noqa: E402


def demangle(nombres):
    if not shutil.which("c++filt"):
        return dict(zip(nombres, nombres))
    r = subprocess.run(["c++filt"], input="\n".join(nombres), capture_output=True, text=True)
    return dict(zip(nombres, r.stdout.splitlines()))


def grupo(legible):
    """nn::gs::Algo(...) -> nn::gs; nvnDevice... -> nvn; lo demás -> (otros)."""
    if legible.startswith("nvn"):
        return "nvn"
    if legible.startswith("nn::"):
        partes = legible.split("(")[0].split("::")
        return "::".join(partes[:2])
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("exefs")
    ap.add_argument("--sustituir", action="append", default=[os.path.join(ROOT, "recomp", "sustituciones.txt"),
                                                            os.path.join(ROOT, "recomp", "sustituciones-switch2.txt")])
    ap.add_argument("--todas", action="store_true")
    a = ap.parse_args()
    prog = load_program([a.exefs], 0x08000000)
    funcs = {s: d for s, d in prog.symbols.items() if prog.in_text(d)}
    cubiertas, _ = leer_sustituciones(a.sustituir, prog.symbols, prog.in_text)
    legibles = demangle(sorted(funcs))
    grupos = collections.defaultdict(list)
    for sim, leg in legibles.items():
        g = grupo(leg)
        if g:
            grupos[g].append((leg, sim, funcs[sim] in cubiertas))
    total = sum(len(v) for v in grupos.values())
    hechas = sum(1 for v in grupos.values() for x in v if x[2])
    print(f"{total} funciones del SDK en {len(grupos)} grupos; {hechas} con sustitución")
    for g in sorted(grupos, key=lambda g: -len(grupos[g])):
        v = grupos[g]
        print(f"\n{g}: {len(v)} ({sum(1 for x in v if x[2])} con sustitución)")
        for leg, sim, ok in sorted(v):
            if a.todas or not ok:
                print(f"  {'✓' if ok else '·'} {leg}\n      {sim}")


if __name__ == "__main__":
    main()
