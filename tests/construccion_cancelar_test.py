#!/usr/bin/env python3
"""Cancelar una construcción de NX2Linux no deja procesos huérfanos.

nxrecomp.py emite el C con un multiprocessing.Pool (fork). Si Construccion.cancelar() solo termina al proceso
principal, los trabajadores siguen calculando su bloque y, al devolverlo por la tubería del pool que ya nadie
lee, mueren con BrokenPipeError (las trazas acababan en registro.txt de Nintendo Switch Sports Resort). La prueba
corre por Construccion._ejecutar un script con el mismo patrón, cancela y comprueba que ningún trabajador
sobrevive ni escribe BrokenPipeError.

Uso: python3 tests/construccion_cancelar_test.py"""
import io
import os
import sys
import tempfile
import threading
import time

RAIZ = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(RAIZ, "app"))
from nx2linux import motor  # noqa: E402

SCRIPT = r'''
import multiprocessing, os, sys, time
pids = sys.argv[1]
def trabajo(i):
    with open(pids, "a") as f:
        f.write(f"{os.getpid()}\n")
    time.sleep(3)
    return "x" * 100000
if __name__ == "__main__":
    print("empieza", flush=True)
    with multiprocessing.get_context("fork").Pool(4) as pool:
        for r in pool.imap_unordered(trabajo, range(4)):
            print("bloque", flush=True)
'''


def vivo(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            return f.read().split()[2] != "Z"
    except OSError:
        return False


def main():
    tmp = tempfile.mkdtemp(prefix="nx2linux-cancelar-")
    script, pids = os.path.join(tmp, "pool.py"), os.path.join(tmp, "pids")
    with open(script, "w") as f:
        f.write(SCRIPT)
    c = motor.Construccion(juego=type("J", (), {"id": "prueba"})(), avisar=lambda *a: None)
    c.log = io.StringIO()
    error = {}

    def correr():
        try:
            c._ejecutar("recompilar", [sys.executable, script, pids])
        except motor.Cancelado:
            error["cancelado"] = True
    hilo = threading.Thread(target=correr)
    hilo.start()
    limite = time.time() + 10
    while time.time() < limite and (not os.path.exists(pids) or len(open(pids).read().split()) < 4):
        time.sleep(0.05)
    trabajadores = [int(p) for p in open(pids).read().split()]
    c.cancelar()
    hilo.join(15)
    time.sleep(4)  # más que el sleep(3) de cada trabajador: si siguen vivos, ya habrán intentado devolver el bloque
    vivos = [p for p in trabajadores if vivo(p)]
    log = c.log.getvalue()
    fallos = 0
    if hilo.is_alive() or not error.get("cancelado"):
        print("FALLO: _ejecutar no terminó con Cancelado")
        fallos += 1
    if vivos:
        print(f"FALLO: trabajadores del pool siguen vivos tras cancelar: {vivos}")
        fallos += 1
    if "BrokenPipeError" in log:
        print("FALLO: un trabajador huérfano escribió BrokenPipeError en el registro")
        fallos += 1
    print("construccion_cancelar: " + ("FALLO" if fallos else "ok") + f" ({len(trabajadores)} trabajadores)")
    return 1 if fallos else 0


sys.exit(main())
