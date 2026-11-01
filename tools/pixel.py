#!/usr/bin/env python3
"""Comprueba el color de un píxel de una captura PPM: pixel.py cuadro.ppm X Y R,G,B"""
import sys

ruta, x, y, esperado = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), tuple(map(int, sys.argv[4].split(',')))
datos = open(ruta, 'rb').read()
campos = datos.split(maxsplit=3)  # P6 ancho alto, y "max" + un espacio + los datos
ancho = int(campos[1])
cabecera = len(datos) - len(campos[3]) + len(campos[3].split(maxsplit=1)[0]) + 1
i = cabecera + (y * ancho + x) * 3
color = tuple(datos[i:i + 3])
if color != esperado:
    print(f'{ruta}: el píxel ({x},{y}) es {color}, se esperaba {esperado}', file=sys.stderr)
    sys.exit(1)
