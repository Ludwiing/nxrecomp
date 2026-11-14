"""recomp/sustituciones-switch2.txt leído con el parser real del recompilador (leer_sustituciones): todas sus líneas se
entienden y las consultas de hardware "Ounce" (Switch 2) devuelven lo que espera el SDK. Sin juegos reales: la tabla de
símbolos es sintética."""
import contextlib
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "recomp"))
import nxrecomp  # noqa: E402

RUTA = os.path.join(ROOT, "recomp", "sustituciones-switch2.txt")
OUNCE = "_ZN2nn2pl16IsRunningOnOunceEv"
VIBRACION = "_ZN2nn3hid29IsOunceVibrationDeviceMountedERKNS0_21VibrationDeviceHandleE"


def main():
    simbolos = {OUNCE: 0x1000, VIBRACION: 0x2000}
    salida = io.StringIO()
    with contextlib.redirect_stdout(salida):
        res, _ = nxrecomp.leer_sustituciones([RUTA], simbolos, lambda a: True)
    mal = []
    if "no se entiende" in salida.getvalue():
        mal.append("hay líneas que el parser no entiende:\n" + salida.getvalue())
    if res.get(0x1000) != ("valor", 1):
        mal.append(f"IsRunningOnOunce: {res.get(0x1000)}, se esperaba ('valor', 1)")
    if res.get(0x2000) != ("valor", 0):
        mal.append(f"IsOunceVibrationDeviceMounted: {res.get(0x2000)}, se esperaba ('valor', 0)")
    if mal:
        print("MAL: " + "\n".join(mal))
        return 1
    print("OK: sustituciones de Switch 2 legibles; IsRunningOnOunce = 1, IsOunceVibrationDeviceMounted = 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
