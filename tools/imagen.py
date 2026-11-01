#!/usr/bin/env python3
"""Comprueba propiedades de una captura PPM que no dependen del cuadro exacto (para ejemplos animados):
fracción de píxeles dibujados (no negros) y su color medio.
Uso: imagen.py cuadro.ppm MIN MAX [R,G,B TOLERANCIA]"""
import sys

ruta, mn, mx = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
d = open(ruta, 'rb').read()
c = d.split(maxsplit=3)
w, h = int(c[1]), int(c[2])
i = len(d) - w * h * 3
px = [d[k:k + 3] for k in range(i, len(d), 3 * 17)]
dib = [p for p in px if max(p) > 16]
cob = len(dib) / len(px)
mal = not (mn <= cob <= mx)
if mal:
    print(f'{ruta}: cobertura {cob:.3f}, se esperaba entre {mn} y {mx}', file=sys.stderr)
if len(sys.argv) > 5 and dib:
    esp, tol = [int(x) for x in sys.argv[4].split(',')], int(sys.argv[5])
    medio = [sum(p[j] for p in dib) / len(dib) for j in range(3)]
    if any(abs(a - b) > tol for a, b in zip(medio, esp)):
        print(f'{ruta}: color medio {[int(a) for a in medio]}, se esperaba {esp} ± {tol}', file=sys.stderr)
        mal = True
sys.exit(1 if mal else 0)
