"""Bajada de instrucciones SM86 (Ampere, Switch 2) a Maxwell, para que el traductor de shaders de Ryujinx (totk_xlate)
las convierta a SPIR-V igual que los shaders de Switch 1. Código propio (MIT): el decodificador SM86 es tools/sm86.py y
la codificación Maxwell sale de tools/mxisa.json (tablas de Ryujinx, MIT).

    python3 tools/sm86_baja.py probar [n]     n instrucciones SM86 al azar (de las que ya tienen regla): se decodifican,
                                              se bajan y el Maxwell resultante se vuelve a leer con las tablas de
                                              Ryujinx para comprobar operación, registros y modificadores

Cada regla recibe la instrucción decodificada y devuelve una lista de palabras Maxwell de 64 bits (sin las palabras de
control del planificador, que agrega quien arma el programa). Una instrucción Ampere puede necesitar varias Maxwell
(p. ej. un valor absoluto que Maxwell no tiene en esa forma): para eso están los registros temporales.
"""
import json
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import sm86  # noqa: E402

ISA = json.load(open(os.path.join(AQUI, "mxisa.json")))
PATRONES = {p["op"]: p for p in ISA["patrones"]}
_TERMINO = re.compile(r"\(\(\(o\) >> (\d+)\) & (0x[0-9A-Fa-f]+)\)|\(\(o\) & (0x[0-9A-Fa-f]+)\)")


def _inversa(expr):
    """[(desplazamiento, máscara)] de una expresión de campo; bool: [(-1, máscara)]."""
    if expr.endswith("!= 0"):
        return [(-1, int(_TERMINO.search(expr).group(3), 16))]
    m = re.fullmatch(r"\(\(o\) >> (\d+)\)", expr)
    if m:   # (int)(opcode >> n) en Ryujinx: el cast corta a 32 bits
        return [(int(m.group(1)), 0xFFFFFFFF)]
    partes = []
    for m in _TERMINO.finditer(expr):
        partes.append((int(m.group(1)), int(m.group(2), 16)) if m.group(1) else (0, int(m.group(3), 16)))
    return partes


CAMPOS = {op: {c["nombre"]: _inversa(c["expr"]) for c in cs} for op, cs in ISA["campos"].items()}


def codificar(op, **campos):
    """La palabra Maxwell de la operación `op` (nombre de mxisa.json, p. ej. FfmaR) con esos campos; Pred=7 (PT) si
    no se da otro."""
    p = PATRONES[op]
    o = int(p["bits"].replace("x", "0"), 2)
    campos.setdefault("Pred", 7)
    if "DestPred" in CAMPOS[op]:   # LOP3/LOP/LEA de Maxwell: sin decirlo, Ryujinx escribiría P0 = falso
        campos.setdefault("DestPred", 7)
    for nombre, valor in campos.items():
        if nombre not in CAMPOS[op]:
            raise KeyError(f"{op} no tiene el campo {nombre}")
        valor = int(valor)
        for desp, mascara in CAMPOS[op][nombre]:
            if desp < 0:
                o |= mascara if valor else 0
            else:
                o |= (valor & mascara) << desp
    return o


def leer(o):
    """(operación, campos) de una palabra Maxwell, con las tablas de Ryujinx (como tools/mxdis.py)."""
    import mxdis
    op, campos = mxdis.decodifica(o)
    for c in ISA["campos"].get(op, []):   # los campos int de Ryujinx son de 32 bits ((int) en C#)
        if c["tipo"] == "int" and isinstance(campos.get(c["nombre"]), int):
            campos[c["nombre"]] &= 0xFFFFFFFF
    return op, campos


# ---------- reglas

RZ, PT = 255, 7


class SinRegla(Exception):
    pass


def _guarda(d):
    g = d["guarda"]
    return {"Pred": 7 if g.endswith("PT") else int(g[-1]), "PredInv": g.startswith("!")}


def _c(d, campo, defecto=0):
    return d["campos"].get(campo, defecto)


def _mods(d):
    return set(d["nombre"].split(".")[1:])


def _fuente(d, i):
    """Cómo viene el operando i de la forma base: 'reg', 'imm', 'const' (y sus campos)."""
    _, _, ops = sm86.partes(d["base"])
    return sm86.operando(ops[i])["tipo"] if i < len(ops) else None


def _redondeo(m):
    return 1 if "RM" in m else 2 if "RP" in m else 3 if "RZ" in m else 0


