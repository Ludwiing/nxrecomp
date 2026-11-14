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


def _familia(tid):
    """El juego base al que pertenece un title ID: base (…000), actualización (…800) o DLC (base + 0x1000 + n)."""
    return tid & ~0xFFF if tid & 0xFFF in (0, 0x800) else (tid - 0x1000) & ~0xFFF


def _programa_npdm(raw):
    """El id de programa del ACI0 de un main.npdm (0 si no tiene META/ACI0), como recomp/nso.py."""
    import struct
    if len(raw) < 0x80 or raw[:4] != b"META":
        return 0
    off, size = struct.unpack_from("<II", raw, 0x70)
    if size >= 0x18 and off + 0x18 <= len(raw) and raw[off:off + 4] == b"ACI0":
        return struct.unpack_from("<Q", raw, off + 0x10)[0]
    return 0


def info(ruta):
    """Qué trae el paquete, sacado de su TOC y del main.npdm (no del nombre del archivo). Imprime "base <id>" solo si
    hay un título de juego base; "update"/"dlc" para el resto; "npdm <id>" con el programa del main.npdm del título que
    se extraería y "falta <qué>" si al código le falta algo. Termina con error si mezcla títulos de juegos distintos."""
    p = Paquete(ruta)
    if not p.titulos:
        sys.exit(f"{os.path.basename(ruta)} no trae ningún título")
    familias = {_familia(t["id"]) for t in p.titulos}
    if len(familias) > 1:
        sys.exit(f"{os.path.basename(ruta)} mezcla títulos de juegos distintos: "
                 + ", ".join(f"{t['id']:016x}" for t in p.titulos))
    for t in p.titulos:
        print(f"title {t['id']:016x} version {t['version']}")
    for t in p.titulos:
        clase = {0: "base", 0x800: "update"}.get(t["id"] & 0xFFF, "dlc")
        print(f"{clase} {t['id']:016x}")
    con_codigo = [t for t in p.titulos if t["id"] & 0xFFF in (0, 0x800)]
    if con_codigo:
        # El título que se extrae es el más nuevo; los NSO que le falten se toman de los otros (_completar_modulos),
        # pero el main.npdm tiene que venir en él.
        t = max(con_codigo, key=lambda x: x["version"])
        exefs = {rel: a for rel, a in p.recorrer(t["exefs"])}
        if "main.npdm" in exefs:
            print(f"npdm {_programa_npdm(p.leer(exefs['main.npdm'])):016x}")
        else:
            print("falta main.npdm")
        if not any("main" in {rel for rel, _ in p.recorrer(x["exefs"])} for x in con_codigo):
            print("falta main")
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
