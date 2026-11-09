"""Decodificador propio de SM86 (Ampere, la GPU de Switch 2), generado desde el mapa de campos (tools/sm86_campos.py).

    python3 tools/sm86.py generar [build/sm86_campos.json] [build/sm86_spec.json]   arma la especificación
    python3 tools/sm86.py validar [n] [build/sm86_spec.json]                          n instrucciones al azar vs nvdisasm
    python3 tools/sm86.py <lo hex> <hi hex>                                           decodifica una

La especificación dice, por código de operación (bits 0-11): los bits del predicado de guarda, los de cada operando
(número de registro o predicado, inmediato en bits crudos, banco y offset de constante, negativo, valor absoluto...) y
las tablas de los modificadores del nombre (.FTZ, .SAT, redondeo...), estas sacadas probando todas las combinaciones de
cada grupo de bits con el oráculo. decodificar() devuelve la instrucción como datos, no como texto.
"""
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ESPACIO = 0x7000   # predicado de guarda PT: así se armaron las instrucciones base


# ---------- leer el texto de nvdisasm

def partes(texto):
    pred = ""
    if texto.startswith("@"):
        pred, texto = texto.split(" ", 1)
    nombre, _, resto = texto.partition(" ")
    resto = re.sub(r"\]\s+\[", "][", resto)   # "c[0x0] [0x0]" (instrucciones de medio float) = "c[0x0][0x0]"
    ops = [o.strip() for o in re.split(r",(?![^\[]*\])", resto)] if resto.strip() else []
    return pred, nombre, ops


def num16(t):
    """Un inmediato de medio float (HADD2, HFMA2...) en sus 16 bits crudos, o None."""
    try:
        f = float(t.strip().replace("+INF", "inf").replace("INF", "inf"))
        return struct.unpack("<H", struct.pack("<e", f))[0]
    except (ValueError, OverflowError, struct.error):
        return None


def num(t):
    """Un número del texto de nvdisasm, en bits crudos de 32 o 64: hex tal cual, flotantes como float32."""
    t = t.strip()
    neg = t.startswith("-")
    s = t[1:] if neg else t
    if re.fullmatch(r"0x[0-9a-f]+", s):
        v = int(s, 16)
        return (-v) & 0xFFFFFFFFFFFFFFFF if neg else v
    if s in ("+INF", "INF"):
        f = float("inf")
    elif s in ("QNAN", "+QNAN"):
        return 0x7FC00000 | (0x80000000 if neg else 0)
    else:
        try:
            f = float(s)
        except ValueError:
            return None
    f = -f if neg else f
    try:
        return struct.unpack("<I", struct.pack("<f", f))[0]
    except OverflowError:   # inmediato de doble precisión (DADD, DMUL...): los 32 bits altos del double
        return struct.unpack("<Q", struct.pack("<d", f))[0] >> 32


def operando(o):
    """Un operando como dict: tipo, número/valor y adornos (neg, abs, not, sufijos)."""
    o = re.sub(r"\]\s+\[", "][", o.strip())   # "c[0x0] [0x0]" = "c[0x0][0x0]"
    r = {"neg": o.startswith("-"), "abs": "|" in o, "not": o.startswith("!") or o.startswith("~")}
    s = o.lstrip("-!~").replace("|", "")   # |R1|.H0_H0: las barras pueden quedar antes del sufijo
    s = re.sub(r"^(U?R(?:\d+|Z))\s+[-+]0x[0-9a-f]+$", r"\1", s)   # BRX R249 -0x20...: registro y desplazamiento
    m = re.fullmatch(r"(U?)R(\d+|Z)((?:\.\w+)*)", s)
    if m:
        z = 63 if m.group(1) else 255   # URZ es UR63; RZ es R255
        return {**r, "tipo": "ureg" if m.group(1) else "reg", "n": z if m.group(2) == "Z" else int(m.group(2)),
                "suf": m.group(3)}
    m = re.fullmatch(r"(U?)P(\d|T)", s)
    if m:
        return {**r, "tipo": "upred" if m.group(1) else "pred", "n": 7 if m.group(2) == "T" else int(m.group(2))}
    m = re.fullmatch(r"cx?\[(.+?)\]\[(.+?)\]((?:\.\w+)*)", s)
    if m:
        return {**r, "tipo": "const", "banco": m.group(1), "off": m.group(2), "suf": m.group(3)}
    if s.startswith("["):
        return {**r, "tipo": "mem", "txt": s}
    v = num(o)
    if v is not None:
        return {"tipo": "imm", "v": v, "neg": False, "abs": False, "not": False, "txt16": o}
    return {**r, "tipo": "otro", "txt": s}


