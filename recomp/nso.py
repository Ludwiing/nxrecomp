"""Formatos de módulo de la Switch: NSO (ExeFS de los juegos) y NRO (homebrew), y LZ4.

Un módulo es la imagen en memoria de .text + .rodata + .data (el .bss es solo un
tamaño). En el NSO cada segmento puede venir comprimido con LZ4 y tiene un SHA-256
en la cabecera; si la bandera de verificación está puesta, se comprueba igual que
el cargador de Horizon.
"""

import hashlib
import struct

PAGE = 0x1000


class FormatError(ValueError):
    pass


def lz4_decompress(src, size):
    """Descomprime un bloque LZ4. Usa el paquete lz4 si está; si no, Python puro."""
    try:
        import lz4.block
        out = lz4.block.decompress(bytes(src), uncompressed_size=size)
    except ImportError:
        out = _lz4_decompress_py(src)
    if len(out) != size:
        raise FormatError(f"LZ4 dio {len(out)} bytes y se esperaban {size}")
    return out


def _lz4_decompress_py(src):
    dst = bytearray()
    i, n = 0, len(src)
    while i < n:
        tok = src[i]
        i += 1
        lit = tok >> 4
        if lit == 15:
            while True:
                b = src[i]
                i += 1
                lit += b
                if b != 255:
                    break
        dst += src[i:i + lit]
        i += lit
        if i >= n:
            break
        off = src[i] | src[i + 1] << 8
        i += 2
        ml = tok & 15
        if ml == 15:
            while True:
                b = src[i]
                i += 1
                ml += b
                if b != 255:
                    break
        ml += 4
        start = len(dst) - off
        if off <= 0 or start < 0:
            raise FormatError("LZ4 con un desplazamiento inválido")
        if off >= ml:
            dst += dst[start:start + ml]
        else:
            for k in range(ml):
                dst.append(dst[start + k])
    return bytes(dst)


def lz4_compress(data):
    """Compresor LZ4 sencillo (voraz), para empaquetar los módulos de prueba."""
    try:
        import lz4.block
        return lz4.block.compress(bytes(data), store_size=False)
    except ImportError:
        pass
    n = len(data)
    out = bytearray()

    def seq(lit, off=0, ml=0):
        ln = len(lit)
        tok = (min(ln, 15) << 4) | (min(ml - 4, 15) if off else 0)
        out.append(tok)
        if ln >= 15:
            r = ln - 15
            while r >= 255:
                out.append(255)
                r -= 255
            out.append(r)
        out.extend(lit)
        if off:
            out.extend(struct.pack("<H", off))
            if ml - 4 >= 15:
                r = ml - 4 - 15
                while r >= 255:
                    out.append(255)
                    r -= 255
                out.append(r)

    table, anchor, i = {}, 0, 0
    while i < n - 12:
        key = bytes(data[i:i + 4])
        cand = table.get(key)
        table[key] = i
        if cand is not None and i - cand <= 0xFFFF:
            m = 4
            while i + m < n - 5 and data[cand + m] == data[i + m]:
                m += 1
            seq(data[anchor:i], i - cand, m)
            i += m
            anchor = i
        else:
            i += 1
    seq(data[anchor:])
    return bytes(out)


class Image:
    """Imagen de un módulo: bytes de text+ro+data con sus offsets, y el tamaño del bss."""

    def __init__(self, name, data, text, ro, rw, bss, build_id=b"", kind="nso"):
        self.name = name
        self.kind = kind                # "nso": lo reubica rtld; "nro": se reubica solo (crt0 de libnx)
        self.data = bytearray(data)
        self.original = bytes(data)     # antes de aplicar reubicaciones
        self.text, self.ro, self.rw = text, ro, rw  # (offset, tamaño) dentro de la imagen
        self.bss = bss
        self.build_id = build_id

    @property
    def size(self):
        return len(self.data)


