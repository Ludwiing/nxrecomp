#!/usr/bin/env python3
"""Comprueba que @wrapper conserva y emite el cuerpo SDK original bajo otro símbolo."""
import os
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
exefs = root / "build/multi_mod/exefs"
if not (exefs / "main").is_file():
    print("(falta build/multi_mod: corre make primero)")
    raise SystemExit(77)

with tempfile.TemporaryDirectory() as d:
    rule = pathlib.Path(d) / "wrap.txt"
    rule.write_text("sdk_add @test_sdk_wrapper\n")
    out = pathlib.Path(d) / "recomp"
    p = subprocess.run([sys.executable, str(root / "recomp/nxrecomp.py"), str(exefs), "-o", str(out),
                        "--sustituir", str(rule)], capture_output=True, text=True)
    if p.returncode:
        print(p.stdout[-1200:], p.stderr[-1200:])
        raise SystemExit(1)
    fuentes = "\n".join(x.read_text(errors="replace") for x in out.glob("*.c"))
    required = ("test_sdk_wrapper(c, f_" in fuentes and "_original);" in fuentes and
                "void f_" in fuentes and "_original(Ctx *c)" in fuentes)
    if not required:
        print("FAIL: falta el cuerpo original o el wrapper generado")
        raise SystemExit(1)
print("wrapped substitution emits both the SDK body and the runtime wrapper: PASS")
