#!/usr/bin/env python3
"""Módulos de nn::ro traducidos de antemano (recomp/program.py modulos_ro_de y Program(modulos_ro=...),
recomp/nxrecomp.py --modulos-ro), con NRO sintéticos hechos aquí byte a byte:

- modulos_ro_de toma solo <sha256>.nro cuyo contenido tiene ese SHA-256 y que son NRO, en orden de nombre;
- el módulo va detrás de los del arranque, a una dirección fija; su init no corre al arrancar (lo llama
  nn::ro::LoadModule) pero su código sí se traduce desde el init y sus exportaciones;
- la búsqueda global (lo que rtld publica para enlazar el NRO) solo ve los módulos del arranque;
- nxrecomp.py emite nx_modulos_ro con el índice y el SHA-256 de los bytes originales, y los símbolos globales en el
  orden de strcmp (el runtime los busca con bsearch). Sin --modulos-ro no emite la tabla de símbolos.
"""
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "recomp"))
from nso import load_nro  # noqa: E402
from program import Program, modulos_ro_de  # noqa: E402

RET = struct.pack("<I", 0xD65F03C0)
GLOBAL_FUNC, GLOBAL_NOTYPE = 0x12, 0x10


def nro(simbolos, init=0x100, marca=0):
    """NRO de 0x3000 bytes (text, ro y data de 0x1000) y bss 0x1000. simbolos: [(nombre, valor, info, shndx)].
    El text son RET; MOD0 en 0x1000 apunta a la dinámica en 0x2000 (HASH, SYMTAB, STRTAB, INIT)."""
    d = bytearray(0x3000)
    d[0:0x1000] = RET * 0x400
    struct.pack_into("<I", d, 4, 0x1000)                     # desplazamiento de MOD0
    d[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", d, 0x18, len(d))
    struct.pack_into("<6I", d, 0x20, 0, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000)
    struct.pack_into("<I", d, 0x38, 0x1000)
    d[0x40] = marca                                          # build id: distingue módulos con el mismo contenido
    d[0x1000:0x1004] = b"MOD0"
    struct.pack_into("<i", d, 0x1004, 0x2000 - 0x1000)       # dinámica relativa a MOD0
    nombres = b"\0" + b"".join(n.encode() + b"\0" for n, _, _, _ in simbolos)
    strtab, symtab, hashtab = 0x1300, 0x1200, 0x1100
    d[strtab:strtab + len(nombres)] = nombres
    off = 1
    for k, (n, valor, info, shndx) in enumerate(simbolos, 1):
        struct.pack_into("<IBBHQQ", d, symtab + 24 * k, off, info, 0, shndx, valor, 0)
        off += len(n) + 1
    struct.pack_into("<II", d, hashtab, 1, len(simbolos) + 1)  # nbucket, nchain = número de símbolos
    dyn = [(4, hashtab), (6, symtab), (5, strtab), (10, len(nombres)), (11, 24), (12, init), (0, 0)]
    for k, (tag, val) in enumerate(dyn):
        struct.pack_into("<qQ", d, 0x2000 + 16 * k, tag, val)
    return bytes(d)


def comprobar(cond, texto):
    if not cond:
        print("MAL: " + texto)
        sys.exit(1)


def main():
    estatico = nro([("importada", 0x200, GLOBAL_FUNC, 1), ("zeta", 0x204, GLOBAL_FUNC, 1),
                    ("Alfa", 0x208, GLOBAL_FUNC, 1)], init=0x180)
    dinamico = nro([("exportada", 0x300, GLOBAL_FUNC, 1), ("importada", 0, GLOBAL_NOTYPE, 0)], marca=1)
    otro = nro([("segunda", 0x310, GLOBAL_FUNC, 1)], marca=2)
    sha_din, sha_otro = hashlib.sha256(dinamico).hexdigest(), hashlib.sha256(otro).hexdigest()

    with tempfile.TemporaryDirectory() as tmp:
        carpeta = os.path.join(tmp, "nro")
        os.mkdir(carpeta)
        for nombre, datos in ((sha_din + ".nro", dinamico), (sha_otro + ".nro", otro),
                              ("0" * 64 + ".nro", dinamico),        # el nombre no es su SHA-256
                              (hashlib.sha256(b"x" * 64).hexdigest() + ".nro", b"x" * 64),  # no es un NRO
                              ("notas.txt", b"")):
            open(os.path.join(carpeta, nombre), "wb").write(datos)
        # El que no es NRO tiene un nombre que sí coincide con su SHA-256 pero load_module lo rechaza: se ignora.
        try:
            imgs = modulos_ro_de(carpeta)
        except Exception as e:  # un archivo cualquiera no debe tumbar la recompilación
            comprobar(False, f"modulos_ro_de falló con un archivo que no es NRO: {e!r}")
        comprobar([i.sha256.hex() for i in imgs] == sorted([sha_din, sha_otro]),
                  f"modulos_ro_de dio {[i.sha256.hex() for i in imgs]}")
        comprobar(modulos_ro_de(os.path.join(tmp, "no-existe")) == [] and modulos_ro_de(None) == [],
                  "una carpeta ausente debe dar []")

        img_est = load_nro(estatico, "main")
        base = 0x8000000
        p = Program([img_est], base, None, imgs)
        est, *dins = p.modules
        comprobar(not est.dinamico and all(m.dinamico for m in dins), "marca de dinámico")
        esperada = base + 0x4000                                # 0x3000 + bss 0x1000, alineado a página
        for m in dins:
            comprobar(m.base == esperada, f"{m.name} en 0x{m.base:x}, se esperaba 0x{esperada:x}")
            esperada += 0x4000
        boot = p.boot()
        comprobar(boot["inits"] == [base + 0x180], f"inits del arranque {[hex(a) for a in boot['inits']]}")
        din = next(m for m in dins if m.img.sha256.hex() == sha_din)
        entradas = p.entry_points()
        comprobar(din.base + 0x100 in entradas and din.base + 0x300 in entradas,
                  "el init y las exportaciones del NRO deben traducirse")
        comprobar(p.globales == {"importada": base + 0x200, "zeta": base + 0x204, "Alfa": base + 0x208},
                  f"globales {p.globales}")
        comprobar(p.symbols["exportada"] == din.base + 0x300, "los símbolos del NRO siguen en la tabla general")

        # De punta a punta: nxrecomp.py con y sin --modulos-ro sobre el NRO estático.
        ruta_est = os.path.join(tmp, "main.nro")
        open(ruta_est, "wb").write(estatico)
        for con in (True, False):
            salida = os.path.join(tmp, "c-con" if con else "c-sin")
            orden = [sys.executable, os.path.join(RAIZ, "recomp", "nxrecomp.py"), ruta_est, "-o", salida]
            if con:
                orden += ["--modulos-ro", carpeta]
            r = subprocess.run(orden, capture_output=True, text=True)
            comprobar(r.returncode == 0, f"nxrecomp.py falló:\n{r.stdout}\n{r.stderr}")
            prog_c = open(os.path.join(salida, "programa.c")).read()
            n_ro = int(re.search(r"nx_nmodulos_ro = (\d+);", prog_c).group(1))
            n_glob = int(re.search(r"nx_nsimbolos_globales = (\d+);", prog_c).group(1))
            if not con:
                comprobar(n_ro == 0 and n_glob == 0, "sin --modulos-ro no hay tablas")
                continue
            comprobar(n_ro == 2, f"nx_nmodulos_ro = {n_ro}")
            filas = re.findall(r"\{ (\d+), \{ ((?:0x[0-9a-f]{2}, ){31}0x[0-9a-f]{2}) \} \}", prog_c)
            emitidos = [(int(k), bytes(int(b, 16) for b in bs.split(", ")).hex()) for k, bs in filas]
            comprobar(emitidos == list(zip((1, 2), sorted([sha_din, sha_otro]))), f"nx_modulos_ro {emitidos}")
            bloque = prog_c.split("nx_simbolos_globales[] = {")[1].split("};")[0]
            nombres = re.findall(r'\{ "([^"]+)", 0x', bloque)
            comprobar(nombres == ["Alfa", "importada", "zeta"], f"orden de los globales {nombres}")
            # El .bin del módulo dinámico son sus bytes originales (los que el juego arma y el SDK reubica).
            bins = sorted(n for n in os.listdir(salida) if n.startswith("modulo_") and n.endswith(".bin"))
            datos = open(os.path.join(salida, bins[1]), "rb").read()
            comprobar(hashlib.sha256(datos).hexdigest() == sorted([sha_din, sha_otro])[0], "imagen del módulo 1")
    print("OK: módulos de nn::ro traducidos detrás del arranque, sin init al arrancar, tablas de SHA-256 y símbolos "
          "globales ordenados")
    return 0


if __name__ == "__main__":
    sys.exit(main())
