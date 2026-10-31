#!/usr/bin/env python3
"""Empaqueta un módulo ELF de AArch64 (enlazado con guest/multi/module.ld) como NSO,
con los segmentos comprimidos con LZ4 y sus SHA-256, como los de un juego.
Con --npdm escribe además un main.npdm mínimo (64 bits, 39 bits de direcciones)."""

import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "recomp"))
from nso import PAGE, Image, write_nso  # noqa: E402


def align(v, a=PAGE):
    return (v + a - 1) & ~(a - 1)


def image_from_elf(elf, name):
    if elf[:4] != b"\x7fELF" or elf[4] != 2 or struct.unpack_from("<H", elf, 18)[0] != 0xB7:
        sys.exit("se esperaba un ELF de 64 bits para AArch64")
    phoff = struct.unpack_from("<Q", elf, 0x20)[0]
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    loads = []
    for k in range(phnum):
        typ, flags, off, vaddr, _, filesz, memsz, _ = struct.unpack_from("<IIQQQQQQ", elf, phoff + k * phentsize)
        if typ == 1:  # PT_LOAD
            loads.append((vaddr, off, filesz, memsz, flags))
    text = [s for s in loads if s[4] & 1]
    data = [s for s in loads if s[4] & 2]
    ro = [s for s in loads if not s[4] & 3]
    if len(text) != 1 or len(data) != 1 or len(ro) > 1 or text[0][0] != 0:
        sys.exit("se esperaban segmentos .text (en 0), .rodata y .data separados")
    data_end = align(data[0][0] + data[0][2])
    img = bytearray(data_end)
    for vaddr, off, filesz, _, _ in loads:
        img[vaddr:vaddr + filesz] = elf[off:off + filesz]
    t_end = align(text[0][0] + text[0][3])
    ro_lo = ro[0][0] if ro else t_end
    ro_end = align(ro[0][0] + ro[0][3]) if ro else t_end
    bss = align(data[0][0] + data[0][3]) - data_end
    return Image(name, img, (0, t_end), (ro_lo, ro_end - ro_lo), (data[0][0], data_end - data[0][0]), bss,
                 hashlib.sha256(img).digest())


def npdm(name, stack=0x100000):
    raw = bytearray(0x80)
    raw[0:4] = b"META"
    raw[0x0C] = 1 | (3 << 1)  # 64 bits, espacio de direcciones de 39 bits
    raw[0x0E] = 44
    struct.pack_into("<I", raw, 0x1C, stack)
    raw[0x20:0x30] = name.encode()[:0x10].ljust(0x10, b"\0")
    return bytes(raw)


def main():
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == "--npdm":
        open(args[2], "wb").write(npdm(args[1]))
        return
    if len(args) != 2:
        sys.exit("uso: mknso.py módulo.elf salida   |   mknso.py --npdm nombre main.npdm")
    name = os.path.basename(args[1])
    image = image_from_elf(open(args[0], "rb").read(), name)
    write_nso(args[1], image)
    print(f"{args[1]}: NSO .text {image.text[1]:#x}, .rodata {image.ro[1]:#x}, .data {image.rw[1]:#x}, .bss {image.bss:#x}")


if __name__ == "__main__":
    main()
