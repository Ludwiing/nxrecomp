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

import glob
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONFIG_DIR = os.path.join(os.environ.get("XDG_CONFIG_HOME", os.path.expanduser("~/.config")), "nx2linux")
DATA_DIR = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "nx2linux")
APPS_DIR = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "applications")
EXTRACTOR_DIR = os.path.join(ROOT, "build", "tools", "nx_extract")
EXTENSIONES = (".nsp", ".nsz", ".xci", ".xcz", ".nca", ".nro", ".nup")
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
    "generacion_cuadros": False,   # TOTK_FRAMEGEN del runtime NVN (vk_framegen.c)
    "telemetria": "off",         # TOTK_PERF_OVERLAY del runtime NVN: off, fps o full
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
        """El nombre para el menú. Una ficha vieja guardó a veces el nombre de la carpeta del dump ("base", "65536") o
        del archivo con etiquetas; esa se muestra con el nombre del dump, sin reescribir juego.json."""
        n = self.datos.get("nombre")
        base = (self.datos.get("origen") or {}).get("base")
        if base and (self.datos.get("identidad") or {}).get("nombre_fuente") not in NOMBRES_VERIFICADOS and (
                not n or n.isdecimal() or n.lower() in _NOMBRES_GENERICOS or re.search(r"\[[0-9A-Fa-f]{16}\]", n)):
            try:
                return nombre_externo(base, int(self.id, 16))
            except ValueError:
                pass
        return n or self.id

    @property
    def version(self):
        return self.datos.get("version", "")

    @property
    def ejecutable_nativo(self):
        """Ruta estable del ejecutable recompilado por NX2Linux, independiente de motores alternativos."""
        return os.path.join(self.carpeta, "juego")

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
        if e:
            return e.exe(self)
        return self.ejecutable_nativo

    @property
    def listo(self):
        return all(self.paso_hecho(k) for k, _, _ in PASOS) and os.path.exists(self.ejecutable)

    @property
    def runtime_desactualizado(self):
        if (not self.datos["pasos"].get("compilar") or motor_especial(self)
                or not os.path.isfile(self.ejecutable_nativo)):
            return False
        try:
            binario_mtime = os.stat(self.ejecutable_nativo).st_mtime_ns
            fuentes = [os.path.join(ROOT, "tools", "compilar.py"), os.path.join(ROOT, "Makefile")]
            carpeta_runtime = os.path.join(ROOT, "runtime")
            # compilar.py enlaza también C++ y el runtime incluye tablas .inc.
            for extension in ("*.c", "*.h", "*.cpp", "*.inc"):
                fuentes.extend(glob.glob(os.path.join(carpeta_runtime, "**", extension), recursive=True))
            return any(os.stat(ruta).st_mtime_ns > binario_mtime for ruta in fuentes)
        except OSError:
            return True

    def paso_hecho(self, clave):
        if not self.datos["pasos"].get(clave):
            return False
        if motor_especial(self):
            return True
        if clave == "recompilar":
            # El marcador de juego.json puede sobrevivir a una carpeta C incompleta
            # (por ejemplo, si se cerró NX2Linux a mitad de la escritura). Sin estos
            # archivos compilar.py termina con FileNotFoundError en nxgame.h.
            carpeta_c = os.path.join(self.carpeta, "c")
            if not all(os.path.isfile(os.path.join(carpeta_c, nombre))
                       for nombre in ("nxgame.h", "programa.c")):
                return False
            # Una mejora del decodificador o de las sustituciones deja obsoleto el C
            # generado, aunque el ejecutable se haya enlazado después con ese C viejo.
            try:
                sello = time.mktime(time.strptime(self.datos["pasos"][clave], "%Y-%m-%d %H:%M:%S"))
                fuentes = [os.path.join(ROOT, "app", "nx2linux", "motor.py")]
                carpeta_recomp = os.path.join(ROOT, "recomp")
                for extension in ("*.py", "*.txt"):
                    fuentes.extend(glob.glob(os.path.join(carpeta_recomp, "**", extension), recursive=True))
                return not any(os.path.getmtime(ruta) > sello for ruta in fuentes)
            except (OSError, ValueError):
                return False
        if clave != "compilar":
            return True
        if not os.path.isfile(self.ejecutable_nativo):
            return False
        if not self.paso_hecho("recompilar"):
            return False
        # El ejecutable incluye el runtime, además del C generado para este juego.
        try:
            return not self.runtime_desactualizado
        except OSError:
            return False

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


