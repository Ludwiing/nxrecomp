#!/usr/bin/env python3
"""Comprueba colores de píxeles de una captura PPM, con una tolerancia de 3 por canal (la GPU
puede redondear distinto la interpolación). Uso: pixel.py cuadro.ppm X Y R,G,B [X Y R,G,B ...]"""
import sys

ruta = sys.argv[1]
datos = open(ruta, 'rb').read()
campos = datos.split(maxsplit=3)  # P6 ancho alto, y "max" + un espacio + los datos
ancho = int(campos[1])
cabecera = len(datos) - len(campos[3]) + len(campos[3].split(maxsplit=1)[0]) + 1
mal = 0
args = sys.argv[2:]
for k in range(0, len(args), 3):
    x, y, esperado = int(args[k]), int(args[k + 1]), tuple(map(int, args[k + 2].split(',')))
    i = cabecera + (y * ancho + x) * 3
    color = tuple(datos[i:i + 3])
    if any(abs(a - b) > 3 for a, b in zip(color, esperado)):
        print(f'{ruta}: el píxel ({x},{y}) es {color}, se esperaba {esperado}', file=sys.stderr)
        mal = 1
sys.exit(mal)