# ---------- generar la especificación desde el mapa

def _bit_de(antes, despues):
    """Qué bit de un valor numérico cambió, o None."""
    if antes is None or despues is None:
        return None
    x = antes ^ despues
    return x.bit_length() - 1 if x and not x & (x - 1) else None


MEDIO = ("HADD2", "HMUL2", "HFMA2", "HSET2", "HSETP2", "HMNMX2")


def _campo_operando(i, oa, ob, nombre=""):
    """(nombre del campo, bit del valor) para el cambio de un operando de oa (base) a ob, o None."""
    a, b = operando(oa), operando(ob)
    if a["tipo"] != b["tipo"]:
        return (f"op{i}.forma", 0)
    for adorno in ("neg", "abs", "not"):
        if a[adorno] != b[adorno]:
            return (f"op{i}.{adorno}", 0)
    t = a["tipo"]
    if t in ("reg", "ureg", "pred", "upred"):
        if a.get("suf") != b.get("suf"):
            return (f"op{i}.suf", 0)
        k = _bit_de(a["n"], b["n"])
        return (f"op{i}.n", k) if k is not None else None
    if t == "imm":
        k = None if nombre.split(".")[0] in MEDIO else _bit_de(a["v"], b["v"])   # en H*, siempre fp16
        if k is not None:
            return (f"op{i}.v", k)
        k = _bit_de(num16(oa), num16(ob))   # medio float
        return (f"op{i}.h", k) if k is not None else None
    if t == "const":
        if a["suf"] != b["suf"]:
            return (f"op{i}.suf", 0)
        for parte in ("banco", "off"):
            if a[parte] != b[parte]:
                va, vb = num(a[parte]), num(b[parte])
                k = _bit_de(va, vb)
                return (f"op{i}.{parte}", k) if k is not None else (f"op{i}.{parte}_reg", 0)
    return (f"op{i}.otro", 0)


def generar(mapa_ruta, spec_ruta):
    import sm86_oraculo as orc
    import concurrent.futures as cf
    mapa = json.load(open(mapa_ruta))
    import progreso
    import threading
    hechos, lock = [0], threading.Lock()

    def uno(kv):
        r = (kv[0], _spec_de(kv[0], kv[1], orc))
        with lock:
            hechos[0] += 1
            if hechos[0] % 5 == 0 or hechos[0] == len(mapa):
                progreso.poner("sm86_generar", hechos[0], len(mapa), f"código {kv[0]} ({kv[1]['base'][:40]})",
                               "Generar especificación SM86", "Fase 4: shaders Ampere")
        return r
    with cf.ThreadPoolExecutor(16) as ex:
        spec = dict(ex.map(uno, mapa.items()))
    e = progreso.leer()
    e.pop("sm86_generar", None)
    progreso.escribir(e)
    json.dump(spec, open(spec_ruta, "w"), indent=1, ensure_ascii=False)
    n_raros = sum(len(s["raros"]) for s in spec.values())
    print(f"{len(spec)} códigos en {spec_ruta}; {n_raros} cambios sin campo")