TEMPS = [254, 253, 252]   # registros temporales para lo que Maxwell hace en dos pasos (los asigna quien arma el programa)
# Registros uniformes de Ampere (UR0..UR62; UR63 = URZ): Maxwell no los tiene. URn vive en el registro común
# UR_BASE + n (R188..R250); URZ es RZ. Quien arma el programa comprueba que el shader no use esos registros.
UR_BASE = 188


def ur(n):
    """El registro común Maxwell donde vive el uniforme URn."""
    return RZ if n == 63 else UR_BASE + n


def _const_off(d, i):
    """Offset en bytes (16 bits sin signo) de un operando c[banco][off]. Lo que la especificación llama "off_reg" no es
    un registro: es el bit 53, el bit 15 del offset (nvdisasm lo muestra con signo, c[0x0][-0x8000], porque el campo
    es de 16 bits; en cx[] lo muestra sin signo, 0x8000). Como los bancos son de 64 KiB, se toma sin signo."""
    return _c(d, f"op{i}.off") | (0x8000 if _c(d, f"op{i}.off_reg") else 0)


def _const(d, i):
    """(banco, offset en palabras) de un operando constante directo c[banco][off]. cx[URn][off] (bit 91, banco
    elegido por un registro uniforme) es una constante sin enlace (bindless): necesita apoyo del runtime, ver
    docs/pendientes.md."""
    if _c(d, f"op{i}.banco_reg"):
        raise SinRegla("cx[UR][off]: banco por registro uniforme (bindless), falta apoyo del runtime")
    off = _const_off(d, i)
    if off & 3:
        raise SinRegla("constante sin alinear")
    return _c(d, f"op{i}.banco"), off >> 2


def _abs_a_temp(d, i, temp, g):
    """Maxwell no tiene |x| en esa forma: temp = |op| (con el signo del operando) con FADD |x| + -0; una constante
    pasa antes por el temporal (MOV)."""
    if _fuente(d, i) == "const":
        banco, off = _const(d, i)
        return [codificar("MovC", Dest=temp, CbufSlot=banco, CbufOffset=off, QuadMask=0xF, **g),
                codificar("FaddR", Dest=temp, SrcA=temp, SrcB=RZ, AbsA=1, NegA=_c(d, f"op{i}.neg"), NegB=1, **g)]
    return [codificar("FaddR", Dest=temp, SrcA=_c(d, f"op{i}.n"), SrcB=RZ, AbsA=1, NegA=_c(d, f"op{i}.neg"),
                      NegB=1, **g)]


def baja_mov(d):
    g = _guarda(d)
    t = _fuente(d, 1)
    if t == "reg":
        return [codificar("MovR", Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), QuadMask=0xF, **g)]
    if t == "imm":
        return [codificar("Mov32i", Dest=_c(d, "op0.n"), Imm32=_c(d, "op1.v") & 0xFFFFFFFF, QuadMask=0xF, **g)]
    if t == "const":
        banco, off = _const(d, 1)
        return [codificar("MovC", Dest=_c(d, "op0.n"), CbufSlot=banco, CbufOffset=off, QuadMask=0xF, **g)]
    raise SinRegla("MOV con " + str(t))


def _float_b(d, i, prefijo, g, campos, previas):
    """Elige la forma Maxwell según el operando B (índice i): R (registro), 32i (inmediato) o C (constante)."""
    t = _fuente(d, i)
    if t == "reg":
        return prefijo + "R", {**campos, "SrcB": _c(d, f"op{i}.n")}
    if t == "imm":
        return prefijo + "32i", {**campos, "Imm32": _c(d, f"op{i}.v") & 0xFFFFFFFF}
    if t == "const":
        banco, off = _const(d, i)
        return prefijo + "C", {**campos, "CbufSlot": banco, "CbufOffset": off}
    raise SinRegla(f"{prefijo} con B {t}")


def baja_fadd(d):
    g, m = _guarda(d), _mods(d)
    campos = dict(Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), NegA=_c(d, "op1.neg"), AbsA=_c(d, "op1.abs"),
                  NegB=_c(d, "op2.neg"), AbsB=_c(d, "op2.abs"), Ftz="FTZ" in m)
    op, campos = _float_b(d, 2, "Fadd", g, campos, [])
    if op != "Fadd32i":
        campos.update(Sat="SAT" in m, RoundMode=_redondeo(m))
    elif "SAT" in m or _redondeo(m):
        raise SinRegla("FADD inmediato con SAT o redondeo")
    return [codificar(op, **campos, **g)]


