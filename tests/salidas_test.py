#!/usr/bin/env python3
"""Cada código de salida fijo del runtime (exit(N) / _exit(N) en runtime/*.c, sin runtime/nvn) tiene explicación en
app/nx2linux/motor.py SALIDAS, y significado_salida distingue señales (negativo de subprocess, 128+n de una
terminal). Código propio (MIT)."""
import glob
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "app"))
from nx2linux import motor  # noqa: E402

fallos = []
usados = {}
for ruta in sorted(glob.glob(os.path.join(RAIZ, "runtime", "*.c"))):
    for n, linea in enumerate(open(ruta, errors="replace"), 1):
        if linea.lstrip().startswith("//"):
            continue
        for m in re.finditer(r"\b_?exit\((\d+)\)", linea):
            usados.setdefault(int(m.group(1)), f"{os.path.basename(ruta)}:{n}")
for codigo, donde in sorted(usados.items()):
    if codigo not in motor.SALIDAS:
        fallos.append(f"el runtime termina con {codigo} ({donde}) y motor.SALIDAS no lo explica")
if not {1, 3}.issubset(usados):
    fallos.append(f"no se encontraron los exit(1)/exit(3) del runtime: {usados} (¿cambió el patrón?)")
for rc, texto in ((-11, "SIGSEGV"), (139, "SIGSEGV"), (3, "salto indirecto"), (1, "svcBreak"), (77, "no usa")):
    if texto not in motor.significado_salida(rc):
        fallos.append(f"significado_salida({rc}) = {motor.significado_salida(rc)!r}, falta {texto!r}")
for f in fallos:
    print("MAL: " + f)
if not fallos:
    print(f"OK: los códigos de salida del runtime ({', '.join(map(str, sorted(usados)))}) tienen significado en NX2Linux")
sys.exit(1 if fallos else 0)
