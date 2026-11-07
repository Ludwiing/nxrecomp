"""El motor de NX2Linux: la biblioteca de juegos y los pasos que convierten tu juego en un programa de Linux.

No depende de GTK: la ventana (ventana.py) y la línea de órdenes (python3 -m nx2linux) lo usan igual.
Cada juego vive en su propia carpeta, fuera del repositorio:

    ~/.local/share/nx2linux/juegos/<title id>/
        juego.json      nombre, versión, archivos de origen, pasos hechos y ajustes
        icon.jpg        el ícono del juego (de sus datos de control)
        ExeFS/          los módulos de código extraídos
        RomFS.img       los datos del juego (con la actualización ya aplicada)
        aoc/<n>.img     el contenido descargable
        c/              el C que escribió el recompilador
        juego           el ejecutable
        guardado/       la tarjeta SD y las partidas guardadas
        registro.txt    lo que dijo cada paso
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONFIG_DIR = os.path.join(os.environ.get("XDG_CONFIG_HOME", os.path.expanduser("~/.config")), "nx2linux")
DATA_DIR = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "nx2linux")
APPS_DIR = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "applications")
EXTRACTOR_DIR = os.path.join(ROOT, "build", "tools", "nx_extract")
EXTENSIONES = (".nsp", ".nsz", ".xci", ".xcz", ".nca", ".nro")
CIFRADOS = (".nsp", ".nsz", ".xci", ".xcz", ".nca")  # los que se abren con las llaves (nx_extract)

# Los pasos de una construcción, en orden: (clave, nombre para mostrar, peso en la barra de progreso).
PASOS = [
    ("extraer", "Extraer el juego", 25),
    ("recompilar", "Recompilar el código ARM64 a C", 25),
    ("compilar", "Compilar a x86-64", 45),
    ("terminar", "Agregar al menú de aplicaciones", 5),
]

IDIOMAS = [
    ("es-419", "Español (Latinoamérica)"), ("es", "Español (España)"), ("en-US", "Inglés (EE. UU.)"),
    ("en-GB", "Inglés (Reino Unido)"), ("fr", "Francés"), ("de", "Alemán"), ("it", "Italiano"),
    ("pt-BR", "Portugués (Brasil)"), ("ja", "Japonés"), ("ko", "Coreano"), ("zh-Hans", "Chino simplificado"),
]

AJUSTES_POR_DEFECTO = {
    "renderer": "opengl",     # "vulkan" usa la capa Zink de Mesa
    "modo": "base",           # "base" (TV, 1080p) o "portatil" (720p)
    "idioma": "es-419",
    "pantalla_completa": False,
    "audio": True,
    "variables": "",          # líneas CLAVE=VALOR extra para el entorno del juego
}


# ---------- configuración ----------

def leer_config():
    try:
        with open(os.path.join(CONFIG_DIR, "config.json")) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def guardar_config(cfg):
    os.makedirs(CONFIG_DIR, exist_ok=True)
    tmp = os.path.join(CONFIG_DIR, "config.json.tmp")
    with open(tmp, "w") as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)
    os.replace(tmp, os.path.join(CONFIG_DIR, "config.json"))


def ruta_llaves():
    """El prod.keys predeterminado (Preferencias)."""
    p = leer_config().get("llaves")
    return p if p and os.path.isfile(p) else None


# Donde se suele guardar el prod.keys sacado de la consola (lockpick_rcm lo deja en /switch/prod.keys).
CARPETAS_LLAVES = [
    "~/.switch/prod.keys",
    "~/.local/share/yuzu/keys/prod.keys", "~/.local/share/suyu/keys/prod.keys",
    "~/.local/share/citron/keys/prod.keys", "~/.local/share/eden/keys/prod.keys",
    "~/.config/Ryujinx/system/prod.keys", "~/.var/app/org.ryujinx.Ryujinx/config/Ryujinx/system/prod.keys",
]


def llaves_conocidas():
    """Los prod.keys que existen: el predeterminado, los usados antes y los de las carpetas habituales (sin repetir)."""
    cfg = leer_config()
    rutas = [cfg.get("llaves")] + cfg.get("llaves_usadas", []) + [os.path.expanduser(c) for c in CARPETAS_LLAVES]
    vistos, lista = set(), []
    for r in rutas:
        if r and os.path.isfile(r) and os.path.realpath(r) not in vistos:
            vistos.add(os.path.realpath(r))
            lista.append(r)
    return lista


def recordar_llaves(ruta):
    """Anota un prod.keys usado; si todavía no hay predeterminado, pasa a serlo."""
    cfg = leer_config()
    cfg["llaves_usadas"] = ([ruta] + [r for r in cfg.get("llaves_usadas", []) if r != ruta])[:8]
    if not ruta_llaves():
        cfg["llaves"] = ruta
    guardar_config(cfg)


def llaves_de(juego):
    """Las llaves con que se agregó el juego; si ya no están, las predeterminadas."""
    p = juego.datos.get("llaves")
    return p if p and os.path.isfile(p) else ruta_llaves()


def carpeta_juegos():
    return leer_config().get("carpeta_juegos") or os.path.join(DATA_DIR, "juegos")


# ---------- dependencias ----------

def _ok(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired):
        return None


def dependencias():
    """Lista de (nombre, presente, paquete de Arch que la trae, para qué sirve)."""
    r = _ok(["dotnet", "--list-sdks"])
    dotnet = bool(r and r.returncode == 0 and r.stdout.strip())
    sdl = _ok(["pkg-config", "--exists", "sdl3"])
    egl = any(os.path.exists(p) for p in ("/usr/lib/libEGL.so", "/usr/lib64/libEGL.so"))
    gl = any(os.path.exists(p) for p in ("/usr/lib/libOpenGL.so", "/usr/lib64/libOpenGL.so"))
    return [
        ("SDK de .NET", dotnet, "dotnet-sdk", "abrir los NSP/XCI con LibHac"),
        ("clang", bool(shutil.which("clang")), "clang", "compilar el C del juego"),
        ("SDL3", bool(sdl and sdl.returncode == 0), "sdl3", "ventana, mandos y sonido"),
        ("EGL y OpenGL", egl and gl, "libglvnd", "dibujar con tu GPU"),
        ("pkg-config", bool(shutil.which("pkg-config")), "pkgconf", "encontrar SDL3"),
        ("libopus", bool(_ok(["pkg-config", "--exists", "opus"]) and _ok(["pkg-config", "--exists", "opus"]).returncode == 0),
         "opus", "el audio Opus de los juegos"),
    ]


def comando_instalar(faltan):
    """La orden que instala los paquetes que faltan (pide la contraseña con pkexec)."""
    return ["pkexec", "pacman", "-S", "--needed", "--noconfirm", *faltan]


# ---------- la biblioteca ----------

class Juego:
    def __init__(self, carpeta):
        self.carpeta = carpeta
        self.datos = {}
        self.cargar()

    @property
    def id(self):
        return os.path.basename(self.carpeta)

    def cargar(self):
        try:
            with open(os.path.join(self.carpeta, "juego.json")) as f:
                self.datos = json.load(f)
        except (OSError, ValueError):
            self.datos = {}
        self.datos.setdefault("pasos", {})
        aj = dict(AJUSTES_POR_DEFECTO)
        aj.update(self.datos.get("ajustes", {}))
        self.datos["ajustes"] = aj

    def guardar(self):
        os.makedirs(self.carpeta, exist_ok=True)
        tmp = os.path.join(self.carpeta, "juego.json.tmp")
        with open(tmp, "w") as f:
            json.dump(self.datos, f, indent=2, ensure_ascii=False)
        os.replace(tmp, os.path.join(self.carpeta, "juego.json"))

    @property
    def nombre(self):
        return self.datos.get("nombre") or self.id

    @property
    def version(self):
        return self.datos.get("version", "")

    @property
    def icono(self):
        for nombre in ("icon.jpg", "icon.png"):
            p = os.path.join(self.carpeta, nombre)
            if os.path.exists(p):
                return p
        return None

    @property
    def ejecutable(self):
        e = motor_especial(self)
        return e.exe(self) if e else os.path.join(self.carpeta, "juego")

    @property
    def listo(self):
        return all(self.datos["pasos"].get(k) for k, _, _ in PASOS) and os.path.exists(self.ejecutable)

    def paso_hecho(self, clave):
        return bool(self.datos["pasos"].get(clave))

    def registro(self):
        return os.path.join(self.carpeta, "registro.txt")


def biblioteca():
    base = carpeta_juegos()
    if not os.path.isdir(base):
        return []
    juegos = [Juego(os.path.join(base, d)) for d in sorted(os.listdir(base))
              if os.path.isfile(os.path.join(base, d, "juego.json"))]
    return sorted(juegos, key=lambda j: j.nombre.lower())


# ---------- reconocer los archivos de un juego ----------

def extractor():
    """El extractor (nx_extract, de nxnative, con LibHac); se compila la primera vez."""
    exe = os.path.join(EXTRACTOR_DIR, "nx_extract")
    fuente = os.path.join(ROOT, "tools", "nx_extract", "Program.cs")
    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(fuente):
        return exe
    r = subprocess.run(["dotnet", "build", os.path.join(ROOT, "tools", "nx_extract"), "-c", "Release", "-o",
                        EXTRACTOR_DIR, "-v", "quiet", "-nologo"], capture_output=True, text=True)
    if r.returncode or not os.path.exists(exe):
        raise RuntimeError("no se pudo compilar el extractor:\n" + (r.stdout + r.stderr)[-2000:])
    return exe


def _title_del_nombre(nombre):
    m = re.search(r"\[([0-9A-Fa-f]{16})\]", nombre)
    return int(m.group(1), 16) if m else None


COMPRIMIDOS = {b"7z\xbc\xaf": "7z", b"PK\x03\x04": "zip", b"Rar!": "rar"}


def revisar_archivo(archivo):
    """Un NSP/NSZ empieza con PFS0 y un XCI tiene HEAD en 0x100; un archivo comprimido con otro nombre no sirve."""
    with open(archivo, "rb") as f:
        inicio = f.read(4)
        f.seek(0x100)
        cabecera = f.read(4)
    nombre = os.path.basename(archivo)
    if inicio[:4] in COMPRIMIDOS:
        raise RuntimeError(f"{nombre} no es un NSP: es un archivo comprimido ({COMPRIMIDOS[inicio[:4]]}) con otra "
                           "extensión. Descomprímelo primero y elige lo que trae adentro.")
    if inicio != b"PFS0" and cabecera != b"HEAD":
        raise RuntimeError(f"{nombre} no parece un NSP, NSZ, XCI ni XCZ (no empieza como uno). ¿Está completo?")


def _primera_linea_error(texto):
    for linea in texto.splitlines():
        linea = linea.strip()
        if linea and not linea.startswith("at "):
            return linea[:400]
    return texto.strip()[:400]


def _carpeta_exefs(d):
    """Los módulos de un juego ya extraído: la carpeta misma (main y main.npdm) o su subcarpeta exefs/ExeFS."""
    try:
        subs = [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.lower() == "exefs"]
    except OSError:
        return None
    for c in subs + [d]:
        main = os.path.join(c, "main")
        if os.path.isfile(main) and (c != d or os.path.isfile(os.path.join(c, "main.npdm"))):
            with open(main, "rb") as f:
                cab = f.read(0x14)
            if cab[:4] == b"NSO0" or cab[0x10:0x14] == b"NRO0":
                return c
    return None


def tipo(ruta):
    """Qué es lo elegido: "paquete" (NSP, NSZ, XCI, XCZ o un NCA), "ncas" (carpeta de NCA sueltos), "extraido"
    (carpeta de un juego ya extraído: ExeFS y, si tiene, romfs) o "nro" (homebrew). None si no es nada de eso."""
    if os.path.isdir(ruta):
        if _carpeta_exefs(ruta):
            return "extraido"
        try:
            return "ncas" if any(n.lower().endswith(".nca") for n in os.listdir(ruta)) else None
        except OSError:
            return None
    bajo = ruta.lower()
    if bajo.endswith(".nro"):
        return "nro"
    return "paquete" if bajo.endswith(CIFRADOS) else None


def _id_local(datos):
    """Un title ID para lo que no trae uno (un NRO, un juego extraído sin NPDM): 05 + parte de su SHA-256, terminado
    en 000 para que cuente como juego base. Es estable: el mismo archivo da siempre el mismo id."""
    return 0x0500000000000000 | (int.from_bytes(hashlib.sha256(datos).digest()[:5], "big") << 12)


def title_id(archivo, llaves):
    t = tipo(archivo)
    if t == "nro":
        with open(archivo, "rb") as f:
            datos = f.read()
        if datos[0x10:0x14] != b"NRO0":
            raise RuntimeError(f"{os.path.basename(archivo)} no es un NRO (no tiene la cabecera NRO0)")
        return _id_local(datos)
    if t == "extraido":
        exefs = _carpeta_exefs(archivo)
        npdm = os.path.join(exefs, "main.npdm")
        if os.path.isfile(npdm):
            sys.path.insert(0, os.path.join(ROOT, "recomp"))
            from nso import parse_npdm
            with open(npdm, "rb") as f:
                programa = parse_npdm(f.read())["programa"]
            if programa:
                return programa
        with open(os.path.join(exefs, "main"), "rb") as f:
            return _id_local(f.read())
    if not llaves:
        raise RuntimeError(f"para abrir {os.path.basename(archivo)} hace falta el prod.keys de tu consola "
                           "(Preferencias); sin llaves solo se pueden agregar homebrew NRO y juegos ya extraídos")
    if t == "paquete" and not archivo.lower().endswith(".nca"):
        revisar_archivo(archivo)
    t = _title_del_nombre(os.path.basename(archivo))
    if t is not None:
        return t
    r = subprocess.run([extractor(), llaves, "--title", archivo], capture_output=True, text=True)
    m = re.search(r"title ([0-9a-f]{16})", r.stdout)
    if not m:
        raise RuntimeError(f"no se reconoce {os.path.basename(archivo)}: " + _primera_linea_error(r.stderr or r.stdout))
    return int(m.group(1), 16)


def _archivos(rutas):
    """Lo que hay que reconocer: cada archivo elegido y, en una carpeta, lo que tiene adentro. Una carpeta de NCA
    sueltos o de un juego extraído cuenta como una sola cosa, igual que una subcarpeta así."""
    archivos = []
    for r in rutas:
        t = tipo(r)
        if t in ("ncas", "extraido") or (t and not os.path.isdir(r)):
            archivos.append(r)
        elif os.path.isdir(r):
            archivos += [p for p in (os.path.join(r, f) for f in sorted(os.listdir(r))) if tipo(p)]
    if not archivos:
        raise RuntimeError("no hay nada que se pueda agregar: se aceptan NSP, NSZ, XCI, XCZ, NCA, NRO, carpetas de "
                           "NCA y carpetas de juegos ya extraídos (con ExeFS)")
    return archivos


def clasificar(rutas, llaves):
    """De una carpeta o una lista de archivos saca {base, update, dlc[], title}.

    Los title ID de la Switch dicen qué es cada cosa: el juego termina en 000, su actualización en 800
    y los DLC tienen el bloque siguiente (base + 0x1000 + n)."""
    archivos = _archivos(rutas)
    bases, updates, dlcs = [], [], []
    for a in archivos:
        t = title_id(a, llaves)
        if t & 0xFFF == 0:
            bases.append((t, a))
        elif t & 0xFFF == 0x800:
            updates.append((t, a))
        else:
            dlcs.append((t, a))
    if not bases:
        raise RuntimeError("falta el juego base (su title ID termina en 000); solo hay actualizaciones o DLC")
    if len({t for t, _ in bases}) > 1:
        raise RuntimeError("hay más de un juego base en la selección: agrega uno a la vez")
    t, base = bases[0]
    upd = sorted((a for u, a in updates if u == t | 0x800), key=_version_del_nombre)
    # Un DLC es base + 0x1000 + índice; el juego lo abre por ese id.
    dlc = [{"ruta": a, "indice": d - t - 0x1000} for d, a in sorted(dlcs) if t + 0x1000 < d < t + 0x2000]
    return {"title": f"{t:016x}", "base": base, "update": upd[-1] if upd else None, "dlc": dlc}


def _version_del_nombre(a):
    m = re.search(r"\[v(\d+)\]", os.path.basename(a))
    return int(m.group(1)) if m else 0


def motor_especial(j):
    """El motor especial (especiales.py) que construye y abre este juego, o None si va por el camino genérico."""
    from . import especiales
    return especiales.para(j)


def registrar_construido(title, nombre, version, datos=None, icono=None):
    """Agrega a la biblioteca un juego que un motor especial ya construyó fuera de NX2Linux (sin volver a construirlo).
    `datos` va a juego.json (p. ej. {"dump": carpeta}); `icono` es una imagen que pasa a ser la del juego."""
    j = Juego(os.path.join(carpeta_juegos(), title))
    j.datos.update({"nombre": nombre, "version": version, "importado": True, **(datos or {})})
    if not motor_especial(j):
        raise RuntimeError(f"{nombre} {version} no tiene un motor especial instalado")
    ahora = time.strftime("%Y-%m-%d %H:%M:%S")
    j.datos["pasos"] = {k: ahora for k, _, _ in PASOS}
    j.guardar()
    if icono:
        shutil.copyfile(icono, os.path.join(j.carpeta, "icon.png"))
    crear_acceso(j)
    return j


def nuevo_juego(origen, llaves=None):
    j = Juego(os.path.join(carpeta_juegos(), origen["title"]))
    j.datos["origen"] = {k: origen[k] for k in ("base", "update", "dlc")}
    j.datos["pasos"] = {}
    base = os.path.normpath(origen["base"])
    j.datos.setdefault("nombre", os.path.basename(base) if os.path.isdir(base) else os.path.splitext(os.path.basename(base))[0])
    if llaves:
        j.datos["llaves"] = llaves
        recordar_llaves(llaves)
    j.guardar()
    return j


def clasificar_contenido(juego, rutas):
    """Archivos para un juego ya instalado: {update, dlc[]}. Falla si alguno es de otro juego o es el juego base."""
    llaves = llaves_de(juego)
    if not llaves:
        raise RuntimeError("falta elegir tu archivo prod.keys en Preferencias")
    t = int(juego.id, 16)
    updates, dlc = [], []
    for a in _archivos(rutas):
        d = title_id(a, llaves)
        if d == t | 0x800:
            updates.append(a)
        elif t + 0x1000 < d < t + 0x2000:
            dlc.append({"ruta": a, "indice": d - t - 0x1000})
        elif d == t:
            raise RuntimeError(f"{os.path.basename(a)} es el juego base, que ya está en la biblioteca")
        else:
            raise RuntimeError(f"{os.path.basename(a)} es de otro juego (title ID {d:016x}), no de {juego.nombre}")
    return {"update": max(updates, key=_version_del_nombre) if updates else None, "dlc": dlc}


def aplicar_contenido(juego, nuevo):
    """Anota la actualización y los DLC en el juego. Devuelve "extraer" si hay que reconstruirlo (una actualización
    cambia el código) o la lista de índices de DLC que basta con extraer."""
    o = juego.datos.setdefault("origen", {})
    if nuevo["update"] and not (o.get("base") and os.path.exists(o["base"])):
        raise RuntimeError("para aplicar una actualización hace falta el archivo del juego base con que se agregó, "
                           "y ya no está: agrega el juego de nuevo junto con la actualización")
    actuales = {d["indice"]: d for d in o.get("dlc") or [] if isinstance(d, dict)}
    for d in nuevo["dlc"]:
        actuales[d["indice"]] = d
    o["dlc"] = [actuales[k] for k in sorted(actuales)]
    if nuevo["update"]:
        o["update"] = nuevo["update"]
    juego.guardar()
    return "extraer" if nuevo["update"] else [d["indice"] for d in nuevo["dlc"]]


def quitar(juego):
    shutil.rmtree(juego.carpeta, ignore_errors=True)
    try:
        os.remove(os.path.join(APPS_DIR, f"nx2linux-{juego.id}.desktop"))
    except OSError:
        pass


# ---------- NACP y ASET (nombre e ícono de lo que no pasa por nx_extract) ----------

ICONOS_NACP = ("AmericanEnglish", "BritishEnglish", "LatinAmericanSpanish", "Spanish", "Japanese", "French", "German",
               "Italian", "Dutch", "CanadianFrench", "Portuguese", "Russian", "Korean", "TraditionalChinese",
               "SimplifiedChinese", "BrazilianPortuguese")


def leer_nacp(nacp):
    """(nombre, versión) de un control.nacp. Los títulos son 16 entradas de 0x300 bytes (nombre 0x200 + editor
    0x100); los SDK nuevos comprimen esos 0x3000 bytes (u16 tamaño + deflate a 32 idiomas), como en nx_extract."""
    import zlib
    if len(nacp) < 0x3070:
        return "", ""
    tabla, n = nacp, nacp[0] | nacp[1] << 8
    if 0 < n <= 0x2FFE:
        try:
            d = zlib.decompress(nacp[2:2 + n], -15)
            if len(d) == 0x6000:
                tabla = d
        except zlib.error:
            pass
    nombre = ""
    for i in range(len(tabla) // 0x300 if tabla is not nacp else 16):
        crudo = tabla[i * 0x300:i * 0x300 + 0x200].split(b"\0")[0]
        if crudo:
            nombre = crudo.decode("utf-8", "replace")
            break
    version = nacp[0x3060:0x3070].split(b"\0")[0].decode("ascii", "replace")
    return nombre, version


def leer_aset(nro):
    """(ícono JPEG, NACP) de la sección ASET que va después del código de un NRO; (None, None) si no tiene."""
    if len(nro) < 0x20 or nro[0x10:0x14] != b"NRO0":
        return None, None
    fin = int.from_bytes(nro[0x18:0x1C], "little")
    if nro[fin:fin + 4] != b"ASET" or len(nro) < fin + 0x38:
        return None, None

    def parte(k):
        off = int.from_bytes(nro[fin + 8 + 16 * k:fin + 16 + 16 * k], "little")
        tam = int.from_bytes(nro[fin + 16 + 16 * k:fin + 24 + 16 * k], "little")
        return nro[fin + off:fin + off + tam] if tam else None
    return parte(0), parte(1)


# ---------- construir ----------

class Cancelado(Exception):
    pass


class Construccion:
    """Corre los pasos que falten de un juego en un hilo. `avisar(paso, fraccion, linea)` se llama desde
    ese hilo con cada novedad: paso es la clave del paso en curso (o "fin"/"error"), fraccion el
    progreso total de 0 a 1 y linea el texto nuevo del registro (o None)."""

    def __init__(self, juego, avisar, desde=None, solo_dlc=None):
        # solo_dlc: índices de DLC nuevos de un juego ya construido; se extraen y no se recompila nada.
        self.juego, self.avisar, self.desde, self.solo_dlc = juego, avisar, desde, solo_dlc
        self.proc = None
        self.cancelado = False
        self.hilo = threading.Thread(target=self._correr, daemon=True)
        self.inicio = None

    def empezar(self):
        self.inicio = time.time()
        self.hilo.start()

    def cancelar(self):
        self.cancelado = True
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()

    def _peso_antes(self, clave):
        total = sum(p for _, _, p in PASOS)
        antes = 0
        for k, _, p in PASOS:
            if k == clave:
                break
            antes += p
        return antes / total, dict((k, p) for k, _, p in PASOS)[clave] / total

    def _correr(self):
        j = self.juego
        if self.desde:  # reconstruir desde un paso: ese y los siguientes se rehacen
            borrar = False
            for k, _, _ in PASOS:
                borrar |= k == self.desde
                if borrar:
                    j.datos["pasos"].pop(k, None)
            j.guardar()
        try:
            with open(j.registro(), "a") as self.log:
                self.log.write(f"\n==== {time.strftime('%Y-%m-%d %H:%M:%S')} ====\n")
                if self.solo_dlc:
                    self._linea("extraer", 0, "— Agregar contenido descargable")
                    self._extraer_dlc([d for d in j.datos["origen"]["dlc"] if d["indice"] in self.solo_dlc])
                    self.avisar("fin", 1.0, f"Contenido descargable agregado en {self._duracion()}.")
                    return
                for clave, nombre, _ in PASOS:
                    if j.paso_hecho(clave):
                        continue
                    self._linea(clave, 0, f"— {nombre}")
                    getattr(self, "_" + clave)()
                    j.datos["pasos"][clave] = time.strftime("%Y-%m-%d %H:%M:%S")
                    j.guardar()
            self.avisar("fin", 1.0, f"Listo en {self._duracion()}.")
        except Cancelado:
            self.avisar("error", None, "Construcción detenida. Al volver a construir sigue donde quedó.")
        except Exception as e:  # cualquier fallo se muestra en la app con su registro
            self.avisar("error", None, f"Error: {e}")

    def _duracion(self):
        s = int(time.time() - self.inicio)
        return f"{s // 60} min {s % 60} s" if s >= 60 else f"{s} s"

    def _linea(self, clave, frac_paso, texto):
        a, w = self._peso_antes(clave)
        if texto is not None:
            self.log.write(texto + "\n")
            self.log.flush()
        self.avisar(clave, a + w * max(0.0, min(1.0, frac_paso)), texto)

    def _ejecutar(self, clave, cmd, progreso=None, entorno=None, cwd=None):
        """Corre una orden y pasa su salida al registro. `progreso(linea)` devuelve la fracción del paso o None."""
        if self.cancelado:
            raise Cancelado()
        self.log.write("$ " + " ".join(cmd) + "\n")
        env = dict(os.environ, PYTHONUNBUFFERED="1", **(entorno or {}))
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env,
                                     errors="replace", bufsize=1, cwd=cwd)
        frac = 0.0
        ultimas = []
        for linea in self.proc.stdout:
            linea = linea.rstrip("\n")
            f = progreso(linea) if progreso else None
            if f is not None:
                frac = f
                self._linea(clave, frac, None)  # las líneas de progreso no van al registro
                continue
            ultimas = (ultimas + [linea])[-30:]
            self._linea(clave, frac, linea)
        rc = self.proc.wait()
        if self.cancelado:
            raise Cancelado()
        if rc:
            raise RuntimeError(f"{os.path.basename(cmd[1] if cmd[0] == sys.executable else cmd[0])} terminó con "
                               f"código {rc}:\n" + "\n".join(ultimas[-8:]))

    # Paso 1: extraer ExeFS, RomFS (con la actualización), ícono y nombre; los DLC aparte.
    def _extraer(self):
        j = self.juego
        o = j.datos["origen"]
        t = tipo(o["base"])
        if t == "extraido":
            return self._extraer_extraido(o["base"])
        if t == "nro":
            return self._extraer_nro(o["base"])
        llaves = self._llaves()
        self._linea("extraer", 0.02, "Preparando el extractor (la primera vez se compila)...")
        exe = extractor()
        for d in ("ExeFS",):
            shutil.rmtree(os.path.join(j.carpeta, d), ignore_errors=True)

        def info(linea):
            m = re.match(r"name (.+)", linea)
            if m:
                j.datos["nombre"] = m.group(1).strip()
            m = re.match(r"display version (.+)", linea)
            if m:
                j.datos["version"] = m.group(1).strip()
            return None

        self._ejecutar("extraer", [exe, llaves, o["base"], o.get("update") or "-", j.carpeta, "--romfs-image"], info)
        j.guardar()
        self._extraer_dlc(o.get("dlc") or [])

    def _nuevo_exefs(self):
        exefs = os.path.join(self.juego.carpeta, "ExeFS")
        shutil.rmtree(exefs, ignore_errors=True)
        os.makedirs(exefs)
        return exefs

    def _anotar_nacp(self, nacp):
        nombre, version = leer_nacp(nacp)
        if nombre:
            self.juego.datos["nombre"] = nombre
        if version:
            self.juego.datos["version"] = version
        self._linea("extraer", 0.9, f"{nombre or 'sin nombre'} {version}".strip())

    # Un juego ya extraído (por nxdumptool, hactool, un emulador...): se copian los módulos y el RomFS se arma con
    # nx_extract --romfs-dir; no hacen falta llaves.
    def _extraer_extraido(self, carpeta):
        j = self.juego
        origen = _carpeta_exefs(carpeta)
        if not origen:
            raise RuntimeError(f"{carpeta} ya no tiene los módulos del juego (main, main.npdm...)")
        exefs = self._nuevo_exefs()
        for n in sorted(os.listdir(origen)):
            if os.path.isfile(os.path.join(origen, n)):
                shutil.copyfile(os.path.join(origen, n), os.path.join(exefs, n))
        self._linea("extraer", 0.2, f"Módulos copiados de {origen}")
        imagen = os.path.join(j.carpeta, "RomFS.img")
        for viejo in (imagen, imagen + ".native"):
            if os.path.exists(viejo):
                os.remove(viejo)
        hijos = {n.lower(): os.path.join(carpeta, n) for n in os.listdir(carpeta)}
        if os.path.isfile(hijos.get("romfs.img", "")) and open(hijos["romfs.img"], "rb").read(8) == b"DMROMFS1":
            subprocess.run(["cp", "--reflink=auto", hijos["romfs.img"], imagen], check=True)
            self._linea("extraer", 0.8, "RomFS copiado")
        else:
            fuente = next((hijos[n] for n in ("romfs", "romfs.bin", "romfs.img") if n in hijos), None)
            if fuente:
                self._ejecutar("extraer", [extractor(), "--romfs-dir", fuente, imagen])
            else:
                self._linea("extraer", 0.8, "El juego no trae RomFS")
        control = next((c for c in (carpeta, hijos.get("control", "")) if os.path.isfile(os.path.join(c, "control.nacp"))), None)
        if control:
            with open(os.path.join(control, "control.nacp"), "rb") as f:
                self._anotar_nacp(f.read())
            icono = next((os.path.join(control, f"icon_{l}.dat") for l in ICONOS_NACP
                          if os.path.isfile(os.path.join(control, f"icon_{l}.dat"))), None)
            if icono:
                shutil.copyfile(icono, os.path.join(j.carpeta, "icon.jpg"))
        for n in ("icon.jpg", "icon.png"):
            if n in hijos:
                shutil.copyfile(hijos[n], os.path.join(j.carpeta, n))
        j.guardar()

    # Un homebrew NRO: el recompilador lo toma tal cual (su RomFS, si tiene, queda dentro del ejecutable); el nombre
    # y el ícono salen de su sección ASET.
    def _extraer_nro(self, ruta):
        j = self.juego
        exefs = self._nuevo_exefs()
        shutil.copyfile(ruta, os.path.join(exefs, "main"))
        for viejo in ("RomFS.img", "RomFS.img.native"):
            if os.path.exists(os.path.join(j.carpeta, viejo)):
                os.remove(os.path.join(j.carpeta, viejo))
        with open(ruta, "rb") as f:
            datos = f.read()
        icono, nacp = leer_aset(datos)
        if nacp:
            self._anotar_nacp(nacp)
        if icono:
            with open(os.path.join(j.carpeta, "icon.jpg"), "wb") as f:
                f.write(icono)
        j.guardar()

    def _llaves(self):
        llaves = llaves_de(self.juego)
        if not llaves:
            raise RuntimeError("falta elegir tu archivo prod.keys en Preferencias")
        return llaves

    def _extraer_dlc(self, lista):
        j = self.juego
        llaves, exe = self._llaves(), extractor()
        for k, d in enumerate(lista):
            self._linea("extraer", (k + 1) / (len(lista) + 1), f"Contenido descargable {k + 1} de {len(lista)}")
            os.makedirs(os.path.join(j.carpeta, "aoc"), exist_ok=True)
            ruta, indice = (d["ruta"], d["indice"]) if isinstance(d, dict) else (d, k + 1)
            self._ejecutar("extraer", [exe, llaves, "--aoc", ruta, os.path.join(j.carpeta, "aoc", f"{indice}.img"),
                                       "--romfs-image"])

    # Paso 2: el recompilador estático. Antes, qué servicios del sistema nombra el juego y cuáles
    # faltan en el runtime (solo informa: el registro dice qué arreglar si no arranca).
    def _recompilar(self):
        j = self.juego
        e = motor_especial(j)
        if e:
            self._linea("recompilar", 0, f"Motor especial para este juego: {e.nombre} ({e.repo()})")
            return e.recompilar(self, j)
        try:
            r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "servicios.py"), os.path.join(j.carpeta, "ExeFS")],
                               capture_output=True, text=True, timeout=600)
            for linea in r.stdout.splitlines():
                self._linea("recompilar", 0, linea)
        except (OSError, subprocess.TimeoutExpired):
            pass

        def prog(linea):
            m = re.match(r"progreso (\d+)/(\d+)", linea)
            return int(m.group(1)) / int(m.group(2)) if m else None

        pistas = os.path.join(j.carpeta, "pistas.txt")
        self._ejecutar("recompilar", [sys.executable, os.path.join(ROOT, "recomp", "nxrecomp.py"),
                                      os.path.join(j.carpeta, "ExeFS"), "-o", os.path.join(j.carpeta, "c"), "--progreso",
                                      "--pistas", pistas,
                                      "--sustituir", os.path.join(ROOT, "recomp", "sustituciones.txt")], prog)
        j.datos["pistas_usadas"] = _contar_pistas(pistas)
        j.guardar()

    # Paso 3: clang en todos los núcleos.
    def _compilar(self):
        j = self.juego
        e = motor_especial(j)
        if e:
            return e.compilar(self, j)
        romfs = os.path.join(j.carpeta, "RomFS.img")
        if os.path.isfile(romfs):
            self._ejecutar("compilar", [sys.executable, os.path.join(ROOT, "tools", "romfs_native.py"), romfs])

        def prog(linea):
            m = re.match(r"progreso (\d+)/(\d+)", linea)
            return int(m.group(1)) / int(m.group(2)) if m else None

        self._ejecutar("compilar", [sys.executable, os.path.join(ROOT, "tools", "compilar.py"),
                                    os.path.join(j.carpeta, "c"), "-o", j.ejecutable, "--progreso"], prog)

    # Paso 4: ícono PNG y entrada .desktop, para abrirlo desde el lanzador de Omarchy.
    def _terminar(self):
        crear_acceso(self.juego)
        self._linea("terminar", 1, "Agregado al menú de aplicaciones.")


def crear_acceso(j):
    icono = j.icono or ""
    if icono:
        png = os.path.join(j.carpeta, "icon.png")
        try:
            from gi.repository import GdkPixbuf
            GdkPixbuf.Pixbuf.new_from_file(icono).savev(png, "png", [], [])
            icono = png
        except Exception:
            pass
    os.makedirs(APPS_DIR, exist_ok=True)
    lanzador = os.path.join(ROOT, "app", "nx2linux.sh")
    with open(os.path.join(APPS_DIR, f"nx2linux-{j.id}.desktop"), "w") as f:
        f.write("[Desktop Entry]\nType=Application\n"
                f"Name={j.nombre}\nComment=Juego de Switch nativo (NX2Linux)\n"
                f"Exec={lanzador} --jugar {j.id}\nIcon={icono}\nCategories=Game;\nTerminal=false\n")


def _contar_pistas(ruta):
    try:
        with open(ruta) as f:
            return len({l.strip() for l in f if l.strip()})
    except OSError:
        return 0


def pistas_nuevas(j):
    """Cuántas direcciones anotó el juego desde la última recompilación (saltos a código sin recompilar)."""
    return _contar_pistas(os.path.join(j.carpeta, "pistas.txt")) - j.datos.get("pistas_usadas", 0)


# ---------- jugar ----------

def entorno_juego(j):
    aj = j.datos["ajustes"]
    env = dict(os.environ)
    env["NX_RENDERER"] = aj.get("renderer", "opengl")
    env["NX_ROMFS"] = os.path.join(j.carpeta, "RomFS.img")
    env["NX_SDMC"] = os.path.join(j.carpeta, "guardado", "sdmc")
    env["NX_GUARDADO"] = os.path.join(j.carpeta, "guardado")
    env["NX_TITLE_ID"] = j.id
    env["NX_PISTAS"] = os.path.join(j.carpeta, "pistas.txt")
    env["NX_TITULO"] = j.nombre
    if leer_config().get("usuario"):
        env["NX_USUARIO"] = leer_config()["usuario"]
    env["NX_IDIOMA"] = aj["idioma"]
    env["NX_MODO"] = aj["modo"]
    if os.path.isdir(os.path.join(j.carpeta, "aoc")):
        env["NX_AOC"] = os.path.join(j.carpeta, "aoc")
    if aj["pantalla_completa"]:
        env["NX_PANTALLA_COMPLETA"] = "1"
    if not aj["audio"]:
        env["NX_SIN_AUDIO"] = "1"
    for linea in (aj.get("variables") or "").splitlines():
        if "=" in linea and not linea.strip().startswith("#"):
            k, v = linea.split("=", 1)
            env[k.strip()] = v.strip()
    # NVN en el host (runtime/nvn): el backend Vulkan traduce los shaders del juego a SPIR-V con totk_xlate (el
    # traductor de Ryujinx, compilado por totk-pc-recomp) y los guarda en la caché del juego.
    xlate = os.path.join(motor_especial_repo("totk-pc-recomp"), "build", "xlate", "totk_xlate")
    if os.path.isfile(xlate):
        env.setdefault("TOTK_XLATE", xlate)
    env.setdefault("TOTK_SHADER_CACHE", os.path.join(j.carpeta, "shader-cache"))
    os.makedirs(env["NX_SDMC"], exist_ok=True)
    return env


def motor_especial_repo(nombre):
    """La carpeta de un port (especiales.py), esté o no listo para algún juego."""
    from . import especiales
    return next((e.repo() for e in especiales.ESPECIALES.values() if e.nombre == nombre), "")


def jugar(j, salida=None):
    """Arranca el juego; su salida va a <carpeta>/ultima_partida.txt."""
    if not os.path.exists(j.ejecutable):
        raise RuntimeError("el juego todavía no está construido")
    log = salida or open(os.path.join(j.carpeta, "ultima_partida.txt"), "w")
    e = motor_especial(j)
    if e:
        return e.lanzar(j, log)
    return subprocess.Popen([j.ejecutable], env=entorno_juego(j), stdout=log, stderr=subprocess.STDOUT,
                            cwd=j.carpeta, start_new_session=True)
