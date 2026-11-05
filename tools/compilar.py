#!/usr/bin/env python3
"""Compila en paralelo el C que generó nxrecomp y lo enlaza con el runtime.

    python3 tools/compilar.py build/multi_rec -o build/multi   [-j 16] [-O1]

Un juego real genera cientos de archivos; cada uno se compila por separado (y solo
se recompila si cambió). Se usan hasta ocho tareas para limitar la presión de memoria;
con -j se puede elegir otro límite.
La carpeta generada puede contener compilacion.json con hilos, optimizacion y
reutilizar_optimizados. Las opciones explícitas de la terminal tienen prioridad.
"""

import argparse
import concurrent.futures
import glob
import fcntl
import hashlib
import json
import tempfile
import shutil
from dispatch_table import prepare as prepare_dispatch
from branch_local import prepare as prepare_br, prepare_tail
import os
import subprocess
import sys
import signal
import threading

cancelled = threading.Event()
active_compilers = set()


def terminate_process(process):
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass


def cancel_compilation(signum, frame):
    cancelled.set()
    for process in tuple(active_compilers):
        terminate_process(process)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDL = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl3"], capture_output=True, text=True).stdout.split()
FLAGS = ["-march=x86-64-v3", f"-I{ROOT}/runtime", "-w"] + [f for f in SDL if f.startswith("-I")]


