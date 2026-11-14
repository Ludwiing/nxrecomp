#!/usr/bin/env python3
"""recomp/program.py rtld_ro_vars: las variables nn::ro::detail::g_p* que rtld rellena al enlazar salen de los símbolos
de cada módulo (globales o débiles, definidos), nunca de direcciones fijas de un juego; rtld no cuenta y los locales o
sin definir no se publican. El runtime las escribe con nx_rtld_publicar_ro (tests/ldr_ro_test.c)."""
import os
import sys
from types import SimpleNamespace

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "recomp"))
from program import Program, Symbol  # noqa: E402

GLOBAL, WEAK, LOCAL = 0x10, 0x20, 0x00
R = Program.RTLD_RO


def modulo(nombre, base, simbolos):
    return SimpleNamespace(name=nombre, base=base,
                           syms=[Symbol(n, v, info, 0, shndx) for n, v, info, shndx in simbolos])


def main():
    p = Program.__new__(Program)
    p.modules = [
        modulo("rtld", 0x8000000, [(R[4], 0x10, GLOBAL, 5)]),          # rtld no se escribe a sí mismo
        modulo("main", 0x8020000, [(R[0], 0x0, GLOBAL, 0),             # sin definir: no
                                   (R[1], 0x100, LOCAL, 5)]),          # local: no
        modulo("sdk", 0x8d90000, [(R[4], 0xe16520, GLOBAL, 7), (R[0], 0xe16500, WEAK, 7),
                                  (R[2], 0xe16528, GLOBAL, 7), ("otro", 0x8, GLOBAL, 7)]),
    ]
    obtenido = p.rtld_ro_vars()
    esperado = [(0, 0x8d90000 + 0xe16500), (2, 0x8d90000 + 0xe16528), (4, 0x8d90000 + 0xe16520)]
    if obtenido != esperado:
        print(f"MAL: rtld_ro_vars dio {[(k, hex(a)) for k, a in obtenido]}, se esperaba "
              f"{[(k, hex(a)) for k, a in esperado]}")
        return 1
    print("OK: variables de nn::ro que publica rtld halladas por símbolo (globales y débiles; sin rtld, locales ni "
          "indefinidos)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