def baja_fmul(d):
    g, m = _guarda(d), _mods(d)
    previas = []
    a = _c(d, "op1.n")
    if _c(d, "op1.abs"):
        previas.extend(_abs_a_temp(d, 1, TEMPS[0], g))
        a = TEMPS[0]
    neg = (0 if _c(d, "op1.abs") else _c(d, "op1.neg")) ^ (0 if _c(d, "op2.abs") else _c(d, "op2.neg"))
    campos = dict(Dest=_c(d, "op0.n"), SrcA=a, Sat="SAT" in m, Fmz=1 if "FTZ" in m else 2 if "FMZ" in m else 0)
    if _c(d, "op2.abs"):
        previas.extend(_abs_a_temp(d, 2, TEMPS[1], g))
        return previas + [codificar("FmulR", SrcB=TEMPS[1], NegA=neg, RoundMode=_redondeo(m), **campos, **g)]
    op, campos = _float_b(d, 2, "Fmul", g, campos, previas)
    if op == "Fmul32i":
        if _redondeo(m):   # FMUL32I no redondea a pedido: el inmediato va a un temporal
            previas.append(codificar("Mov32i", Dest=TEMPS[1], Imm32=campos.pop("Imm32"), QuadMask=0xF, **g))
            return previas + [codificar("FmulR", SrcB=TEMPS[1], NegA=neg, RoundMode=_redondeo(m), **campos, **g)]
        if neg:   # el signo va en el inmediato
            campos["Imm32"] ^= 0x80000000
    else:
        campos.update(NegA=neg, RoundMode=_redondeo(m))
    return previas + [codificar(op, **campos, **g)]


def baja_ffma(d):
    g, m = _guarda(d), _mods(d)
    previas = []
    a, c = _c(d, "op1.n"), _c(d, "op3.n")
    nega = _c(d, "op1.neg") ^ _c(d, "op2.neg")
    if _c(d, "op1.abs"):
        previas.extend(_abs_a_temp(d, 1, TEMPS[0], g))
        a, nega = TEMPS[0], _c(d, "op2.neg")
    b_reg = None
    if _c(d, "op2.abs"):
        previas.extend(_abs_a_temp(d, 2, TEMPS[2], g))
        b_reg = TEMPS[2]
        nega = _c(d, "op1.neg") if not _c(d, "op1.abs") else 0
    negc = _c(d, "op3.neg")
    t3 = _fuente(d, 3)
    if t3 == "reg" and _c(d, "op3.abs"):
        previas.extend(_abs_a_temp(d, 3, TEMPS[1], g))
        c, negc = TEMPS[1], 0
    base = dict(Dest=_c(d, "op0.n"), SrcA=a, NegA=nega, NegC=negc, Sat="SAT" in m,
                Fmz=1 if "FTZ" in m else 2 if "FMZ" in m else 0)
    t2 = _fuente(d, 2)
    if t3 == "imm":   # Maxwell no tiene C inmediato: el número va a un temporal
        previas.append(codificar("Mov32i", Dest=TEMPS[1], Imm32=_c(d, "op3.v") & 0xFFFFFFFF, QuadMask=0xF, **g))
        c, t3 = TEMPS[1], "reg"
    if t2 == "imm" and (_redondeo(m) or True):
        # FFMA32I suma el destino y no redondea a pedido: más simple y seguro, B a un temporal
        previas.append(codificar("Mov32i", Dest=TEMPS[2], Imm32=_c(d, "op2.v") & 0xFFFFFFFF, QuadMask=0xF, **g))
        b_reg, t2 = TEMPS[2], "reg"
    if t2 == "reg" and t3 == "reg":
        b = b_reg if b_reg is not None else _c(d, "op2.n")
        return previas + [codificar("FfmaR", SrcB=b, SrcC=c, RoundMode=_redondeo(m), **base, **g)]
    if t2 == "const" and t3 == "reg":
        banco, off = _const(d, 2)
        return previas + [codificar("FfmaC", CbufSlot=banco, CbufOffset=off, SrcC=c, RoundMode=_redondeo(m),
                                    **base, **g)]
    if t2 == "reg" and t3 == "const":
        banco, off = _const(d, 3)
        return previas + [codificar("FfmaRc", SrcC=_c(d, "op2.n"), CbufSlot=banco, CbufOffset=off,
                                    RoundMode=_redondeo(m), **base, **g)]
    if t2 == "imm" and t3 == "reg":
        # FFMA32I de Maxwell suma el destino (C = D): si C no es D, primero se copia C al destino
        if _redondeo(m):
            raise SinRegla("FFMA inmediato con redondeo")
        if c != _c(d, "op0.n"):
            previas.append(codificar("MovR", Dest=_c(d, "op0.n"), SrcA=c, QuadMask=0xF, **g))
        return previas + [codificar("Ffma32i", Imm32=_c(d, "op2.v") & 0xFFFFFFFF, **base, **g)]
    raise SinRegla(f"FFMA con {t2}/{t3}")


