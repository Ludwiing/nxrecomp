#!/usr/bin/env python3
"""runtime/nvn/sm86_propia.c (NX_SM86_BAJADA=propia): el runtime baja SM86 -> Maxwell con tools/sm86_programa.py y
recibe exactamente lo que da armar() en Python, también el c1 reescrito de un BRX. Una instrucción que la bajada no
conoce, o un script que no está, dan un error con el motivo (sin vuelta silenciosa a la otra vía). La huella que va
en las claves de la caché de shaders cambia cuando cambia una regla de bajada. Código propio (MIT).

    python3 tests/sm86_propia_test.py build/sm86_propia_test     0 bien, 1 mal, 77 sin build/sm86_spec.json

Los programas son los de tests/sm86_programa_test.py (cómputo, saltos con BRX y su tabla en c1, fragmento con SPH);
esta prueba no necesita totk_xlate ni CUDA: lo que el traductor hace con el Maxwell ya lo prueba aquella."""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(RAIZ, "tools", "sm86_programa.py")
fallos = 0


def mal(msg):
    global fallos
    fallos += 1
    print("MAL: " + msg)


def correr(driver, *args, env=None):
    e = dict(os.environ)
    e.update(env or {})
    return subprocess.run([driver, *args], capture_output=True, text=True, env=e, timeout=300)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    driver = os.path.abspath(sys.argv[1])
    if not os.path.isfile(os.path.join(RAIZ, "build", "sm86_spec.json")):
        print("(sin build/sm86_spec.json no se prueba la bajada SM86 propia del runtime)")
        return 77
    sys.path.insert(0, os.path.join(RAIZ, "tests"))
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import sm86_programa as sp
    import sm86_programa_test as t

    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as tmp:
        casos = (("cómputo", t.computo(), b"", "cs", b""),
                 ("saltos", t.saltos(), b"", "cs", t.C1_SALTOS),
                 ("fragmento", t.fragmento(), t.sph_fragmento(), "ps", b""))
        for nombre, prog, sph, etapa, c1 in casos:
            area = sph + b"".join(struct.pack("<QQ", lo, hi) for (lo, hi), _ in prog)
            esperado = sp.armar(area, etapa, c1)
            d = os.path.join(tmp, "caso")
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
            pa, pc = os.path.join(tmp, "area.bin"), os.path.join(tmp, "c1.bin")
            open(pa, "wb").write(area)
            open(pc, "wb").write(c1)
            r = correr(driver, "bajar", SCRIPT, d, etapa, pa, *([pc] if c1 else []))
            if r.returncode:
                mal(f"{nombre}: {r.stdout}{r.stderr}")
                continue
            codigo = open(os.path.join(d, "salida.bin"), "rb").read()
            if codigo != esperado["codigo"]:
                mal(f"{nombre}: el código Maxwell del runtime ({len(codigo)} bytes) no es el de armar() "
                    f"({len(esperado['codigo'])})")
            if c1:
                c1_rt = open(os.path.join(d, "salida.c1"), "rb").read()
                if c1_rt != esperado["c1"]:
                    mal(f"{nombre}: c1 reescrito distinto: {c1_rt.hex()} / {esperado['c1'].hex()}")
                if c1_rt == c1:
                    mal(f"{nombre}: la tabla del BRX no se reescribió (la prueba no prueba nada)")
            sobras = sorted(set(os.listdir(d)) - {"salida.bin", "salida.c1"})
            if sobras:
                mal(f"{nombre}: quedaron archivos de paso: {sobras}")

        # Una instrucción desconocida: el error del runtime es el de sm86_programa.py.
        area = struct.pack("<QQ", 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF)
        try:
            sp.armar(area, "cs")
            mal("instrucción inventada: armar() no falló (cambiar el caso)")
        except sp.ErrorPrograma as e:
            motivo = str(e)
            pa = os.path.join(tmp, "mala.bin")
            open(pa, "wb").write(area)
            r = correr(driver, "bajar", SCRIPT, tmp, "cs", pa)
            if r.returncode != 1 or motivo not in r.stdout:
                mal(f"instrucción desconocida: se esperaba 'error: sm86_programa: {motivo}', salió {r.stdout!r}")

        # Sin script: error claro, no Maxwell de otro lado.
        r = correr(driver, "bajar", "-", tmp, "cs", pa)
        if r.returncode != 1 or "NX_SM86_PROGRAMA" not in r.stdout:
            mal(f"sin script: {r.stdout!r}")
        r = correr(driver, "script", env={"NX_SM86_PROGRAMA": os.path.join(tmp, "no_esta.py")})
        if not r.stdout.startswith("0 "):
            mal(f"NX_SM86_PROGRAMA a un archivo que no está: {r.stdout!r}")
        r = correr(driver, "script", env={"NX_SM86_PROGRAMA": ""})   # desde build/: sube hasta tools/
        if r.stdout.strip() != f"1 {SCRIPT}":
            mal(f"script hallado desde el ejecutable: {r.stdout!r}, se esperaba {SCRIPT}")

        # Solo "propia" la enciende; un valor desconocido avisa.
        for valor, esperado in (("propia", "1"), ("nxnative", "0"), ("", "0"), ("otra", "0")):
            r = correr(driver, "activa", env={"NX_SM86_BAJADA": valor})
            if r.stdout.strip() != esperado or (valor == "otra") != ("no se conoce" in r.stderr):
                mal(f"NX_SM86_BAJADA={valor!r}: {r.stdout.strip()} {r.stderr.strip()!r}")

        # La huella cambia si cambia una regla (copia del árbol de la bajada; el repositorio no se toca).
        copia = os.path.join(tmp, "arbol")
        os.makedirs(os.path.join(copia, "tools")), os.makedirs(os.path.join(copia, "build"))
        for f in os.listdir(os.path.join(RAIZ, "tools")):
            if f.startswith("sm86") and f.endswith(".py") or f == "mxisa.json":
                shutil.copy(os.path.join(RAIZ, "tools", f), os.path.join(copia, "tools", f))
        shutil.copy(os.path.join(RAIZ, "build", "sm86_spec.json"), os.path.join(copia, "build"))
        script_copia = os.path.join(copia, "tools", "sm86_programa.py")
        antes = correr(driver, "huella", script_copia).stdout
        with open(os.path.join(copia, "tools", "sm86_baja_enteros.py"), "a") as f:
            f.write("\n# cambio\n")
        despues = correr(driver, "huella", script_copia).stdout
        if not antes.strip() or antes == despues:
            mal(f"la huella no cambia con una regla nueva: {antes!r} {despues!r}")
        os.remove(os.path.join(copia, "build", "sm86_spec.json"))
        r = correr(driver, "huella", script_copia)
        if r.returncode != 1:
            mal(f"huella sin build/sm86_spec.json: {r.stdout!r}")

    if not fallos:
        print("OK: el runtime baja SM86 -> Maxwell con tools/sm86_programa.py igual que armar() (cómputo, BRX con c1, "
              "fragmento); errores con motivo; huella de caché por versión de la bajada")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
