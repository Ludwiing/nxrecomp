#!/usr/bin/env python3
"""Ensamblador mínimo de instrucciones Maxwell, con las mismas tablas que el traductor
(tools/mxisa.json): bits fijos del patrón y campos simples (desplazamiento y máscara).
Uso como módulo: ensamblar('Hadd2R', Dest=2, SrcA=0, SrcB=1, ...) → entero de 64 bits."""
import json
import os
import re

_isa = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mxisa.json')))
_patron = {p['op']: p['bits'] for p in _isa['patrones']}


def _campo(expr):
    m = re.fullmatch(r'\(\(\(o\) >> (\d+)\) & (0x[0-9A-Fa-f]+|\d+)\)', expr)
    if m:
        return int(m.group(1)), int(m.group(2), 0)
    m = re.fullmatch(r'\(\(o\) >> (\d+)\)', expr)  # (int)(o >> s): 32 bits
    if m:
        return int(m.group(1)), 0xFFFFFFFF
    m = re.fullmatch(r'\(\(o\) & (0x[0-9A-Fa-f]+|\d+)\)', expr)
    if m:
        return 0, int(m.group(1), 0)
    m = re.fullmatch(r'\(\(o\) & (0x[0-9A-Fa-f]+|\d+)\) != 0', expr)
    if m:
        bit = int(m.group(1), 0)
        return bit.bit_length() - 1, 1
    return None


def ensamblar(op, **campos):
    bits = _patron[op]
    o = int(bits.replace('x', '0'), 2)
    defs = {c['nombre']: c['expr'] for c in _isa['campos'][op]}
    campos.setdefault('Pred', 7)
    for nombre, valor in campos.items():
        if nombre not in defs:
            raise KeyError(f'{op} no tiene el campo {nombre}')
        sm = _campo(defs[nombre])
        if not sm:
            raise ValueError(f'{op}.{nombre}: campo compuesto ({defs[nombre]})')
        sh, mask = sm
        o &= ~(mask << sh)
        o |= (int(valor) & mask) << sh
    return o


def programa(insns):
    """Palabras de código con una palabra de control cada 3 instrucciones (vale 0: no se usa)."""
    out = []
    for i, x in enumerate(insns):
        if len(out) % 4 == 0:
            out.append(0)
        out.append(x)
    while len(out) % 4:
        out.append(ensamblar('Nop'))
    return out
