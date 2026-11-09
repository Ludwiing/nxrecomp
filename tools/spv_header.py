"""Compila un compute shader de runtime/shaders/ con glslc y escribe runtime/nvn/<nombre>_spv.h.

    python3 tools/spv_header.py runtime/shaders/fg_motion.comp [...]
"""
import hashlib
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def generar(fuente):
    nombre = os.path.splitext(os.path.basename(fuente))[0]
    with tempfile.TemporaryDirectory() as d:
        spv = os.path.join(d, "s.spv")
        subprocess.run(["glslc", "-O", "--target-env=vulkan1.2", fuente, "-o", spv], check=True)
        datos = open(spv, "rb").read()
    palabras = [int.from_bytes(datos[i:i + 4], "little") for i in range(0, len(datos), 4)]
    sha = hashlib.sha256(open(fuente, "rb").read()).hexdigest()
    lineas = [f"// Generado por tools/spv_header.py desde runtime/shaders/{os.path.basename(fuente)}; no editar.",
              f"// sha256 de la fuente {sha}", "#pragma once", "#include <stdint.h>",
              f"static const uint32_t {nombre}_spv[] = {{"]
    for i in range(0, len(palabras), 8):
        lineas.append("    " + ", ".join(f"0x{w:08x}" for w in palabras[i:i + 8]) + ",")
    lineas.append("};")
    salida = os.path.join(ROOT, "runtime", "nvn", f"{nombre}_spv.h")
    with open(salida, "w") as f:
        f.write("\n".join(lineas) + "\n")
    print(f"{salida}: {len(palabras)} palabras")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for a in sys.argv[1:]:
        generar(a)
