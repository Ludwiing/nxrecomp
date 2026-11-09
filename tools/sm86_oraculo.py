"""Oráculo de instrucciones SM86 (Ampere, la GPU de Switch 2) con nvdisasm del kit CUDA (sudo pacman -S cuda).

Solo para desarrollar y probar el traductor de shaders de Switch 2: nxrecomp no lleva nada de CUDA y el juego no lo
necesita. nvdisasm se usa como está pensado, para desensamblar instrucciones (las nuestras, armadas a mano).

    python3 tools/sm86_oraculo.py <lo hex> <hi hex>        una instrucción de 16 bytes (las dos mitades de 64 bits)
    python3 tools/sm86_oraculo.py --codigos [archivo]      qué códigos de operación (bits 0-11) existen
"""
import concurrent.futures as cf
import os
import shutil
import struct
import subprocess
import sys
import tempfile

NVDISASM = shutil.which("nvdisasm") or "/opt/cuda/bin/nvdisasm"
# Partes altas (bits 64-127: control de planificación y campos altos) con las que la mayoría de las clases decodifica
ALTAS = [0x0, 0x000fc00000000000, 0x000fe40000000f00, 0x000fe20000000000, 0x0000000000000f00, 0x000fea0003800000,
         0x000ff00000000000,
         # conversiones (F2I, F2F, FRND, I2F de 64 bits): sus formatos van en estos bits altos
         0x101000, 0x201000, 0x300000, 0x301400, 0x1c00]


def disponible():
    return os.access(NVDISASM, os.X_OK)


def desensamblar(lo, hi):
    """El texto de la instrucción (sin ';'), o None si nvdisasm dice que no es válida."""
    fd, ruta = tempfile.mkstemp(suffix=".bin")
    try:
        os.write(fd, struct.pack("<QQ", lo, hi))
        os.close(fd)
        r = subprocess.run([NVDISASM, "-b", "SM86", ruta], capture_output=True, text=True)
        for linea in r.stdout.splitlines():
            linea = linea.strip()
            if linea.startswith("/*0000*/"):
                return linea[len("/*0000*/"):].split(";")[0].strip()
        return None
    finally:
        os.remove(ruta)


def primera_valida(lo):
    for hi in ALTAS:
        t = desensamblar(lo, hi)
        if t:
            return hi, t
    return None, None


def codigos(hilos=16):
    """{código: (hi, texto)} de los códigos de operación que nvdisasm reconoce (predicado PT, demás bits en 0)."""
    with cf.ThreadPoolExecutor(hilos) as ex:
        res = dict(zip(range(4096), ex.map(lambda op: primera_valida(op | 0x7000), range(4096))))
    return {op: r for op, r in res.items() if r[1]}


def main():
    a = sys.argv[1:]
    if not disponible():
        sys.exit("falta nvdisasm: sudo pacman -S cuda")
    if a[:1] == ["--codigos"]:
        cs = codigos()
        texto = "\n".join(f"{op:#05x} {hi:#018x} {t}" for op, (hi, t) in sorted(cs.items()))
        if len(a) > 1:
            open(a[1], "w").write(texto + "\n")
        else:
            print(texto)
        print(f"{len(cs)} códigos válidos", file=sys.stderr)
    elif len(a) == 2:
        print(desensamblar(int(a[0], 16), int(a[1], 16)) or "(no válida)")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
