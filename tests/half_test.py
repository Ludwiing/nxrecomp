#!/usr/bin/env python3
"""Prueba de las instrucciones de media precisión (HADD2, HMUL2, HFMA2, HSET2, HSETP2), que uam no
genera: se ensamblan a mano (tools/mxasm.py), se traducen y ejecutan en la GPU con shader_test, y
el resultado se compara con numpy en float16.
Uso: half_test.py build/shader_test build/shaders/basico.dksh (de ahí se toman la cabecera y el SPH)."""
import os
import struct
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from mxasm import ensamblar, programa  # noqa: E402

prueba, base = sys.argv[1], sys.argv[2]
d = open(base, 'rb').read()
cab = struct.unpack_from('<6I', d)
prog = struct.unpack_from('<5I', d, cab[4])
inicio = cab[2] + prog[1]  # cabecera SPH del programa
tmp = os.path.join(os.path.dirname(os.path.abspath(prueba)), 'half_test.dksh')


def h2(a, b):  # par de half → uint32 (a en los bits bajos)
    return int(np.float16(a).view(np.uint16)) | int(np.float16(b).view(np.uint16)) << 16


def des(u):
    return np.uint16(u & 0xFFFF).view(np.float16), np.uint16(u >> 16).view(np.float16)


def correr(insns):
    codigo = programa(insns + [ensamblar('Exit', Ccc=15)])
    cuerpo = struct.pack(f'<{len(codigo)}Q', *codigo)
    datos = bytearray(d)
    fin = inicio + 0x50 + len(cuerpo)
    if fin > len(datos):
        datos += bytes(fin - len(datos))
    datos[inicio + 0x50:fin] = cuerpo
    open(tmp, 'wb').write(datos)
    r = subprocess.run([prueba, '--ejecutar', tmp], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-2000:])
        return None
    return [int(x, 16) for x in r.stdout.split()]


A, B, C = h2(1.5, -2.25), h2(0.375, 3.0), h2(-0.5, 0.125)
casos = []


def caso(nombre, insns, esperado):
    casos.append((nombre, insns, esperado))


carga = [ensamblar('Mov32i', Dest=4, Imm32=A, QuadMask=15), ensamblar('Mov32i', Dest=5, Imm32=B, QuadMask=15),
         ensamblar('Mov32i', Dest=6, Imm32=C, QuadMask=15)]
a, b, c = des(A), des(B), des(C)
f16 = np.float16
caso('HADD2', carga + [ensamblar('Hadd2R', Dest=0, SrcA=4, SrcB=5)], [h2(a[0] + b[0], a[1] + b[1])])
caso('HADD2 neg/abs', carga + [ensamblar('Hadd2R', Dest=0, SrcA=4, SrcB=5, NegA=1, AbsB=1)],
     [h2(-a[0] + abs(b[0]), -a[1] + abs(b[1]))])
caso('HADD2 H1H1 y F32 de salida', carga + [ensamblar('Hadd2R', Dest=0, SrcA=4, SrcB=5, ASwizzle=3, OFmt=1)],
     [int(np.float32(f16(a[1] + b[0])).view(np.uint32))])
caso('HMUL2', carga + [ensamblar('Hmul2R', Dest=0, SrcA=4, SrcB=5)], [h2(a[0] * b[0], a[1] * b[1])])
caso('HMUL2 sat', carga + [ensamblar('Hmul2R', Dest=0, SrcA=4, SrcB=5, Sat=1)],
     [h2(min(max(a[0] * b[0], 0), 1), min(max(a[1] * b[1], 0), 1))])
caso('HFMA2', carga + [ensamblar('Hfma2R', Dest=0, SrcA=4, SrcB=5, SrcC=6)],
     [h2(f16(np.float32(a[0]) * np.float32(b[0]) + np.float32(c[0])), f16(np.float32(a[1]) * np.float32(b[1]) + np.float32(c[1])))])
caso('HFMA2 negC, MrgH0', carga + [ensamblar('Mov32i', Dest=0, Imm32=h2(7, 9), QuadMask=15),
                                   ensamblar('Hfma2R', Dest=0, SrcA=4, SrcB=5, SrcC=6, NegC=1, OFmt=2)],
     [h2(f16(np.float32(a[0]) * np.float32(b[0]) - np.float32(c[0])), 9)])
caso('HADD2 32i', carga + [ensamblar('Hadd232i', Dest=0, SrcA=4, Imm=h2(0.25, -1))], [h2(a[0] + f16(0.25), a[1] - 1)])
caso('HSET2 bool', carga + [ensamblar('Hset2R', Dest=0, SrcA=4, SrcB=5, Cmp=1, Bop=0, SrcPred=7, Bval=1)],
     [h2(1.0 if a[0] < b[0] else 0.0, 1.0 if a[1] < b[1] else 0.0)])
caso('HSET2 máscara', carga + [ensamblar('Hset2R', Dest=0, SrcA=4, SrcB=5, Cmp=4, Bop=0, SrcPred=7)],
     [(0xFFFF if a[0] > b[0] else 0) | (0xFFFF0000 if a[1] > b[1] else 0)])
caso('HSETP2', carga + [ensamblar('Hsetp2R', DestPred=0, DestPredInv=1, SrcA=4, SrcB=5, FComp2=1, Bop=0, SrcPred=7),
                        ensamblar('Mov32i', Dest=0, Imm32=0, QuadMask=15), ensamblar('Mov32i', Dest=1, Imm32=0, QuadMask=15),
                        ensamblar('Mov32i', Dest=0, Imm32=1, QuadMask=15, Pred=0), ensamblar('Mov32i', Dest=1, Imm32=1, QuadMask=15, Pred=1)],
     [1 if a[0] < b[0] else 0, 1 if a[1] < b[1] else 0])

malos = 0
for nombre, insns, esperado in casos:
    r = correr(insns)
    if r is None or any(x != y for x, y in zip(r, esperado)):
        malos += 1
        print(f'{nombre}: {[hex(x) for x in r] if r else None}, se esperaba {[hex(x) for x in esperado]}')
print(f'{len(casos) - malos} de {len(casos)} casos de media precisión bien')
sys.exit(malos != 0)
