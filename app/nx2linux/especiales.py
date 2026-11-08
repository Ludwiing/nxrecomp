"""Motores especiales: juegos con un port propio, más completo que el recompilador genérico.

NX2Linux extrae el juego como siempre (llaves, actualización, DLC) y luego, en lugar de nxrecomp, construye con el
port a partir de lo extraído; al jugar abre el ejecutable del port. Cada motor dice qué versiones del juego sabe
construir; con otra versión, o sin el port instalado, el juego sigue por el camino genérico.

Un motor recibe la construcción en curso (motor.Construccion) para correr sus órdenes con su registro y su barra.
"""

import os
import re
import shutil
import subprocess
import sys

from . import motor


def _progreso(patron, escala=1.0):
    def leer(linea):
        m = re.match(patron, linea)
        return escala * int(m.group(1)) / max(1, int(m.group(2))) if m else None
    return leer


def _romfs_estandar(c, j):
    """RomFS.img.native (RomFS estándar) a partir del RomFS.img que dejó el extractor; devuelve su ruta."""
    romfs = os.path.join(j.carpeta, "RomFS.img")
    if os.path.isfile(romfs):
        c._ejecutar("recompilar", [sys.executable, os.path.join(motor.ROOT, "tools", "romfs_native.py"), romfs])
    return romfs + ".native"


def _memoria_gb():
    try:
        return int(next(l for l in open("/proc/meminfo") if l.startswith("MemAvailable:")).split()[1]) // (1 << 20)
    except (OSError, StopIteration, ValueError):
        return 8


class Especial:
    nombre = ""
    versiones = {}      # versión del juego -> etiqueta del port
    clave_config = ""   # config.json: carpeta del port (si no, ~/Projects/<nombre>)

    def repo(self):
        return motor.leer_config().get(self.clave_config) or os.path.expanduser(f"~/Projects/{self.nombre}")

    def instalado(self):
        return True

    def sirve(self, j):
        return j.version in self.versiones and os.path.isdir(self.repo()) and self.instalado()

    def exe(self, j):
        raise NotImplementedError

    def lanzar(self, j, log):
        # El port lee su configuración y sus datos junto a su ejecutable.
        return subprocess.Popen([self.exe(j)], stdout=log, stderr=subprocess.STDOUT, cwd=os.path.dirname(self.exe(j)),
                                start_new_session=True)


class Totk(Especial):
    """totk-pc-recomp (MIT): su build/extracted/<ver> se arma con el ExeFS y el RomFS de este juego (el RomFS
    estándar enlazado, sin copiar sus gigas); gen.py traduce el código y build_game.py lo compila."""
    nombre, clave_config = "totk-pc-recomp", "totk_pc"
    versiones = {"1.2.1": "v121", "1.4.3": "v143"}

    def instalado(self):
        return all(os.path.isfile(os.path.join(self.repo(), v, "bin", "python")) for v in (".venv", ".venv-py314"))

    def exe(self, j):
        return os.path.join(self.repo(), "build", self.versiones[j.version], "totk")

    def recompilar(self, c, j):
        repo, ver = self.repo(), self.versiones[j.version]
        nativa = _romfs_estandar(c, j)
        destino = os.path.join(repo, "build", "extracted", ver)
        shutil.rmtree(os.path.join(destino, "exefs"), ignore_errors=True)
        shutil.copytree(os.path.join(j.carpeta, "ExeFS"), os.path.join(destino, "exefs"))
        os.makedirs(os.path.join(destino, "romfs"), exist_ok=True)
        imagen = os.path.join(destino, "romfs", "romfs.bin")
        if not (os.path.exists(imagen) and os.path.samefile(imagen, nativa)):
            if os.path.exists(imagen):
                os.remove(imagen)
            try:
                os.link(nativa, imagen)
            except OSError:   # otro sistema de archivos: copia (reflink si se puede)
                c._ejecutar("recompilar", ["cp", "--reflink=auto", nativa, imagen])
        c._ejecutar("recompilar", [sys.executable, os.path.join(motor.ROOT, "tools", "indice_romfs.py"), repo,
                                   os.path.dirname(imagen)])
        c._ejecutar("recompilar", [os.path.join(repo, ".venv", "bin", "python"), os.path.join(repo, "recomp", "gen.py"),
                                   "--exefs", os.path.join(destino, "exefs"), "--out", os.path.join(repo, "generated", ver),
                                   "--patches", os.path.join(repo, "recomp", f"patches-{ver}.txt")],
                    _progreso(r"emitted (\d+)/(\d+) chunks"), cwd=repo)

    def compilar(self, c, j):
        repo, ver = self.repo(), self.versiones[j.version]
        py, py314 = os.path.join(repo, ".venv", "bin", "python"), os.path.join(repo, ".venv-py314", "bin", "python")
        c._ejecutar("compilar", [py314, os.path.join(repo, "tools", "make_romfs_patches.py"), "--version", ver], cwd=repo)
        c._ejecutar("compilar", [py, os.path.join(repo, "tools", "build_game.py"), "--version", ver],
                    _progreso(r"\[(\d+)/(\d+)\]", 0.8), cwd=repo)
        c._linea("compilar", 0.8, "Preparando los shaders (se traducen una vez y quedan en caché)...")
        c._ejecutar("compilar", [py314, os.path.join(repo, "tools", "prepare_shaders.py"), os.path.join(repo, "build", ver),
                                 "--version", ver], cwd=repo)