def _spec_de(op_txt, e, orc):
    if True:
        op = int(op_txt, 16)
        hi = int(e["hi"], 16)
        _, nombre_base, ob = partes(e["base"])
        campos, nombre_bits, invalidos, raros = _campos_de(e["bits"], ob, nombre_base)
        nombre_bits = list(nombre_bits)
        if False:
          for b_txt, efecto in e["bits"].items():
            b = int(b_txt)
            if efecto == "invalida":
                invalidos.append(b)
                continue
            for c in efecto.split("; "):
                if c == "predicado de guarda":
                    continue   # bits 12-15 en todas: 12-14 índice, 15 negado
                if c.startswith("nombre:"):
                    nombre_bits.append(b)
                    continue
                m = re.match(r"op(\d+):\w+:(.*)->(.*)", c)
                if not m:
                    raros.append((b, c))
                    continue
                i = int(m.group(1))
                r = _campo_operando(i, m.group(2), m.group(3)) if i < len(ob) else (f"op{i}.extra", 0)
                if r is None:
                    raros.append((b, c))
                    continue
                campo, k = r
                campos.setdefault(campo, {})[str(k if k is not None else 0)] = b
        # Los bits que cambian el nombre, en grupos seguidos; cada grupo se prueba entero (todas sus combinaciones)
        grupos, actual = [], []
        for b in sorted(set(nombre_bits)):
            if actual and b != actual[-1] + 1:
                grupos.append(actual)
                actual = []
            actual.append(b)
        if actual:
            grupos.append(actual)
        tablas, variantes, cambian = [], [], []
        lo0 = op | ESPACIO
        for gi, g in enumerate(grupos):
            if len(g) > 8:   # no debería pasar; se parte
                g = g[:8]
            tabla = {}
            for combo in range(1 << len(g)):
                lo, h = lo0, hi
                for j, b in enumerate(g):
                    if combo >> j & 1:
                        if b < 64:
                            lo |= 1 << b
                        else:
                            h |= 1 << (b - 64)
                t = orc.desensamblar(lo, h)
                tabla[str(combo)] = partes(t)[1] if t else None
                # Un bit de modificador que cambia la cantidad de operandos (.X, .DC, .LC...): anotado para su mapa
                if t and len(partes(t)[2]) != len(ob) and combo and not combo & (combo - 1):
                    cambian.append(g[combo.bit_length() - 1])
            tablas.append({"bits": g, "nombres": tabla})
        # Un mapa de campos propio por cada uno de esos bits y por cada par de ellos (los modificadores de las
        # texturas se combinan): el decodificador usa el del conjunto más grande que esté encendido
        import itertools
        import sm86_campos
        conjuntos = [(b,) for b in cambian] + list(itertools.combinations(cambian, 2))
        vistos = set()
        for conj in conjuntos[:24]:
            lo, h = lo0, hi
            for b in conj:
                if b < 64:
                    lo |= 1 << b
                else:
                    h |= 1 << (b - 64)
            t = orc.desensamblar(lo, h)
            if not t or len(partes(t)[2]) == len(ob):
                continue
            firma = (len(partes(t)[2]), conj)
            if firma in vistos:
                continue
            vistos.add(firma)
            bits_v = sm86_campos.mapa(op, h, t, lo)
            cv, _, _, _ = _campos_de(bits_v, partes(t)[2], nombre_base)
            variantes.append({"encendidos": list(conj), "base": t, "campos": cv})
        return {"base": e["base"], "hi": e["hi"], "nombre": nombre_base, "n_ops": len(ob),
                "campos": campos, "modificadores": tablas, "variantes": variantes, "invalidos": sorted(invalidos),
                "raros": raros}


def _campos_de(bits, ob, nombre=""):
    """Del mapa de bits {bit: efecto} y los operandos base: (campos, bits del nombre, bits inválidos, sin campo)."""
    campos, nombre_bits, invalidos, raros = {}, [], [], []
    for b_txt, efecto in bits.items():
        b = int(b_txt)
        if efecto == "invalida":
            invalidos.append(b)
            continue
        for c in efecto.split("; "):
            if c == "predicado de guarda":
                continue
            if c.startswith("nombre:"):
                nombre_bits.append(b)
                continue
            m = re.match(r"op(\d+):\w+:(.*)->(.*)", c)
            if not m:
                raros.append((b, c))
                continue
            i = int(m.group(1))
            r = _campo_operando(i, m.group(2), m.group(3), nombre) if i < len(ob) else (f"op{i}.extra", 0)
            if r is None:
                raros.append((b, c))
                continue
            campo, k = r
            campos.setdefault(campo, {})[str(k if k is not None else 0)] = b
    return campos, nombre_bits, invalidos, raros


# ---------- decodificar

def _bit(lo, hi, b):
    return (lo >> b & 1) if b < 64 else (hi >> (b - 64) & 1)


