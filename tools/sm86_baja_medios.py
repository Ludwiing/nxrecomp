"""Bajada SM86 -> Maxwell de las instrucciones de medio float: HADD2, HMUL2, HFMA2 (no .MMA), HSETP2 y HSET2.
Código propio (MIT). Lo carga tools/sm86_baja.py (_cargar_modulos); se prueba con
`python3 tools/sm86_baja_medios.py probar 3000` (cambia también los bits de modificadores).

Encodings, comprobados con nvdisasm (sm_86) cambiando un bit a la vez (la especificación generada solo tiene un bit
de sufijo por operando, así que este módulo lee los bits crudos):
- A: registro en 24-31; -A bit 72, |A| bit 73, sufijo en 74-75.
- Lugar 1 (bits 32-63): registro en 32-39, constante c[banco 54-58][palabra en 40-53] (bit 91: banco por UR, sin
  regla) o inmediato de dos mitades = los 32 bits de 32-63 tal cual (H0 en 32-47, H1 en 48-63: ptxas pone
  0x40003c00 de `mov.b32 r, 0x40003C00` ahí; nvdisasm imprime H1 primero). Registro y constante: sufijo en 60-61,
  || en 62, - en 63.
- Lugar 2 (HFMA2): registro en 64-71; sufijo en 81-82, || en 83, - en 84; el bit 86 da `.H0_NH1` (sin regla).
- Sufijo de 2 bits = HalfSwizzle de Maxwell: 0 H1_H0 (las dos mitades), 1 F32, 2 H0_H0, 3 H1_H1.
- HFMA2: B = lugar 1 y C = lugar 2 en 0x231, 0x831 (B inmediato), 0xa31 (B constante); B = lugar 2 y C = lugar 1 en
  0x431 (C inmediato) y 0x631 (C constante).
- Modificadores: FMZ 76, SAT 77, salida .F32 78, RELU 79 (HFMA2; sin regla), FTZ 80, BF16_V2 85 (sin regla).
- HSETP2 / HSET2: operación lógica 69-70 (AND, OR, XOR), bit 71 = H_AND (HSETP2) o .BF (HSET2), comparación 76-79 (el
  orden de FComp), FTZ 80; HSETP2: Pd 81-83, Pq 84-86; predicado de entrada 87-89 y su ! en 90.

Maxwell según Ryujinx (InstEmitFloatArithmetic / InstEmitFloatComparison / InstEmitHelper.GetHalfSrc):
- Las formas I de Maxwell guardan 10 bits por mitad (imm << 6) y las C leen la constante como F32 para las dos
  mitades: por eso todo operando que no es registro va primero a un temporal (MOV32I con los 32 bits del inmediato,
  MOV desde c[]) y se usa la forma R con el sufijo de SM86.
- HFMA2 de Maxwell no tiene || en ningún operando ni - en A/B por separado: NegA niega B (el producto). HMUL2: NegA
  niega B, AbsA y AbsB sí. HADD2, HSET2 y HSETP2 tienen - y || en A y B. Un || que la forma Maxwell no tiene se hace
  antes en un temporal (LOP32I AND con 0x7fff7fff, o 0x7fffffff con el sufijo F32); un - de A o B en HMUL2/HFMA2 va
  al signo del producto (-a*b = a*-b).
- FTZ y FMZ: los campos se ponen, pero Ryujinx no los aplica (como en FSET/FSETP).
- .F32 de salida = OFmt F32 (el resultado de H0 como float de 32 bits). Ryujinx calcula cada mitad en float de 32
  bits y empaqueta a medio float al final: una suma o un FMA puede redondear dos veces donde el hardware redondea una.
  Es lo mismo que pasa con los shaders de Switch 1 y no depende de esta bajada.
- HSETP2 con H_AND: Pd = (r0 y r1) op P, Pq = !(r0 y r1) op P, el modelo de Ryujinx (EmitHsetp2); sin hardware no
  se puede confirmar que SM86 haga lo mismo con Pq.

Sin regla: HFMA2.MMA (0x235...), RELU, BF16_V2, el sufijo .H0_NH1, cx[UR][], HADD2 inmediato con .F32 (nvdisasm muestra
un solo inmediato: no se sabe cómo lo lee), HADD2.F32 con el bit 73 (en las demás formas es |A|; en esta nvdisasm no lo
muestra), HMUL2/HFMA2 con FMZ y FTZ a la vez (nvdisasm: INVALID3; el oráculo lo encontró: la primera versión la
aceptaba) y HSET2/HSETP2 con la operación lógica 3 (inválida).

La comprobación (COMPROBAR) ejecuta la SM86 y lo bajado sobre los mismos estados en un warp de 32 hilos, con medios
floats especiales (±0, ±inf, NaN, denormales, 1, -2, 65504). Las dos ejecuciones usan la misma aritmética (la de
Ryujinx, arriba): lo que se comprueba es la bajada (operandos, sufijos, signos, inmediatos, constantes, SAT, salida,
comparación, predicados), no la exactitud del redondeo. Un NaN compara igual a cualquier NaN.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, codificar, leer, _guarda  # noqa: E402
import sm86_baja_uniformes as U  # noqa: E402
import sm86_baja_predicados as PR  # noqa: E402

M32 = 0xFFFFFFFF
FMA = {0x231: (1, 2), 0x431: (2, 1), 0x631: (2, 1), 0x831: (1, 2), 0xA31: (1, 2)}   # (lugar de B, lugar de C)
MMA = {0x235, 0x435, 0x635, 0x835, 0xA35}


def _bits(d, a, n=1):
    return (d["lo"] | d["hi"] << 64) >> a & ((1 << n) - 1)


def _forma(d):
    """'R', 'I' o 'C' del lugar 1 (bits 8-11 del código: 0x2 R, 0x4/0x8 I, 0x6/0xa C)."""
    return {0x2: "R", 0x4: "I", 0x8: "I", 0x6: "C", 0xA: "C"}[int(d["op"], 16) >> 8]


def _operandos(d):
    """{'A', 'B'[, 'C']}: cada uno {'t': 'reg'|'imm'|'const', 'n' | 'v' | ('banco', 'pal'), 'swz', 'neg', 'abs'}."""
    a = {"t": "reg", "n": _bits(d, 24, 8), "neg": _bits(d, 72), "abs": _bits(d, 73), "swz": _bits(d, 74, 2)}
    f = _forma(d)
    if f == "R":
        l1 = {"t": "reg", "n": _bits(d, 32, 8)}
    elif f == "I":
        l1 = {"t": "imm", "v": _bits(d, 32, 32), "swz": 0, "neg": 0, "abs": 0}
    else:
        if _bits(d, 91):
            raise SinRegla(U.SIN_CX)
        l1 = {"t": "const", "banco": _bits(d, 54, 5), "pal": _bits(d, 40, 14)}
    if l1["t"] != "imm":
        l1.update(swz=_bits(d, 60, 2), abs=_bits(d, 62), neg=_bits(d, 63))
    ops = {"A": a}
    codigo = int(d["op"], 16)
    if codigo in FMA:
        if _bits(d, 86):
            raise SinRegla("HFMA2 con el sufijo .H0_NH1")
        l2 = {"t": "reg", "n": _bits(d, 64, 8), "swz": _bits(d, 81, 2), "abs": _bits(d, 83), "neg": _bits(d, 84)}
        lugar_b, lugar_c = FMA[codigo]
        ops["B"], ops["C"] = (l1, l2) if lugar_b == 1 else (l2, l1)
    else:
        ops["B"] = l1
    return ops


def _a_registro(op, temp, g, previas, quitar_abs):
    """El operando en un registro: inmediato y constante a temp; si quitar_abs, el || se aplica en temp."""
    if op["t"] == "imm":
        previas.append(codificar("Mov32i", Dest=temp, Imm32=op["v"], QuadMask=0xF, **g))
        return temp
    if op["t"] == "const":
        previas.append(codificar("MovC", Dest=temp, CbufSlot=op["banco"], CbufOffset=op["pal"], QuadMask=0xF, **g))
        r = temp
    else:
        r = op["n"]
    if quitar_abs and op["abs"]:
        mask = 0x7FFFFFFF if op["swz"] == 1 else 0x7FFF7FFF
        previas.append(codificar("Lop32i", Dest=temp, SrcA=r, Imm32=mask, LogicOp=0, **g))
        r = temp
    return r


def baja_medio(d):
    nombre = d["nombre"].split(".")[0]
    codigo = int(d["op"], 16)
    if codigo in MMA:
        raise SinRegla("HFMA2.MMA")
    g = _guarda(d)
    ops = _operandos(d)
    A, Bo = ops["A"], ops["B"]
    previas = []
    if nombre in ("HADD2", "HMUL2", "HFMA2"):
        if nombre == "HFMA2" and (_bits(d, 79) or _bits(d, 85)):
            raise SinRegla("HFMA2.RELU" if _bits(d, 79) else "HFMA2.BF16_V2")
        if nombre == "HADD2" and Bo["t"] == "imm" and _bits(d, 78):
            raise SinRegla("HADD2 inmediato con .F32 (nvdisasm muestra un solo inmediato)")
        if nombre == "HADD2" and _bits(d, 78) and _bits(d, 73):
            raise SinRegla("HADD2.F32 con el bit 73 (|A| en las demás formas; nvdisasm no lo muestra aquí)")
        if nombre in ("HMUL2", "HFMA2") and _bits(d, 76) and _bits(d, 80):
            raise SinRegla(f"{nombre} con FMZ y FTZ a la vez (nvdisasm: INVALID3)")
        comun = dict(Dest=_bits(d, 16, 8), OFmt=_bits(d, 78), Sat=_bits(d, 77), **g)
        if nombre == "HADD2":
            ra = _a_registro(A, TEMPS[0], g, previas, False)
            rb = _a_registro(Bo, TEMPS[1], g, previas, False)
            return previas + [codificar("Hadd2R", SrcA=ra, SrcB=rb, ASwizzle=A["swz"], BSwizzle=Bo["swz"],
                                        NegA=A["neg"], AbsA=A["abs"], NegB=Bo["neg"], AbsB=Bo["abs"],
                                        Ftz=_bits(d, 80), **comun)]
        neg_prod = A["neg"] ^ Bo["neg"]
        if nombre == "HMUL2":
            ra = _a_registro(A, TEMPS[0], g, previas, False)
            rb = _a_registro(Bo, TEMPS[1], g, previas, False)
            return previas + [codificar("Hmul2R", SrcA=ra, SrcB=rb, ASwizzle=A["swz"], BSwizzle=Bo["swz"],
                                        NegA=neg_prod, AbsA=A["abs"], AbsB=Bo["abs"], Fmz=_bits(d, 76), **comun)]
        C = ops["C"]
        ra = _a_registro(A, TEMPS[0], g, previas, True)
        rb = _a_registro(Bo, TEMPS[1], g, previas, True)
        rc = _a_registro(C, TEMPS[2], g, previas, True)
        return previas + [codificar("Hfma2R", SrcA=ra, SrcB=rb, SrcC=rc, ASwizzle=A["swz"], BSwizzle=Bo["swz"],
                                    CSwizzle=C["swz"], NegA=neg_prod, NegC=C["neg"], Fmz=_bits(d, 76), **comun)]
    bop = _bits(d, 69, 2)
    if bop == 3:
        raise SinRegla(f"{nombre} con operación lógica inválida")
    ra = _a_registro(A, TEMPS[0], g, previas, False)
    rb = _a_registro(Bo, TEMPS[1], g, previas, False)
    comun = dict(SrcA=ra, SrcB=rb, ASwizzle=A["swz"], BSwizzle=Bo["swz"], NegA=A["neg"], AbsA=A["abs"],
                 NegB=Bo["neg"], AbsB=Bo["abs"], Bop=bop, SrcPred=_bits(d, 87, 3), SrcPredInv=_bits(d, 90),
                 Ftz=_bits(d, 80), **g)
    if nombre == "HSETP2":
        return previas + [codificar("Hsetp2R", DestPred=_bits(d, 81, 3), DestPredInv=_bits(d, 84, 3),
                                    FComp2=_bits(d, 76, 4), HAnd=_bits(d, 71), **comun)]
    return previas + [codificar("Hset2R", Dest=_bits(d, 16, 8), Cmp=_bits(d, 76, 4), Bval=_bits(d, 71), **comun)]


REGLAS = {k: baja_medio for k in ("HADD2", "HMUL2", "HFMA2", "HSETP2", "HSET2")}
ROLES = {}
FAMILIA = {"HADD2": "Hadd2", "HMUL2": "Hmul2", "HFMA2": "Hfma2", "HSETP2": "Hsetp2", "HSET2": "Hset2"}


# ---------- comprobar ejecutando (la misma aritmética en los dos lados: ver la cabecera)

def _h(x):
    return np.float32(np.uint16(x & 0xFFFF).view(np.float16))


def _f(x):
    return np.uint32(x & M32).view(np.float32)


def _mitades(v, swz):
    return {0: (_h(v), _h(v >> 16)), 1: (_f(v), _f(v)), 2: (_h(v), _h(v)), 3: (_h(v >> 16), _h(v >> 16))}[swz]


def _absneg(xs, ab, ng):
    return tuple((-abs(x) if ng else abs(x)) if ab else (-x if ng else x) for x in xs)


def _sat(x, s):
    if not s:
        return x
    return np.float32(0) if np.isnan(x) else np.float32(min(max(float(x), 0.0), 1.0))


def _medio_bits(x):
    v = int(np.float16(x).view(np.uint16))
    return 0x7E00 if (v & 0x7C00) == 0x7C00 and v & 0x3FF else v   # NaN canónico


def _empaca(res, ofmt):
    if ofmt == 1:
        v = int(np.float32(res[0]).view(np.uint32))
        return 0x7FC00000 if np.isnan(res[0]) else v
    return _medio_bits(res[0]) | _medio_bits(res[1]) << 16


def _fma(a, b, c):
    return np.float32(np.float64(a) * np.float64(b) + np.float64(c))


def _canon(v):
    """Un resultado guardado con NaN canónico en cada mitad (para comparar sin el signo ni la carga de un NaN)."""
    lo, hi = v & 0xFFFF, v >> 16
    if (lo & 0x7C00) == 0x7C00 and lo & 0x3FF:
        lo = 0x7E00
    if (hi & 0x7C00) == 0x7C00 and hi & 0x3FF:
        hi = 0x7E00
    return lo | hi << 16


def _valor(h, op):
    if op["t"] == "reg":
        return h["R"][op["n"]]
    if op["t"] == "imm":
        return op["v"]
    return U._cb(op["banco"], op["pal"])


def _ejecuta_sm86(d, hilos):
    nombre = d["nombre"].split(".")[0]
    ops = _operandos(d)
    g = d["guarda"]
    gn, ginv = (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")
    for h in hilos:
        if h["P"][gn] == ginv:
            continue
        x = {k: _absneg(_mitades(_valor(h, o), o["swz"]), o["abs"], o["neg"]) for k, o in ops.items()}
        if nombre in ("HADD2", "HMUL2", "HFMA2"):
            res = []
            for i in (0, 1):
                a, b = x["A"][i], x["B"][i]
                r = a + b if nombre == "HADD2" else a * b if nombre == "HMUL2" else _fma(a, b, x["C"][i])
                res.append(_sat(np.float32(r), _bits(d, 77)))
            U._w(h, _bits(d, 16, 8), _empaca(res, _bits(d, 78)))
            continue
        cmp_, bop = _bits(d, 76, 4), _bits(d, 69, 2)
        r = [PR._fcomp(cmp_, float(x["A"][i]), float(x["B"][i])) for i in (0, 1)]
        p = h["P"][_bits(d, 87, 3)] != bool(_bits(d, 90))
        if nombre == "HSETP2":
            if _bits(d, 71):
                r = [r[0] and r[1], not (r[0] and r[1])]
            U._wp(h, _bits(d, 81, 3), PR._bop(bop, r[0], p))
            U._wp(h, _bits(d, 84, 3), PR._bop(bop, r[1], p))
        else:
            r = [PR._bop(bop, ri, p) for ri in r]
            uno = 0x3C00 if _bits(d, 71) else 0xFFFF
            U._w(h, _bits(d, 16, 8), (uno if r[0] else 0) | (uno if r[1] else 0) << 16)


def _ejecuta_maxwell(palabras, hilos):
    otros = []
    for w in palabras:
        op, c = leer(w)
        if op not in ("Lop32i", "Hadd2R", "Hmul2R", "Hfma2R", "Hsetp2R", "Hset2R"):
            otros.append(w)
            continue
        if otros:
            U._ejecuta_maxwell(otros, hilos)
            otros = []
        for h in hilos:
            R, P = h["R"], h["P"]
            if P[c.get("Pred", 7)] == bool(c.get("PredInv")):
                continue
            if op == "Lop32i":
                if c["LogicOp"] != 0 or c["NegA"] or c["NegB"]:
                    raise ValueError("Lop32i: el simulador solo conoce AND sin negar")
                U._w(h, c["Dest"], R[c["SrcA"]] & c["Imm32"])
                continue
            fa = _mitades(R[c["SrcA"]], c["ASwizzle"])
            fb = _mitades(R[c["SrcB"]], c["BSwizzle"])
            if op == "Hadd2R":
                a, b = _absneg(fa, c["AbsA"], c["NegA"]), _absneg(fb, c["AbsB"], c["NegB"])
                res = [_sat(np.float32(a[i] + b[i]), c["Sat"]) for i in (0, 1)]
                U._w(h, c["Dest"], _empaca(res, c["OFmt"]))
            elif op == "Hmul2R":   # GetHalfSrc(A, false, AbsA), GetHalfSrc(B, NegA, AbsB)
                a, b = _absneg(fa, c["AbsA"], 0), _absneg(fb, c["AbsB"], c["NegA"])
                res = [_sat(np.float32(a[i] * b[i]), c["Sat"]) for i in (0, 1)]
                U._w(h, c["Dest"], _empaca(res, c["OFmt"]))
            elif op == "Hfma2R":   # A sin nada, B con NegA, C con NegC
                fc = _mitades(R[c["SrcC"]], c["CSwizzle"])
                b, cc = _absneg(fb, 0, c["NegA"]), _absneg(fc, 0, c["NegC"])
                res = [_sat(_fma(fa[i], b[i], cc[i]), c["Sat"]) for i in (0, 1)]
                U._w(h, c["Dest"], _empaca(res, c["OFmt"]))
            else:
                a, b = _absneg(fa, c["AbsA"], c["NegA"]), _absneg(fb, c["AbsB"], c["NegB"])
                cmp_ = c["FComp2"] if op == "Hsetp2R" else c["Cmp"]
                r = [PR._fcomp(cmp_, float(a[i]), float(b[i])) for i in (0, 1)]
                p = P[c["SrcPred"]] != bool(c["SrcPredInv"])
                if op == "Hsetp2R":
                    if c["HAnd"]:
                        r = [r[0] and r[1], not (r[0] and r[1])]
                    U._wp(h, c["DestPred"], PR._bop(c["Bop"], r[0], p))
                    U._wp(h, c["DestPredInv"], PR._bop(c["Bop"], r[1], p))
                else:
                    r = [PR._bop(c["Bop"], ri, p) for ri in r]
                    uno = 0x3C00 if c["Bval"] else 0xFFFF
                    U._w(h, c["Dest"], (uno if r[0] else 0) | (uno if r[1] else 0) << 16)
    if otros:
        U._ejecuta_maxwell(otros, hilos)


_ESPECIALES = [0x0000, 0x8000, 0x3C00, 0xC000, 0x7C00, 0xFC00, 0x7E00, 0xFE01, 0x0001, 0x83FF, 0x7BFF, 0x3555,
               0x3800, 0xB800]


def comprobar_ejecutando(d, palabras):
    errores = []
    with np.errstate(all="ignore"):
        for semilla in range(8):
            a = U._estado(U._h(d["lo"], d["hi"], semilla))
            for k, h in enumerate(a):   # medios floats especiales en las mitades de muchos registros
                for r in range(255):
                    s = (k * 31 + r * 17 + semilla) % 5
                    if s < 2:
                        lo = _ESPECIALES[(k + r) % len(_ESPECIALES)]
                        hi = _ESPECIALES[(k * 3 + r + semilla) % len(_ESPECIALES)]
                        h["R"][r] = lo | hi << 16 if s == 0 else (h["R"][r] & 0xFFFF0000) | lo
            b = [{"R": h["R"][:], "P": h["P"][:]} for h in a]
            _ejecuta_sm86(d, a)
            try:
                _ejecuta_maxwell(palabras, b)
            except (ValueError, KeyError) as e:
                return [f"Maxwell: {e}"]
            for i, (x, y) in enumerate(zip(a, b)):
                for t in TEMPS:
                    y["R"][t] = x["R"][t]
                for r in range(256):
                    if x["R"][r] != y["R"][r] and _canon(x["R"][r]) != _canon(y["R"][r]):
                        errores.append(f"hilo {i} R{r}: {x['R'][r]:#x} vs {y['R'][r]:#x}")
                        break
                if x["P"] != y["P"]:
                    errores.append(f"hilo {i} P: {x['P']} vs {y['P']}")
                if errores:
                    return errores[:4]
    return errores


COMPROBAR = {k: comprobar_ejecutando for k in REGLAS}

MODIFICADORES = {"HADD2": (60, 61, 62, 63, 72, 73, 74, 75, 77, 78, 80),
                 "HMUL2": (60, 61, 62, 63, 72, 73, 74, 75, 76, 77, 78, 80),
                 "HFMA2": (60, 61, 62, 63, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86),
                 "HSETP2": (60, 61, 62, 63, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80),
                 "HSET2": (60, 61, 62, 63, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80)}


def probar(n, semilla=86):
    """Como sm86_baja_predicados.probar: operandos, guarda y todos los bits de modificadores al azar."""
    import json
    import random
    import sm86
    spec = json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build",
                                       "sm86_spec.json")))
    claves = [k for k, s in spec.items() if s["nombre"].split(".")[0] in REGLAS]
    rnd = random.Random(semilla)
    bien, sin, malas = 0, {}, {}
    for _ in range(n):
        k = rnd.choice(claves)
        s = spec[k]
        lo, hi = int(k, 16) | rnd.randrange(16) << 12, int(s["hi"], 16)
        bits = [b for c in s["campos"].values() for b in c.values()] + list(range(32, 64))
        bits += list(MODIFICADORES.get(s["nombre"].split(".")[0], ()))
        for b in set(bits):
            if b == 91:
                continue
            if rnd.random() < 0.5:
                lo, hi = (lo ^ 1 << b, hi) if b < 64 else (lo, hi ^ 1 << (b - 64))
        d = sm86.decodificar(lo, hi, spec)
        if not d or d.get("invalida"):
            continue
        try:
            palabras = B.bajar(d)
        except SinRegla as e:
            sin[str(e)] = sin.get(str(e), 0) + 1
            continue
        errores = comprobar_ejecutando(d, palabras)
        if errores:
            clave = f"{d['nombre']} {d['op']}: {errores[0]}"
            malas[clave] = malas.get(clave, 0) + 1
        else:
            bien += 1
    return bien, sin, malas


# ---------- oráculo: la lectura de operandos y modificadores contra el texto de nvdisasm
# La comprobación por ejecución usa _operandos en los dos lados, así que un error al leer los bits (B y C cambiados, un
# sufijo en otro bit) no se vería. Esto lo compara con nvdisasm, que es independiente.

import re  # noqa: E402

_OP = re.compile(r"^(-)?(\|)?(R\d+|RZ|c\[0x([0-9a-f]+)\] \[(-?)0x([0-9a-f]+)\])(\|)?(?:\.(F32|H0_H0|H1_H1))?(\|)?$")
_SWZ = {None: 0, "F32": 1, "H0_H0": 2, "H1_H1": 3}
CMP = ["F", "LT", "EQ", "LE", "GT", "NE", "GE", "NUM", "NAN", "LTU", "EQU", "LEU", "GTU", "NEU", "GEU", "T"]


def _de_texto(t):
    """'-|R3|.H0_H0' -> dict como los de _operandos (sin 'v'); None si no es registro ni constante."""
    m = _OP.match(t.strip())
    if not m:
        return None
    o = {"neg": int(bool(m.group(1))), "abs": int(bool(m.group(2))), "swz": _SWZ[m.group(8)]}
    if m.group(4) is not None:   # nvdisasm muestra el offset de 16 bits con signo
        off = (-int(m.group(6), 16) if m.group(5) else int(m.group(6), 16)) & 0xFFFF
        o.update(t="const", banco=int(m.group(4), 16), pal=off >> 2)
    else:
        o.update(t="reg", n=255 if m.group(3) == "RZ" else int(m.group(3)[1:]))
    return o


def _medio_texto(t):
    """Valor de un inmediato impreso por nvdisasm (o None si no se sabe leer)."""
    t = t.strip()
    especiales = {"+INF": np.inf, "-INF": -np.inf, "INF": np.inf}
    try:
        return especiales[t] if t in especiales else float(t)
    except ValueError:
        return None


def oraculo(n, semilla=7):
    """n instrucciones al azar: operandos (lugar, registro, constante, sufijo, -, ||), inmediatos y modificadores de la
    lectura de este módulo contra nvdisasm. Devuelve (comparadas, [diferencias])."""
    import json
    import random
    import sm86
    import sm86_oraculo
    spec = json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build",
                                       "sm86_spec.json")))
    claves = [k for k, s in spec.items() if s["nombre"].split(".")[0] in REGLAS and int(k, 16) not in MMA]
    rnd = random.Random(semilla)
    hechas, difs = 0, []
    for _ in range(n):
        k = rnd.choice(claves)
        s = spec[k]
        lo, hi = int(k, 16) | 7 << 12, int(s["hi"], 16) | 0x000fe20000000000
        for b in set([b for c in s["campos"].values() for b in c.values()] + list(range(32, 64)) +
                     list(MODIFICADORES.get(s["nombre"].split(".")[0], ()))):
            if b not in (91, 79, 85, 86) and rnd.random() < 0.5:
                lo, hi = (lo ^ 1 << b, hi) if b < 64 else (lo, hi ^ 1 << (b - 64))
        texto = sm86_oraculo.desensamblar(lo, hi)
        d = sm86.decodificar(lo, hi, spec)
        if not texto or not d or d.get("invalida"):
            continue
        nombre = d["nombre"].split(".")[0]
        try:
            ops = _operandos(d)
        except SinRegla:
            continue
        if nombre == "HADD2" and ops["B"]["t"] == "imm" and _bits(d, 78):
            continue   # sin regla: nvdisasm muestra un solo inmediato
        if nombre in ("HSETP2", "HSET2") and _bits(d, 69, 2) == 3:
            continue   # operación lógica inválida: sin regla
        hechas += 1
        mnem, _, resto = texto.partition(" ")
        partes = [x.strip() for x in resto.split(",")]
        mods = set(mnem.split(".")[1:])
        if nombre in ("HMUL2", "HFMA2") and _bits(d, 76) and _bits(d, 80):
            continue   # inválida: sin regla
        if nombre == "HADD2" and _bits(d, 78) and _bits(d, 73):
            continue   # sin regla: el bit 73 no se ve en HADD2.F32
        quiero = {"SAT": _bits(d, 77) and nombre in ("HADD2", "HMUL2", "HFMA2"),
                  "F32": _bits(d, 78) and nombre in ("HADD2", "HMUL2", "HFMA2"),
                  "FTZ": _bits(d, 80), "FMZ": _bits(d, 76) and nombre in ("HMUL2", "HFMA2"),
                  "BF": _bits(d, 71) and nombre == "HSET2", "H_AND": _bits(d, 71) and nombre == "HSETP2"}
        for m, si in quiero.items():
            if bool(si) != (m in mods):
                difs.append(f"{texto}: modificador {m}")
        if nombre in ("HSETP2", "HSET2"):
            if CMP[_bits(d, 76, 4)] not in mods or ["AND", "OR", "XOR"][_bits(d, 69, 2)] not in mods:
                difs.append(f"{texto}: comparación u operación")
        orden = (["A", "B"] if nombre != "HFMA2" else ["A", "B", "C"])
        i = 2 if nombre == "HSETP2" else 1   # después de Rd (o Pd, Pq)
        for nom in orden:
            o = ops[nom]
            if i + (1 if o["t"] != "imm" else 2) > len(partes):
                difs.append(f"{texto}: faltan operandos para {nom}")
                break
            if o["t"] == "imm":
                h1, h0 = _medio_texto(partes[i]), _medio_texto(partes[i + 1])
                i += 2
                mio = (float(_h(o["v"] >> 16)), float(_h(o["v"])))
                for x, y in zip((h1, h0), mio):
                    if x is not None and not (np.isnan(y) or abs(x - y) <= abs(y) * 1e-3 + 1e-12):
                        difs.append(f"{texto}: inmediato de {nom}: {mio} (H1, H0)")
                continue
            t = _de_texto(partes[i])
            i += 1
            if t is None:
                difs.append(f"{texto}: no se leyó el operando {nom} ({partes[i - 1]})")
                continue
            for campo in ("t", "n", "banco", "pal", "swz", "neg", "abs"):
                if campo in t and t[campo] != o.get(campo):
                    difs.append(f"{texto}: {nom}.{campo} = {o.get(campo)}, nvdisasm {t[campo]}")
    return hechas, difs


if __name__ == "__main__" and sys.argv[1:2] == ["oraculo"]:
    sys.modules["sm86_baja"] = B
    B._cargar_modulos()
    hechas, difs = oraculo(int(sys.argv[2]) if len(sys.argv) > 2 else 400)
    print(f"{hechas} instrucciones contra nvdisasm, {len(difs)} diferencias")
    for x in difs[:12]:
        print("  " + x)
    sys.exit(1 if difs or not hechas else 0)

if __name__ == "__main__":
    sys.modules["sm86_baja"] = B
    B._cargar_modulos()
    bien, sin, malas = probar(int(sys.argv[2]) if sys.argv[1:2] == ["probar"] and len(sys.argv) > 2 else 3000)
    print(f"{bien} iguales, {sum(sin.values())} sin regla, {sum(malas.values())} distintas")
    for k, v in sorted(malas.items(), key=lambda x: -x[1])[:10] + sorted(sin.items(), key=lambda x: -x[1])[:8]:
        print(f"  {v}: {k}")
    sys.exit(1 if malas else 0)
