"""Lector de paquetes .nup: dumps de Switch 2 ya descifrados, con uno o más títulos (juego base, actualización), cada uno
con su árbol exefs/ y romfs/. Escrito para nxrecomp (MIT) a partir del formato; todo en little-endian:

    0x00  "NUP\\x01"; +0x40: tamaño total del archivo (u64)
    0x80  "TOC\\x01", u32 títulos, u64 carpetas, u64 archivos, u64 offset de los nombres (desde 0x80), u64 su tamaño
    0xb0  títulos, 0x40 bytes c/u: u64 title id, u32 carpeta raíz, u32 carpeta exefs, u32 carpeta romfs, u32 versión
    luego carpetas (24 bytes: u32 nombre, u32 largo, u32 primer hijo, u32 cantidad de hijos, u64 reservado),
          archivos (80 bytes: u32 nombre, u32 largo, u64 tamaño, u64 offset, u64 tamaño guardado, 32 de hash,
          u8 códec [1 = zstd], 15 reservados), la lista de hijos (u32; < carpetas es carpeta, si no archivo - carpetas)
          y los nombres (sin terminador).
Un archivo puede venir guardado como un frame zstd cuyo contenido es el archivo. Los .zs del juego ya son zstd por sí
mismos: solo cuentan como guardados comprimidos si lo que contienen vuelve a ser zstd.
"""
import os
import struct

ZSTD = b"\x28\xb5\x2f\xfd"


class Archivo:
    __slots__ = ("nombre", "tamano", "offset", "guardado", "codec")

    def __init__(self, nombre, tamano, offset, guardado, codec):
        self.nombre, self.tamano, self.offset, self.guardado, self.codec = nombre, tamano, offset, guardado, codec


def _tamano_frame(cab):
    """El tamaño de contenido que declara la cabecera de un frame zstd, o None."""
    if cab[:4] != ZSTD or len(cab) < 18:
        return None
    fhd = cab[4]
    unico = fhd & 0x20
    campo = (1 if unico else 0, 2, 4, 8)[fhd >> 6]
    if not campo:
        return None
    pos = 5 + (0 if unico else 1) + (0, 1, 2, 4)[fhd & 3]
    v = int.from_bytes(cab[pos:pos + campo], "little")
    return v + 256 if campo == 2 else v


