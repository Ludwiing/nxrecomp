#!/usr/bin/env python3
"""recomp/program.py thread_hook_gots: las entradas de la GOT de los hooks de hilo de nnmusl salen de la imagen,
no de direcciones fijas de un juego. El código AArch64 lo ensamblan y enlazan clang y ld.lld (oráculo independiente
de las codificaciones de ADRP y LDR); lo esperado son las direcciones de los símbolos según llvm-nm.

También comprueba que el runtime no lea direcciones fijas del invitado (ld32/ld64 de una constante): con ellas
el manejador de fallos de nxrt.c fallaba otra vez en cualquier título que no fuera el build de OoT del que salían."""
import glob
import os
import re
import subprocess
import sys
import tempfile
from types import SimpleNamespace

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "recomp"))
from program import Program  # noqa: E402

BASE, DATOS = 0x8000000, 0x8100000
fallos = 0


def mal(msg):
    global fallos
    fallos += 1
    print("MAL: " + msg)


ENTRADA = """
    mrs x1, tpidrro_el0
    add x1, x1, #0x1f8
    str x0, [x1]
    stp x29, x30, [sp, #-16]!
    adrp x8, {ini}
    ldr x8, [x8, :lo12:{ini}]
    mov x0, x19
    adrp x9, {fin}
    ldr x9, [x9, :lo12:{fin}]
    ret
"""
SIN_FIRMA = """
    mrs x2, tpidrro_el0
    adrp x8, got_ini
    ldr x8, [x8, :lo12:got_ini]
    adrp x9, got_fin
    ldr x9, [x9, :lo12:got_fin]
    ret
"""


def armar(tmp, nombre, cuerpo):
    s = os.path.join(tmp, nombre + ".s")
    with open(s, "w") as f:
        f.write(".text\n.globl _start\n_start:\n    nop\n" + cuerpo +
                "\n.data\n.balign 4096\nrelleno: .skip 0x30\n"
                "got_ini: .quad 0\ngot_fin: .quad 0\ngot_otro: .quad 0\n")
    o, elf, binario = (os.path.join(tmp, nombre + e) for e in (".o", ".elf", ".bin"))
    subprocess.run(["clang", "--target=aarch64-none-elf", "-c", s, "-o", o], check=True)
    subprocess.run(["ld.lld", "-Ttext=0x%x" % BASE, "-Tdata=0x%x" % DATOS, o, "-o", elf], check=True)
    subprocess.run(["llvm-objcopy", "-O", "binary", elf, binario], check=True)
    simbolos = {}
    for linea in subprocess.run(["llvm-nm", elf], capture_output=True, text=True, check=True).stdout.splitlines():
        partes = linea.split()
        if len(partes) == 3:
            simbolos[partes[2]] = int(partes[0], 16)
    texto = subprocess.run(["llvm-readelf", "-S", elf], capture_output=True, text=True, check=True).stdout
    tam_texto = int(re.search(r"\.text\s+PROGBITS\s+\S+\s+\S+\s+(\S+)", texto).group(1), 16)
    datos = bytearray(open(binario, "rb").read())
    p = Program.__new__(Program)
    p.modules = [SimpleNamespace(name="main", base=BASE, img=SimpleNamespace(data=datos, text=(0, tam_texto)))]
    return p, simbolos


def main():
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as tmp:
        p, sim = armar(tmp, "una", ENTRADA.format(ini="got_ini", fin="got_fin"))
        par, avisos = p.thread_hook_gots()
        if par != (sim["got_ini"], sim["got_fin"]) or avisos:
            mal(f"una entrada de hilo: {par} {avisos}, se esperaba ({sim['got_ini']:#x}, {sim['got_fin']:#x})")

        # Dos entradas iguales (el SDK puede tener más de un trampolín): mismo resultado.
        p, sim = armar(tmp, "dos", ENTRADA.format(ini="got_ini", fin="got_fin") * 2)
        par, avisos = p.thread_hook_gots()
        if par != (sim["got_ini"], sim["got_fin"]) or avisos:
            mal(f"dos entradas iguales: {par} {avisos}")

        # Entradas que no coinciden: no se adivina, ningún hook y un aviso.
        p, sim = armar(tmp, "distintas", ENTRADA.format(ini="got_ini", fin="got_fin") +
                       ENTRADA.format(ini="got_ini", fin="got_otro"))
        par, avisos = p.thread_hook_gots()
        if par != (0, 0) or len(avisos) != 1:
            mal(f"entradas distintas: {par} {avisos}, se esperaba (0, 0) y un aviso")

        # Sin la firma de la entrada de hilo del SDK: ningún hook, aunque haya pares ADRP + LDR.
        p, sim = armar(tmp, "sin_firma", SIN_FIRMA)
        par, avisos = p.thread_hook_gots()
        if par != (0, 0) or avisos:
            mal(f"sin firma: {par} {avisos}, se esperaba (0, 0)")

    fijas = []
    for ruta in sorted(glob.glob(os.path.join(RAIZ, "runtime", "*.c"))):
        for n, linea in enumerate(open(ruta, errors="replace"), 1):
            if re.search(r"\bld(8|16|32|64)\(\s*0x[0-9a-fA-F]+", linea):
                fijas.append(f"{os.path.relpath(ruta, RAIZ)}:{n}")
    if fijas:
        mal("el runtime lee direcciones fijas del invitado: " + ", ".join(fijas))

    if not fallos:
        print("OK: hooks de hilo hallados en la imagen (una y dos entradas, distintas = ninguno, sin firma = ninguno); "
              "el runtime no lee direcciones fijas")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
