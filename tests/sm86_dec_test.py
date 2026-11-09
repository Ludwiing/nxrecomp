"""Compara el decodificador SM86 en C (runtime/nvn/sm86_dec.c) con el de Python (tools/sm86.py decodificar()).

    python3 tests/sm86_dec_test.py <build/sm86_dec_test> [n=24000] [build/sm86_spec.json]

Arma n instrucciones al azar recorriendo todos los códigos de la especificación (bits de campos, modificadores y
variantes al azar, guarda al azar) más algunas del todo al azar (códigos desconocidos), las decodifica con Python,
escribe los casos en build/sm86_dec_casos.txt y corre el binario de C, que compara campo por campo.
Sale con 77 si falta build/sm86_spec.json; con 1 si las tablas en C son de otra especificación (hay que
regenerarlas con tools/sm86_c.py) o si hay diferencias.
"""
import hashlib
import json
import os
import random
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tools"))
import sm86   # noqa: E402


def casos(spec, n, semilla=86):
    rnd = random.Random(semilla)
    ops = sorted(spec, key=lambda x: int(x, 16))
    for i in range(n):
        if i % 16 == 15:   # del todo al azar: casi siempre un código desconocido
            yield rnd.getrandbits(64), rnd.getrandbits(64)
            continue
        op_txt = ops[i % len(ops)]
        s = spec[op_txt]
        lo, hi = int(op_txt, 16) | (rnd.randrange(16) << 12), int(s["hi"], 16)
        usados = {b for bits in s["campos"].values() for b in bits.values()}
        usados |= {b for t in s["modificadores"] for b in t["bits"]}
        for v in s["variantes"]:
            usados |= {b for bits in v["campos"].values() for b in bits.values()}
        p = rnd.choice((0.1, 0.5, 0.9))
        for b in usados:
            if rnd.random() < p:
                if b < 64:
                    lo ^= 1 << b
                else:
                    hi ^= 1 << (b - 64)
        if s["variantes"] and rnd.random() < 0.3:   # encender a propósito el conjunto de una variante
            for b in rnd.choice(s["variantes"])["encendidos"]:
                if b < 64:
                    lo |= 1 << b
                else:
                    hi |= 1 << (b - 64)
        if rnd.random() < 0.2:   # bits fuera de los campos: no deben cambiar nada
            lo ^= rnd.getrandbits(64) & ~0xFFFF
            hi ^= rnd.getrandbits(64)
        yield lo, hi


def linea(lo, hi, d):
    if d is None:
        return f"{lo:016x}\t{hi:016x}\t-1\n"
    if d.get("invalida"):
        return f"{lo:016x}\t{hi:016x}\t-2\n"
    campos = " ".join(f"{c}={v:x}" for c, v in d["campos"].items())
    return f"{lo:016x}\t{hi:016x}\t0\t{d['nombre']}\t{d['guarda']}\t{d['base']}\t{campos}\n"


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    binario = a[0]
    n = int(a[1]) if len(a) > 1 else 24000
    spec_ruta = a[2] if len(a) > 2 else os.path.join(RAIZ, "build/sm86_spec.json")
    if not os.path.exists(spec_ruta):
        print(f"falta {spec_ruta}: no se prueba el decodificador SM86 en C")
        sys.exit(77)
    crudo = open(spec_ruta, "rb").read()
    sha = hashlib.sha256(crudo).hexdigest()
    tablas = open(os.path.join(RAIZ, "runtime/nvn/sm86_tablas.h")).read()
    m = re.search(r'#define SM86_SPEC_SHA256 "([0-9a-f]+)"', tablas)
    if not m or m.group(1) != sha:
        print("runtime/nvn/sm86_tablas.h es de otra especificación: regenerar con python3 tools/sm86_c.py")
        sys.exit(1)
    spec = json.loads(crudo)
    salida = os.path.join(os.path.dirname(os.path.abspath(binario)), "sm86_dec_casos.txt")
    with open(salida, "w") as f:
        for lo, hi in casos(spec, n):
            f.write(linea(lo, hi, sm86.decodificar(lo, hi, spec)))
    r = subprocess.run([binario, salida])
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