def _cabe20(v):
    """¿El inmediato de 32 bits cabe en el Imm20 de Maxwell (20 bits con signo, extendido)?"""
    v &= 0xFFFFFFFF
    return v < 0x80000 or v >= 0xFFF80000


def _b(d, i, prefijo, g, previas, temp, imm20=True):
    """El operando i como "B" de una instrucción Maxwell de enteros: (operación, campos). Registro -> R, constante ->
    C, inmediato que cabe en 20 bits -> I; si no cabe, MOV32I a un temporal y R."""
    t = _fuente(d, i)
    if t == "reg":
        return prefijo + "R", {"SrcB": _c(d, f"op{i}.n")}
    if t == "const":
        banco, off = _const(d, i)
        return prefijo + "C", {"CbufSlot": banco, "CbufOffset": off}
    if t == "imm":
        v = _c(d, f"op{i}.v") & 0xFFFFFFFF
        if imm20 and _cabe20(v) and prefijo + "I" in PATRONES:
            return prefijo + "I", {"Imm20": v & 0xFFFFF}
        previas.append(codificar("Mov32i", Dest=temp, Imm32=v, QuadMask=0xF, **g))
        return prefijo + "R", {"SrcB": temp}
    raise SinRegla(f"{prefijo} con {t}")


def _pt(d, i):
    """El predicado del operando i es PT (no se usa)."""
    return _c(d, f"op{i}.n", 7) == 7 and not _c(d, f"op{i}.not")


COMP = {"F": 0, "LT": 1, "EQ": 2, "LE": 3, "GT": 4, "NE": 5, "GE": 6, "NUM": 7, "NAN": 8, "LTU": 9, "EQU": 10,
        "LEU": 11, "GTU": 12, "NEU": 13, "GEU": 14, "T": 15}
BOP = {"AND": 0, "OR": 1, "XOR": 2}


def baja_setp(d, entero):
    """ISETP / FSETP cmp[.U32].bop Pd, Pq, A, B, Pc -> Maxwell Isetp/Fsetp (DestPred, DestPredInv = 2.º destino)."""
    g, m = _guarda(d), _mods(d)
    if "EX" in m:
        raise SinRegla("ISETP.EX")
    partes = d["nombre"].split(".")
    comp, bop = partes[1], partes[-1]
    if comp not in COMP or bop not in BOP or (entero and COMP[comp] > 7):
        raise SinRegla(f"comparación {comp}.{bop}")
    previas = []
    op, b = _b(d, 3, "Isetp" if entero else "Fsetp", g, previas, TEMPS[0])
    campos = dict(DestPred=_c(d, "op0.n", 7), DestPredInv=_c(d, "op1.n", 7), SrcA=_c(d, "op2.n"),
                  SrcPred=_c(d, "op4.n", 7), SrcPredInv=_c(d, "op4.not"), Bop=BOP[bop], **b)
    if entero:
        campos.update(IComp=COMP[comp], Signed="U32" not in m)
    else:
        if op == "FsetpI":   # el inmediato de FSETP de Maxwell es un float recortado: mejor al temporal
            previas.append(codificar("Mov32i", Dest=TEMPS[0], Imm32=_c(d, "op3.v") & 0xFFFFFFFF, QuadMask=0xF, **g))
            op, campos = "FsetpR", {k: v for k, v in campos.items() if k != "Imm20"}
            campos["SrcB"] = TEMPS[0]
        campos.update(FComp=COMP[comp], Ftz="FTZ" in m, NegA=_c(d, "op2.neg"), AbsA=_c(d, "op2.abs"),
                      NegB=_c(d, "op3.neg"), AbsB=_c(d, "op3.abs"))
    return previas + [codificar(op, **campos, **g)]


def baja_sel(d):
    """SEL / FSEL Rd, A, B, Pc: Rd = Pc ? A : B (FSEL igual: Maxwell no tiene FSEL y elegir no toca los bits)."""
    g = _guarda(d)
    previas = []
    op, b = _b(d, 2, "Sel", g, previas, TEMPS[0], imm20=d["nombre"].startswith("SEL"))
    return previas + [codificar(op, Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), SrcPred=_c(d, "op3.n", 7),
                                SrcPredInv=_c(d, "op3.not"), **b, **g)]


