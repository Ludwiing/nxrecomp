"""Bajada SM86 -> Maxwell de PLOP3 (predicados), VOTE y FSET. Código propio (MIT). Lo carga tools/sm86_baja.py
(_cargar_modulos); se prueba con `python3 tools/sm86_baja_predicados.py probar 3000` (también cambia
los bits de modificadores, que `probar` de sm86_baja.py no toca).

Encodings comprobados con nvdisasm (sm_86), cambiando un bit a la vez sobre la forma base:
- VOTE (0x806) Rd, Pd, [!]Ps: modo en los bits 72-73 (0 ALL, 1 ANY, 2 EQ; 3 inválido).
- FSET (0x20a registro, 0x80a inmediato de 32 bits, 0xa0a constante) Rd, A, B, [!]Pc: operación lógica en 74-75
  (AND, OR, XOR), comparación en 76-79 (el mismo orden que FComp de Maxwell: F, LT, EQ, LE, GT, NE, GE, NUM, NAN, LTU,
  ..., T), FTZ en el bit 80. No hay bit para el resultado entero: nvdisasm siempre muestra .BF (1.0 o 0.0).
- PLOP3.LUT (0x81c) Pd, Pq, [!]Pa, [!]Pb, [!]Pc, tabla_d, tabla_q (con el bit 67 la tercera entrada es UPn: sin
  regla todavía): las dos salidas con su tabla de 8 bits sobre las
  mismas entradas (bit A*4 + B*2 + C). Las formas 0x21d-0x21f y 0xa1d-0xa1f toman como entrada el signo de un registro
  o de una constante (R.SIGN, c[][].SIGN): sin regla todavía.

Semántica de Maxwell, de Ryujinx (tools/vendor/ryujinx de totk-pc-recomp): InstEmitWarp.Vote (VpDest = el voto, Dest =
ballot), InstEmitFloatComparison.EmitFset (BVal: 1.0f / 0), InstEmitIntegerLogical.EmitLop3 + EmitLopPredWrite
(PredicateOp Nz = resultado != 0) e InstEmitMove.EmitSel (Pc ? A : B).

Bajadas:
- VOTE -> Vote con los mismos campos.
- FSET -> FsetR / FsetC con BVal = 1; un inmediato va a un temporal (MOV32I), como el de FSETP: el Imm20 de Maxwell
  recorta el float. Límite conocido: Ryujinx no aplica FTZ en FSET ni en FSETP (EmitFset no lo recibe), así que una
  entrada denormal compara como su valor, no como 0. Pasa igual con los shaders de Switch 1.
- PLOP3 -> cada entrada distinta a un temporal como -1/0 (SEL con Imm20 = -1), la negación dentro del SEL; PT y !PT
  son RZ con la tabla invertida o tal cual; cada salida es un LOP3 a RZ con DestPred y PredicateOp = Nz. Las
  entradas ya están en temporales, así que escribir Pd antes de calcular Pq no cambia Pq aunque Pd sea una entrada;
  la salida que escribe el predicado de la guarda va última (si no, cambiaría la guarda de la otra). Las dos salidas
  en el mismo predicado quedan sin regla: el resultado del hardware no se conoce.

La comprobación (COMPROBAR) ejecuta la instrucción SM86 y lo bajado sobre los mismos estados al azar en un warp de 32
hilos (el simulador y los estados de tools/sm86_baja_uniformes.py, más SelI, Lop3R con DestPred y Fset). Los floats al
azar incluyen NaN, infinitos, ceros con signo y denormales; FTZ se ejecuta como lo traduce Ryujinx (ver arriba).
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, codificar, leer, _c, _const, _fuente, _guarda  # noqa: E402
import sm86_baja_uniformes as U  # noqa: E402

VOTO = {0: "ALL", 1: "ANY", 2: "EQ"}


def _bits(d, a, n=1):
    v = d["lo"] | d["hi"] << 64
    return v >> a & ((1 << n) - 1)


# ---------- reglas

def baja_vote(d):
    """VOTE.modo Rd, Pd, [!]Ps -> Vote: Rd = ballot(Ps) en los hilos activos, Pd = todos / alguno / todos iguales."""
    modo = _bits(d, 72, 2)
    if modo not in VOTO:
        raise SinRegla("VOTE con modo inválido")
    return [codificar("Vote", Dest=_c(d, "op0.n"), VpDest=_c(d, "op1.n", 7), SrcPred=_c(d, "op2.n", 7),
                      SrcPredInv=_c(d, "op2.not"), VoteMode=modo, **_guarda(d))]


def baja_fset(d):
    """FSET.BF.cmp[.FTZ].bop Rd, A, B, [!]Pc -> FsetR / FsetC con BVal (1.0 / 0.0)."""
    if _bits(d, 74, 2) == 3:
        raise SinRegla("FSET con operación lógica inválida")
    g = _guarda(d)
    campos = dict(Dest=_c(d, "op0.n"), SrcA=_c(d, "op1.n"), NegA=_c(d, "op1.neg"), AbsA=_c(d, "op1.abs"),
                  FComp=_bits(d, 76, 4), Bop=_bits(d, 74, 2), Ftz=_bits(d, 80), BVal=1,
                  SrcPred=_c(d, "op3.n", 7), SrcPredInv=_c(d, "op3.not"))
    t = _fuente(d, 2)
    if t == "reg":
        return [codificar("FsetR", SrcB=_c(d, "op2.n"), NegB=_c(d, "op2.neg"), AbsB=_c(d, "op2.abs"), **campos, **g)]
    if t == "const":
        banco, off = _const(d, 2)
        return [codificar("FsetC", CbufSlot=banco, CbufOffset=off, NegB=_c(d, "op2.neg"), AbsB=_c(d, "op2.abs"),
                          **campos, **g)]
    if t == "imm":
        return [codificar("Mov32i", Dest=TEMPS[0], Imm32=_c(d, "op2.v") & 0xFFFFFFFF, QuadMask=0xF, **g),
                codificar("FsetR", SrcB=TEMPS[0], NegB=0, AbsB=0, **campos, **g)]
    raise SinRegla(f"FSET con {t}")


def baja_plop3(d):
    """PLOP3.LUT Pd, Pq, [!]Pa, [!]Pb, [!]Pc, tabla_d, tabla_q (solo predicados, 0x81c)."""
    if int(d["op"], 16) != 0x81C:
        raise SinRegla("PLOP3 con el signo de un registro o constante como entrada")
    if _c(d, "op4.forma"):   # bit 67: la tercera entrada es UPn (nvdisasm), no Pn
        raise SinRegla("PLOP3 con un predicado uniforme (UP) como entrada")
    g = _guarda(d)
    salidas = [(n, lut) for n, lut in ((_c(d, "op0.n", 7), _c(d, "op5.v") & 0xFF),
                                         (_c(d, "op1.n", 7), _c(d, "op6.v") & 0xFF)) if n != 7]
    if not salidas:
        return [codificar("Nop", Ccc=15, **g)]
    if len(salidas) == 2 and salidas[0][0] == salidas[1][0]:
        raise SinRegla("PLOP3 con las dos salidas en el mismo predicado (no se sabe cuál gana)")
    # La salida que pisa el predicado de la guarda va última: si no, la guarda de la otra ya sería la nueva.
    salidas.sort(key=lambda s: s[0] == g["Pred"])
    out, regs, temp, inv = [], [], {}, []
    for k, i in enumerate((2, 3, 4)):
        n, neg = _c(d, f"op{i}.n", 7), bool(_c(d, f"op{i}.not"))
        if n == 7:                     # PT (verdadero) = RZ negado; !PT (falso) = RZ
            regs.append(RZ)
            inv.append((k, not neg))
            continue
        clave = (n, neg)
        if clave not in temp:
            temp[clave] = TEMPS[len(temp)]
            # SEL T, RZ, -1, Pn: T = Pn ? 0 : -1, así que con la guarda invertida queda Pn ? -1 : 0 (y al revés si !Pn)
            out.append(codificar("SelI", Dest=temp[clave], SrcA=RZ, Imm20=0xFFFFF, SrcPred=n, SrcPredInv=not neg,
                                 **g))
        regs.append(temp[clave])
    for p, lut in salidas:
        for k, invertir in inv:
            if invertir:
                lut = U._lut_invierte(lut, k)
        out.append(codificar("Lop3R", Dest=RZ, SrcA=regs[0], SrcB=regs[1], SrcC=regs[2], Imm=lut, DestPred=p,
                             PredicateOp=3, **g))
    return out


REGLAS = {"VOTE": baja_vote, "FSET": baja_fset, "PLOP3": baja_plop3}
ROLES = {}
FAMILIA = {"VOTE": "Vote", "FSET": "Fset", "PLOP3": "Lop3R"}


# ---------- comprobar ejecutando

M32 = 0xFFFFFFFF
_ESPECIALES = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001,
               0x00000001, 0x807FFFFF, 0x3F800001, 0x40490FDB]


def _f(x):
    return struct.unpack("<f", struct.pack("<I", x & M32))[0]


def _absneg(x, ab, ng):
    x &= M32
    if ab:
        x &= 0x7FFFFFFF
    if ng:
        x ^= 0x80000000
    return _f(x)


def _fcomp(cmp, a, b):
    nan = math.isnan(a) or math.isnan(b)
    if cmp == 0:
        return False
    if cmp == 15:
        return True
    if cmp in (7, 8):
        return nan if cmp == 8 else not nan
    base = cmp & 7
    r = False if nan else (a < b, a == b, a <= b, a > b, a != b, a >= b)[base - 1]
    return r or (nan and bool(cmp & 8))


def _bop(bop, r, p):
    return (r and p, r or p, r != p)[bop]


def _ejecuta_maxwell(palabras, hilos):
    otros = []
    for w in palabras:
        op, c = leer(w)
        if op not in ("SelI", "Lop3R", "FsetR", "FsetC"):
            otros.append(w)
            continue
        if otros:
            U._ejecuta_maxwell(otros, hilos)
            otros = []
        for h in hilos:
            R, P = h["R"], h["P"]
            if P[c.get("Pred", 7)] == bool(c.get("PredInv")):
                continue
            if op == "SelI":
                imm = c["Imm20"] | (0xFFF00000 if c["Imm20"] & 0x80000 else 0)
                U._w(h, c["Dest"], R[c["SrcA"]] if P[c["SrcPred"]] != bool(c["SrcPredInv"]) else imm)
            elif op == "Lop3R":
                r = U._lop3(R[c["SrcA"]], R[c["SrcB"]], R[c["SrcC"]], c["Imm"])
                if c["DestPred"] != 7:   # EmitLopPredWrite: F, T, Z, Nz
                    U._wp(h, c["DestPred"], (False, True, r == 0, r != 0)[c["PredicateOp"]])
                U._w(h, c["Dest"], r)
            else:
                b = R[c["SrcB"]] if op == "FsetR" else U._cb(c["CbufSlot"], c["CbufOffset"])
                r = _fcomp(c["FComp"], _absneg(R[c["SrcA"]], c["AbsA"], c["NegA"]), _absneg(b, c["AbsB"], c["NegB"]))
                r = _bop(c["Bop"], r, P[c["SrcPred"]] != bool(c["SrcPredInv"]))
                U._w(h, c["Dest"], (0x3F800000 if r else 0) if c["BVal"] else (M32 if r else 0))
    if otros:
        U._ejecuta_maxwell(otros, hilos)


def _ejecuta_sm86(d, hilos):
    nombre = d["nombre"].split(".")[0]
    g = d["guarda"]
    gn, ginv = (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")
    activos = [h for h in hilos if h["P"][gn] != ginv]
    if nombre == "VOTE":
        p, pn = _c(d, "op2.n", 7), bool(_c(d, "op2.not"))
        votos = [h["P"][p] != pn for h in activos]
        ids = {id(h) for h in activos}
        ballot = sum(1 << i for i, h in enumerate(hilos) if id(h) in ids and h["P"][p] != pn)
        res = (all(votos), any(votos), all(votos) or not any(votos))[_bits(d, 72, 2)]
        for h in activos:
            U._w(h, _c(d, "op0.n"), ballot)
            U._wp(h, _c(d, "op1.n", 7), res)
        return
    for h in activos:
        R, P = h["R"], h["P"]
        if nombre == "PLOP3":
            ent = [P[_c(d, f"op{i}.n", 7)] != bool(_c(d, f"op{i}.not")) for i in (2, 3, 4)]
            idx = ent[0] << 2 | ent[1] << 1 | ent[2]
            r0, r1 = _c(d, "op5.v") >> idx & 1, _c(d, "op6.v") >> idx & 1
            U._wp(h, _c(d, "op0.n", 7), r0)
            U._wp(h, _c(d, "op1.n", 7), r1)
        elif nombre == "FSET":
            t = _fuente(d, 2)
            if t == "reg":
                b = _absneg(R[_c(d, "op2.n")], _c(d, "op2.abs"), _c(d, "op2.neg"))
            elif t == "const":
                off = _c(d, "op2.off") | _c(d, "op2.off_reg") << 15
                b = _absneg(U._cb(_c(d, "op2.banco"), off >> 2), _c(d, "op2.abs"), _c(d, "op2.neg"))
            else:
                b = _f(_c(d, "op2.v"))
            a = _absneg(R[_c(d, "op1.n")], _c(d, "op1.abs"), _c(d, "op1.neg"))
            r = _bop(_bits(d, 74, 2), _fcomp(_bits(d, 76, 4), a, b), P[_c(d, "op3.n", 7)] != bool(_c(d, "op3.not")))
            U._w(h, _c(d, "op0.n"), 0x3F800000 if r else 0)   # FTZ: como Ryujinx (ver la cabecera)


def comprobar_ejecutando(d, palabras):
    errores = []
    for semilla in range(8):
        base = U._h(d["lo"], d["hi"], semilla)
        a = U._estado(base)
        for k, h in enumerate(a):   # floats con casos especiales en los registros que la instrucción lee
            for r in range(255):
                if (k * 31 + r * 17 + semilla) % 3 == 0:
                    h["R"][r] = _ESPECIALES[(k * 5 + r + semilla) % len(_ESPECIALES)]
        b = [{"R": h["R"][:], "P": h["P"][:]} for h in a]
        _ejecuta_sm86(d, a)
        try:
            _ejecuta_maxwell(palabras, b)
        except (ValueError, KeyError) as e:
            return [f"Maxwell: {e}"]
        for i, (x, y) in enumerate(zip(a, b)):
            for t in TEMPS:
                y["R"][t] = x["R"][t]
            if x["R"] != y["R"]:
                r = next(r for r in range(256) if x["R"][r] != y["R"][r])
                errores.append(f"hilo {i} R{r}: {x['R'][r]:#x} vs {y['R'][r]:#x}")
            if x["P"] != y["P"]:
                errores.append(f"hilo {i} P: {x['P']} vs {y['P']}")
            if errores:
                return errores[:4]
    return errores


COMPROBAR = {k: comprobar_ejecutando for k in REGLAS}

# Bits de modificadores que `probar` de sm86_baja.py no cambia (solo cambia los de operandos): sin esto, todo FSET al
# azar sale con la comparación F (siempre falso) y VOTE siempre ALL, y un error en BVal o en el modo no se ve.
MODIFICADORES = {"VOTE": (72, 73), "FSET": tuple(range(74, 81))}


def probar(n, semilla=86):
    """n instrucciones al azar de VOTE, FSET y PLOP3, cambiando operandos, guarda y modificadores; cada una se baja y
    se ejecuta contra la SM86. Devuelve (bien, {motivo sin regla: cuántas}, {error: cuántas})."""
    import json
    import random
    import sm86
    spec = json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build",
                                       "sm86_spec.json")))
    claves = [k for k, s in spec.items() if s["nombre"].split(".")[0] in REGLAS]
    rnd = random.Random(semilla)
    bien = 0
    sin, malas = {}, {}
    for _ in range(n):
        k = rnd.choice(claves)
        s = spec[k]
        lo, hi = int(k, 16) | rnd.randrange(16) << 12, int(s["hi"], 16)
        bits = [b for c in s["campos"].values() for b in c.values()]
        bits += list(MODIFICADORES.get(s["nombre"].split(".")[0], ()))
        for b in bits:
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
            clave = f"{d['nombre']}: {errores[0]}"
            malas[clave] = malas.get(clave, 0) + 1
        else:
            bien += 1
    return bien, sin, malas


if __name__ == "__main__":
    sys.modules["sm86_baja"] = B
    B._cargar_modulos()
    bien, sin, malas = probar(int(sys.argv[2]) if sys.argv[1:2] == ["probar"] and len(sys.argv) > 2 else 3000)
    print(f"{bien} iguales, {sum(sin.values())} sin regla, {sum(malas.values())} distintas")
    for k, v in sorted(malas.items(), key=lambda x: -x[1])[:8] + sorted(sin.items(), key=lambda x: -x[1])[:8]:
        print(f"  {v}: {k}")
    sys.exit(1 if malas else 0)
