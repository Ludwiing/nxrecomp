#!/usr/bin/env python3
"""Evita que el linker del juego omita fuentes del runtime declaradas en Makefile."""
import ast
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
makefile = (root / "Makefile").read_text()
compiler = (root / "tools/compilar.py").read_text()

match = re.search(r"^RUNTIME\s*:=\s*(.+)$", makefile, re.MULTILINE)
if not match:
    sys.exit("FAIL: Makefile no declara RUNTIME")
make_sources = {pathlib.Path(p).name for p in match.group(1).split() if p.endswith(".c")}

tree = ast.parse(compiler)
runtime_sources = None
for node in ast.walk(tree):
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "runtime" for t in node.targets):
        runtime_sources = ast.literal_eval(node.value)
        break
if runtime_sources is None:
    sys.exit("FAIL: tools/compilar.py no tiene la lista runtime esperada")
compiler_sources = set(runtime_sources)

if make_sources != compiler_sources:
    missing = sorted(make_sources - compiler_sources)
    extra = sorted(compiler_sources - make_sources)
    sys.exit(f"FAIL: runtime Makefile/compilar.py divergen; faltan={missing}, sobran={extra}")
if any(not (root / "runtime" / name).is_file() for name in compiler_sources):
    sys.exit("FAIL: tools/compilar.py declara una fuente runtime inexistente")
print(f"OK: compilar.py enlaza las {len(compiler_sources)} fuentes runtime declaradas en Makefile")