def decodificar(lo, hi, spec):
    """La instrucción como datos: {'op', 'nombre', 'guarda', 'campos': {campo: valor}}, o None si el código no está."""
    op_txt = f"{lo & 0xFFF:#05x}"
    s = spec.get(op_txt)
    if not s:
        return None
    campos = {}
    for campo, bits in s["campos"].items():
        v = 0
        for k, b in bits.items():
            v |= _bit(lo, hi, b) << int(k)
        campos[campo] = v
    base = s["base"]
    # los modificadores encendidos que cambian los operandos: manda el mapa del conjunto más grande que esté encendido
    activas = [v for v in s.get("variantes", []) if all(_bit(lo, hi, b) for b in v["encendidos"])]
    if activas:
        v = max(activas, key=lambda v: len(v["encendidos"]))
        base = v["base"]
        campos = {}
        for campo, bits in v["campos"].items():
            x = 0
            for k, b in bits.items():
                x |= _bit(lo, hi, b) << int(k)
            campos[campo] = x
    nombre = s["nombre"]
    for t in s["modificadores"]:
        combo = sum(_bit(lo, hi, b) << j for j, b in enumerate(t["bits"]))
        n = t["nombres"].get(str(combo))
        if n is None:
            return {"op": op_txt, "invalida": True}
        if n != s["nombre"]:
            nombre = n if nombre == s["nombre"] else nombre   # el primer grupo que cambia el nombre (validar lo afina)
    guarda = lo >> 12 & 0xF
    return {"op": op_txt, "nombre": nombre, "guarda": ("!" if guarda & 8 else "") + ("PT" if guarda & 7 == 7 else f"P{guarda & 7}"),
            "campos": campos, "base": base, "lo": lo, "hi": hi}


def operandos(mio, spec_op):
    """Los operandos decodificados en orden: [(índice, tipo, valor o None, neg, abs)] con el tipo de la forma base."""
    _, _, base = partes(mio.get("base") or spec_op["base"])
    c = mio["campos"]
    res = []
    for i, o in enumerate(base):
        t = operando(o)["tipo"]
        v = c.get(f"op{i}.n", c.get(f"op{i}.v"))
        if v is None and f"op{i}.h" in c:
            v = ("h", c[f"op{i}.h"])
        res.append((i, t, v, bool(c.get(f"op{i}.neg", 0) or c.get(f"op{i}.not", 0)), bool(c.get(f"op{i}.abs", 0))))
    return res


SALTOS = ("BRA", "BSSY", "CALL", "BRX", "SSY", "PBK")   # inmediato relativo: nvdisasm muestra el destino (+0x10)


def _coincide(m, x, nombre):
    """Si el operando decodificado m = (i, tipo, valor, neg, abs) es el del texto x (dict de operando())."""
    _, t, v, neg, ab = m
    tx = x["tipo"]
    if t == "otro":   # en la forma base era un campo indefinido (???N): cualquier cosa
        return True
    if {t, tx} <= {"reg", "ureg"} or {t, tx} <= {"pred", "upred"}:
        if v is not None and v != x["n"]:
            return False
    elif t == "imm" and tx == "imm" and isinstance(v, tuple):   # medio float: se compara en fp16
        if v[1] != num16(x["txt16"]):
            return False
    elif t == "imm" and tx == "imm" and ".F16" in nombre and v is not None and not isinstance(v, tuple):
        if v & 0xFFFF != num16(x["txt16"]):   # MUFU.*.F16: el inmediato son los 16 bits bajos, en fp16
            return False
    elif t == "imm" and tx == "imm":
        if v is not None:
            w = x["v"]
            if nombre.split(".")[0] in SALTOS and ".ABS" not in nombre and not nombre.startswith(("JMP", "JMX")):
                w = (w - 0x10) & 0xFFFFFFFFFFFFFFFF
            if v not in (w, w & 0xFFFFFFFF, w >> 32):
                return False
    elif t != tx:
        return False
    if neg != bool(x.get("neg") or x.get("not")):
        return False
    if ab != bool(x.get("abs")):
        return False
    return True


def _emparejar(mios, textos, nombre):
    """Empareja en orden los operandos decodificados con los del texto: nvdisasm omite los predicados que valen PT (y
    a veces agrega operandos según los modificadores). True si todos los del texto tienen pareja coherente."""
    from functools import lru_cache

    @lru_cache(None)
    def ir(i, j):
        if j == len(textos):
            return all(m[1] in ("pred", "upred") and m[2] in (7, None) for m in mios[i:]) or i == len(mios)
        if i == len(mios):
            return False
        m = mios[i]
        if _coincide(m, textos[j], nombre) and ir(i + 1, j + 1):
            return True
        if m[1] in ("pred", "upred", "otro", "mem", "const") and ir(i + 1, j):   # omitido en el texto
            return True
        return ir(i, j + 1) if textos[j]["tipo"] in ("otro",) else False
    return ir(0, 0)


