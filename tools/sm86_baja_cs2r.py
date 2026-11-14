"""Bajada SM86 -> Maxwell de CS2R cuando lee SRZ.

CS2R de 32 o 64 bits desde SRZ siempre produce cero. Se representa con uno o
dos MOV32I, preservando la guarda. Otras fuentes de sistema quedan rechazadas:
no se deben tratar como cero sin una traducción comprobada.

Código propio (MIT); lo carga tools/sm86_baja.py.
"""

from sm86_baja import RZ, SinRegla, _guarda, codificar


def baja_cs2r(d):
    lo, hi = d["lo"], d["hi"]
    raw = lo | (hi << 64)
    sr = (raw >> 72) & 0xFF
    if sr != 0xFF:  # SRZ
        raise SinRegla(f"CS2R desde SR {sr:#x}")

    rd = (lo >> 16) & 0xFF
    palabras = 2 if ((raw >> 80) & 1) else 1
    guarda = _guarda(d)
    return [codificar("Mov32i", Dest=rd + i, Imm32=0, **guarda)
            for i in range(palabras) if rd + i != RZ]


REGLAS = {"CS2R": baja_cs2r}
