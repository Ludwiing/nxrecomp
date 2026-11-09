"""nxrecomp.py --sustituir de punta a punta con el programa de prueba multi: un valor fijo (sdk_add =5000), un aviso
(sdk_sub !) y un prefijo (sdk_init* =77), en dos archivos; recompila, compila con tools/compilar.py y corre."""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXEFS = os.path.join(ROOT, "build", "multi_mod", "exefs")
SALIDA = os.path.join(ROOT, "build", "sustituir_rec")


def main():
    if not os.path.isfile(os.path.join(EXEFS, "main")):
        print("(falta build/multi_mod: corre make primero)")
        return 77
    with tempfile.TemporaryDirectory() as d:
        a, b = os.path.join(d, "a.txt"), os.path.join(d, "b.txt")
        open(a, "w").write("sdk_add =1\nsdk_init* =77\n")
        open(b, "w").write("sdk_add =5000   # el archivo posterior manda\nsdk_sub !\nno_existe =3\n")
        r = subprocess.run([sys.executable, os.path.join(ROOT, "recomp", "nxrecomp.py"), EXEFS, "-o", SALIDA,
                            "--sustituir", a, "--sustituir", b], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-2000:])
        return 1
    ok = "1 en C" not in r.stdout and "3 funciones del juego reemplazadas" in r.stdout and "no_existe" in r.stdout
    print(("OK" if ok else "MAL") + ": el recompilador lee los dos archivos (3 sustituciones, avisa del símbolo ausente)")
    exe = os.path.join(SALIDA, "juego")
    c = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "compilar.py"), SALIDA, "-o", exe],
                       capture_output=True, text=True)
    if c.returncode:
        print(c.stdout[-2000:], c.stderr[-2000:])
        return 1
    j = subprocess.run([exe], capture_output=True, text=True, timeout=60)
    esperado = ["suma (sdk)(1000, 7) = 5000", "resta (sdk)(1000, 7) = 0", "constructores: main 12, sdk 77",
                "multiplicación(1000, 7) = 7000", "Fin."]
    falta = [e for e in esperado if e not in j.stdout]
    avisos = j.stderr.count("[hle] sdk_sub: sin implementar")
    for e in esperado:
        print(("OK" if e not in falta else "MAL") + f": «{e}»")
    print(("OK" if avisos == 1 else "MAL") + f": el aviso de sdk_sub sale una sola vez ({avisos})")
    ok = ok and not falta and avisos == 1
    if not ok:
        print(j.stdout[-1500:], j.stderr[-1500:])
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