def baja_lop3(d):
    """LOP3.LUT [Pp,] Rd, A, B, C, lut, Pq: sin predicado de salida ni de entrada, Maxwell LOP3 con la misma tabla."""
    g, m = _guarda(d), _mods(d)
    if not _pt(d, 0) or "PAND" in m or "POR" in m:
        raise SinRegla("LOP3 con predicado")
    previas = []
    op, b = _b(d, 3, "Lop3", g, previas, TEMPS[0], imm20=False)
    if op == "Lop3C":
        raise SinRegla("LOP3 con constante en B")   # Lop3C de Maxwell no tiene campo para la tabla en el mismo lugar
    return previas + [codificar(op, Dest=_c(d, "op1.n"), SrcA=_c(d, "op2.n"), SrcC=_c(d, "op4.n"),
                                Imm=_c(d, "op5.v") & 0xFF, **b, **g)]


def baja_iadd3(d):
    """IADD3 Rd, [Pc1, Pc2,] A, B, C sin acarreo usado -> Maxwell IADD3."""
    g, m = _guarda(d), _mods(d)
    if "X" in m or not _pt(d, 1) or not _pt(d, 2):
        raise SinRegla("IADD3 con acarreo")
    previas = []
    op, b = _b(d, 4, "Iadd3", g, previas, TEMPS[0])
    if _fuente(d, 5) != "reg":
        raise SinRegla("IADD3 con C no registro")
    return previas + [codificar(op, Dest=_c(d, "op0.n"), SrcA=_c(d, "op3.n"), SrcC=_c(d, "op5.n"),
                                NegA=_c(d, "op3.neg"), NegB=_c(d, "op4.neg"), NegC=_c(d, "op5.neg"), **b, **g)]


def _a_registro(d, i, g, previas, temp):
    """El operando i en un registro: el suyo, o un temporal cargado con su inmediato o su constante."""
    t = _fuente(d, i)
    if t == "reg":
        return _c(d, f"op{i}.n")
    if t == "imm":
        previas.append(codificar("Mov32i", Dest=temp, Imm32=_c(d, f"op{i}.v") & 0xFFFFFFFF, QuadMask=0xF, **g))
        return temp
    if t == "const":
        banco, off = _const(d, i)
        previas.append(codificar("MovC", Dest=temp, CbufSlot=banco, CbufOffset=off, QuadMask=0xF, **g))
        return temp
    raise SinRegla(f"operando {t}")


MUFU = {"COS": 0, "SIN": 1, "EX2": 2, "LG2": 3, "RCP": 4, "RSQ": 5, "RCP64H": 6, "RSQ64H": 7, "SQRT": 8}


def baja_mufu(d):
    g, m = _guarda(d), _mods(d)
    partes = d["nombre"].split(".")
    if len(partes) < 2 or partes[1] not in MUFU or "F16" in m:
        raise SinRegla("MUFU " + d["nombre"])
    previas = []
    a = _a_registro(d, 1, g, previas, TEMPS[0])
    return previas + [codificar("Mufu", Dest=_c(d, "op0.n"), SrcA=a, MufuOp=MUFU[partes[1]],
                                NegA=_c(d, "op1.neg"), AbsA=_c(d, "op1.abs"), **g)]


def baja_fmnmx(d):
    """FMNMX Rd, A, B, P: P ? min : max, igual que Maxwell con SrcPred."""
    g, m = _guarda(d), _mods(d)
    if "NAN" in m or "XORSIGN" in m:
        raise SinRegla("FMNMX." + ".".join(sorted(m)))
    previas = []
    b = _a_registro(d, 2, g, previas, TEMPS[0])
    return previas + [codificar("FmnmxR", Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), SrcB=b, NegA=_c(d, "op1.neg"),
                                AbsA=_c(d, "op1.abs"), NegB=_c(d, "op2.neg"), AbsB=_c(d, "op2.abs"), Ftz="FTZ" in m,
                                SrcPred=_c(d, "op3.n", 7), SrcPredInv=_c(d, "op3.not"), **g)]


def baja_imnmx(d):
    g, m = _guarda(d), _mods(d)
    previas = []
    b = _a_registro(d, 2, g, previas, TEMPS[0])
    return previas + [codificar("ImnmxR", Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), SrcB=b, Signed="U32" not in m,
                                SrcPred=_c(d, "op3.n", 7), SrcPredInv=_c(d, "op3.not"), **g)]


def baja_popc(d):
    g = _guarda(d)
    previas = []
    b = _a_registro(d, 1, g, previas, TEMPS[0])
    return previas + [codificar("PopcR", Dest=_c(d, "op0.n"), SrcB=b, NegB=_c(d, "op1.not"), **g)]


