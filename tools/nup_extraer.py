"""Saca de un .nup (dump de Switch 2) el título más nuevo: exefs/ y romfs/ en una carpeta, para NX2Linux.

    python3 tools/nup_extraer.py <archivo.nup> <carpeta>      escribe <carpeta>/exefs y <carpeta>/romfs
    python3 tools/nup_extraer.py <archivo.nup> --info         title id y versión de cada título

Imprime "progreso <hechos>/<total>" (en archivos) para la barra. Si la actualización no trae un módulo completo (un
NSO), se toma del otro título del paquete.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nup import Paquete, ruta_segura

MODULOS = ["rtld", "main", "sdk"] + [f"subsdk{i}" for i in range(10)]


def info(ruta):
    p = Paquete(ruta)
    for t in p.titulos:
        print(f"title {t['id']:016x} version {t['version']}")
    print(f"base {min(t['id'] for t in p.titulos) & ~0xFFF:016x}")
    print(f"display version v{p.mas_nuevo()['version']}")


def _completar_modulos(p, t, salida):
    for m in MODULOS:
        destino = ruta_segura(salida, "exefs", m)
        if os.path.isfile(destino) and open(destino, "rb").read(4) == b"NSO0":
            continue
        for otro in sorted(p.titulos, key=lambda x: -x["version"]):
            if otro is t:
                continue
            a = next((a for rel, a in p.recorrer(otro["exefs"]) if rel == m), None)
            if a and p.leer(a)[:4] == b"NSO0":
                with open(destino, "wb") as g:
                    p.copiar(a, g)
                print(f"{m}: tomado del título v{otro['version']}", flush=True)
                break


def extraer(ruta, salida):
    p = Paquete(ruta)
    t = p.mas_nuevo()
    lista = [(parte, rel, a) for parte in ("exefs", "romfs") for rel, a in p.recorrer(t[parte])]
    print(f"título {t['id']:016x} v{t['version']}: {len(lista)} archivos", flush=True)
    for k, (parte, rel, a) in enumerate(lista):
        destino = ruta_segura(salida, parte, rel)
        if not (os.path.isfile(destino) and os.path.getsize(destino) == a.tamano):   # se puede retomar
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            with open(destino + ".part", "wb") as g:
                p.copiar(a, g)
            os.replace(destino + ".part", destino)
        if k % 200 == 0 or k + 1 == len(lista):
            print(f"progreso {k + 1}/{len(lista)}", flush=True)
    if len(p.titulos) > 1:
        _completar_modulos(p, t, salida)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[2] == "--info":
        info(sys.argv[1])
    elif len(sys.argv) == 3:
        extraer(sys.argv[1], sys.argv[2])
    else:
        sys.exit(__doc__)
