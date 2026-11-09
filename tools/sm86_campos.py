"""Mapa de campos de bits de cada instrucción SM86, sacado con el oráculo (tools/sm86_oraculo.py).

Para cada código de operación válido se cambia uno por uno cada bit 12..127 y se compara el texto de nvdisasm con el de
la instrucción base: el bit puede no cambiar nada, cambiar un operando (en qué posición y de qué tipo: registro,
predicado, constante, inmediato) o cambiar un modificador del nombre (.FTZ, .SAT...). Bits seguidos que cambian el
mismo operando forman un campo. El resultado (JSON) es la base para escribir el decodificador propio.

    python3 tools/sm86_campos.py build/sm86_campos.json [código ...]
"""
import concurrent.futures as cf
import json
import re
import sys

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import sm86_oraculo as orc  # noqa: E402


def partes(texto):
    """'@P0 FADD.FTZ R1, R2, c[0x0][0x10]' -> (predicado, nombre con modificadores, [operandos])."""
    pred = ""
    if texto.startswith("@"):
        pred, texto = texto.split(" ", 1)
    nombre, _, resto = texto.partition(" ")
    ops = [o.strip() for o in re.split(r",(?![^\[]*\])", resto)] if resto.strip() else []
    return pred, nombre, ops


def tipo_operando(o):
    o = o.lstrip("-|!~").rstrip("|")
    if re.fullmatch(r"U?R\d+|RZ|URZ", o.split(".")[0]):
        return "registro"
    if re.fullmatch(r"U?P\d|PT|UPT", o):
        return "predicado"
    if o.startswith("c[") or o.startswith("cx["):
        return "constante"
    if o.startswith("[") or o.startswith("desc["):
        return "memoria"
    if re.fullmatch(r"-?(0x[0-9a-f]+|\d+(\.\d+)?(e[+-]?\d+)?|\+?INF|-?QNAN|-?INF)", o):
        return "inmediato"
    return "otro"


def mapa(op, hi, base, lo=None):
    lo = op | 0x7000 if lo is None else lo
    pb, nb, ob = partes(base)
    bits = {}
    for b in range(12, 128):
        l, h = (lo ^ (1 << b), hi) if b < 64 else (lo, hi ^ (1 << (b - 64)))
        t = orc.desensamblar(l, h)
        if t is None:
            bits[b] = "invalida"
            continue
        if t == base:
            continue
        p, n, o = partes(t)
        cambios = []
        if p != pb:
            cambios.append("predicado de guarda")
        if n != nb:
            cambios.append(f"nombre:{n}")
        for i in range(max(len(o), len(ob))):
            x = o[i] if i < len(o) else None
            y = ob[i] if i < len(ob) else None
            if x != y:
                cambios.append(f"op{i}:{tipo_operando(x or y)}:{y}->{x}")
        bits[b] = "; ".join(cambios) if cambios else "otro"
    return bits


def main():
    salida = sys.argv[1] if len(sys.argv) > 1 else "build/sm86_campos.json"
    elegidos = {int(x, 0) for x in sys.argv[2:]}
    cs = orc.codigos()
    if elegidos:
        cs = {k: v for k, v in cs.items() if k in elegidos}
    with cf.ThreadPoolExecutor(16) as ex:
        res = dict(zip(cs, ex.map(lambda k: mapa(k, cs[k][0], cs[k][1]), cs)))
    datos = {f"{op:#05x}": {"base": cs[op][1], "hi": f"{cs[op][0]:#018x}", "bits": res[op]} for op in sorted(cs)}
    json.dump(datos, open(salida, "w"), indent=1, ensure_ascii=False)
    print(f"{len(datos)} instrucciones; mapa en {salida}")


if __name__ == "__main__":
    main()
