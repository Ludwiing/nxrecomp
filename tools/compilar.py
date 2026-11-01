#!/usr/bin/env python3
"""Compila en paralelo el C que generó nxrecomp y lo enlaza con el runtime.

    python3 tools/compilar.py build/multi_rec -o build/multi   [-j 16] [-O1]

Un juego real genera cientos de archivos; cada uno se compila por separado (y solo
se recompila si cambió), así que conviene usar todos los núcleos.
"""

import argparse
import concurrent.futures
import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDL = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl3"], capture_output=True, text=True).stdout.split()
FLAGS = ["-march=x86-64-v3", f"-I{ROOT}/runtime", "-w"] + [f for f in SDL if f.startswith("-I")]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("carpeta", help="la carpeta que escribió nxrecomp.py")
    ap.add_argument("-o", "--salida", required=True)
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("--progreso", action="store_true", help="una línea \"progreso i/n\" por archivo (para la app)")
    ap.add_argument("-O", dest="opt", default="2", help="nivel de optimización de clang (por defecto 2)")
    args = ap.parse_args()
    runtime = ["nxrt.c", "nxmain.c", "nxkern.c", "nxipc.c", "nxservices.c", "nxsvc_am.c", "nxsvc_hid.c", "nxsvc_time.c", "nxsvc_fs.c", "nxsvc_vi.c", "nxdisplay.c", "nxinput.c", "nxaudio.c", "nxsvc_audio.c", "nxsvc_audren.c", "nxgpu.c", "nxgl.c", "nxshader.c", "nxastc.c"]
    srcs = sorted(glob.glob(os.path.join(args.carpeta, "*.c"))) + [os.path.join(ROOT, "runtime", f) for f in runtime]
    objdir = os.path.join(args.carpeta, "obj")
    os.makedirs(objdir, exist_ok=True)
    headers = glob.glob(os.path.join(ROOT, "runtime", "*.h")) + [os.path.join(args.carpeta, "nxgame.h")]
    newest_h = max(os.path.getmtime(h) for h in headers)

    def compile_one(src):
        obj = os.path.join(objdir, os.path.basename(src) + ".o")
        if os.path.exists(obj) and os.path.getmtime(obj) >= max(os.path.getmtime(src), newest_h):
            return obj, None
        r = subprocess.run(["clang", f"-O{args.opt}", *FLAGS, f"-I{args.carpeta}", "-c", src, "-o", obj],
                           capture_output=True, text=True)
        return obj, (r.stderr if r.returncode else None)

    with concurrent.futures.ThreadPoolExecutor(args.j) as pool:
        futs = [pool.submit(compile_one, s) for s in srcs]
        for k, _ in enumerate(concurrent.futures.as_completed(futs)):
            if args.progreso:
                print(f"progreso {k + 1}/{len(srcs)}", flush=True)
        results = [f.result() for f in futs]
    errors = [e for _, e in results if e]
    if errors:
        sys.exit(errors[0])
    subprocess.run(["clang", *[o for o, _ in results], "-lm", "-lpthread", *[f for f in SDL if not f.startswith("-I")], "-lEGL", "-lOpenGL", "-o", args.salida], check=True)
    print(f"{args.salida}: {len(srcs)} archivos compilados con {args.j} hilos")


if __name__ == "__main__":
    main()
