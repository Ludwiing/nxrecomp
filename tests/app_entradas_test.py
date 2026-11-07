#!/usr/bin/env python3
"""Prueba de las entradas de NX2Linux que no necesitan llaves, de punta a punta con el motor de la app
(reconocer → extraer → recompilar → compilar → jugar), en carpetas aisladas (XDG_* temporales):

- un homebrew NRO (guest/libnx/basico): nombre de su NACP (ASET) y la misma salida que guest/esperado/basico.txt;
- un juego ya extraído (los módulos NSO de `multi` en exefs/, con romfs/ y control.nacp): title ID de su NPDM,
  RomFS.img armado desde la carpeta, nombre y versión del NACP, y la misma salida que el `multi` de make test;
- una carpeta de NCA sueltos se reconoce como tal, y sin prod.keys da un error claro (no hay NCA de prueba
  sin llaves reales, así que no se extrae).

Uso: python3 tests/app_entradas_test.py   (después de make all; necesita devkitPro para el NRO de libnx)."""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
B = os.path.join(RAIZ, "build")
tmp = tempfile.mkdtemp(prefix="nx2linux-prueba-", dir=B)
for v in ("XDG_DATA_HOME", "XDG_CONFIG_HOME"):
    os.environ[v] = os.path.join(tmp, v.lower())
sys.path.insert(0, os.path.join(RAIZ, "app"))
from nx2linux import motor  # noqa: E402

fallos = 0


def mal(msg):
    global fallos
    fallos += 1
    print("MAL: " + msg)


def construir_y_jugar(rutas, salida_esperada, nombre_esperado=None, version_esperada=None, extra=None):
    origen = motor.clasificar(rutas, None)
    j = motor.nuevo_juego(origen)
    errores = []
    c = motor.Construccion(j, lambda paso, frac, linea: errores.append(linea) if paso == "error" else None)
    c.empezar()
    c.hilo.join()
    j.cargar()
    etiqueta = os.path.basename(os.path.normpath(rutas[0]))
    if errores or not j.listo:
        mal(f"{etiqueta}: no se construyó: {errores}\n" + open(j.registro()).read()[-3000:])
        return None
    if nombre_esperado and j.nombre != nombre_esperado:
        mal(f"{etiqueta}: nombre {j.nombre!r}, se esperaba {nombre_esperado!r}")
    if version_esperada and j.version != version_esperada:
        mal(f"{etiqueta}: versión {j.version!r}, se esperaba {version_esperada!r}")
    env = motor.entorno_juego(j)
    env.update({"NX_SIN_VENTANA": "1", "NX_SIN_AUDIO": "1"}, **(extra or {}))
    r = subprocess.run([j.ejecutable], env=env, cwd=j.carpeta, capture_output=True, timeout=120)
    if r.stdout.decode(errors="replace") != salida_esperada:
        mal(f"{etiqueta}: salida distinta de la esperada:\n{r.stdout.decode(errors='replace')[-1500:]}\n"
            f"{r.stderr.decode(errors='replace')[-1500:]}")
    return j


def nacp(nombre, version):
    raw = bytearray(0x4000)
    raw[0:len(nombre)] = nombre.encode()
    raw[0x3060:0x3060 + len(version)] = version.encode()
    return bytes(raw)


# 1. NRO de libnx
nro = os.path.join(RAIZ, "guest", "libnx", "basico", "basico.nro")
if os.path.isfile(nro):
    if motor.tipo(nro) != "nro":
        mal("un .nro no se reconoce como NRO")
    _, datos_nacp = motor.leer_aset(open(nro, "rb").read())
    nombre_nro = motor.leer_nacp(datos_nacp)[0] if datos_nacp else None
    j = construir_y_jugar([nro], open(os.path.join(RAIZ, "guest", "esperado", "basico.txt")).read(), nombre_nro)
    if j and not fallos:
        print(f"OK: NX2Linux construye y corre un NRO sin llaves ({j.nombre})")
else:
    print("(sin devkitPro no se prueba el NRO de libnx)")

# 2. Juego ya extraído: exefs/ (los NSO de multi), romfs/ y control/control.nacp
antes = fallos
exefs = os.path.join(B, "multi_mod", "exefs")
if not os.path.isdir(exefs):
    mal("falta build/multi_mod/exefs (corre make all)")
else:
    juego = os.path.join(tmp, "Mi juego extraído")
    shutil.copytree(exefs, os.path.join(juego, "exefs"))
    os.makedirs(os.path.join(juego, "romfs", "datos"))
    with open(os.path.join(juego, "romfs", "datos", "nivel1.bin"), "wb") as f:
        f.write(bytes(range(256)) * 64)
    os.makedirs(os.path.join(juego, "control"))
    with open(os.path.join(juego, "control", "control.nacp"), "wb") as f:
        f.write(nacp("Multi de prueba", "1.0.2"))
    if motor.tipo(juego) != "extraido":
        mal("la carpeta de un juego extraído no se reconoce")
    ref = os.path.join(B, "multi.referencia.txt")
    esperado = open(ref).read() if os.path.isfile(ref) else subprocess.run([os.path.join(B, "multi_ref")],
                                                                          capture_output=True, text=True).stdout
    j = construir_y_jugar([juego], esperado, "Multi de prueba", "1.0.2")
    if j:
        img = os.path.join(j.carpeta, "RomFS.img")
        if not os.path.isfile(img) or open(img, "rb").read(8) != b"DMROMFS1":
            mal("el juego extraído no tiene su RomFS.img")
        if not j.id.startswith("05"):
            mal(f"title ID {j.id}: el NPDM de multi no trae ACI0, se esperaba un id local 05...")
    if fallos == antes:
        print("OK: NX2Linux construye y corre un juego ya extraído (ExeFS + romfs/ + control.nacp) sin llaves")

# 3. Carpeta de NCA: se reconoce, y sin llaves lo dice
antes = fallos
ncas = os.path.join(tmp, "dump en NCA")
os.makedirs(ncas)
for n in ("0123456789abcdef0123456789abcdef.nca", "fedcba9876543210fedcba9876543210.cnmt.nca"):
    with open(os.path.join(ncas, n), "wb") as f:
        f.write(struct.pack("<I", 0) * 0x100)
if motor.tipo(ncas) != "ncas":
    mal("una carpeta de NCA no se reconoce")
try:
    motor.clasificar([ncas], None)
    mal("una carpeta de NCA se aceptó sin llaves")
except RuntimeError as e:
    if "prod.keys" not in str(e):
        mal(f"sin llaves, la carpeta de NCA da otro error: {e}")
if fallos == antes:
    print("OK: una carpeta de NCA sueltos se reconoce y pide prod.keys")

shutil.rmtree(tmp, ignore_errors=True)
sys.exit(1 if fallos else 0)
