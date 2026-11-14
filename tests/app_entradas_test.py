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
import time
from types import SimpleNamespace
from unittest.mock import patch

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


def probar_framegen_env():
    """Framegen y telemetría por juego deben dominar variables heredadas."""
    keys = ("TOTK_FRAMEGEN", "TOTK_PERF_OVERLAY")
    anterior = {k: os.environ.get(k) for k in keys}
    try:
        os.environ.update(TOTK_FRAMEGEN="on", TOTK_PERF_OVERLAY="full")
        j = SimpleNamespace(
            id="framegen-env-test", nombre="Framegen env test", carpeta=os.path.join(tmp, "framegen-env"),
            datos={"ajustes": {"renderer": "vulkan", "modo": "base", "idioma": "es-419",
                               "pantalla_completa": False, "audio": True, "generacion_cuadros": False,
                               "telemetria": "off", "variables": ""}},
        )
        env = motor.entorno_juego(j)
        sm86 = os.path.join(RAIZ, "tools", "sm86_programa.py")
        if os.path.isfile(sm86) and env.get("NX_SM86_PROGRAMA") != sm86:
            mal("el lanzador no pasa la ruta local de tools/sm86_programa.py al runtime")
        if env["TOTK_FRAMEGEN"] != "off":
            mal("generación apagada: el entorno heredado prevaleció sobre el ajuste del juego")
        if env["TOTK_PERF_OVERLAY"] != "off":
            mal("telemetría apagada: el entorno heredado prevaleció sobre el ajuste del juego")
        j.datos["ajustes"]["generacion_cuadros"] = True
        j.datos["ajustes"]["telemetria"] = "fps"
        env = motor.entorno_juego(j)
        if env["TOTK_FRAMEGEN"] != "on":
            mal("generación activada: TOTK_FRAMEGEN no llegó al runtime")
        if env["TOTK_PERF_OVERLAY"] != "fps":
            mal("telemetría FPS: el modo seleccionado no llegó al runtime")
        j.datos["ajustes"]["telemetria"] = "full"
        if motor.entorno_juego(j)["TOTK_PERF_OVERLAY"] != "full":
            mal("telemetría detallada: no se habilitó la medición CPU/GPU")
        j.datos["ajustes"]["variables"] = "NX_SM86_PROGRAMA=/tmp/sm86-personal.py"
        if motor.entorno_juego(j).get("NX_SM86_PROGRAMA") != "/tmp/sm86-personal.py":
            mal("NX_SM86_PROGRAMA específico del juego no prevalece sobre el valor predeterminado")
    finally:
        for k, v in anterior.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    if not fallos:
        print("OK: framegen y telemetría controlan el runtime NVN nativo")


probar_framegen_env()


def probar_modulos_ro():
    """Los NRO que el juego arma en memoria: el runtime los vuelca en <juego>/nro y, si hay uno que la última
    recompilación no tradujo, la app pide recompilar (como con las pistas)."""
    carpeta = os.path.join(tmp, "modulos-ro")
    j = SimpleNamespace(id="modulos-ro-test", nombre="Módulos ro", carpeta=carpeta,
                        datos={"ajustes": {"renderer": "opengl", "modo": "base", "idioma": "es-419",
                                           "pantalla_completa": False, "audio": True, "generacion_cuadros": False,
                                           "telemetria": "off", "variables": ""}})
    if motor.entorno_juego(j).get("NX_VOLCAR_NRO") != os.path.join(carpeta, "nro"):
        mal("NX_VOLCAR_NRO no apunta a <juego>/nro")
    if motor.modulos_ro_nuevos(j) != 0:
        mal("sin carpeta nro no hay módulos nuevos")
    os.makedirs(os.path.join(carpeta, "nro"), exist_ok=True)
    a, b = "a" * 64 + ".nro", "b" * 64 + ".nro"
    for n in (a, "nro-1234.tmp", "notas.txt"):
        open(os.path.join(carpeta, "nro", n), "wb").close()
    if motor.modulos_ro_nuevos(j) != 1:
        mal("un <sha256>.nro sin traducir debe contar (y los temporales no)")
    j.datos["modulos_ro_usados"] = [a]
    if motor.modulos_ro_nuevos(j) != 0:
        mal("el NRO ya traducido no debe pedir otra recompilación")
    open(os.path.join(carpeta, "nro", b), "wb").close()
    if motor.modulos_ro_nuevos(j) != 1:
        mal("un segundo NRO volcado debe pedir recompilar")
    if not fallos:
        print("OK: NRO volcados en <juego>/nro y aviso de recompilación")


