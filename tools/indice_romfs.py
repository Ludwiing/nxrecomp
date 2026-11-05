#!/usr/bin/env python3
"""Escribe <carpeta>/index.json para un RomFS estándar <carpeta>/romfs.bin, como lo deja tools/extract.py de
totk-pc-recomp: {status, romfs_size, romfs_sha256, files: [{path, offset, size}]}.

Uso: indice_romfs.py <repo de totk-pc-recomp> <carpeta con romfs.bin>

El índice se arma con index_romfs de ese repo (tools/vendor/macnativegames/extract_romfs.py). Si index.json ya
describe un romfs.bin del mismo tamaño y no más nuevo que él, no se vuelve a calcular.
"""
import hashlib
import json
import os
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    repo, carpeta = Path(sys.argv[1]), Path(sys.argv[2])
    imagen, indice = carpeta / "romfs.bin", carpeta / "index.json"
    try:
        viejo = json.loads(indice.read_text())
        if viejo.get("romfs_size") == imagen.stat().st_size and indice.stat().st_mtime >= imagen.stat().st_mtime:
            print(f"índice del RomFS al día: {len(viejo['files'])} archivos")
            return
    except (OSError, ValueError):
        pass
    sys.path.insert(0, str(repo / "tools" / "vendor" / "macnativegames"))
    from extract_romfs import index_romfs
    archivos = index_romfs(imagen)
    h = hashlib.sha256()
    with imagen.open("rb") as f:
        for bloque in iter(lambda: f.read(1 << 24), b""):
            h.update(bloque)
    informe = dict(status="pass", romfs_size=imagen.stat().st_size, romfs_sha256=h.hexdigest(), files=archivos,
                   integrity="RomFS extraído por NX2Linux (actualización aplicada); sin el hash de la extracción oficial")
    tmp = indice.with_suffix(".tmp")
    tmp.write_text(json.dumps(informe, indent=2) + "\n")
    os.replace(tmp, indice)
    print(f"índice del RomFS: {len(archivos)} archivos")


if __name__ == "__main__":
    main()
