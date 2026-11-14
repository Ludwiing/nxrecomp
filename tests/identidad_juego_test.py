#!/usr/bin/env python3
"""Identidad de los juegos que se agregan a NX2Linux (Switch 1 y Switch 2), con archivos de prueba sintéticos.

Comprueba que el Title ID sale de la estructura (TOC del .nup, ACI0 del main.npdm, lo que lee el extractor) y no del
nombre del archivo; que un .nup incompleto, de solo actualización o con títulos de juegos distintos no pasa por un juego
base; que el nombre visible no queda en "base" ni en etiquetas como [id] o [v...], y que nombre y Title ID se conservan
al reconocer, construir (extracción del .nup), volver a importar y en el menú (biblioteca y acceso .desktop).
No toca la biblioteca del usuario: todo va a una carpeta temporal.
"""
import os
import struct
import subprocess
import sys
import tempfile
from unittest.mock import patch

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "app"))
from nx2linux import motor  # noqa: E402

BASE = 0x0400ABCD12340000
OTRO = 0x0400FFFF00010000
fallos = []


def mal(msg):
    fallos.append(msg)
    print("MAL:", msg)


def npdm(programa):
    """META mínimo con un ACI0 cuyo id de programa es `programa` (como lo lee recomp/nso.py)."""
    raw = bytearray(0xC0)
    raw[:4] = b"META"
    raw[0x20:0x2B] = b"Application"
    struct.pack_into("<II", raw, 0x70, 0x80, 0x40)
    raw[0x80:0x84] = b"ACI0"
    struct.pack_into("<Q", raw, 0x90, programa)
    return bytes(raw)


NSO = b"NSO0" + bytes(0x40)


def nup(ruta, titulos):
    """Escribe un .nup sin comprimir. `titulos`: [(title id, versión, {"main": bytes, ...})]; cada uno con exefs/ y un
    romfs/ vacío, con el formato que documenta tools/nup.py."""
    carpetas, archivos, hijos, nombres, datos = [], [], [], bytearray(), []

    def nombre(s):
        o = len(nombres)
        nombres.extend(s.encode())
        return o, len(s.encode())

    filas = []
    for tid, ver, exefs in titulos:
        raiz = len(carpetas)
        carpetas.append(None)
        c_exefs = len(carpetas)
        carpetas.append(None)
        c_romfs = len(carpetas)
        carpetas.append(None)
        lista_exefs = []
        for n, contenido in exefs.items():
            lista_exefs.append(len(archivos))
            archivos.append((nombre(n), contenido))
        carpetas[raiz] = (0, 0, [c_exefs, c_romfs])
        carpetas[c_exefs] = (*nombre("exefs"), [("a", i) for i in lista_exefs])
        carpetas[c_romfs] = (*nombre("romfs"), [])
        filas.append((tid, raiz, c_exefs, c_romfs, ver))
    nc, na, nt = len(carpetas), len(archivos), len(titulos)
    tabla_c = []
    for o, n, hs in carpetas:
        primero = len(hijos)
        for h in hs:
            hijos.append(nc + h[1] if isinstance(h, tuple) else h)
        tabla_c.append((o, n, primero, len(hs)))
    p_carp = 0xB0 + 0x40 * nt
    p_arch = p_carp + 24 * nc
    p_hijos = p_arch + 80 * na
    p_nom = p_hijos + 4 * len(hijos)
    p_datos = p_nom + len(nombres)
    out = bytearray(p_datos)
    out[:4] = b"NUP\x01"
    out[0x80:0x84] = b"TOC\x01"
    struct.pack_into("<IQQQQ", out, 0x84, nt, nc, na, p_nom - 0x80, len(nombres))
    for i, (tid, raiz, ce, cr, ver) in enumerate(filas):
        struct.pack_into("<QIIII", out, 0xB0 + 0x40 * i, tid, raiz, ce, cr, ver)
    for i, (o, n, primero, cant) in enumerate(tabla_c):
        struct.pack_into("<4IQ", out, p_carp + 24 * i, o, n, primero, cant, 0)
    off = p_datos
    for i, ((o, n), contenido) in enumerate(archivos):
        struct.pack_into("<IIQQQ", out, p_arch + 80 * i, o, n, len(contenido), off, len(contenido))
        datos.append(contenido)
        off += len(contenido)
    struct.pack_into(f"<{len(hijos)}I", out, p_hijos, *hijos)
    out[p_nom:p_datos] = nombres
    out += b"".join(datos)
    struct.pack_into("<Q", out, 0x40, len(out))
    with open(ruta, "wb") as f:
        f.write(out)
    return ruta


