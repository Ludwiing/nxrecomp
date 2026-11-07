#!/usr/bin/env python3
"""Prueba del cargador de NSO/NPDM con lo nuevo del sistema 22.0.0+ y de Switch 2.

- ZBIC: los módulos del programa de prueba `multi` (build/multi_mod/exefs, hechos con LZ4 por
  `make all`) se reempaquetan con ZBIC; tienen que cargar con la misma imagen en memoria y el
  recompilador tiene que generar exactamente el mismo C.
- NPDM de Switch 2: espacio de direcciones de 42 bits, Flags1, RemoteMemorySize y un ACID con
  firma RSA-3072 (0x180 bytes) delante del ACI0.

Si falta el paquete zbic (`make pydeps`), la parte de ZBIC se salta sin fallar."""
import filecmp
import os
import shutil
import struct
import subprocess
import sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, os.path.join(RAIZ, "recomp"))
from nso import ZBIC, FormatError, Image, _zbic, load_nso, parse_npdm, write_nso  # noqa: E402

B = os.path.join(RAIZ, "build")
fallos = 0


def mal(msg):
    global fallos
    fallos += 1
    print("MAL: " + msg)


def prueba_npdm():
    acid_off, acid_size = 0x80, 0x400  # ACID de Switch 2: firma 0x180 + llave 0x180 + resto
    aci_off = acid_off + acid_size
    raw = bytearray(aci_off + 0x40)
    raw[0:4] = b"META"
    raw[0x0C] = 1 | (4 << 1)  # 64 bits, 42 bits de direcciones
    raw[0x0D] = 1  # EnableShadowStack
    struct.pack_into("<I", raw, 0x10, 0x2000000)  # RemoteMemorySize
    raw[0x0E] = 44
    struct.pack_into("<I", raw, 0x1C, 0x100000)
    raw[0x20:0x26] = b"prueba"
    struct.pack_into("<IIII", raw, 0x70, aci_off, 0x40, acid_off, acid_size)
    raw[acid_off + 0x300:acid_off + 0x304] = b"ACID"
    raw[aci_off:aci_off + 4] = b"ACI0"
    struct.pack_into("<Q", raw, aci_off + 0x10, 0x0400486024DEC000)
    n = parse_npdm(bytes(raw))
    esperado = {"nombre": "prueba", "64 bits": True, "espacio de direcciones": "42 bits", "pila sombra": True,
                "memoria remota": 0x2000000, "pila": 0x100000, "programa": 0x0400486024DEC000}
    for k, v in esperado.items():
        if n.get(k) != v:
            mal(f"NPDM de Switch 2: {k} = {n.get(k)!r}, se esperaba {v!r}")
    if not fallos:
        print("OK: NPDM de Switch 2 (42 bits, pila sombra, memoria remota, ACID RSA-3072)")


def prueba_zbic():
    try:
        _zbic()
    except FormatError:
        print("(sin el paquete zbic no se prueba ZBIC: make pydeps)")
        return
    exefs = os.path.join(B, "multi_mod", "exefs")
    if not os.path.isfile(os.path.join(exefs, "main")):
        mal("falta build/multi_mod/exefs (corre make all)")
        return
    zdir = os.path.join(B, "multi_zbic", "exefs")
    shutil.rmtree(os.path.dirname(zdir), ignore_errors=True)
    os.makedirs(zdir)
    shutil.copy(os.path.join(exefs, "main.npdm"), zdir)
    antes = fallos
    for nombre in ("main", "sdk"):
        lz4 = load_nso(open(os.path.join(exefs, nombre), "rb").read(), nombre)
        destino = os.path.join(zdir, nombre)
        write_nso(destino, lz4, zbic=True)
        raw = open(destino, "rb").read()
        flags = struct.unpack_from("<I", raw, 0x0C)[0]
        if not flags & ZBIC or raw[0x100:0x104] != b"ZBIC":
            mal(f"{nombre}: el NSO escrito no está en ZBIC")
        z = load_nso(raw, nombre)
        if (bytes(z.data), z.text, z.ro, z.rw, z.bss) != (bytes(lz4.data), lz4.text, lz4.ro, lz4.rw, lz4.bss):
            mal(f"{nombre}: la imagen cargada desde ZBIC no es igual a la de LZ4")
    # Un byte cambiado en los datos comprimidos tiene que dar error, no una imagen distinta.
    raw = bytearray(open(os.path.join(zdir, "main"), "rb").read())
    raw[0x100 + 40] ^= 0x55
    try:
        load_nso(bytes(raw), "main")
        mal("un NSO ZBIC dañado cargó sin error")
    except FormatError:
        pass
    salida = os.path.join(B, "multi_zbic", "rec")
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "recomp", "nxrecomp.py"), zdir, "-o", salida],
                       capture_output=True, text=True)
    if r.returncode:
        mal("nxrecomp falló con los módulos ZBIC:\n" + r.stdout + r.stderr)
    else:
        ref = os.path.join(B, "multi_rec")
        for f in sorted(os.listdir(ref)):
            a, b = os.path.join(ref, f), os.path.join(salida, f)
            if f == "programa.c":  # lleva la ruta de salida en los .incbin
                igual = (open(a).read().replace(os.path.abspath(ref), "@") ==
                         open(b).read().replace(os.path.abspath(salida), "@"))
            else:
                igual = os.path.isfile(b) and filecmp.cmp(a, b, shallow=False)
            if not igual:
                mal(f"{f} recompilado desde ZBIC es distinto del de LZ4")
    if fallos == antes:
        print("OK: los módulos de multi en ZBIC cargan igual que en LZ4 y se recompilan al mismo C")


def prueba_vacio():
    try:
        _zbic()
    except FormatError:
        return
    img = Image("vacio", bytearray(0x2000), (0, 0x1000), (0x1000, 0), (0x1000, 0x1000), 0)
    img.data[:4] = b"\x1f\x20\x03\xd5"  # nop
    ruta = os.path.join(B, "vacio.nso")
    write_nso(ruta, img, zbic=True)
    z = load_nso(open(ruta, "rb").read(), "vacio")
    if bytes(z.data) != bytes(img.data):
        mal("NSO ZBIC con un segmento vacío")


prueba_npdm()
prueba_zbic()
prueba_vacio()
sys.exit(1 if fallos else 0)
