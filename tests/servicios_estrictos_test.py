#!/usr/bin/env python3
"""NX_SERVICIOS_ESTRICTOS=1: un comando que la tabla de servicios no conoce (runtime/nxsvc_otros.c) responde
sf::cmif::ResultUnknownCommandId (0x1ba0a), como Horizon; sin la variable, éxito con ceros (lo de siempre). Usa
build/servicios (guest/servicios.c): solo cambia la línea del comando 0x7777. Código propio (MIT)."""
import os
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe = sys.argv[1]
esperado = open(os.path.join(RAIZ, "guest", "esperado", "servicios.txt")).read()
linea = "IParentalControlService comando 0x7777 (no existe): 0x0\n"
if linea not in esperado:
    print("MAL: guest/esperado/servicios.txt no tiene la línea del comando 0x7777")
    sys.exit(1)
env = dict(os.environ, NX_ROMFS=os.path.join(RAIZ, "build", "romfs_prueba.img"), NX_SERVICIOS_ESTRICTOS="1")
r = subprocess.run([exe], capture_output=True, text=True, env=env, timeout=60)
quiere = esperado.replace(linea, linea.replace(": 0x0", ": 0x1ba0a"))
fallos = []
if r.returncode != 0:
    fallos.append(f"código {r.returncode}")
if r.stdout != quiere:
    fallos.append("la salida no es la esperada con el modo estricto:\n" + r.stdout)
if "(UnknownCommandId) por NX_SERVICIOS_ESTRICTOS" not in r.stderr:
    fallos.append("falta el aviso en stderr")
for f in fallos:
    print("MAL: " + f)
if not fallos:
    print("OK: NX_SERVICIOS_ESTRICTOS responde 0x1ba0a (UnknownCommandId) a un comando desconocido de la tabla; "
          "Initialize sigue en 0x0")
sys.exit(1 if fallos else 0)
