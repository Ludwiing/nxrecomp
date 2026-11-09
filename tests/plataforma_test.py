"""recomp/nso.py plataforma: Switch 1 o Switch 2 a partir de un ExeFS (main.npdm sintéticos)."""
import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "recomp"))
from nso import plataforma


def npdm(firma, remota=0, espacio=3):
    meta = bytearray(0x80)
    meta[:4] = b"META"
    meta[0x0C] = 1 | (espacio << 1)
    struct.pack_into("<I", meta, 0x10, remota)
    acid = bytearray(2 * firma + 0x100)
    acid[2 * firma:2 * firma + 4] = b"ACID"
    aci0 = bytearray(0x40)
    aci0[:4] = b"ACI0"
    struct.pack_into("<II", meta, 0x70, 0x80 + len(acid), len(aci0))
    struct.pack_into("<II", meta, 0x78, 0x80, len(acid))
    return bytes(meta + acid + aci0)


def caso(nombre, datos, esperado):
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "main.npdm"), "wb").write(datos)
        cual, razones = plataforma(d)
    print(("OK" if cual == esperado else "MAL") + f": {nombre}: {cual} {razones}")
    return cual == esperado


ok = all([caso("Switch 1 (RSA-2048, 39 bits)", npdm(0x100), "switch1"),
          caso("Switch 2 (RSA-3072)", npdm(0x180, espacio=4), "switch2"),
          caso("Switch 2 (memoria remota)", npdm(0x100, remota=0x100000), "switch2"),
          caso("42 bits sin más señales", npdm(0x100, espacio=4), "switch1")])
sys.exit(0 if ok else 1)