def baja_flo(d):
    """FLO[.U32][.SH] Rd, Pp, B: sin predicado de salida (Maxwell no tiene)."""
    g, m = _guarda(d), _mods(d)
    if not _pt(d, 1):
        raise SinRegla("FLO con predicado de salida")
    previas = []
    b = _a_registro(d, 2, g, previas, TEMPS[0])
    return previas + [codificar("FloR", Dest=_c(d, "op0.n"), SrcB=b, Signed="U32" not in m, Sh="SH" in m,
                                NegB=_c(d, "op2.not"), **g)]


PMODE = {"F4E": 1, "B4E": 2, "RC8": 3, "ECL": 4, "ECR": 5, "RC16": 6}


def baja_prmt(d):
    """PRMT Rd, A, sel, C [modo]: Maxwell PRMT con la misma semántica (A y C, selector B)."""
    g, m = _guarda(d), _mods(d)
    modo = [PMODE[x] for x in m if x in PMODE]
    if any(x.startswith("INVALID") for x in m):
        raise SinRegla("PRMT inválido")
    previas = []
    b = _a_registro(d, 2, g, previas, TEMPS[0])
    c = _a_registro(d, 3, g, previas, TEMPS[1])
    return previas + [codificar("PrmtR", Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), SrcB=b, SrcC=c,
                                PMode=modo[0] if modo else 0, **g)]


def baja_s2r(d):
    """S2R Rd, SR_x: los números de registro especial de Ampere son los de Maxwell (bits 72-79)."""
    return [codificar("S2r", Dest=_c(d, "op0.n"), SReg=d["hi"] >> 8 & 0xFF, **_guarda(d))]


def baja_iabs(d):
    """IABS Rd, B: Maxwell no tiene IABS; I2I.S32.S32 con valor absoluto hace lo mismo."""
    g = _guarda(d)
    previas = []
    b = _a_registro(d, 1, g, previas, TEMPS[0])
    return previas + [codificar("I2iR", Dest=_c(d, "op0.n"), SrcB=b, AbsB=1, ISrcDstFmt=6, IDstFmt=6, **g)
                      if "ISrcDstFmt" in CAMPOS["I2iR"] else
                      codificar("I2iR", Dest=_c(d, "op0.n"), SrcB=b, AbsB=1, **_formatos_i2i(), **g)]


def _formatos_i2i():
    c = CAMPOS["I2iR"]
    r = {}
    if "IDstFmt" in c:
        r["IDstFmt"] = 6   # S32
    if "ISrcFmt" in c:
        r["ISrcFmt"] = 6
    return r


def baja_exit(d):
    return [codificar("Exit", Ccc=15, **_guarda(d))]


def baja_nop(d):
    return [codificar("Nop", Ccc=15, **_guarda(d))]


REGLAS = {"MOV": baja_mov, "FADD": baja_fadd, "FMUL": baja_fmul, "FFMA": baja_ffma, "EXIT": baja_exit, "NOP": baja_nop,
          "ISETP": lambda d: baja_setp(d, True), "FSETP": lambda d: baja_setp(d, False), "SEL": baja_sel,
          "FSEL": baja_sel, "LOP3": baja_lop3, "IADD3": baja_iadd3, "MUFU": baja_mufu, "FMNMX": baja_fmnmx,
          "IMNMX": baja_imnmx, "POPC": baja_popc, "FLO": baja_flo, "PRMT": baja_prmt, "S2R": baja_s2r,
          "IABS": baja_iabs}