class Paquete:
    def __init__(self, ruta):
        self.f = open(ruta, "rb")
        cab = self.f.read(0xb0)
        if cab[:4] != b"NUP\x01" or cab[0x80:0x84] != b"TOC\x01":
            raise ValueError(f"{os.path.basename(ruta)} no es un paquete .nup")
        declarado = struct.unpack_from("<Q", cab, 0x40)[0]
        real = os.fstat(self.f.fileno()).st_size
        if declarado and real < declarado:
            raise ValueError(f"{os.path.basename(ruta)} está incompleto: mide {real >> 20} MB y debería medir "
                             f"{declarado >> 20} MB")
        nt, nc, na, onom, tnom = struct.unpack_from("<IQQQQ", cab, 0x84)
        p_carp = 0xb0 + 0x40 * nt
        p_arch = p_carp + 24 * nc
        p_hijos = p_arch + 80 * na
        p_nom = 0x80 + onom
        self.f.seek(0)
        meta = self.f.read(p_nom + tnom)
        self.nombres = meta[p_nom:p_nom + tnom]
        self.titulos = []
        for i in range(nt):
            tid, raiz, exefs, romfs, ver = struct.unpack_from("<QIIII", meta, 0xb0 + 0x40 * i)
            self.titulos.append({"id": tid, "exefs": exefs, "romfs": romfs, "version": ver, "indice": i})
        # Los nombres se leen recién al recorrer (la raíz no tiene uno que sirva y no se usa nunca).
        self.carpetas = [struct.unpack_from("<4I", meta, p_carp + 24 * i) for i in range(nc)]
        self.archivos = []
        for i in range(na):
            o, n, tam, off, guard = struct.unpack_from("<IIQQQ", meta, p_arch + 80 * i)
            self.archivos.append(Archivo((o, n), tam, off, guard, meta[p_arch + 80 * i + 64]))
        self.hijos = struct.unpack_from(f"<{(p_nom - p_hijos) // 4}I", meta, p_hijos)
        self.nc = nc

    def _nombre(self, o, n):
        return self.nombres[o:o + n].decode("utf-8", errors="replace")

    def recorrer(self, carpeta, prefijo=""):
        """(ruta relativa, Archivo) de todo lo que hay bajo una carpeta."""
        _, _, primero, cant = self.carpetas[carpeta]
        for h in self.hijos[primero:primero + cant]:
            if h < self.nc:
                o, n = self.carpetas[h][:2]
                yield from self.recorrer(h, prefijo + self._nombre(o, n) + "/")
            else:
                a = self.archivos[h - self.nc]
                if isinstance(a.nombre, tuple):
                    a.nombre = self._nombre(*a.nombre)
                yield prefijo + a.nombre, a

    def mas_nuevo(self):
        return max(self.titulos, key=lambda t: t["version"])

    def _comprimido(self, a):
        if a.codec == 1:
            return True
        self.f.seek(a.offset)
        cab = self.f.read(18)
        if cab[:4] != ZSTD:
            return False
        declarado = _tamano_frame(cab)
        if declarado is not None and declarado != a.tamano:
            return False
        if a.nombre.endswith(".zs") or declarado is None:
            # Un .zs del juego también empieza con zstd: está guardado comprimido solo si adentro hay otro frame.
            import zstandard
            self.f.seek(a.offset)
            try:
                inicio = zstandard.ZstdDecompressor().decompressobj().decompress(self.f.read(min(a.guardado or a.tamano, 1 << 20)))
            except zstandard.ZstdError:
                return False
            return inicio[:4] == ZSTD if a.nombre.endswith(".zs") else len(inicio) > 0
        return True

    def copiar(self, a, destino):
        """Escribe el archivo, ya descomprimido si hacía falta, en el archivo abierto `destino`."""
        if self._comprimido(a):
            import zstandard
            self.f.seek(a.offset)
            leer = a.guardado if a.codec == 1 and a.guardado else a.tamano + (1 << 16)
            d = zstandard.ZstdDecompressor().decompressobj()
            escrito = 0
            while leer and not d.eof:
                bloque = self.f.read(min(leer, 8 << 20))
                if not bloque:
                    break
                leer -= len(bloque)
                try:
                    salida = d.decompress(bloque)
                except zstandard.ZstdError:
                    break
                destino.write(salida)
                escrito += len(salida)
            if escrito != a.tamano:
                raise ValueError(f"{a.nombre}: salieron {escrito} bytes y se esperaban {a.tamano}")
            return
        self.f.seek(a.offset)
        falta = a.tamano
        while falta:
            bloque = self.f.read(min(falta, 8 << 20))
            if not bloque:
                raise ValueError(f"{a.nombre}: el paquete termina antes de tiempo (¿copia incompleta?)")
            destino.write(bloque)
            falta -= len(bloque)

    def leer(self, a):
        import io
        b = io.BytesIO()
        self.copiar(a, b)
        return b.getvalue()


def ruta_segura(salida, parte, rel):
    """Dónde va un archivo del paquete, sin dejar que un nombre raro (.., /, enlaces) salga de la carpeta."""
    partes = rel.split("/")
    if any(p in ("", ".", "..") or "\\" in p or "\0" in p for p in partes):
        raise ValueError(f"nombre inseguro en el paquete: {rel!r}")
    base = os.path.realpath(os.path.join(salida, parte))
    destino = os.path.join(base, *partes)
    actual = os.path.realpath(salida)
    for p in [parte] + partes:
        actual = os.path.join(actual, p)
        if os.path.islink(actual):
            raise ValueError(f"la ruta de extracción es un enlace: {actual}")
    return destino
