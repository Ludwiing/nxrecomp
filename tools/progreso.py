"""Panel de avance del trabajo de Switch 2 en NX2Linux: una barra con su porcentaje por tarea.

    python3 tools/progreso.py --vivo        se refresca cada segundo (Ctrl+C para salir)
    python3 tools/progreso.py               una vez

El estado vive en build/progreso.json. Lo escriben las herramientas largas mientras corren (tools/sm86.py generar y
validar) y se actualiza a mano al terminar cada paso:

    python3 tools/progreso.py poner <clave> <hechos> <total> [nota]     una tarea (la crea si no existe)
    python3 tools/progreso.py titulo <clave> "<texto>"                   el nombre que se muestra
"""
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARCHIVO = os.path.join(ROOT, "build", "progreso.json")

# Orden y nombres por defecto; cada tarea: {"titulo", "hechos", "total", "nota", "grupo", "cuando"}
INICIAL = {
    "fase1": {"grupo": "Switch 2", "titulo": "Fase 1: HLE por símbolo", "hechos": 1, "total": 1, "nota": "listo"},
    "fase2": {"grupo": "Switch 2", "titulo": "Fase 2: servicios del sistema", "hechos": 0, "total": 1,
              "nota": "espera los nombres del SDK (tools/sdk_simbolos.py sobre un dump)"},
    "fase3": {"grupo": "Switch 2", "titulo": "Fase 3: NVN2 (GPU)", "hechos": 0, "total": 1, "nota": "pendiente"},
    "fase4_dec": {"grupo": "Fase 4: shaders Ampere", "titulo": "Decodificador SM86 (vs nvdisasm)", "hechos": 0,
                  "total": 1, "nota": ""},
    "fase4_c": {"grupo": "Fase 4: shaders Ampere", "titulo": "Decodificador en C (runtime)", "hechos": 0, "total": 1,
                "nota": "pendiente"},
    "fase4_baja": {"grupo": "Fase 4: shaders Ampere", "titulo": "Bajada a nxshader.c (traducción)", "hechos": 0,
                   "total": 1, "nota": "pendiente"},
    "fase5": {"grupo": "Switch 2", "titulo": "Fase 5: texturas de Switch 2", "hechos": 1, "total": 4,
              "nota": "GOB listo; falta ASTC, mesh, caché"},
    "deswizzle_gpu": {"grupo": "Switch 1 y 2", "titulo": "Reacomodo de texturas en la GPU", "hechos": 0, "total": 1,
                      "nota": "después del decodificador"},
    "framegen": {"grupo": "Switch 1 y 2", "titulo": "Generación de cuadros", "hechos": 1, "total": 1,
                 "nota": "lista (falta probar en un juego)"},
    "fase6": {"grupo": "Switch 2", "titulo": "Fase 6: parches por juego", "hechos": 0, "total": 1,
              "nota": "resolución, FPS, pantalla ancha; uno por juego"},
}


def leer():
    try:
        with open(ARCHIVO) as f:
            return json.load(f)
    except (OSError, ValueError):
        return json.loads(json.dumps(INICIAL))


def escribir(estado):
    os.makedirs(os.path.dirname(ARCHIVO), exist_ok=True)
    tmp = ARCHIVO + ".tmp"
    with open(tmp, "w") as f:
        json.dump(estado, f, indent=1, ensure_ascii=False)
    os.replace(tmp, ARCHIVO)


def poner(clave, hechos, total, nota=None, titulo=None, grupo=None):
    """Para las herramientas: actualiza una tarea (barato; se puede llamar seguido)."""
    e = leer()
    t = e.setdefault(clave, {"grupo": grupo or "Otros", "titulo": titulo or clave, "nota": ""})
    t["hechos"], t["total"], t["cuando"] = hechos, max(total, 1), time.time()
    if nota is not None:
        t["nota"] = nota
    if titulo:
        t["titulo"] = titulo
    if grupo:
        t["grupo"] = grupo
    escribir(e)


V, A, R, G, N, B = "\033[32m", "\033[33m", "\033[31m", "\033[90m", "\033[0m", "\033[1m"


def barra(frac, ancho=30):
    llenos = frac * ancho
    enteros = int(llenos)
    parcial = " ▏▎▍▌▋▊▉"[int((llenos - enteros) * 8)] if enteros < ancho else ""
    color = V if frac >= 1 else A if frac > 0 else G
    return color + "█" * enteros + parcial + G + "·" * (ancho - enteros - len(parcial)) + N


# Cuánto pesa cada tarea en el total "un juego de Switch 2 que se vea" (las de Switch 1 no cuentan)
PESOS = {"fase1": 4, "fase2": 15, "fase3": 30, "fase4_dec": 8, "fase4_c": 5, "fase4_baja": 18, "fase5": 10,
         "fase6": 10}


def total(estado):
    hecho = peso = 0.0
    for k, w in PESOS.items():
        t = estado.get(k)
        frac = min(1.0, t.get("hechos", 0) / max(t.get("total", 1), 1)) if t else 0.0
        hecho += w * frac
        peso += w
    return hecho / peso


def mostrar(estado):
    tot = total(estado)
    lineas = [f"{B}NX2Linux · avance de Switch 2{N}   {G}{time.strftime('%H:%M:%S')}{N}", "",
              f"{B}TOTAL para que un juego de Switch 2 se vea{N}  {barra(tot, 40)} {tot * 100:5.1f}%",
              f"{G}  (ponderado: NVN2 30, bajada de shaders 18, servicios 15, texturas 10, parches 10, decodificador 8...){N}",
              ""]
    grupos = {}
    for clave, t in estado.items():
        grupos.setdefault(t.get("grupo", "Otros"), []).append((clave, t))
    for grupo, tareas in grupos.items():
        lineas.append(f"{B}{grupo}{N}")
        for clave, t in tareas:
            frac = min(1.0, t.get("hechos", 0) / max(t.get("total", 1), 1))
            vivo = time.time() - t.get("cuando", 0) < 5 and frac < 1
            marca = f"{A}●{N}" if vivo else (f"{V}✓{N}" if frac >= 1 else " ")
            lineas.append(f" {marca} {t.get('titulo', clave)[:38]:<38} {barra(frac)} {frac * 100:5.1f}%")
            if t.get("nota"):
                lineas.append(f"     {G}{t['nota'][:110]}{N}")
            for f in t.get("faltan", [])[:10]:
                lineas.append(f"     {R}falta:{N} {f[:100]}")
        lineas.append("")
    return "\n".join(lineas)


def main():
    a = sys.argv[1:]
    if a[:1] == ["poner"] and len(a) >= 4:
        poner(a[1], float(a[2]), float(a[3]), " ".join(a[4:]) or None)
    elif a[:1] == ["faltan"] and len(a) >= 2:   # faltan <clave> "cosa 1" "cosa 2" ...
        e = leer()
        e.setdefault(a[1], {"hechos": 0, "total": 1})["faltan"] = a[2:]
        escribir(e)
    elif a[:1] == ["titulo"] and len(a) == 3:
        e = leer()
        e.setdefault(a[1], {"hechos": 0, "total": 1})["titulo"] = a[2]
        escribir(e)
    elif a[:1] == ["--vivo"]:
        try:
            while True:
                sys.stdout.write("\033[H\033[2J" + mostrar(leer()) + "\n")
                sys.stdout.flush()
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        print(mostrar(leer()))


if __name__ == "__main__":
    main()
