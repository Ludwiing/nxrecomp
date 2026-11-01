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
EXTENSIONES = (".nsp", ".nsz", ".xci", ".xcz")

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
    p = leer_config().get("llaves")
    return p if p and os.path.isfile(p) else None


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
        p = os.path.join(self.carpeta, "icon.jpg")
        return p if os.path.exists(p) else None

    @property
    def ejecutable(self):
        return os.path.join(self.carpeta, "juego")

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
    if os.path.exists(exe):
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


def title_id(archivo, llaves):
    revisar_archivo(archivo)
    t = _title_del_nombre(os.path.basename(archivo))
    if t is not None:
        return t
    r = subprocess.run([extractor(), llaves, "--title", archivo], capture_output=True, text=True)
    m = re.search(r"title ([0-9a-f]{16})", r.stdout)
    if not m:
        raise RuntimeError(f"no se reconoce {os.path.basename(archivo)}: " + _primera_linea_error(r.stderr or r.stdout))
    return int(m.group(1), 16)


def clasificar(rutas, llaves):
    """De una carpeta o una lista de archivos saca {base, update, dlc[], title}.

    Los title ID de la Switch dicen qué es cada cosa: el juego termina en 000, su actualización en 800
    y los DLC tienen el bloque siguiente (base + 0x1000 + n)."""
    archivos = []
    for r in rutas:
        if os.path.isdir(r):
            archivos += [os.path.join(r, f) for f in sorted(os.listdir(r)) if f.lower().endswith(EXTENSIONES)]
        elif r.lower().endswith(EXTENSIONES):
            archivos.append(r)
    if not archivos:
        raise RuntimeError("no hay archivos .nsp, .nsz, .xci ni .xcz")
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


def nuevo_juego(origen):
    j = Juego(os.path.join(carpeta_juegos(), origen["title"]))
    j.datos["origen"] = {k: origen[k] for k in ("base", "update", "dlc")}
    j.datos["pasos"] = {}
    j.datos.setdefault("nombre", os.path.splitext(os.path.basename(origen["base"]))[0])
    j.guardar()
    return j


def quitar(juego):
    shutil.rmtree(juego.carpeta, ignore_errors=True)
    try:
        os.remove(os.path.join(APPS_DIR, f"nx2linux-{juego.id}.desktop"))
    except OSError:
        pass


# ---------- construir ----------

class Cancelado(Exception):
    pass


class Construccion:
    """Corre los pasos que falten de un juego en un hilo. `avisar(paso, fraccion, linea)` se llama desde
    ese hilo con cada novedad: paso es la clave del paso en curso (o "fin"/"error"), fraccion el
    progreso total de 0 a 1 y linea el texto nuevo del registro (o None)."""

    def __init__(self, juego, avisar, desde=None):
        self.juego, self.avisar, self.desde = juego, avisar, desde
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

    def _ejecutar(self, clave, cmd, progreso=None, entorno=None):
        """Corre una orden y pasa su salida al registro. `progreso(linea)` devuelve la fracción del paso o None."""
        if self.cancelado:
            raise Cancelado()
        self.log.write("$ " + " ".join(cmd) + "\n")
        env = dict(os.environ, PYTHONUNBUFFERED="1", **(entorno or {}))
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env,
                                     errors="replace", bufsize=1)
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
        llaves = ruta_llaves()
        if not llaves:
            raise RuntimeError("falta elegir tu archivo prod.keys en Preferencias")
        o = j.datos["origen"]
        self._linea("extraer", 0.02, "Preparando el extractor (la primera vez se compila)...")
        exe = extractor()
        for d in ("ExeFS",):
            shutil.rmtree(os.path.join(j.carpeta, d), ignore_errors=True)
        n = 1 + len(o.get("dlc") or [])

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
        for k, d in enumerate(o.get("dlc") or []):
            self._linea("extraer", (k + 1) / n, f"Contenido descargable {k + 1} de {n - 1}")
            os.makedirs(os.path.join(j.carpeta, "aoc"), exist_ok=True)
            ruta, indice = (d["ruta"], d["indice"]) if isinstance(d, dict) else (d, k + 1)
            self._ejecutar("extraer", [exe, llaves, "--aoc", ruta, os.path.join(j.carpeta, "aoc", f"{indice}.img"),
                                       "--romfs-image"])

    # Paso 2: el recompilador estático.
    def _recompilar(self):
        j = self.juego

        def prog(linea):
            m = re.match(r"progreso (\d+)/(\d+)", linea)
            return int(m.group(1)) / int(m.group(2)) if m else None

        self._ejecutar("recompilar", [sys.executable, os.path.join(ROOT, "recomp", "nxrecomp.py"),
                                      os.path.join(j.carpeta, "ExeFS"), "-o", os.path.join(j.carpeta, "c"), "--progreso"], prog)

    # Paso 3: clang en todos los núcleos.
    def _compilar(self):
        j = self.juego

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


# ---------- jugar ----------

def entorno_juego(j):
    aj = j.datos["ajustes"]
    env = dict(os.environ)
    env["NX_ROMFS"] = os.path.join(j.carpeta, "RomFS.img")
    env["NX_SDMC"] = os.path.join(j.carpeta, "guardado", "sdmc")
    env["NX_GUARDADO"] = os.path.join(j.carpeta, "guardado")
    env["NX_TITLE_ID"] = j.id
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
    os.makedirs(env["NX_SDMC"], exist_ok=True)
    return env


def jugar(j, salida=None):
    """Arranca el juego; su salida va a <carpeta>/ultima_partida.txt."""
    if not os.path.exists(j.ejecutable):
        raise RuntimeError("el juego todavía no está construido")
    log = salida or open(os.path.join(j.carpeta, "ultima_partida.txt"), "w")
    return subprocess.Popen([j.ejecutable], env=entorno_juego(j), stdout=log, stderr=subprocess.STDOUT,
                            cwd=j.carpeta, start_new_session=True)