def _ultima_linea(texto):
    """La línea que explica un error de Python: la última de su traceback (la primera es "Traceback ...")."""
    lineas = [l.strip() for l in texto.splitlines() if l.strip()]
    return lineas[-1][:400] if lineas else "error desconocido"


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
    (carpeta de un juego ya extraído: ExeFS y, si tiene, romfs), "nro" (homebrew) o "nup" (dump de Switch 2, ya
    descifrado: no necesita llaves). None si no es nada de eso."""
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
    if bajo.endswith(".nup"):
        return "nup"
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
    if t == "nup":
        return _title_nup(archivo)
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
    # El [title id] del nombre del archivo no cuenta: un archivo renombrado pasaría por otro juego. Lo dice el extractor.
    r = subprocess.run([extractor(), llaves, "--title", archivo], capture_output=True, text=True)
    m = re.search(r"title ([0-9a-f]{16})", r.stdout)
    if not m:
        raise RuntimeError(f"no se reconoce {os.path.basename(archivo)}: " + _primera_linea_error(r.stderr or r.stdout))
    return int(m.group(1), 16)


def _title_nup(archivo):
    """Title ID de un .nup según su TOC (tools/nup_extraer.py --info): el del juego base si lo trae; si no, el de la
    actualización (o el DLC), para que clasificar no lo tome por un juego. El main.npdm del título con código tiene que
    estar y nombrar al mismo juego; si falta algo, el paquete está incompleto."""
    nombre = os.path.basename(archivo)
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "nup_extraer.py"), archivo, "--info"],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(f"no se pudo leer {nombre}: " + _ultima_linea(r.stderr or r.stdout))
    ids = {clase: [int(x, 16) for x in re.findall(rf"^{clase} ([0-9a-f]{{16}})$", r.stdout, re.M)]
           for clase in ("base", "update", "dlc")}
    falta = re.findall(r"^falta (.+)$", r.stdout, re.M)
    if falta:
        raise RuntimeError(f"{nombre} está incompleto: le falta {' y '.join(falta)} (el código del juego)")
    tid = (ids["base"] or ids["update"] or ids["dlc"] or [None])[0]
    if tid is None:
        raise RuntimeError(f"no se pudo leer {nombre}: no trae títulos")
    m = re.search(r"^npdm ([0-9a-f]{16})$", r.stdout, re.M)
    if (ids["base"] or ids["update"]) and not (m and int(m.group(1), 16) == tid & ~0xFFF):
        raise RuntimeError(f"{nombre}: el programa de su main.npdm ({m.group(1) if m else 'ninguno'}) no coincide "
                           f"con el title ID del paquete ({tid:016x})")
    return tid


# De dónde sale un nombre que no se reemplaza al volver a agregar el juego: el NACP del juego o el motor especial elegido
# por su title ID. "archivo" es provisional (nombre_externo).
NOMBRES_VERIFICADOS = ("nacp", "motor especial")

# Carpetas que no dicen qué juego es (las de un dump: base/, updates/<versión>/, exefs/...): el nombre se busca más arriba.
_NOMBRES_GENERICOS = {"base", "update", "updates", "upd", "dlc", "aoc", "exefs", "romfs", "game", "juego", "dump",
                      "nup", "switch", "switch2", "switch 2"}


def _limpiar_nombre(texto):
    """El nombre de un juego a partir del nombre de un archivo o carpeta: sin extensión ni etiquetas ([id], [v..],
    _v1.1.1, (Switch 2), BASE Game...), con guiones separadores como espacios y con espacios entre palabras pegadas
    (JuegoEjemplo)."""
    s = re.sub(r"\[[^\]]*\]|\([^)]*\)", " ", texto)
    s = re.sub(r"(?i)[ _.-]v\d+(\.\d+)*\b", " ", s).replace("_", " ").strip()
    # Nombre de archivo con guiones por espacios (Juego-Ejemplo-Switch-2); con uno solo puede ser parte del
    # título (Spider-Man) y, si ya hay espacios, el guion es del título.
    if " " not in s and s.count("-") >= 2:
        s = s.replace("-", " ")
    # Solo al final: "... Switch 2 BASE Game" es etiqueta, "Game Builder Garage" no.
    s = re.sub(r"(?i)([ -]+(switch[ -]?2|nsw2|base|basw|update|game|dump))+\s*$", "", " " + s.strip())
    s = re.sub(r"\s+", " ", s).strip(" -.")
    if " " not in s and not s.isupper():
        s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", s)
    return s


def nombre_externo(ruta, title=None):
    """Nombre provisional (solo para mostrar) de lo que no trae NACP, como los dumps de Switch 2: sale del archivo o de
    la carpeta del juego, saltando las genéricas (base/, 65536/...). Si el nombre trae un [title id] que no es el del
    juego, no se usa: es de otro juego. Sin nada útil, "Juego <title id>"."""
    ruta = os.path.normpath(ruta)
    candidato = os.path.basename(ruta) if os.path.isdir(ruta) else os.path.splitext(os.path.basename(ruta))[0]
    padre = os.path.dirname(ruta)
    for _ in range(4):
        dice = _title_del_nombre(candidato)
        if title is not None and dice is not None and dice & ~0xFFF != title & ~0xFFF:
            break
        limpio = _limpiar_nombre(candidato)
        if limpio and not limpio.isdecimal() and limpio.lower() not in _NOMBRES_GENERICOS:
            return limpio
        candidato, padre = os.path.basename(padre), os.path.dirname(padre)
        # No subir hasta carpetas que no son del juego: la raíz, el home ni las suyas (Downloads, Juegos...).
        if not candidato or padre in ("/", "") or os.path.dirname(padre) == "/" or \
                os.path.join(padre, candidato) == os.path.expanduser("~") or padre == os.path.expanduser("~"):
            break
    return f"Juego {title:016x}" if title is not None else ""


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
        raise RuntimeError("no hay nada que se pueda agregar: se aceptan NSP, NSZ, XCI, XCZ, NCA, NRO, NUP, carpetas de "
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
    return {"title": f"{t:016x}", "base": base, "update": upd[-1] if upd else None, "dlc": dlc,
            "nombre": nombre_externo(base, t)}


def _version_del_nombre(a):
    m = re.search(r"\[v(\d+)\]", os.path.basename(a))
    if not m and os.path.basename(os.path.normpath(a)).isdecimal():
        return int(os.path.basename(os.path.normpath(a)))
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
    # El nombre del NACP (o uno que ya se mostraba bien) se conserva al volver a agregar el juego; uno provisional
    # sacado del archivo se recalcula. El Title ID es la carpeta del juego y no cambia.
    identidad = j.datos.get("identidad") or {}
    if identidad.get("nombre_fuente") not in NOMBRES_VERIFICADOS or not j.datos.get("nombre"):
        j.datos["nombre"] = origen.get("nombre") or nombre_externo(origen["base"], int(origen["title"], 16))
        identidad["nombre_fuente"] = "archivo"
    identidad["title_id"] = origen["title"]
    j.datos["identidad"] = identidad
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
        if d == t | 0x800 or (d == t and _actualizacion_extraida(a)):
            updates.append(a)
        elif t + 0x1000 < d < t + 0x2000:
            dlc.append({"ruta": a, "indice": d - t - 0x1000})
        elif d == t:
            raise RuntimeError(f"{os.path.basename(a)} es el juego base, que ya está en la biblioteca")
        else:
            raise RuntimeError(f"{os.path.basename(a)} es de otro juego (title ID {d:016x}), no de {juego.nombre}")
    return {"update": max(updates, key=_version_del_nombre) if updates else None, "dlc": dlc}


def _actualizacion_extraida(ruta):
    """Snapshot extraído de un update NUP: <versión>/<exefs, romfs>.

    El NPDM de un título actualizado conserva el Program ID del juego base, así que su
    title_id no termina en 800 como el contenedor NUP. La carpeta numérica de versión
    permite reconocer ese snapshot sin confundirlo con una carpeta base normal.
    """
    nombre = os.path.basename(os.path.normpath(ruta))
    return (nombre.isdecimal() and int(nombre) > 0 and tipo(ruta) == "extraido"
            and os.path.isdir(os.path.join(ruta, "romfs")))


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
    try:
        shutil.rmtree(juego.carpeta)
    except FileNotFoundError:
        pass
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

    def __init__(self, juego, avisar, desde=None, solo_dlc=None, motivo=None):
        # solo_dlc: índices de DLC nuevos de un juego ya construido; se extraen y no se recompila nada.
        # motivo: qué botón o aviso pidió la construcción; queda en registro.txt junto a la fecha.
        self.juego, self.avisar, self.desde, self.solo_dlc = juego, avisar, desde, solo_dlc
        self.motivo = motivo
        self.proc = None
        self.cancelado = False
        # No es daemon: si el proceso de la app termina con la construcción en marcha, Python espera a que
        # el hilo acabe en vez de matarlo a mitad de un paso (y de dejar el compilador sin quien lea su salida).
        self.hilo = threading.Thread(target=self._correr, daemon=False, name=f"construccion-{juego.id}")
        self.inicio = None

    def empezar(self):
        self.inicio = time.time()
        self.hilo.start()

    def activa(self):
        """True mientras el hilo de la construcción sigue corriendo (aunque ya haya avisado "fin"/"error")."""
        return self.hilo.is_alive()

    def cancelar(self):
        self.cancelado = True
        if self.proc and self.proc.poll() is None:
            # Todo el grupo (_ejecutar lo abre con start_new_session): los trabajadores del Pool de nxrecomp.py o
            # los clang de compilar.py seguirían solos y morirían con BrokenPipeError en el registro.
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
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
                self.log.write(f"[nx2linux] pedida por: {self.motivo or 'sin indicar'}"
                               f" (desde: {self.desde or 'lo que falte'})\n")
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
                                     errors="replace", bufsize=1, cwd=cwd, start_new_session=True)
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
        e = motor_especial(j)
        if e and hasattr(e, "extraer"):  # el port lee el juego a su manera (p. ej. el dump tal cual)
            return e.extraer(self, j)
        t = tipo(o["base"])
        if t == "extraido":
            update = o.get("update")
            if update:
                tipo_update = tipo(update)
                if tipo_update == "extraido":
                    # Un snapshot extraído del update contiene los módulos y el RomFS
                    # completos de esa versión; usarlo como título actualizado.
                    return self._extraer_extraido(update)
                if tipo_update == "nup":
                    return self._extraer_nup(update)
                raise RuntimeError("para actualizar un juego ya extraído, elige el snapshot extraído de la versión "
                                   "(carpeta numérica con exefs/ y romfs/) o el archivo .nup; un update cifrado "
                                   "necesita el paquete base original")
            return self._extraer_extraido(o["base"])
        if t == "nro":
            return self._extraer_nro(o["base"])
        if t == "nup":
            return self._extraer_nup(o["base"])
        llaves = self._llaves()
        self._linea("extraer", 0.02, "Preparando el extractor (la primera vez se compila)...")
        exe = extractor()
        for d in ("ExeFS",):
            shutil.rmtree(os.path.join(j.carpeta, d), ignore_errors=True)

        def info(linea):
            m = re.match(r"name (.+)", linea)
            if m:
                j.datos["nombre"] = m.group(1).strip()
                j.datos.setdefault("identidad", {})["nombre_fuente"] = "nacp"
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
            self.juego.datos.setdefault("identidad", {})["nombre_fuente"] = "nacp"
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

    # Un dump de Switch 2 (.nup, ya descifrado): tools/nup_extraer.py saca exefs/ y romfs/ del título más nuevo a una
    # carpeta temporal; de ahí sigue como un juego ya extraído y la carpeta se borra al final (el RomFS queda en la imagen).
    def _extraer_nup(self, ruta):
        j = self.juego
        tmp = os.path.join(j.carpeta, "nup")
        self._ejecutar("extraer", [sys.executable, os.path.join(ROOT, "tools", "nup_extraer.py"), ruta, tmp],
                       lambda l: (lambda m: 0.7 * int(m.group(1)) / max(1, int(m.group(2))) if m else None)(
                           re.match(r"progreso (\d+)/(\d+)", l)))
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "nup_extraer.py"), ruta, "--info"],
                           capture_output=True, text=True)
        m = re.search(r"^display version (.+)$", r.stdout, re.M)
        if m:
            j.datos["version"] = m.group(1).strip()
        self._extraer_extraido(tmp)
        shutil.rmtree(tmp, ignore_errors=True)

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
        self._anotar_plataforma()
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
        modulos_ro = carpeta_modulos_ro(j)
        self._ejecutar("recompilar", [sys.executable, os.path.join(ROOT, "recomp", "nxrecomp.py"),
                                      os.path.join(j.carpeta, "ExeFS"), "-o", os.path.join(j.carpeta, "c"), "--progreso",
                                      "--pistas", pistas, "--modulos-ro", modulos_ro,
                                      "--sustituir", os.path.join(ROOT, "recomp", "sustituciones.txt")]
                      + (["--sustituir", os.path.join(ROOT, "recomp", "sustituciones-switch2.txt")]
                        if j.datos.get("plataforma") == "switch2" else []), prog)
        j.datos["pistas_usadas"] = _contar_pistas(pistas)
        j.datos["modulos_ro_usados"] = _modulos_ro(modulos_ro)
        j.guardar()

    def _anotar_plataforma(self):
        """Switch 1 o Switch 2 (recomp/nso.py plataforma), según el ExeFS extraído; queda en juego.json."""
        j = self.juego
        exefs = os.path.join(j.carpeta, "ExeFS")
        if not os.path.isdir(exefs):
            return
        sys.path.insert(0, os.path.join(ROOT, "recomp"))
        from nso import plataforma
        cual, razones = plataforma(exefs)
        j.datos["plataforma"] = cual
        j.guardar()
        if cual == "switch2":
            self._linea("recompilar", 0, "Juego de Switch 2 detectado (" + ", ".join(razones) + "). La compatibilidad "
                        "del runtime Linux con Switch 2 sigue en desarrollo; consulta el registro de ejecución si falla.")

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
                                    os.path.join(j.carpeta, "c"), "-o", j.ejecutable_nativo, "--progreso"], prog)

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
        direcciones = set()
        with open(ruta) as f:
            for linea in f:
                try:
                    direccion = int(linea.split()[0], 0)
                except (ValueError, IndexError):
                    continue
                if direccion > 0 and not direccion & 3:
                    direcciones.add(direccion)
        return len(direcciones)
    except OSError:
        return 0


# Qué quiere decir el código con que terminó un juego. Los números fijos son los exit() del runtime (runtime/*.c, sin
# runtime/nvn): tests/salidas_test.py falla si el runtime agrega uno que esta tabla no explica. subprocess da las
# señales en negativo (-11) y una terminal como 128 + señal (139).
SALIDAS = {
    0: "terminó normalmente",
    1: "el runtime cerró el juego: svcBreak que no es solo aviso, fatal:u, una SVC, función, instrucción o función "
       "NVN sin implementar, o memoria que no se pudo reservar (la línea [nxrt] o [RED] anterior dice cuál)",
    3: "salto indirecto a código que la recompilación no vio ([RED] salto indirecto); se anota en pistas y se "
       "arregla recompilando",
    4: "no se pudo crear la ventana o el contexto gráfico (SDL o EGL/Zink)",
}
_SENALES = {6: "SIGABRT (abort)", 9: "SIGKILL (lo mató el sistema u otro proceso)", 11: "SIGSEGV (violación de "
            "segmento; el primer fallo es la línea [nxrt] violación de segmento)", 15: "SIGTERM (cerrado desde afuera)"}


def significado_salida(rc):
    """Texto para el código de salida rc de un juego (Popen.returncode o el de una terminal)."""
    if rc is None:
        return "sigue corriendo"
    senal = -rc if rc < 0 else rc - 128 if 128 < rc < 160 else None
    if senal is not None:
        return f"señal {senal}: " + _SENALES.get(senal, "terminado por una señal")
    return SALIDAS.get(rc, "código que el runtime no usa: lo puso el juego (svcExitProcess) o una biblioteca")


def carpeta_modulos_ro(j):
    """Donde el runtime vuelca los NRO que el juego arma en memoria y carga con nn::ro (NX_VOLCAR_NRO); la
    recompilación los traduce desde ahí (nxrecomp.py --modulos-ro)."""
    return os.path.join(j.carpeta, "nro")


def _modulos_ro(carpeta):
    try:
        return sorted(n for n in os.listdir(carpeta) if re.fullmatch(r"[0-9a-f]{64}\.nro", n))
    except OSError:
        return []


def modulos_ro_nuevos(j):
    """Cuántos NRO volcó el juego que la última recompilación todavía no tradujo."""
    return len(set(_modulos_ro(carpeta_modulos_ro(j))) - set(j.datos.get("modulos_ro_usados", [])))


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
    env["NX_PLATAFORMA"] = j.datos.get("plataforma", "switch1")   # runtime/nvn/tex_deswizzle.c: GOB de Switch 2
    env["NX_PISTAS"] = os.path.join(j.carpeta, "pistas.txt")
    env["NX_VOLCAR_NRO"] = carpeta_modulos_ro(j)
    env["NX_TITULO"] = j.nombre
    # La bajada SM86 propia necesita el traductor Python. Los juegos viven
    # fuera del repositorio, así que el runtime no puede descubrirlo junto al
    # ejecutable; la configuración específica del juego aún puede sustituirlo.
    sm86_programa = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                 "tools", "sm86_programa.py")
    if os.path.isfile(sm86_programa):
        env["NX_SM86_PROGRAMA"] = sm86_programa
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
    # El ajuste del juego manda sobre el entorno heredado: el runtime NVN de NX2Linux lee TOTK_FRAMEGEN y, si
    # solo se exportara "on", un valor heredado dejaría la generación activa aunque el usuario la apague.
    env["TOTK_FRAMEGEN"] = "on" if aj.get("generacion_cuadros") else "off"
    telemetria = aj.get("telemetria", "off")
    env["TOTK_PERF_OVERLAY"] = telemetria if telemetria in ("off", "fps", "full") else "off"
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


class EjecutableObsoleto(RuntimeError):
    """El ejecutable nativo es anterior a algún archivo del runtime: hay que volver a enlazarlo."""


def jugar(j, salida=None):
    """Arranca el juego; su salida va a <carpeta>/ultima_partida.txt."""
    e = motor_especial(j)
    exe = j.ejecutable
    if not os.path.exists(exe):
        raise RuntimeError("el juego todavía no está construido")
    # La ventana decide si "Jugar" está activo al refrescarse; si el runtime cambia después (o se abre desde
    # el menú con --jugar), sin esta comprobación se lanzaría un ejecutable enlazado con el runtime anterior.
    if not e and j.runtime_desactualizado:
        raise EjecutableObsoleto("el runtime cambió después de compilar este juego; actualiza el ejecutable "
                                 "(Actualizar runtime) antes de jugar")
    log = salida or open(os.path.join(j.carpeta, "ultima_partida.txt"), "w")
    if e:
        return e.lanzar(j, log)
    return subprocess.Popen([exe], env=entorno_juego(j), stdout=log, stderr=subprocess.STDOUT,
                            cwd=j.carpeta, start_new_session=True)


# ---------- partidas en marcha ----------

def _exe_de(pid):
    """Ruta del ejecutable de pid, sin el " (deleted)" que pone el núcleo cuando se recompiló encima."""
    try:
        r = os.readlink(f"/proc/{pid}/exe")
    except OSError:
        return None
    return r[:-len(" (deleted)")] if r.endswith(" (deleted)") else r


def _hijos():
    """{ppid: [pids]} de todos los procesos visibles."""
    hijos = {}
    for n in os.listdir("/proc"):
        if not n.isdigit():
            continue
        try:
            with open(f"/proc/{n}/stat") as f:
                ppid = int(f.read().rsplit(")", 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            continue
        hijos.setdefault(ppid, []).append(int(n))
    return hijos


def descendientes(pid, hijos=None):
    """pid y todos sus descendientes."""
    hijos = _hijos() if hijos is None else hijos
    vistos, pila = [], [pid]
    while pila:
        p = pila.pop()
        if p not in vistos:
            vistos.append(p)
            pila.extend(hijos.get(p, []))
    return vistos


def procesos_del_juego(j):
    """PIDs que ejecutan el binario del juego, los haya abierto esta ventana de NX2Linux o una anterior que se
    cerró (al cerrarse, su partida queda huérfana y la ventana nueva no la conoce)."""
    rutas = {os.path.realpath(p) for p in (j.ejecutable, j.ejecutable_nativo)}
    propio = os.getpid()

    def es_del_juego(pid):
        exe = _exe_de(pid)
        if exe is None:   # ya terminó (o es un zombi esperando su wait)
            return False
        if exe in rutas:
            return True
        # Si el binario se reemplazó por renombre, el enlace exe apunta al nombre viejo: mirar argv[0].
        try:
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                arg0 = f.read().split(b"\0", 1)[0].decode(errors="replace")
            cwd = os.readlink(f"/proc/{pid}/cwd")
        except OSError:
            return False
        return bool(arg0) and os.path.normpath(os.path.join(cwd, arg0)) in rutas

    return sorted(int(n) for n in os.listdir("/proc") if n.isdigit() and int(n) != propio and es_del_juego(n))


def pids_con_ventana():
    """PIDs que tienen una ventana en el compositor, o None si no se puede saber (no es Hyprland)."""
    try:
        r = subprocess.run(["hyprctl", "clients", "-j"], capture_output=True, text=True, timeout=3)
        return {c.get("pid") for c in json.loads(r.stdout)} if r.returncode == 0 else None
    except (OSError, ValueError, subprocess.TimeoutExpired):
        return None


def tiene_ventana(pids):
    """True/False si alguno de pids (o sus descendientes) tiene ventana; None si no se puede saber."""
    con = pids_con_ventana()
    if con is None:
        return None
    hijos = _hijos()
    return any(d in con for p in pids for d in descendientes(p, hijos))


def enfocar(pids):
    """Lleva el foco a la ventana de alguno de pids. True si encontró una."""
    con = pids_con_ventana() or set()
    hijos = _hijos()
    for p in pids:
        for d in descendientes(p, hijos):
            if d in con:
                subprocess.run(["hyprctl", "dispatch", "focuswindow", f"pid:{d}"], capture_output=True, timeout=3)
                return True
    return False


def cola_salida(j, lineas=12):
    """Las últimas líneas de ultima_partida.txt (lo último que dijo el juego), o "" si no hay."""
    try:
        with open(os.path.join(j.carpeta, "ultima_partida.txt"), "rb") as f:
            f.seek(0, os.SEEK_END)
            f.seek(max(0, f.tell() - 16384))
            texto = f.read().decode(errors="replace")
    except OSError:
        return ""
    return "\n".join(texto.rstrip().splitlines()[-lineas:])


def cerrar_partidas(pids, espera=3.0):
    """SIGTERM a cada partida y su grupo (se lanzan con start_new_session); SIGKILL a lo que siga vivo tras
    `espera` segundos. Devuelve los PIDs que no se pudieron cerrar."""
    todos = set()
    hijos = _hijos()
    for p in pids:
        todos.update(descendientes(p, hijos))
    for p in pids:
        try:
            os.killpg(p, signal.SIGTERM) if os.getpgid(p) == p else os.kill(p, signal.SIGTERM)
        except OSError:
            pass
    for p in todos:
        try:
            os.kill(p, signal.SIGTERM)
        except OSError:
            pass
    fin = time.monotonic() + espera
    while time.monotonic() < fin and any(_exe_de(p) is not None for p in todos):   # los zombis no cuentan
        time.sleep(0.1)
    for p in todos:
        try:
            os.kill(p, signal.SIGKILL)
        except OSError:
            pass
    time.sleep(0.2)
    return sorted(p for p in todos if _exe_de(p) is not None)