def compile_source(src, carpeta, opt, newest_h, reuse_optimized=False):
    """Share completed objects with the live precompiler, without concurrent writes."""
    if cancelled.is_set():
        return "", "Compilación cancelada"
    objdir = os.path.join(carpeta, "obj")
    os.makedirs(objdir, exist_ok=True)
    obj = os.path.join(objdir, os.path.basename(src) + ".o")
    command = ["clang", f"-O{opt}", *FLAGS, f"-I{carpeta}", "-c", src]
    if os.path.dirname(os.path.abspath(src)).startswith(os.path.join(ROOT, "runtime", "nvn")):
        # La capa NVN y el backend Vulkan: vk.h incluye Vulkan Memory Allocator (runtime/nvn/third_party/vma). Solo
        # aquí, para no cambiar la firma de compilación de los objetos del juego.
        vma = f"-I{ROOT}/runtime/nvn/third_party/vma"
        command = (["clang++", f"-O{opt}", "-std=c++17", *FLAGS, vma, "-c", src] if src.endswith(".cpp")
                   else ["clang", f"-O{opt}", *FLAGS, vma, f"-I{carpeta}", "-c", src])
    signature = hashlib.sha256(json.dumps(command).encode()).hexdigest()
    allowed_signatures = {signature}
    if reuse_optimized and opt == "0":
        for level in ("1", "2", "3"):
            optimized = [command[0], f"-O{level}", *command[2:]]
            allowed_signatures.add(hashlib.sha256(json.dumps(optimized).encode()).hexdigest())
    stamp = obj + ".command"
    with open(obj + ".lock", "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if os.path.basename(src).startswith(("funciones_", "pistas_extra")):
            prepare_br(src)
            prepare_tail(src)
        original = os.stat(src)
        try:
            with open(stamp) as saved_command:
                saved_signature = saved_command.read()
            valid = (os.path.getmtime(obj) >= max(original.st_mtime, newest_h)
                     and saved_signature in allowed_signatures)
        except OSError:
            valid = False
        if valid:
            return obj, None
        fd, temporary = tempfile.mkstemp(prefix=".compilar-", suffix=".o", dir=objdir)
        os.close(fd)
        try:
            process = subprocess.Popen([*command, "-o", temporary], stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True, start_new_session=True)
            active_compilers.add(process)
            try:
                if cancelled.is_set():
                    terminate_process(process)
                _, stderr = process.communicate()
            finally:
                active_compilers.discard(process)
            if cancelled.is_set():
                return obj, "Compilación cancelada"
            if process.returncode:
                return obj, stderr or f"clang terminó con código {process.returncode}"
            after = os.stat(src)
            if (original.st_size, original.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                return obj, f"El archivo cambió durante la compilación: {src}"
            os.replace(temporary, obj)
            with open(stamp, "w") as output:
                output.write(signature)
            return obj, None
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("carpeta", help="la carpeta que escribió nxrecomp.py")
    ap.add_argument("-o", "--salida", required=True)
    ap.add_argument("-j", type=int, default=None)
    ap.add_argument("--progreso", action="store_true", help="una línea \"progreso i/n\" por archivo (para la app)")
    ap.add_argument("-O", dest="opt", default=None, help="nivel de optimización de clang (por defecto 2)")
    ap.add_argument("--reutilizar-optimizados", action="store_true", default=None,
                    help="con -O0, conserva objetos vigentes ya compilados con -O1/2/3")
    args = ap.parse_args()
    # A per-game profile also applies when the GUI invokes this compiler.
    profile_path = os.path.join(args.carpeta, "compilacion.json")
    try:
        with open(profile_path) as profile_file:
            profile = json.load(profile_file)
    except FileNotFoundError:
        profile = {}
    except (OSError, ValueError) as e:
        ap.error(f"perfil de compilación inválido: {e}")
    if not isinstance(profile, dict):
        ap.error("el perfil de compilación debe ser un objeto JSON")
    args.j = args.j if args.j is not None else profile.get("hilos", min(8, os.cpu_count() or 1))
    args.opt = args.opt if args.opt is not None else str(profile.get("optimizacion", "2"))
    args.reutilizar_optimizados = args.reutilizar_optimizados or profile.get("reutilizar_optimizados", False)
    if type(args.j) is not int or args.opt not in ("0", "1", "2", "3", "s", "z") or type(args.reutilizar_optimizados) is not bool:
        ap.error("perfil inválido: comprueba hilos, optimizacion y reutilizar_optimizados")
    if profile:
        print(f"Perfil de compilación: -O{args.opt}, {args.j} hilos", flush=True)
    if args.j < 1:
        ap.error("-j debe ser mayor que cero")
    signal.signal(signal.SIGTERM, cancel_compilation)
    signal.signal(signal.SIGINT, cancel_compilation)
    prepare_dispatch(os.path.join(args.carpeta, "programa.c"))
    runtime = ["nxrt.c", "nxmain.c", "nxkern.c", "nxipc.c", "nxservices.c", "nxsvc_am.c", "nxsvc_hid.c", "nxsvc_time.c", "nxsvc_fs.c", "nxsvc_vi.c", "nxdisplay.c", "nxinput.c", "nxaudio.c", "nxsvc_audio.c", "nxsvc_audren.c", "nxaudren.c", "nxsvc_acc.c", "nxsvc_otros.c", "nxsvc_opus.c", "nxgpu.c", "nxgl.c", "nxshader.c", "nxastc.c", "nxnvn.c",
               ]
    # La capa NVN y el backend Vulkan de totk-pc-recomp (runtime/nvn/, con sus archivos de adaptación)
    nvn = os.path.join(ROOT, "runtime", "nvn")
    runtime += [os.path.relpath(f, os.path.join(ROOT, "runtime"))
                for f in sorted(glob.glob(os.path.join(nvn, "*.c")) + glob.glob(os.path.join(nvn, "platform", "*.c")))]
    runtime.append("nvn/vk_vma.cpp")
    srcs = sorted(glob.glob(os.path.join(args.carpeta, "*.c"))) + [os.path.join(ROOT, "runtime", f) for f in runtime]
    objdir = os.path.join(args.carpeta, "obj")
    os.makedirs(objdir, exist_ok=True)
    headers = glob.glob(os.path.join(ROOT, "runtime", "*.h")) + [os.path.join(args.carpeta, "nxgame.h")]
    newest_h = max(os.path.getmtime(h) for h in headers)

    # Los encabezados de runtime/nvn/ solo los incluye esa capa: si cambian, se recompila ella y no todo el juego.
    nvn_dir = os.path.join(ROOT, "runtime", "nvn")
    newest_nvn_h = max([newest_h] + [os.path.getmtime(h) for h in glob.glob(os.path.join(nvn_dir, "**", "*.h"), recursive=True)])

    def compile_one(src):
        h = newest_nvn_h if os.path.dirname(src).startswith(nvn_dir) else newest_h
        return compile_source(src, args.carpeta, args.opt, h, args.reutilizar_optimizados)

    with concurrent.futures.ThreadPoolExecutor(args.j) as pool:
        futs = [pool.submit(compile_one, s) for s in srcs]
        for k, _ in enumerate(concurrent.futures.as_completed(futs)):
            if args.progreso:
                print(f"progreso {k + 1}/{len(srcs)}", flush=True)
        results = [f.result() for f in futs]
    errors = [e for _, e in results if e]
    if errors:
        sys.exit(errors[0])
    destination = os.path.abspath(args.salida)
    fd, temporary = tempfile.mkstemp(prefix=".enlazar-", dir=os.path.dirname(destination))
    os.close(fd)
    try:
        linker = ["-fuse-ld=lld", f"-Wl,--threads={args.j}"] if shutil.which("ld.lld") else []
        process = subprocess.Popen(["clang", *linker, *[o for o, _ in results], "-lm", "-lpthread", *[f for f in SDL if not f.startswith("-I")], "-lEGL", "-lOpenGL", "-lopus", "-lvulkan", "-lstdc++", "-o", temporary], start_new_session=True)
        active_compilers.add(process)
        try:
            if cancelled.is_set():
                terminate_process(process)
            rc = process.wait()
        finally:
            active_compilers.discard(process)
        if cancelled.is_set():
            sys.exit("Compilación cancelada")
        if rc:
            raise subprocess.CalledProcessError(rc, process.args)
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print(f"{args.salida}: {len(srcs)} archivos compilados con {args.j} hilos")


if __name__ == "__main__":
    main()