class Ssbu(Especial):
    """ssbu-recomp: lee el juego como carpeta (<dump>/exefs y <dump>/romfs con sus archivos); scripts/setup.py
    traduce y compila (reanudable) y scripts/run.py lo abre. La 13.0.2 es un port local (memoria del proyecto)."""
    nombre, clave_config = "ssbu-recomp", "ssbu_recomp"
    versiones = {"13.0.2": "13.0.2", "13.0.4": "13.0.4", "13.0.5": "13.0.5"}

    def _py(self):
        return os.path.join(self.repo(), "deps", "venv", "bin", "python")

    def instalado(self):
        return os.path.isfile(self._py())

    def exe(self, j):
        return os.path.join(self.repo(), "build-linux", "work", "ssbu_native")

    def dump(self, j):
        # Un juego registrado desde un dump propio (sin pasar por el extractor) guarda su carpeta en juego.json.
        return j.datos.get("dump") or os.path.join(j.carpeta, "dump")

    def recompilar(self, c, j):
        dump = self.dump(j)
        if j.datos.get("dump"):
            c._linea("recompilar", 1, f"Se usa el dump registrado: {dump}")
            return
        shutil.rmtree(os.path.join(dump, "exefs"), ignore_errors=True)
        shutil.copytree(os.path.join(j.carpeta, "ExeFS"), os.path.join(dump, "exefs"))
        nativa = _romfs_estandar(c, j)
        c._ejecutar("recompilar", [sys.executable, os.path.join(motor.ROOT, "tools", "romfs_archivos.py"), nativa,
                                   os.path.join(dump, "romfs")], _progreso(r"progreso (\d+)/(\d+)"))

    def compilar(self, c, j):
        # Un proceso de compilación por cada 2 GB libres: con más, el núcleo mata procesos por falta de memoria.
        hilos = max(1, min(os.cpu_count() or 1, _memoria_gb() // 2))
        c._ejecutar("compilar", [self._py(), os.path.join(self.repo(), "scripts", "setup.py"), "--target", "linux",
                                 "--dump", self.dump(j), "--jobs", str(hilos)], cwd=self.repo())

    def lanzar(self, j, log):
        datos = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "ssbu-recomp")
        os.makedirs(datos, exist_ok=True)
        return subprocess.Popen([self._py(), os.path.join(self.repo(), "scripts", "run.py"), "--linux", "--log",
                                 os.path.join(datos, "last_run.log")], stdout=log, stderr=subprocess.STDOUT,
                                cwd=self.repo(), start_new_session=True)


class Hookshot(Especial):
    """Hookshot Builder (NotRexed/Hookshot, GPL-3.0): Ocarina of Time de Switch 2 nativo. Se usa como programa aparte
    (hookshot-builder --cli, es decir nx2/make_game.py del AppImage), así cada uno se actualiza
    por su lado. Toma el juego de un dump propio, como .nup o como carpeta con exefs/ y romfs/; el --check de Hookshot
    dice si es el juego y la versión que sabe construir. La carpeta que arma queda en <juego>/hookshot y se abre con su lanzador."""
    nombre, clave_config = "Hookshot", "hookshot_builder"

    def builder(self):
        return motor.leer_config().get(self.clave_config) or os.path.expanduser("~/Applications/Hookshot/hookshot-builder")

    def repo(self):
        return os.path.dirname(self.builder())

    def sirve(self, j):
        # La versión la revisa make_game.py --check; aquí basta con que Hookshot esté instalado.
        return os.access(self.builder(), os.X_OK)

    def salida(self, j):
        return os.path.join(j.carpeta, "hookshot")

    def exe(self, j):
        return os.path.join(self.salida(j), "bin", "hookshot")

    def _cli(self, *args):
        return [self.builder(), "--cli", *args]

    def _fuentes(self, j):
        """Los argumentos de make_game.py que dicen dónde está el juego."""
        h = j.datos.get("hookshot") or {}
        if h.get("nup"):
            if not os.path.isfile(h["nup"]):
                raise RuntimeError(f"ya no está {h['nup']}; agrega el juego otra vez")
            return ["--nup", h["nup"]]
        if not (os.path.isdir(h.get("exefs", "")) and os.path.isdir(h.get("romfs", ""))):
            raise RuntimeError("falta la carpeta del juego (exefs/ y romfs/); agrégalo otra vez")
        return ["--romfs", h["romfs"], "--exefs", h["exefs"]]

    def extraer(self, c, j):
        """Sin copiar nada: Hookshot lee el dump directamente. Solo se anotan sus carpetas, el nombre y el ícono."""
        carpeta = j.datos["origen"]["base"]
        if motor.tipo(carpeta) == "nup":
            j.datos["hookshot"] = {"nup": carpeta}
            if j.datos.get("nombre") in (None, "", os.path.splitext(os.path.basename(carpeta))[0]):
                j.datos["nombre"] = "The Legend of Zelda: Ocarina of Time"
            j.guardar()
            c._linea("extraer", 1, f"Dump: {carpeta} (Hookshot lo lee directamente)")
            return
        exefs = motor._carpeta_exefs(carpeta) if os.path.isdir(carpeta) else None
        hijos = {n.lower(): os.path.join(carpeta, n) for n in os.listdir(carpeta)} if exefs else {}
        romfs = hijos.get("romfs")
        if not exefs or not romfs or not os.path.isdir(romfs):
            raise RuntimeError("Hookshot necesita el juego como carpeta de tu dump, con exefs/ y romfs/ adentro")
        j.datos["hookshot"] = {"exefs": exefs, "romfs": romfs}
        if j.datos.get("nombre") in (None, "", os.path.basename(os.path.normpath(carpeta))):
            j.datos["nombre"] = "The Legend of Zelda: Ocarina of Time"
        for n in ("icon.jpg", "icon.png"):
            if n in hijos:
                shutil.copyfile(hijos[n], os.path.join(j.carpeta, n))
        j.guardar()
        c._linea("extraer", 1, f"Dump: {exefs} y {romfs}")

    def recompilar(self, c, j):
        fuentes = self._fuentes(j)
        malo = []

        def revisar(linea):
            m = re.match(r"@@check (ok|bad) (.*)", linea)
            if m and m.group(1) == "bad":
                malo.append(m.group(2))
            return None
        # --check mira solo el código: el .nup, o el exefs de la carpeta (sin --romfs)
        revisar_con = fuentes if fuentes[0] == "--nup" else fuentes[2:]
        c._ejecutar("recompilar", self._cli("--check", *revisar_con), revisar)
        if malo:
            raise RuntimeError("Hookshot no puede construir este juego: " + malo[0])

    def compilar(self, c, j):
        fuentes = self._fuentes(j)
        idioma = j.datos["ajustes"].get("idioma", "es-419")
        c._ejecutar("compilar", self._cli(*fuentes, "--out", self.salida(j), "--language", idioma),
                    lambda l: float(l.split()[1]) if l.startswith("@@progress ") else None)

    def lanzar(self, j, log):
        # El lanzador de la carpeta (hookshot) abre el juego como lo haría Hookshot Builder.
        lanzador = os.path.join(self.salida(j), "hookshot")
        cmd = [lanzador] if os.access(lanzador, os.X_OK) else [self.exe(j)]
        return subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT, cwd=self.salida(j), start_new_session=True)


ESPECIALES = {"0100f2c0115b6000": Totk(), "01006a800016e000": Ssbu(), "0400486024dec000": Hookshot()}


def para(j):
    """El motor especial que construye este juego, o None si va por el camino genérico."""
    e = ESPECIALES.get(j.id)
    return e if e and e.sirve(j) else None