probar_modulos_ro()


def probar_ejecutable_nativo():
    """NX2Linux compila a <carpeta>/juego y lo abre desde la carpeta del juego con el entorno nativo."""
    carpeta = os.path.join(tmp, "0100000000c0ffee")
    os.makedirs(carpeta, exist_ok=True)
    j = motor.Juego(carpeta)
    j.datos.update({
        "nombre": "Nativo", "version": None,
        "pasos": {clave: time.strftime("%Y-%m-%d %H:%M:%S") for clave, _, _ in motor.PASOS},  # motor.py compara la fecha con las fuentes
        "ajustes": {"renderer": "vulkan", "modo": "base", "idioma": "es-419",
                    "pantalla_completa": False, "audio": True, "generacion_cuadros": False,
                    "telemetria": "off", "variables": "NX_TRAZA_GL=1"},
    })
    j.guardar()
    j = motor.Juego(carpeta)
    nativo = os.path.join(carpeta, "juego")
    if j.ejecutable_nativo != nativo or j.ejecutable != nativo:
        mal("el ejecutable del juego no es el binario nativo <carpeta>/juego")
    if j.listo:
        mal("el juego figura listo sin binario nativo")

    llamadas = []
    build = SimpleNamespace(juego=j, _ejecutar=lambda paso, args, *_: llamadas.append((paso, args)))
    motor.Construccion._compilar(build)
    if not llamadas or llamadas[-1][1][llamadas[-1][1].index("-o") + 1] != nativo:
        mal("la compilación NX2Linux no escribe el binario nativo")

    with open(nativo, "w") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(nativo, 0o755)
    if j.listo:  # paso_hecho("recompilar") exige el C generado (motor.py), no solo el marcador
        mal("el juego figura listo sin c/nxgame.h ni c/programa.c")
    os.makedirs(os.path.join(carpeta, "c"), exist_ok=True)
    for nombre in ("nxgame.h", "programa.c"):
        open(os.path.join(carpeta, "c", nombre), "w").close()
    if not j.listo or j.ejecutable != nativo:
        mal("el binario nativo no quedó como ejecutable del juego")
    with patch.object(motor.subprocess, "Popen") as popen:
        motor.jugar(j, salida=object())
    args, kwargs = popen.call_args
    env = kwargs["env"]
    if args[0] != [nativo] or kwargs["cwd"] != j.carpeta:
        mal("el juego no se lanzó desde el binario nativo en su carpeta")
    if env.get("TOTK_FRAMEGEN") != "off" or env.get("TOTK_PERF_OVERLAY") != "off":
        mal("el entorno nativo no refleja los ajustes del juego")
    if env.get("NX_TRAZA_GL") != "1":
        mal("las variables extra del juego no llegaron al entorno")
    # Un ejecutable anterior al runtime no se lanza aunque la ventana tenga "Jugar" activo de antes.
    runtime_c = os.path.join(motor.ROOT, "runtime", "nxrt.c")
    viejo = os.path.getmtime(runtime_c) - 60
    os.utime(nativo, (viejo, viejo))
    with patch.object(motor.subprocess, "Popen") as popen:
        try:
            motor.jugar(j, salida=object())
            mal("se lanzó un ejecutable compilado antes del runtime actual")
        except motor.EjecutableObsoleto:
            pass
        if popen.called:
            mal("el ejecutable obsoleto llegó a lanzarse")
    if not fallos:
        print("OK: el juego se compila y se abre con el binario nativo de NX2Linux")


probar_ejecutable_nativo()


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