def load_nso(raw, name="main"):
    if raw[:4] != b"NSO0":
        raise FormatError(f"{name}: no es un NSO")
    flags = struct.unpack_from("<I", raw, 0x0C)[0]
    segs = []
    for k, (hoff, foff) in enumerate(((0x10, 0x60), (0x20, 0x64), (0x30, 0x68))):
        file_off, mem_off, size = struct.unpack_from("<III", raw, hoff)
        file_size = struct.unpack_from("<I", raw, foff)[0]
        segs.append((file_off, mem_off, size, file_size))
    bss = struct.unpack_from("<I", raw, 0x3C)[0]
    build_id = raw[0x40:0x60]
    if flags & 0x80:
        raise FormatError(f"{name}: segmentos ZBIC (zstd, sistema 22.0.0 o posterior) todavía no soportados")
    end = max(m + s for _, m, s, _ in segs)
    image = bytearray(end)
    names = ("text", "ro", "data")
    for k, (file_off, mem_off, size, file_size) in enumerate(segs):
        if mem_off % PAGE:
            raise FormatError(f"{name}: segmento {names[k]} no alineado a página")
        stored = raw[file_off:file_off + (file_size if flags & (1 << k) else size)]
        seg = lz4_decompress(stored, size) if flags & (1 << k) else bytes(stored)
        if len(seg) != size:
            raise FormatError(f"{name}: segmento {names[k]} truncado")
        if flags & (8 << k):
            want = raw[0xA0 + 0x20 * k:0xC0 + 0x20 * k]
            if hashlib.sha256(seg).digest() != want:
                raise FormatError(f"{name}: el SHA-256 del segmento {names[k]} no coincide (archivo dañado)")
        image[mem_off:mem_off + size] = seg
    return Image(name, image, (segs[0][1], segs[0][2]), (segs[1][1], segs[1][2]), (segs[2][1], segs[2][2]), bss, build_id)


def load_nro(raw, name="main"):
    if raw[0x10:0x14] != b"NRO0":
        raise FormatError(f"{name}: no es un NRO")
    size = struct.unpack_from("<I", raw, 0x18)[0]
    s = struct.unpack_from("<6I", raw, 0x20)
    bss = struct.unpack_from("<I", raw, 0x38)[0]
    img = Image(name, raw[:size], (s[0], s[1]), (s[2], s[3]), (s[4], s[5]), bss, raw[0x40:0x60], kind="nro")
    # Los homebrew traen después del código una sección ASET: ícono, NACP y RomFS.
    img.romfs = b""
    if raw[size:size + 4] == b"ASET" and len(raw) >= size + 0x38:
        off, rsize = struct.unpack_from("<QQ", raw, size + 0x28)
        img.romfs = bytes(raw[size + off:size + off + rsize])
    return img


def load_module(raw, name):
    if raw[:4] == b"NSO0":
        return load_nso(raw, name)
    if raw[0x10:0x14] == b"NRO0":
        return load_nro(raw, name)
    raise FormatError(f"{name}: no es NSO ni NRO")


def write_nso(path, image):
    """Escribe un NSO con los tres segmentos comprimidos con LZ4 y sus hashes verificables."""
    segs = [image.text, image.ro, image.rw]
    hdr = bytearray(0x100)
    hdr[0:4] = b"NSO0"
    blobs, off = [], 0x100
    for k, (moff, size) in enumerate(segs):
        raw = bytes(image.data[moff:moff + size])
        comp = lz4_compress(raw)
        struct.pack_into("<III", hdr, 0x10 + 0x10 * k, off, moff, size)
        struct.pack_into("<I", hdr, 0x60 + 4 * k, len(comp))
        hdr[0xA0 + 0x20 * k:0xC0 + 0x20 * k] = hashlib.sha256(raw).digest()
        blobs.append(comp)
        off += len(comp)
    struct.pack_into("<I", hdr, 0x0C, 0b111111)  # comprimido y con hash en los tres
    struct.pack_into("<I", hdr, 0x3C, image.bss)
    hdr[0x40:0x60] = image.build_id.ljust(0x20, b"\0")[:0x20]
    with open(path, "wb") as fh:
        fh.write(hdr)
        for b in blobs:
            fh.write(b)


def parse_npdm(raw):
    """Lo que sirve del main.npdm: nombre, pila del hilo principal, prioridad, espacio de direcciones."""
    if raw[:4] != b"META":
        raise FormatError("main.npdm sin META")
    flags = raw[0x0C]
    aspace = (flags >> 1) & 7
    return {
        "nombre": raw[0x20:0x30].split(b"\0")[0].decode(errors="replace"),
        "64 bits": bool(flags & 1),
        "espacio de direcciones": {0: "32 bits", 1: "36 bits", 2: "32 bits sin reserva", 3: "39 bits"}.get(aspace, str(aspace)),
        "prioridad": raw[0x0E],
        "núcleo": raw[0x0F],
        "pila": struct.unpack_from("<I", raw, 0x1C)[0],
        "recurso del sistema": struct.unpack_from("<I", raw, 0x14)[0],
    }
