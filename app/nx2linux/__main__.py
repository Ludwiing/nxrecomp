"""Punto de entrada: sin argumentos abre la ventana; también sirve desde la terminal.

    nx2linux                       abre la app
    nx2linux --jugar <title id>    abre un juego ya construido (lo usan las entradas del menú)
    nx2linux --lista               lista la biblioteca
    nx2linux --construir <carpeta o archivos...>   agrega y construye sin ventana
"""

import sys

from . import motor


def _construir(rutas):
    llaves = motor.ruta_llaves()
    if not llaves:
        sys.exit("falta elegir prod.keys: ábrelo una vez con la ventana (Preferencias)")
    origen = motor.clasificar(rutas, llaves)
    j = motor.nuevo_juego(origen)
    import threading
    fin = threading.Event()
    estado = {}

    def avisar(paso, frac, linea):
        if linea:
            print(linea, flush=True)
        if paso in ("fin", "error"):
            estado["ok"] = paso == "fin"
            fin.set()
    c = motor.Construccion(j, avisar)
    c.empezar()
    try:
        fin.wait()
    except KeyboardInterrupt:
        c.cancelar()
        fin.wait()
    return 0 if estado.get("ok") else 1


def main():
    a = sys.argv[1:]
    if a[:1] == ["--jugar"] and len(a) == 2:
        j = next((j for j in motor.biblioteca() if j.id == a[1]), None)
        if not j:
            sys.exit(f"no hay un juego {a[1]} en la biblioteca")
        return motor.jugar(j).wait()
    if a == ["--lista"]:
        for j in motor.biblioteca():
            print(f"{j.id}  {'listo ' if j.listo else 'a medias'}  {j.nombre} {j.version}")
        return 0
    if a[:1] == ["--construir"] and len(a) > 1:
        return _construir(a[1:])
    if a:
        sys.exit(__doc__)
    from .ventana import App
    return App().run([sys.argv[0]])


sys.exit(main())