# ---------- validar contra nvdisasm

def validar(n, spec_ruta, semilla=1):
    import random
    import sm86_oraculo as orc
    spec = json.load(open(spec_ruta))
    import concurrent.futures as cf
    rnd = random.Random(semilla)
    ops = sorted(spec)
    probadas = bien = definidas = bien_def = 0
    fallas = {}
    candidatas = []
    for _ in range(n * 3):
        op_txt = rnd.choice(ops)
        s = spec[op_txt]
        lo, hi = int(op_txt, 16) | (rnd.randrange(16) << 12), int(s["hi"], 16)
        usados = {b for bits in s["campos"].values() for b in bits.values()}
        usados |= {b for t in s["modificadores"] for b in t["bits"]}
        for b in usados:
            if rnd.random() < 0.5:
                if b < 64:
                    lo ^= 1 << b
                else:
                    hi ^= 1 << (b - 64)
        candidatas.append((op_txt, lo, hi))
    import progreso
    import threading
    hechos, lock = [0], threading.Lock()

    def consulta(c):
        t = orc.desensamblar(c[1], c[2])
        with lock:
            hechos[0] += 1
            if hechos[0] % 50 == 0 or hechos[0] == len(candidatas):
                progreso.poner("sm86_validar", hechos[0], len(candidatas), "consultando a nvdisasm",
                               "Validar contra nvdisasm", "Fase 4: shaders Ampere")
        return t
    with cf.ThreadPoolExecutor(16) as ex:
        textos = list(ex.map(consulta, candidatas))
    for (op_txt, lo, hi), texto in zip(candidatas, textos):
        s = spec[op_txt]
        if texto is None or probadas >= n:
            continue
        probadas += 1
        mio = decodificar(lo, hi, spec)
        pred, nombre, ops_txt = partes(texto)
        errores = []
        if not mio or mio.get("invalida"):
            errores.append("no decodifica")
        else:
            g = pred[1:].replace("UP", "P") if pred else "PT"
            if g != mio["guarda"]:
                errores.append(f"guarda {pred} vs {mio['guarda']}")
            if not _emparejar(tuple(operandos(mio, s)), tuple(operando(o) for o in ops_txt), nombre):
                errores.append("operandos: " + str([m[1:] for m in operandos(mio, s)])[:160])
        # "???", "INVALID": combinación de bits sin definir; ningún compilador la genera
        definida = "???" not in texto and "INVALID" not in texto
        definidas += definida
        if errores:
            if definida:
                fallas.setdefault(s["nombre"], []).append((texto, errores[:3]))
        else:
            bien += 1
            bien_def += definida
    e = progreso.leer()
    e.pop("sm86_validar", None)
    progreso.escribir(e)
    progreso.poner("fase4_dec", bien_def, definidas,
                   f"{bien_def}/{definidas} definidas iguales a nvdisasm ({bien}/{probadas} contando las indefinidas)")
    print(f"{bien}/{probadas} instrucciones al azar decodificadas igual que nvdisasm "
          f"(guarda, registros, predicados, inmediatos, negativo y absoluto)")
    print(f"{bien_def}/{definidas} sin partes indefinidas (\"???\", \"INVALID\"), las que puede traer un shader real; "
          "fallas de esas:")
    for nombre, l in sorted(fallas.items(), key=lambda x: -len(x[1]))[:12]:
        print(f"  {nombre}: {len(l)} fallas; ej. {l[0][0]}  -> {l[0][1]}")
    return bien_def == definidas


def main():
    a = sys.argv[1:]
    if a[:1] == ["generar"]:
        generar(a[1] if len(a) > 1 else "build/sm86_campos.json", a[2] if len(a) > 2 else "build/sm86_spec.json")
    elif a[:1] == ["validar"]:
        sys.exit(0 if validar(int(a[1]) if len(a) > 1 else 500, a[2] if len(a) > 2 else "build/sm86_spec.json") else 1)
    elif len(a) == 2:
        spec = json.load(open("build/sm86_spec.json"))
        print(json.dumps(decodificar(int(a[0], 16), int(a[1], 16), spec), ensure_ascii=False))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
