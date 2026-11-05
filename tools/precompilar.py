#!/usr/bin/env python3
"""Compile finished translation shards while nxrecomp continues generating C.

Does not link or replace the game executable. The normal compiler reuses objects.
"""
import argparse
import concurrent.futures
import glob
import os
import time

from compilar import ROOT, compile_source


def ready_sources(folder):
    sources = sorted(glob.glob(os.path.join(folder, "funciones_*.c")))
    # The newest shard may still be open in the translator: always exclude it.
    return [s for s in sources[:-1] if time.time() - os.path.getmtime(s) > 3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("carpeta")
    parser.add_argument("-j", type=int, default=8)
    parser.add_argument("--reverso", action="store_true",
                        help="recorrer los archivos del último al primero (un segundo precompilador que se encuentra con el primero)")
    args = parser.parse_args()
    folder = os.path.abspath(args.carpeta)
    headers = glob.glob(os.path.join(ROOT, "runtime", "*.h")) + [os.path.join(folder, "nxgame.h")]
    newest_h = max(os.path.getmtime(h) for h in headers)
    done = set()
    pending = {}
    limit = min(max(1, args.j), os.cpu_count() or 1)
    print(f"Precompilación activa: hasta {limit} procesos clang; sin reiniciar la traducción", flush=True)
    with concurrent.futures.ThreadPoolExecutor(limit) as pool:
        while True:
            # programa.c is written only after every translation shard is closed.
            finished = os.path.exists(os.path.join(folder, "programa.c"))
            sources = (sorted(glob.glob(os.path.join(folder, "funciones_*.c")))
                       if finished else ready_sources(folder))
            for source in (reversed(sources) if args.reverso else sources):
                if len(pending) >= limit:
                    break
                if source not in done and source not in pending.values():
                    pending[pool.submit(compile_source, source, folder, "2", newest_h)] = source
            completed, _ = concurrent.futures.wait(pending, timeout=2,
                                                  return_when=concurrent.futures.FIRST_COMPLETED)
            for future in completed:
                source = pending.pop(future)
                _, error = future.result()
                if error:
                    raise RuntimeError(error)
                done.add(source)
                print(f"Listo {os.path.basename(source)} · {len(done)} objetos reutilizables", flush=True)
            if finished and not pending and all(s in done for s in sources):
                break
            if not pending:
                time.sleep(2)
    print("Precompilación terminada; la aplicación continúa con el enlace normal", flush=True)


if __name__ == "__main__":
    main()