# Para comprobar sin repetir la lógica de las reglas: qué campo Maxwell debe tener qué operando SM86, y en qué
# operando SM86 está el "B" (registro, inmediato o constante) de la instrucción Maxwell principal.
ROLES = {
    "FADD": ({"Dest": 0, "SrcA": 1}, 2), "FMUL": ({"Dest": 0}, 2), "FFMA": ({"Dest": 0}, 2), "MOV": ({"Dest": 0}, 1),
    "ISETP": ({"DestPred": 0, "DestPredInv": 1, "SrcA": 2, "SrcPred": 4}, 3),
    "FSETP": ({"DestPred": 0, "DestPredInv": 1, "SrcA": 2, "SrcPred": 4}, 3),
    "SEL": ({"Dest": 0, "SrcA": 1, "SrcPred": 3}, 2), "FSEL": ({"Dest": 0, "SrcA": 1, "SrcPred": 3}, 2),
    "LOP3": ({"Dest": 1, "SrcA": 2, "SrcC": 4}, 3), "IADD3": ({"Dest": 0, "SrcA": 3, "SrcC": 5}, 4),
    "MUFU": ({"Dest": 0}, 1), "FMNMX": ({"Dest": 0, "SrcA": 1, "SrcPred": 3}, 2),
    "IMNMX": ({"Dest": 0, "SrcA": 1, "SrcPred": 3}, 2), "POPC": ({"Dest": 0}, 1), "FLO": ({"Dest": 0}, 2),
    "PRMT": ({"Dest": 0, "SrcA": 1}, 2), "S2R": ({"Dest": 0}, None), "IABS": ({"Dest": 0}, 1),
}
FAMILIA = {"FADD": "Fadd", "FMUL": "Fmul", "FFMA": "Ffma", "MOV": "Mov", "ISETP": "Isetp", "FSETP": "Fsetp",
           "SEL": "Sel", "FSEL": "Sel", "LOP3": "Lop3", "IADD3": "Iadd3", "MUFU": "Mufu", "FMNMX": "Fmnmx",
           "IMNMX": "Imnmx", "POPC": "Popc", "FLO": "Flo", "PRMT": "Prmt", "S2R": "S2r", "IABS": "I2i"}


def _cargar_modulos():
    """Reglas de tools/sm86_baja_*.py: cada módulo define REGLAS, ROLES y FAMILIA (como los de aquí) y, si sus
    instrucciones necesitan otra comprobación, COMPROBAR = {nombre SM86: función(d, palabras) -> [errores]}."""
    import glob
    import importlib
    for ruta in sorted(glob.glob(os.path.join(AQUI, "sm86_baja_*.py"))):
        m = importlib.import_module(os.path.splitext(os.path.basename(ruta))[0])
        REGLAS.update(getattr(m, "REGLAS", {}))
        ROLES.update(getattr(m, "ROLES", {}))
        FAMILIA.update(getattr(m, "FAMILIA", {}))
        COMPROBAR.update(getattr(m, "COMPROBAR", {}))


COMPROBAR = {}


def bajar(d):
    regla = REGLAS.get(d["nombre"].split(".")[0])
    if not regla:
        raise SinRegla(d["nombre"])
    return regla(d)


# ---------- comprobar: lo bajado, leído con las tablas de Ryujinx, dice lo mismo

ESPERADO = {   # operación SM86 -> (operación Maxwell, {campo Maxwell: campo SM86 o función})
    "MOV": None, "EXIT": ("Exit", {}), "NOP": ("Nop", {}),
}