def extraido(ruta, programa):
    exefs = os.path.join(ruta, "exefs")
    os.makedirs(exefs, exist_ok=True)
    with open(os.path.join(exefs, "main"), "wb") as f:
        f.write(NSO)
    with open(os.path.join(exefs, "main.npdm"), "wb") as f:
        f.write(npdm(programa))
    os.makedirs(os.path.join(ruta, "romfs"), exist_ok=True)
    return ruta


def falla(fn, texto, etiqueta):
    try:
        r = fn()
    except RuntimeError as e:
        if texto not in str(e):
            mal(f"{etiqueta}: el error no dice {texto!r}: {e}")
        return str(e)
    mal(f"{etiqueta}: se aceptó ({r!r})")
    return None


def main():
    tmp = tempfile.mkdtemp(prefix="nx2linux-identidad-")
    juegos, apps = os.path.join(tmp, "biblioteca"), os.path.join(tmp, "apps")
    completo = {"main": NSO, "main.npdm": npdm(BASE), "sdk": NSO}
    p = patch.object(motor, "carpeta_juegos", return_value=juegos)
    p.start()
    patch.object(motor, "APPS_DIR", apps).start()
    patch.object(motor, "motor_especial", return_value=None).start()

    # 1. Un .nup válido (juego base + actualización) con el nombre "de tienda" lleva su Title ID y un nombre limpio.
    valido = nup(os.path.join(tmp, f"Juego Bonito [{BASE:016x}] [v131072].nup"),
                 [(BASE, 65536, completo), (BASE | 0x800, 131072, {"main": NSO, "main.npdm": npdm(BASE)})])
    origen = motor.clasificar([valido], None)
    if origen["title"] != f"{BASE:016x}":
        mal(f"nup válido: title {origen['title']}")
    if origen.get("nombre") != "Juego Bonito":
        mal(f"nup válido: nombre {origen.get('nombre')!r}, se esperaba 'Juego Bonito'")

    # 2. Un .nup que solo trae la actualización no es un juego base (antes: base = min(ids) & ~0xFFF).
    solo_update = nup(os.path.join(tmp, "update.nup"), [(BASE | 0x800, 131072, {"main": NSO, "main.npdm": npdm(BASE)})])
    if motor.title_id(solo_update, None) != BASE | 0x800:
        mal("nup de solo actualización: su title ID no termina en 800")
    falla(lambda: motor.clasificar([solo_update], None), "falta el juego base", "nup de solo actualización")

    # 3. Incompleto: sin main.npdm ni main; 4. NPDM de otro juego; 5. títulos de dos juegos distintos.
    falla(lambda: motor.title_id(nup(os.path.join(tmp, "sin-npdm.nup"), [(BASE, 0, {"main": NSO})]), None),
          "main.npdm", "nup sin main.npdm")
    falla(lambda: motor.title_id(nup(os.path.join(tmp, "sin-main.nup"), [(BASE, 0, {"main.npdm": npdm(BASE)})]), None),
          "main", "nup sin main")
    falla(lambda: motor.title_id(nup(os.path.join(tmp, "npdm-ajeno.nup"),
                                     [(BASE, 0, {"main": NSO, "main.npdm": npdm(OTRO)})]), None),
          "no coincide", "nup con NPDM de otro juego")
    falla(lambda: motor.title_id(nup(os.path.join(tmp, "mezcla.nup"),
                                     [(BASE, 0, completo), (OTRO | 0x800, 1, {"main": NSO, "main.npdm": npdm(OTRO)})]),
                                 None), "juegos distintos", "nup con dos juegos")

    # 6. Nombre externo engañoso: el archivo dice ser otro juego (otro Title ID entre corchetes).
    engano = nup(os.path.join(tmp, f"Otro Juego Famoso [{OTRO:016x}].nup"), [(BASE, 0, completo)])
    o = motor.clasificar([engano], None)
    if o["title"] != f"{BASE:016x}":
        mal(f"nombre engañoso: se tomó el title {o['title']} del nombre del archivo")
    if o.get("nombre") != f"Juego {BASE:016x}":
        mal(f"nombre engañoso: se mostró {o.get('nombre')!r}, que contradice el Title ID interno")

    # 6b. Un NSP con [id] en el nombre: manda lo que lee el extractor, no el nombre.
    nsp = os.path.join(tmp, f"Falso [{OTRO:016x}].nsp")
    with open(nsp, "wb") as f:
        f.write(b"PFS0" + bytes(0x200))
    leido = subprocess.CompletedProcess([], 0, stdout=f"title {BASE:016x}\n", stderr="")
    with patch.object(motor, "extractor", return_value="nx_extract"), \
         patch.object(motor.subprocess, "run", return_value=leido) as run:
        t = motor.title_id(nsp, "prod.keys")
    if t != BASE or not run.called:
        mal(f"NSP con [id] engañoso: title {t:016x}, extractor llamado: {run.called}")

    # 7. Un dump extraído de Switch 2 en <Juego_v1.1.1>/base: nombre de la carpeta del juego, no "base".
    base_dir = extraido(os.path.join(tmp, "JuegoEjemplo_v1.1.1", "base"), BASE)
    o = motor.clasificar([base_dir], None)
    if o["title"] != f"{BASE:016x}" or o.get("nombre") != "Juego Ejemplo":
        mal(f"carpeta 'base': title {o['title']}, nombre {o.get('nombre')!r}")

    # 7b. Etiquetas de nombres de dump reales; un título con "Game" en medio no se recorta.
    for crudo, esperado in (("Un Juego Deportivo Switch 2 BASW Game", "Un Juego Deportivo"),
                            ("Game Builder Garage [0100fa500b128000][v0]", "Game Builder Garage"),
                            ("Una Aventura Clasica [0100ABCD56780000]",
                             "Una Aventura Clasica"),
                            ("Juego-Ejemplo-Switch-2", "Juego Ejemplo"),
                            ("Game-Builder-Garage-Switch-2", "Game Builder Garage"),
                            ("Super Game Party BASE Game", "Super Game Party"),
                            ("Spider-Man Remastered [v0]", "Spider-Man Remastered"),
                            ("Spider-Man", "Spider-Man")):
        if motor._limpiar_nombre(crudo) != esperado:
            mal(f"limpiar {crudo!r}: {motor._limpiar_nombre(crudo)!r}, se esperaba {esperado!r}")
    if motor.nombre_externo(os.path.join(tmp, "Juego Raro_v1.2.0", "updates", "65536"), BASE) != "Juego Raro":
        mal("updates/<versión>: no se tomó el nombre de la carpeta del juego")

    # 7c. Con guiones en el nombre manda igual lo interno: el Title ID sale del TOC y del NPDM, no del archivo.
    guiones = nup(os.path.join(tmp, "Juego-Ejemplo-Switch-2.nup"), [(BASE, 0, completo)])
    o = motor.clasificar([guiones], None)
    if (o["title"], o.get("nombre")) != (f"{BASE:016x}", "Juego Ejemplo"):
        mal(f"nup con guiones: {(o['title'], o.get('nombre'))}")
    guiones_falso = nup(os.path.join(tmp, f"Game-Builder-Garage-Switch-2 [{OTRO:016x}].nup"), [(BASE, 0, completo)])
    o = motor.clasificar([guiones_falso], None)
    if (o["title"], o.get("nombre")) != (f"{BASE:016x}", f"Juego {BASE:016x}"):
        mal(f"nup con guiones y [id] ajeno: {(o['title'], o.get('nombre'))}")
    falla(lambda: motor.title_id(nup(os.path.join(tmp, f"Game-Builder-Garage [{BASE:016x}].nup"),
                                     [(BASE, 0, {"main": NSO, "main.npdm": npdm(OTRO)})]), None),
          "no coincide", "nup con [id] correcto en el nombre pero NPDM de otro juego")

    with patch.dict(os.environ, {"HOME": os.path.join(tmp, "casa")}):
        suelto = os.path.join(tmp, "casa", "Downloads", "base.nup")
        if motor.nombre_externo(suelto, BASE) != f"Juego {BASE:016x}":
            mal(f"base.nup en Downloads: {motor.nombre_externo(suelto, BASE)!r}, no debe tomar 'Downloads'")

    # 8. Se conserva al importar, construir (extraer el .nup), volver a importar y en el menú.
    j = motor.nuevo_juego(motor.clasificar([valido], None))
    c = object.__new__(motor.Construccion)
    c.juego = j
    c._linea = lambda *a, **k: None
    c._ejecutar = lambda paso, cmd, *a, **k: subprocess.run(cmd, check=True, capture_output=True)
    c._extraer()
    j = motor.Juego(j.carpeta)
    if (j.id, j.nombre, j.version) != (f"{BASE:016x}", "Juego Bonito", "v131072"):
        mal(f"tras extraer: {(j.id, j.nombre, j.version)}")
    if not os.path.isfile(os.path.join(j.carpeta, "ExeFS", "main.npdm")):
        mal("tras extraer: falta ExeFS/main.npdm")
    j.datos["nombre"] = "Nombre del NACP"
    j.datos["identidad"]["nombre_fuente"] = "nacp"
    j.guardar()
    motor.nuevo_juego(motor.clasificar([valido], None))
    menu = {x.id: x.nombre for x in motor.biblioteca()}
    if menu.get(f"{BASE:016x}") != "Nombre del NACP":
        mal(f"volver a importar cambió el nombre verificado: {menu}")
    motor.crear_acceso(motor.Juego(j.carpeta))
    with open(os.path.join(apps, f"nx2linux-{BASE:016x}.desktop")) as f:
        if "Name=Nombre del NACP\n" not in f.read():
            mal("el acceso .desktop no lleva el nombre del juego")

    # 9. Una ficha vieja con nombre "base" se muestra con el nombre del dump, sin reescribir su juego.json.
    viejo = motor.Juego(os.path.join(juegos, f"{OTRO:016x}"))
    viejo.datos.update({"nombre": "base", "origen": {"base": base_dir, "update": None, "dlc": []}})
    viejo.guardar()
    antes = open(os.path.join(viejo.carpeta, "juego.json")).read()
    if motor.Juego(viejo.carpeta).nombre != "Juego Ejemplo":
        mal(f"ficha vieja: se muestra {motor.Juego(viejo.carpeta).nombre!r}")
    if open(os.path.join(viejo.carpeta, "juego.json")).read() != antes:
        mal("ficha vieja: mostrarla reescribió juego.json")

    # 10. La actualización sola en .nup se agrega a su juego ya instalado.
    with patch.object(motor, "llaves_de", return_value="prod.keys"):
        nuevo = motor.clasificar_contenido(motor.Juego(j.carpeta), [solo_update])
    if nuevo["update"] != solo_update:
        mal(f"update .nup sobre el juego instalado: {nuevo}")

    patch.stopall()
    import shutil
    shutil.rmtree(tmp, ignore_errors=True)
    if fallos:
        sys.exit(1)
    print("OK: identidad de juegos (Title ID por estructura, .nup incompleto/ambiguo, nombre engañoso, nombre conservado)")


if __name__ == "__main__":
    main()
