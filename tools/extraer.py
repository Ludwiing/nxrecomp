#!/usr/bin/env python3
"""Extrae el ExeFS (los módulos de código) y el RomFS de un juego tuyo: NSP, NSZ, XCI o XCZ.

    python3 tools/extraer.py --keys ~/.switch/prod.keys juego.nsp [--update actualizacion.nsp] -o ~/Juegos/juego

Usa nx_extract (de nxnative, con LibHac), que necesita el SDK de .NET:  sudo pacman -S dotnet-sdk
Las llaves tienen que ser las de tu propia consola. Nada de lo extraído va al repositorio.
"""

import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL_DIR = os.path.join(ROOT, "build", "tools", "nx_extract")


def build_tool():
    exe = os.path.join(TOOL_DIR, "nx_extract")
    if os.path.exists(exe):
        return exe
    if not shutil.which("dotnet"):
        sys.exit("falta .NET: instala el SDK con  sudo pacman -S dotnet-sdk")
    sdks = subprocess.run(["dotnet", "--list-sdks"], capture_output=True, text=True).stdout.strip()
    if not sdks:
        sys.exit("hay runtime de .NET pero no el SDK: instálalo con  sudo pacman -S dotnet-sdk")
    print("compilando nx_extract (solo la primera vez)...")
    subprocess.run(["dotnet", "build", os.path.join(ROOT, "tools", "nx_extract"), "-c", "Release", "-o", TOOL_DIR,
                    "-v", "quiet", "-nologo"], check=True)
    return exe


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("juego", help="NSP, NSZ, XCI o XCZ del juego base")
    ap.add_argument("--update", help="NSP de la actualización (opcional)")
    ap.add_argument("--keys", required=True, help="prod.keys sacadas de tu consola")
    ap.add_argument("-o", "--salida", required=True, help="carpeta de salida (fuera del repositorio)")
    ap.add_argument("--sin-romfs", action="store_true", help="solo el ExeFS, sin escribir el RomFS")
    args = ap.parse_args()
    out = os.path.abspath(args.salida)
    if out.startswith(ROOT + os.sep):
        sys.exit("la salida no puede quedar dentro del repositorio: los archivos del juego no se suben")
    exe = build_tool()
    cmd = [exe, args.keys, args.juego, args.update or "-", out, "--manifest" if args.sin_romfs else "--romfs-image"]
    r = subprocess.run(cmd)
    if r.returncode:
        sys.exit(r.returncode)
    print(f"\nlisto. Para recompilarlo:\n  python3 recomp/nxrecomp.py {os.path.join(out, 'ExeFS')} -o {os.path.join(out, 'c')}")


if __name__ == "__main__":
    main()
