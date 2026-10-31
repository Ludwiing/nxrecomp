#!/usr/bin/env python3
"""Empaqueta el ELF de ARM64 del programa de prueba como NRO, el formato de los homebrew de Switch."""

import hashlib
import struct
import sys

PAGE = 0x1000


def align(v, a=PAGE):
    return (v + a - 1) & ~(a - 1)


def sections(elf):
    if elf[:4] != b"\x7fELF" or elf[4] != 2 or elf[18] != 0xB7:
        sys.exit("se esperaba un ELF de 64 bits para AArch64")
    shoff, = struct.unpack_from("<Q", elf, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x3A)
    hdrs = [struct.unpack_from("<IIQQQQIIQQ", elf, shoff + k * shentsize) for k in range(shnum)]
    strtab = hdrs[shstrndx]
    out = {}
    for name, typ, flags, addr, off, size, *_ in hdrs:
        n = elf[strtab[4] + name:].split(b"\0", 1)[0].decode()
        if flags & 2:  # SHF_ALLOC
            out[n] = (typ, addr, off, size)
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit("uso: mknro.py entrada.elf salida.nro")
    elf = open(sys.argv[1], "rb").read()
    sec = sections(elf)
    unknown = set(sec) - {".text", ".rodata", ".data", ".bss"}
    if unknown:
        sys.exit(f"secciones sin ubicar en el NRO: {', '.join(sorted(unknown))} (revisa guest/link.ld)")

    base = sec[".text"][1]
    ro_addr, data_addr = sec[".rodata"][1], sec[".data"][1]
    data_end = align(data_addr + sec[".data"][3])
    bss_end = sec[".bss"][1] + sec[".bss"][3] if ".bss" in sec else data_end

    image = bytearray(data_end - base)
    for name in (".text", ".rodata", ".data"):
        typ, addr, off, size = sec[name]
        image[addr - base:addr - base + size] = elf[off:off + size]

    text = (0, ro_addr - base)
    ro = (ro_addr - base, data_addr - ro_addr)
    data = (data_addr - base, data_end - data_addr)
    bss = align(bss_end - data_end) if bss_end > data_end else 0

    struct.pack_into("<4sIII", image, 0x10, b"NRO0", 0, len(image), 0)
    struct.pack_into("<6I", image, 0x20, *text, *ro, *data)
    struct.pack_into("<II", image, 0x38, bss, 0)
    image[0x40:0x60] = hashlib.sha256(image[0x80:]).digest()  # build ID

    open(sys.argv[2], "wb").write(image)
    print(f"{sys.argv[2]}: .text {text[1]:#x}, .rodata {ro[1]:#x}, .data {data[1]:#x}, .bss {bss:#x}")


if __name__ == "__main__":
    main()
