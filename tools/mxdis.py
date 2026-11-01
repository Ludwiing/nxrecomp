#!/usr/bin/env python3
"""Desensambla código de la GPU Maxwell (instrucciones de 64 bits; cada cuarta palabra es de control
del planificador). Uso: mxdis.py código.bin [--sph] (con --sph salta los 0x50 bytes de cabecera)."""
import json
import os
import struct
import sys

isa = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mxisa.json')))
tabla = [None] * (1 << 14)
xbits = [99] * (1 << 14)
for p in isa['patrones']:
    top = p['bits'][:14]
    xs = [13 - i for i, c in enumerate(top) if c == 'x']
    base = int(top.replace('x', '0'), 2)
    for k in range(1 << len(xs)):
        v = base
        for j, pos in enumerate(xs):
            v |= ((k >> j) & 1) << pos
        if len(xs) < xbits[v]:
            tabla[v], xbits[v] = p, len(xs)


def decodifica(o):
    p = tabla[o >> 50]
    if p is None:
        return 'invalid', {}
    campos = {}
    for c in isa['campos'].get(p['op'], []):
        campos[c['nombre']] = eval(c['expr'].replace('!=', '!='), {'o': o})
    return p['op'], campos


def texto(o):
    op, campos = decodifica(o)
    pred = ''
    if 'Pred' in campos and campos['Pred'] != 7:
        pred = f"@{'!' if campos.get('PredInv') else ''}P{campos['Pred']} "
    resto = ' '.join(f'{k}={int(v) if not isinstance(v, bool) else int(v)}' for k, v in campos.items()
                     if k not in ('Pred', 'PredInv') and v)
    return f'{pred}{op} {resto}'


if __name__ == '__main__':
    datos = open(sys.argv[1], 'rb').read()
    if '--sph' in sys.argv:
        datos = datos[0x50:]
    for i in range(0, len(datos) - 7, 8):
        if (i // 8) % 4 == 0:
            continue  # palabra de control
        o, = struct.unpack_from('<Q', datos, i)
        print(f'{i:05x}: {o:016x}  {texto(o)}')
