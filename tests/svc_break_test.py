#!/usr/bin/env python3
"""svcBreak sin NotificationOnlyFlag: el proceso termina con código 1 y deja razón, hilo, lr/sp/fp y el resultado de
diagAbortWithResult; un búfer de resultado no mapeado no provoca un segundo fallo (SIGSEGV, código 139). El caso con
el bit de aviso (sigue) es guest/romper.c, comparado con guest/esperado/romper.txt. Código propio (MIT).

    python3 tests/svc_break_test.py build/romper_abort build/romper_ilegible"""
import subprocess
import sys

fallos = 0


def mal(m):
    global fallos
    fallos += 1
    print("MAL: " + m)


def correr(exe):
    return subprocess.run([exe], capture_output=True, text=True, timeout=60)


r = correr(sys.argv[1])
if r.returncode != 1:
    mal(f"romper_abort: código {r.returncode}, se esperaba 1")
for texto in ("antes del Break",):
    if texto not in r.stdout:
        mal(f"romper_abort: falta {texto!r} en stdout: {r.stdout!r}")
for texto in ("svcBreak(0x1 Assert", "hilo ", "lr=0x", "romper_abort+0x", "sp=0x", "resultado 0x1234 (2052-0009)",
              "el proceso termina con código 1"):
    if texto not in r.stderr:
        mal(f"romper_abort: falta {texto!r} en stderr: {r.stderr!r}")
if "no debería llegar" in r.stdout:
    mal("romper_abort: siguió después de un Break que no es aviso")

r = correr(sys.argv[2])
if r.returncode != 1:
    mal(f"romper_ilegible: código {r.returncode} (139 = falló dentro del diagnóstico), se esperaba 1")
if "resultado ilegible en 0x10" not in r.stderr or "svcBreak(0x0 Panic" not in r.stderr:
    mal(f"romper_ilegible: stderr {r.stderr!r}")

if not fallos:
    print("OK: svcBreak que no es aviso termina con código 1 y diagnóstico (razón, hilo, lr/sp/fp, resultado), sin "
          "segundo fallo con un búfer ilegible")
sys.exit(1 if fallos else 0)