def comprobar(d, palabras):
    """Diferencias entre la instrucción SM86 y su bajada, leída con las tablas de Ryujinx: guarda, operación,
    registros y predicados por su papel (ROLES), y el operando B donde haya ido (registro, temporal cargado con el
    mismo inmediato, inmediato o constante)."""
    nombre = d["nombre"].split(".")[0]
    if nombre in COMPROBAR:
        return COMPROBAR[nombre](d, palabras)
    errores = []
    op, c = leer(palabras[-1])
    g = _guarda(d)
    if (c.get("Pred", 7), bool(c.get("PredInv")) and c.get("Pred", 7) != 7) != (g["Pred"], g["PredInv"] and g["Pred"] != 7):
        errores.append(f"guarda {g} vs Pred={c.get('Pred')} PredInv={c.get('PredInv')}")
    nombre = d["nombre"].split(".")[0]
    if nombre in ("EXIT", "NOP"):
        return errores if op == {"EXIT": "Exit", "NOP": "Nop"}[nombre] else errores + [f"operación {op}"]
    if not op.startswith(FAMILIA[nombre]):
        return errores + [f"operación {op}"]
    roles, ib = ROLES[nombre]
    for campo, i in roles.items():
        tipo = sm86.operando(sm86.partes(d["base"])[2][i])["tipo"]
        esperado = _c(d, f"op{i}.n", 7 if tipo == "pred" else 0)
        if c.get(campo) != esperado and not (nombre in ("FMUL", "FFMA") and campo == "SrcA"):
            errores.append(f"{campo}={c.get(campo)} vs op{i}={esperado}")
    if ib is None:
        if nombre == "S2R" and c.get("SReg") != (d["hi"] >> 8 & 0xFF):
            errores.append("registro especial")
        return errores
    # el operando B
    t = _fuente(d, ib)
    temporales = {}
    for w in palabras[:-1]:   # lo que cargaron las previas (MOV32I / MOVC a un temporal)
        o2, c2 = leer(w)
        if o2 == "Mov32i":
            temporales[c2["Dest"]] = ("imm", c2["Imm32"])
        if o2 == "MovC":
            temporales[c2["Dest"]] = ("const", c2["CbufSlot"], c2["CbufOffset"] * 4)
    v = _c(d, f"op{ib}.v") & 0xFFFFFFFF
    if t == "imm":
        neg_a = 0 if _c(d, "op1.abs") else _c(d, "op1.neg")
        signo = 0x80000000 if op == "Fmul32i" and (neg_a ^ _c(d, f"op{ib}.neg")) else 0
        if "Imm32" in c:
            ok = c["Imm32"] == v ^ signo
        elif "Imm20" in c:
            ok = (c["Imm20"] | (0xFFF00000 if c["Imm20"] & 0x80000 else 0)) == v
        else:
            ok = any(temporales.get(c.get(f)) == ("imm", v) for f in ("SrcA", "SrcB", "SrcC"))
        if not ok:
            errores.append(f"B inmediato {v:#x}")
    elif t == "const":
        quiere = (_c(d, f"op{ib}.banco"), _c(d, f"op{ib}.off") | _c(d, f"op{ib}.off_reg") << 15)   # bit 53 = bit 15
        if "CbufSlot" in c:
            ok = (c["CbufSlot"], c["CbufOffset"] * 4) == quiere
        else:
            ok = any(x[0] == "const" and x[1:] == quiere for x in temporales.values())
        if not ok:
            errores.append(f"B constante c[{quiere[0]:#x}][{quiere[1]:#x}]")
    elif t == "reg" and nombre not in ("FMUL", "FFMA", "MOV"):
        campo_b = "SrcA" if nombre == "MUFU" else "SrcB"
        if c.get(campo_b) != _c(d, f"op{ib}.n") and c.get(campo_b) not in TEMPS:
            errores.append(f"{campo_b}={c.get(campo_b)} vs op{ib}={_c(d, f'op{ib}.n')}")
    m = _mods(d)
    if "Sat" in c and nombre in ("FADD", "FMUL", "FFMA") and int(c["Sat"]) != ("SAT" in m):
        errores.append("SAT")
    if nombre in ("ISETP", "FSETP"):
        partes = d["nombre"].split(".")
        if c.get("IComp", c.get("FComp")) != COMP.get(partes[1]) or c.get("Bop") != BOP.get(partes[-1]):
            errores.append("comparación")
    if nombre == "LOP3" and c.get("Imm") != _c(d, "op5.v") & 0xFF:
        errores.append("tabla de LOP3")
    return errores


def probar(n):
    import random
    spec = json.load(open(os.path.join(os.path.dirname(AQUI), "build", "sm86_spec.json")))
    rnd = random.Random(7)
    solo = os.environ.get("SOLO")   # SOLO=TEX,TLD: probar solo esas instrucciones
    ops = [k for k, s in spec.items() if s["nombre"].split(".")[0] in REGLAS and
           (not solo or s["nombre"].split(".")[0] in solo.split(","))]
    hechas = bien = sin_regla = 0
    fallas = {}
    for _ in range(n):
        k = rnd.choice(ops)
        s = spec[k]
        lo, hi = int(k, 16) | (rnd.randrange(16) << 12), int(s["hi"], 16)
        for bits in s["campos"].values():
            for b in bits.values():
                if rnd.random() < 0.5:
                    lo, hi = (lo ^ (1 << b), hi) if b < 64 else (lo, hi ^ (1 << (b - 64)))
        d = sm86.decodificar(lo, hi, spec)
        if not d or d.get("invalida"):
            continue
        hechas += 1
        try:
            palabras = bajar(d)
        except SinRegla as e:
            sin_regla += 1
            fallas.setdefault("sin regla: " + str(e), []).append(d["nombre"])
            continue
        errores = comprobar(d, palabras)
        if errores:
            fallas.setdefault(d["nombre"].split(".")[0], []).append(errores)
        else:
            bien += 1
    print(f"{bien}/{hechas} bajadas leídas igual por las tablas de Ryujinx; {sin_regla} sin regla todavía")
    for k, v in sorted(fallas.items(), key=lambda x: -len(x[1]))[:10]:
        print(f"  {k}: {len(v)}; ej. {v[0]}")
    return bien, hechas


if __name__ == "__main__":
    sys.modules["sm86_baja"] = sys.modules["__main__"]   # los módulos importan sm86_baja: que vean este mismo
    _cargar_modulos()
    if sys.argv[1:2] == ["probar"]:
        probar(int(sys.argv[2]) if len(sys.argv) > 2 else 2000)
    else:
        sys.exit(__doc__)
