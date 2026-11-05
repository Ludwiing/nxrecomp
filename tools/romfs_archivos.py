#!/usr/bin/env python3
"""Saca los archivos de un RomFS estándar de Nintendo (como RomFS.img.native) a una carpeta.

Uso: romfs_archivos.py <imagen> <carpeta>

Algunos ports (ssbu-recomp) leen el RomFS como carpeta. Un archivo que ya está con el mismo tamaño no se vuelve a
copiar. La copia usa copy_file_range: en btrfs comparte los bloques alineados en lugar de duplicarlos.
"""
import os
import struct
import sys

NINGUNO = 0xffffffff


def leer(f, pos, n):
    f.seek(pos)
    datos = f.read(n)
    if len(datos) != n:
        raise ValueError("el RomFS está cortado")
    return datos


def archivos(f):
    """[(ruta, desplazamiento, tamaño)] de todos los archivos del RomFS abierto en f."""
    cab = struct.unpack("<10Q", leer(f, 0, 80))
    if cab[0] != 80:
        raise ValueError("no es un RomFS estándar (la cabecera no mide 0x50)")
    dirs, ents, datos = leer(f, cab[3], cab[4]), leer(f, cab[7], cab[8]), cab[9]
    lista, pendientes, vistos = [], [(0, "")], set()
    while pendientes:
        d, padre = pendientes.pop()
        if d in vistos:
            raise ValueError("directorio repetido en el RomFS")
        vistos.add(d)
        _, hermano, hijo, archivo, _, n = struct.unpack_from("<6I", dirs, d)
        nombre = dirs[d + 24:d + 24 + n].decode()
        ruta = f"{padre}{nombre}/" if nombre else padre
        if hermano != NINGUNO:
            pendientes.append((hermano, padre))
        if hijo != NINGUNO:
            pendientes.append((hijo, ruta))
        while archivo != NINGUNO:
            _, siguiente, desp, tam, _, n = struct.unpack_from("<IIQQII", ents, archivo)
            nombre = ents[archivo + 32:archivo + 32 + n].decode()
            if nombre in ("", ".", "..") or "/" in nombre:
                raise ValueError(f"nombre inseguro en el RomFS: {nombre!r}")
            lista.append((ruta + nombre, datos + desp, tam))
            archivo = siguiente
    return lista


def copiar(f, g, pos, tam, ruta):
    """tam bytes de f desde pos al final de g: copy_file_range (sin pasar por Python, y compartiendo bloques en btrfs);
    entre sistemas de archivos distintos, que no lo admiten, lectura y escritura por bloques."""
    falta = tam
    try:
        while falta:
            n = os.copy_file_range(f.fileno(), g.fileno(), min(falta, 1 << 30), pos)
            if n <= 0:
                raise OSError(f"no se pudo copiar {ruta}")
            pos, falta = pos + n, falta - n
        return
    except OSError as e:
        if e.errno not in (18, 22, 38, 95):   # EXDEV, EINVAL, ENOSYS, EOPNOTSUPP
            raise
    f.seek(pos)
    while falta:
        bloque = f.read(min(falta, 1 << 24))
        if not bloque:
            raise OSError(f"el RomFS se terminó antes de {ruta}")
        g.write(bloque)
        falta -= len(bloque)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    imagen, destino = sys.argv[1], os.path.realpath(sys.argv[2])
    with open(imagen, "rb") as f:
        lista = archivos(f)
        total = sum(t for _, _, t in lista)
        hecho = 0
        for k, (ruta, desp, tam) in enumerate(lista):
            salida = os.path.realpath(os.path.join(destino, ruta))
            if not salida.startswith(destino + os.sep):
                raise ValueError(f"ruta fuera de la carpeta: {ruta}")
            if not (os.path.isfile(salida) and os.path.getsize(salida) == tam):
                os.makedirs(os.path.dirname(salida), exist_ok=True)
                tmp = salida + ".tmp"
                with open(tmp, "wb") as g:
                    copiar(f, g, desp, tam, ruta)
                os.replace(tmp, salida)
            hecho += tam
            if k % 200 == 0 or k == len(lista) - 1:
                print(f"progreso {hecho}/{max(total, 1)}", flush=True)
    print(f"{len(lista)} archivos del RomFS en {destino}")


if __name__ == "__main__":
    main()
