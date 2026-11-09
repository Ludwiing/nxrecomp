"""Bajada SM86 -> Maxwell de la aritmética entera: acarreos de 64 bits (IADD3 con predicados de acarreo, IADD3.X,
ISETP.EX, LEA/LEA.HI con acarreo), IMAD en todas sus formas (.MOV/.IADD/.SHL son solo nombres de nvdisasm para
valores concretos de B), SHF, LOP3 y FLO con predicado de salida, BMSK, SGXT, BREV, IDP y las conversiones que la
especificación trae completas (I2I, I2F, I2FP, FRND.F64). Código propio (MIT); lo carga tools/sm86_baja.py.

    python3 tools/sm86_baja_enteros.py probar [n] [--oraculo]

prueba n instrucciones al azar de este módulo cambiando también los bits de los modificadores (el `probar` general
solo cambia los de los operandos) y comprueba cada bajada ejecutándola; con --oraculo, además, que los modificadores
que se leen de la especificación coincidan con el texto de nvdisasm.

Acarreos (diseño y contrato con quien arma el programa, tools/sm86_programa.py)
-------------------------------------------------------------------------------
En Ampere el acarreo vive en predicados (IADD3 Rd, P0, P1, A, B, C lo deja en P0/P1 e IADD3.X lo suma). En Maxwell
vive en el registro de condición CC (WriteCC en quien lo produce, .X en quien lo consume). Pero el traductor de Ryujinx
ignora CC y .X en IADD3, IMAD, LEA y SHF ("TODO: CC, X"), el XMAD.X se olvida de sumar C, y el CC que calcula IADD
con un operando negado está mal cuando el negado vale 0 (toma C = (a - b) < a, que da 0 en vez de 1). Por eso:

- El acarreo se queda en el predicado de Ampere: cada instrucción SM86 se baja sola, sin mirar a sus vecinas. No hay
  estado CC que viva entre dos instrucciones SM86, así que quien arma el programa no tiene que juntarlas ni cuidar
  el orden; solo reservar R252..R254 (TEMPS) como siempre.
- Producir un acarreo (P de IADD3/LEA): se calcula exacto con comparaciones sin signo (ISETP/ISET .U32): la suma
  a + b se pasa si (a + b) < b; a - b (= a + ~b + 1) se pasa si a >= b; a + ~b, si a > b; a + p (p predicado), si
  a == 0xFFFFFFFF y p. Con dos predicados de salida (P, Q) se guarda la cuenta c de acarreos (0..2): P = c >= 1,
  Q = c >= 2. La suma P + Q (lo único que usa el consumidor) es la del hardware; cómo reparte el hardware un único
  acarreo entre P y Q no se puede ver con nvdisasm.
- Consumir un acarreo (IADD3.X, LEA.X, IMAD.X): el predicado se vuelve 0/1 con SEL (SEL T, RZ, 1, !P) y se suma
  como un operando más; un ~x es -x - 1 y el -1 se junta en el mismo SEL cuando se puede (P ? 0 : -1).
- P -> CC solo dentro de una bajada, como borrador: ISETP.EX se baja a ISETP.X de Maxwell, que lee CF y ZF; el
  predicado anterior se pasa a CC con SEL T, RZ, bits, P y R2P.CC T, 0x5 (ZF = bit 0, CF = bit 2). CC -> P sería
  CSETP (no hace falta: los acarreos no salen nunca por CC).
- Los temporales se escriben sin guarda; los destinos reales, con la guarda de la instrucción y al final, primero los
  registros y después los predicados (así un destino que sea el predicado de la guarda no cambia la guarda de lo que
  sigue).

Lo que no se baja (SinRegla con el motivo): F2IP (no se sabe qué empaqueta), I2FP/I2F/FRND con formatos ???,
FRND con inmediato (doble de 64 bits), IMAD.WIDE con predicado de salida (acarreo de 64 bits), LEA.HI con -A y
acarreo o -B, IDP con inmediato. F2I, F2F, el FRND de 32 bits y las conversiones de 64 bits no están en
build/sm86_spec.json (el generador no encontró una parte alta con la que nvdisasm las acepte): sus reglas están
aquí y se prueban con una especificación provisional (SM86_SPEC_EXTRA=archivo.json); ver docs/pendientes.md.

Supuestos que nvdisasm no puede confirmar (solo da nombres): P de FLO = "no hay bit" (resultado 0xFFFFFFFF); P de
IMAD.HI = acarreo sin signo de hi(A*B) + C; LEA.X con ~A = ~(A << s); IDP: el primer tipo es el de A; BMSK A = inicio,
B = ancho; SGXT con ancho 0 da 0.
"""
import json
import os
import random
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import sm86  # noqa: E402
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, leer  # noqa: E402

M32 = 0xFFFFFFFF
PT_ = (7, False)    # PT
NOPT = (7, True)    # !PT
T0, T1, T2 = TEMPS  # suma, cuenta de acarreos (o acumulador), borrador


def codificar(op, **campos):
    """sm86_baja.codificar, con DestPred = PT cuando no se da: LOP, LOP3 y LEA de Maxwell escriben ese predicado
    (con 0 pisarían P0)."""
    if "DestPred" in B.CAMPOS[op]:
        campos.setdefault("DestPred", 7)
    return B.codificar(op, **campos)


# ---------- leer la instrucción SM86

def _bit(d, b):
    return (d["lo"] >> b & 1) if b < 64 else (d["hi"] >> (b - 64) & 1)


def _bits(d, b, n):
    return sum(_bit(d, b + j) << j for j in range(n))


def _op(d, i):
    """El operando i: ('reg', n) | ('imm', v) | ('const', banco, palabra)."""
    t = B._fuente(d, i)
    if t == "reg":
        return ("reg", B._c(d, f"op{i}.n"))
    if t == "imm":
        return ("imm", B._c(d, f"op{i}.v") & M32)
    if t == "const":
        banco, off = B._const(d, i)
        return ("const", banco, off)
    raise SinRegla(f"operando {i}: {t}")


def _pred(d, i):
    """(número, negado) del predicado del operando i; PT si la forma no lo tiene."""
    return (B._c(d, f"op{i}.n", 7), bool(B._c(d, f"op{i}.not")))


def _es_reg(f, n=None):
    return f[0] == "reg" and (n is None or f[1] == n)


def _cero(f):
    return f == ("reg", RZ) or f == ("imm", 0)


def _cabe(v):
    return B._cabe20(v)


# ---------- colocar operandos en Maxwell

def _a_reg(f, previas, temp):
    """El registro con el valor de f (un inmediato o una constante pasan por el temporal)."""
    if f[0] == "reg":
        return f[1]
    if f[0] == "imm":
        if f[1] == 0:
            return RZ
        previas.append(codificar("Mov32i", Dest=temp, Imm32=f[1], QuadMask=0xF))
        return temp
    previas.append(codificar("MovC", Dest=temp, CbufSlot=f[1], CbufOffset=f[2], QuadMask=0xF))
    return temp


def _flex(f, prefijo, previas, temp, imm=True, const=True):
    """(operación, campos) con f como operando B: R, I (si cabe en 20 bits) o C; si no, por un temporal."""
    if f[0] == "reg":
        return prefijo + "R", {"SrcB": f[1]}
    if f[0] == "const" and const and prefijo + "C" in B.PATRONES:
        return prefijo + "C", {"CbufSlot": f[1], "CbufOffset": f[2]}
    if f[0] == "imm" and imm and _cabe(f[1]) and prefijo + "I" in B.PATRONES:
        return prefijo + "I", {"Imm20": f[1] & 0xFFFFF}
    return prefijo + "R", {"SrcB": _a_reg(f, previas, temp)}


def _mov(dest, f, g=None):
    g = g or {}
    if f[0] == "reg":
        return codificar("MovR", Dest=dest, SrcA=f[1], QuadMask=0xF, **g)
    if f[0] == "imm":
        return codificar("Mov32i", Dest=dest, Imm32=f[1], QuadMask=0xF, **g)
    return codificar("MovC", Dest=dest, CbufSlot=f[1], CbufOffset=f[2], QuadMask=0xF, **g)


def _sel01(dest, p, si, no):
    """dest = p ? si : no, con si o no igual a 0 (una sola SEL)."""
    n, inv = p
    if si == 0:
        return codificar("SelI", Dest=dest, SrcA=RZ, Imm20=no & 0xFFFFF, SrcPred=n, SrcPredInv=inv)
    assert no == 0
    return codificar("SelI", Dest=dest, SrcA=RZ, Imm20=si & 0xFFFFF, SrcPred=n, SrcPredInv=not inv)


def _misma_guarda(g, pred):
    return g["Pred"] != 7 and pred[0] == g["Pred"]


# ---------- sumas sin acarreo de salida: una cadena de IADD3

def _cadena(dest, regs, flex, preds, k, g, previas, libres):
    """dest = suma de regs [(registro, negado)] + flex [(fuente, negado), a lo sumo una] + preds [(n, inv)] + k (mod
    2^32), con IADD3 de Maxwell; libres: temporales que se pueden usar. Devuelve las palabras."""
    p = list(previas)
    libres = list(libres)
    k &= M32
    regs = [(r, n) for r, n in regs if r != RZ]
    for i, pr in enumerate(preds):
        t = libres.pop()
        if i == 0 and k in (0, M32):   # el -1 de un ~x entra en la misma SEL: p ? 0 : -1
            p.append(_sel01(t, pr, (1 + k) & M32, k))
            k = 0
        else:
            p.append(_sel01(t, pr, 1, 0))
        regs.append((t, False))
    items = [("r", r, n) for r, n in regs]
    for f, n in flex:
        if f[0] == "imm":
            k = (k + (-f[1] if n else f[1])) & M32
        elif not _cero(f):
            items.append(("b", f, n))
    if k:
        items.append(("b", ("imm", k), False) if _cabe(k) else ("k", ("imm", k), False))
    acc = None
    while True:
        lote, resto, hay_b = [], [], False
        cupo = 3 if acc is None else 2
        for it in items:
            if it[0] == "k" or len(lote) == cupo or (it[0] == "b" and hay_b):
                resto.append(it)
                continue
            hay_b |= it[0] == "b"
            lote.append(it)
        if acc is not None:
            lote.insert(0, ("r", acc, False))
        if not lote:
            lote = [("r", RZ, False)]
        ultimo = not resto
        d = dest if ultimo else (acc if acc is not None else libres.pop())
        gg = g if ultimo else {}
        b = next((x for x in lote if x[0] == "b"), None) or (lote[1] if len(lote) > 1 else ("r", RZ, False))
        otros = [x for x in lote if x is not b]
        a = otros[0] if otros else ("r", RZ, False)
        c = otros[1] if len(otros) > 1 else ("r", RZ, False)
        if b[0] == "r":
            op, cb = "Iadd3R", {"SrcB": b[1]}
        else:
            op, cb = _flex(b[1], "Iadd3", p, T2)
        p.append(codificar(op, Dest=d, SrcA=a[1], NegA=a[2], NegB=b[2], SrcC=c[1], NegC=c[2], **cb, **gg))
        acc = d
        items = resto
        if items and items[0][0] == "k" and len(items) == 1:   # un inmediato de 32 bits que no cabe: IADD32I
            p.append(codificar("Iadd32i", Dest=dest, SrcA=acc, Imm32=items[0][1][1], **g))
            return p
        if ultimo:
            return p


# ---------- sumas exactas con cuenta de acarreos

class _Suma:
    """Suma de 32 bits en T0 con la cuenta de acarreos en un registro (T1), para dar los predicados de acarreo o
    llevarlos a la palabra alta (IMAD.WIDE). Cada paso calcula su acarreo con una comparación sin signo (ISET, que deja
    -1/0) antes o después de sumar. signo = -1: T1 guarda -cuenta (empieza en 0 sin instrucción); +1: T1 ya tiene un
    valor y se le suma la cuenta."""

    def __init__(self, p, signo=-1, contar=True):
        self.p, self.signo, self.contar = p, signo, contar
        self.s_vivo = False
        self.c_vivo = signo == 1

    def _acarreo(self, op, **campos):
        if not self.contar:
            return
        if not self.c_vivo:   # signo -1 y es el primero: ISET directo en T1
            self.p.append(codificar(op, Dest=T1, BVal=0, Signed=0, **campos))
            self.c_vivo = True
            return
        self.p.append(codificar(op, Dest=T2, BVal=0, Signed=0, **campos))
        self.p.append(codificar("Iadd3R", Dest=T1, SrcA=T1, SrcB=T2, SrcC=RZ, NegB=self.signo == 1))

    def _iset(self, comp, a, f, **mas):
        op, cb = _flex(f, "Iset", self.p, T2)
        self._acarreo(op, IComp=B.COMP[comp], SrcA=a, Bop=mas.pop("Bop", 0), SrcPred=mas.pop("SrcPred", 7),
                      SrcPredInv=mas.pop("SrcPredInv", 0), **cb)

    def ya_en_t0(self):
        self.s_vivo = True

    def mas(self, f):
        """T0 += f."""
        if _cero(f):
            return
        if not self.s_vivo:
            self.p.append(_mov(T0, f))
            self.s_vivo = True
            return
        if f[0] == "imm" and not _cabe(f[1]):
            self.p.append(codificar("Iadd32i", Dest=T0, SrcA=T0, Imm32=f[1]))
        else:
            op, cb = _flex(f, "Iadd3", self.p, T2)
            self.p.append(codificar(op, Dest=T0, SrcA=T0, SrcC=RZ, **cb))
        self._iset("LT", T0, f)   # se pasó si la suma quedó menor que lo sumado

    def mas_not(self, f):
        """T0 += ~f (f registro o constante)."""
        if f[0] == "imm" or f == ("reg", RZ):
            return self.mas(("imm", ~(f[1] if f[0] == "imm" else 0) & M32))
        op, cb = _flex(f, "Lop", self.p, T2, imm=False)
        if not self.s_vivo:
            self.p.append(codificar(op, Dest=T0, SrcA=RZ, Lop=3, NegB=1, **cb)
                          if op == "LopR" else codificar(op, Dest=T0, SrcA=RZ, LogicOp=3, NegB=1, **cb))
            self.s_vivo = True
            return
        self._iset("GT", T0, f)   # T0 + ~f se pasa si T0 > f (antes de sumar)
        if f[0] == "reg":
            self.p.append(codificar("Iadd3I", Dest=T0, SrcA=T0, Imm20=0xFFFFF, SrcC=f[1], NegC=1))
        else:
            self.p.append(codificar("Iadd3C", Dest=T0, SrcA=T0, CbufSlot=f[1], CbufOffset=f[2], NegB=1, SrcC=RZ))
            self.p.append(codificar("Iadd32i", Dest=T0, SrcA=T0, Imm32=M32))

    def not_t0(self):
        """T0 = ~T0 (el primer término ya estaba en T0)."""
        self.p.append(codificar("LopR", Dest=T0, SrcA=RZ, SrcB=T0, Lop=3, NegB=1))

    def mas_pred(self, pr):
        if pr == PT_:
            return self.mas(("imm", 1))
        if pr == NOPT:
            return
        if not self.s_vivo:
            self.p.append(_sel01(T0, pr, 1, 0))
            self.s_vivo = True
            return
        self._iset("EQ", T0, ("imm", M32), Bop=0, SrcPred=pr[0], SrcPredInv=pr[1])   # T0 == -1 y p
        self.p.append(_sel01(T2, pr, 1, 0))
        self.p.append(codificar("Iadd3R", Dest=T0, SrcA=T0, SrcB=T2, SrcC=RZ))

    def fin(self, rd, pp, pq, g):
        """Escribe Rd, P (cuenta >= 1) y Q (cuenta >= 2) con la guarda; P y Q al final."""
        if rd != RZ:
            self.p.append(codificar("MovR", Dest=rd, SrcA=T0 if self.s_vivo else RZ, QuadMask=0xF, **g))
        preds = [(x, cual) for x, cual in ((pp, 1), (pq, 2)) if x[0] != 7]
        if len(preds) == 2 and pp[0] == pq[0]:   # el mismo predicado dos veces: queda el segundo
            preds = preds[1:]
        preds.sort(key=lambda x: _misma_guarda(g, x[0]))
        for x, cual in preds:
            if not self.c_vivo:
                self.p.append(codificar("IsetpR", DestPred=x[0], DestPredInv=7, SrcA=RZ, SrcB=RZ, IComp=0, Bop=0,
                                        SrcPred=7, **g))   # sin acarreos: falso
            elif cual == 1:
                self.p.append(codificar("IsetpR", DestPred=x[0], DestPredInv=7, SrcA=T1, SrcB=RZ, IComp=B.COMP["NE"],
                                        Signed=0, Bop=0, SrcPred=7, **g))
            else:   # -cuenta < -1
                self.p.append(codificar("IsetpI", DestPred=x[0], DestPredInv=7, SrcA=T1, Imm20=0xFFFFF,
                                        IComp=B.COMP["LT"], Signed=1, Bop=0, SrcPred=7, **g))
        return self.p


def _terminos(lista, k=0):
    """Normaliza [(fuente, '+' | '-' | '~')]: -x = ~x + 1; los inmediatos con ~ se doblan.
    -> ([(fuente, '+' | '~')], k)."""
    out = []
    for f, s in lista:
        if s == "-":
            s, k = "~", k + 1
        if s == "~" and (f[0] == "imm" or f == ("reg", RZ)):
            f, s = ("imm", ~(f[1] if f[0] == "imm" else 0) & M32), "+"
        if s == "+" and _cero(f):
            continue
        out.append((f, s))
    return out, k


def _suma_exacta(rd, pp, pq, terms, preds, k, g, p, primero_en_t0=False):
    s = _Suma(p)
    if primero_en_t0:
        s.ya_en_t0()
        if terms and terms[0][1] == "~":
            s.not_t0()
        terms = terms[1:]
    for f, sgn in terms:
        (s.mas if sgn == "+" else s.mas_not)(f)
    if k:
        s.mas(("imm", k & M32))
    for pr in preds:
        s.mas_pred(pr)
    return s.fin(rd, pp, pq, g)


def _suma_sin_salida(rd, terms, preds, k, g, p, libres=(T2, T1, T0)):
    """Sin acarreo de salida: ~x = -x - 1 y todo va a la cadena de IADD3."""
    regs, flex = [], []
    for f, sgn in terms:
        neg = sgn != "+"
        if sgn == "~":
            k -= 1
        if f[0] == "reg":
            regs.append((f[1], neg))
        else:
            flex.append((f, neg))
    if len([x for x in flex if x[0][0] == "const"]) > 1:
        raise SinRegla("dos constantes")
    var = [x for x in preds if x[0] != 7]
    k += sum(1 for x in preds if x == PT_)
    return _cadena(rd, regs, flex, var, k, g, p, libres)


# ---------- IADD3

def baja_iadd3(d):
    g = _guarda(d)
    x = _bit(d, 74)
    sgn = (lambda i: "~" if B._c(d, f"op{i}.not") else "+") if x else (lambda i: "-" if B._c(d, f"op{i}.neg") else "+")
    crudos = [(_op(d, i), sgn(i)) for i in (3, 4, 5)]
    preds = [_pred(d, 6), _pred(d, 7)] if x else []
    rd, pp, pq = B._c(d, "op0.n"), _pred(d, 1), _pred(d, 2)
    p = []
    if pp[0] == 7 and pq[0] == 7:
        terms, k = _terminos(crudos)
        return _suma_sin_salida(rd, terms, preds, k, g, p)
    vivos = [(f, s) for f, s in crudos if not (s == "+" and _cero(f))]
    if not x and pq[0] == 7 and len(vivos) == 2:
        r = _iadd3_dos(rd, pp, vivos, g)
        if r:
            return r
    terms, k = _terminos(crudos)
    return _suma_exacta(rd, pp, pq, terms, preds, k, g, p)


def _iadd3_dos(rd, pp, vivos, g):
    """Lo más común: Rd = a + b o a - b con un acarreo (la parte baja de una suma o resta de 64 bits)."""
    (fa, sa), (fb, sb) = vivos
    p = []
    if sa == "+" and sb == "+":
        if rd == RZ or (_es_reg(fa, rd) and _es_reg(fb, rd)):
            return None
        otro = fb if not _es_reg(fb, rd) else fa
        regs = [(f[1], False) for f in (fa, fb) if f[0] == "reg"]
        flex = [(f, False) for f in (fa, fb) if f[0] != "reg"]
        if len(flex) > 1:
            return None
        p += _cadena(rd, regs, flex, [], 0, g, [], [T1, T0])
        op, cb = _flex(otro, "Isetp", p, T2)
        return p + [codificar(op, DestPred=pp[0], DestPredInv=7, SrcA=rd, IComp=B.COMP["LT"], Signed=0, Bop=0,
                              SrcPred=7, **cb, **g)]
    if sa == "-" and sb == "-":
        return None
    if sa == "-":
        (fa, sa), (fb, sb) = (fb, sb), (fa, sa)
    # rd = a - b; se pasa si a >= b (sin signo)
    if fa[0] == "reg":
        comp, a, bb = "GE", fa[1], fb
    elif fb[0] == "reg":
        comp, a, bb = "LE", fb[1], fa
    else:
        return None
    alias = _es_reg(fa, rd) or _es_reg(fb, rd)
    if alias and _misma_guarda(g, pp):
        return None
    op, cb = _flex(bb, "Isetp", p, T2)
    setp = codificar(op, DestPred=pp[0], DestPredInv=7, SrcA=a, IComp=B.COMP[comp], Signed=0, Bop=0, SrcPred=7,
                     **cb, **g)
    regs = [(f[1], s == "-") for f, s in ((fa, "+"), (fb, "-")) if f[0] == "reg"]
    flex = [(f, s == "-") for f, s in ((fa, "+"), (fb, "-")) if f[0] != "reg"]
    suma = _cadena(rd, regs, flex, [], 0, g, [], [T1, T0])
    return (p + [setp] + suma) if alias else (p + suma + [setp])


# ---------- ISETP.EX

def _guarda(d):
    return B._guarda(d)


# Para ISETP.X de Maxwell (como lo traduce Ryujinx) el predicado de la comparación baja va en CF/ZF:
# EQ: hi== && ZF; NE: hi!= || !ZF; LT: hi< || (hi== && !CF); LE: hi< || (hi== && (ZF || !CF));
# GT: hi> || (hi== && !ZF && CF); GE: hi> || (hi== && CF). Valor de R2P (bit 0 = ZF, bit 2 = CF) si P / si !P:
ICOMP = {"F": 0, "LT": 1, "EQ": 2, "LE": 3, "GT": 4, "NE": 5, "GE": 6, "T": 7}   # IComp de Maxwell
_EX_CC = {"EQ": (1, 0), "NE": (0, 1), "LT": (0, 4), "LE": (0, 4), "GT": (4, 0), "GE": (4, 0)}


def baja_isetp(d):
    if "EX" not in B._mods(d):
        return B.baja_setp(d, True)
    g = _guarda(d)
    partes = d["nombre"].split(".")
    comp, bop = partes[1], partes[-2]
    if comp not in ICOMP or bop not in B.BOP:
        raise SinRegla(f"ISETP.EX {comp}.{bop}")
    p = []
    if comp in _EX_CC:
        si, no = _EX_CC[comp]
        p.append(_sel01(T0, _pred(d, 5), si, no))
        p.append(codificar("R2pI", SrcA=T0, Imm20=0x5, ByteSel=0, Ccpr=1))
    op, cb = _flex(_op(d, 3), "Isetp", p, T2)
    pc = _pred(d, 4)
    return p + [codificar(op, DestPred=B._c(d, "op0.n", 7), DestPredInv=B._c(d, "op1.n", 7), SrcA=B._c(d, "op2.n"),
                          IComp=ICOMP[comp], Signed="U32" not in B._mods(d), Bop=B.BOP[bop], X=1, SrcPred=pc[0],
                          SrcPredInv=pc[1], **cb, **g)]


# ---------- LEA

def _lea_campos(d):
    """(hi, x, rd, pp, a, b, c, s, pin) de LEA/LEA.X/LEA.HI/LEA.HI.X."""
    hi, x = _bit(d, 80), _bit(d, 74)
    if "???" in d["base"]:
        raise SinRegla("LEA ???")
    if hi:
        return hi, x, B._c(d, "op0.n"), _pred(d, 1), 2, 3, _op(d, 4), B._c(d, "op5.v") & 31, _pred(d, 6) if x else None
    return hi, x, B._c(d, "op0.n"), _pred(d, 1), 2, 3, None, B._c(d, "op4.v") & 31, _pred(d, 5) if x else None


def baja_lea(d):
    """LEA Rd, P, A, B, s: Rd = (A << s) + B; LEA.HI Rd, P, A, B, C, s: Rd = hi32({C:A} << s) + B; .X suma el
    predicado de acarreo; P = acarreo de la suma."""
    g = _guarda(d)
    hi, x, rd, pp, ia, ib, fc, s, pin = _lea_campos(d)
    a = B._c(d, f"op{ia}.n")
    fb = _op(d, ib)
    nega = B._c(d, f"op{ia}.not" if x else f"op{ia}.neg")
    negb = B._c(d, f"op{ib}.not" if x else f"op{ib}.neg")
    p = []
    if not x and pp[0] == 7 and not negb:   # LEA de Maxwell tal cual
        if hi:
            c = _a_reg(fc, p, T1)
            op, cb = _flex(fb, "LeaHi", p, T2, imm=False)
            return p + [codificar(op, Dest=rd, SrcA=a, SrcC=c, NegA=nega, ImmU5=s, **cb, **g)]
        op, cb = _flex(fb, "Lea", p, T2)
        return p + [codificar(op, Dest=rd, SrcA=a, NegA=nega, ImmU5=s, **cb, **g)]
    if hi and nega and not x:
        raise SinRegla("LEA.HI con -A y acarreo o -B")   # la negación de 64 bits no se parte en ~hi + acarreo
    if hi:   # T0 = hi32({C:A} << s)
        c = _a_reg(fc, p, T1)
        p.append(codificar("ShfLI", Dest=T0, SrcA=a, SrcC=c, Imm6=s, MaxShift=0, M=0))
    else:
        p.append(codificar("ShlI", Dest=T0, SrcA=a, Imm20=s, M=0))
    sa = ("~" if x else "-") if nega else "+"
    sb = ("~" if x else "-") if negb else "+"
    terms, k = _terminos([(("reg", T0), sa), (fb, sb)])
    preds = [pin] if x else []
    if pp[0] == 7:
        return _suma_sin_salida(rd, terms, preds, k, g, p, libres=(T2, T1))
    return _suma_exacta(rd, pp, PT_, terms, preds, k, g, p, primero_en_t0=True)


# ---------- IMAD

def _imad_campos(d):
    """(clase, rd, pp, a, fb, fc, cneg, cnot, pin): clase 0x24 IMAD, 0x25 IMAD.WIDE, 0x27 IMAD.HI."""
    clase = int(d["op"], 16) & 0xFF
    x = _bit(d, 74)
    if clase == 0x24:
        return clase, B._c(d, "op0.n"), PT_, 1, 2, 3, _pred(d, 4) if x else None
    return clase, B._c(d, "op0.n"), _pred(d, 1), 2, 3, 4, _pred(d, 5) if x else None


def _imad1(dest, a, fb, fc, cneg, hilo, signed, g, p):
    """dest = A * B (bajo o alto) +/- C con una IMAD de Maxwell (los temporales que haga falta antes)."""
    campos = dict(Dest=dest, SrcA=a, Hilo=hilo, ASigned=signed, BSigned=signed, AvgMode=1 if cneg else 0, **g)
    if fc[0] == "const":
        b = _a_reg(fb, p, T2)
        p.append(codificar("ImadRc", SrcC=b, CbufSlot=fc[1], CbufOffset=fc[2], **campos))
        return p
    c = _a_reg(fc, p, T1)
    op, cb = _flex(fb, "Imad", p, T2)
    p.append(codificar(op, SrcC=c, **cb, **campos))
    return p


def baja_imad(d):
    g = _guarda(d)
    clase, rd, pp, ia, ib, ic, pin = _imad_campos(d)
    signed, x = _bit(d, 73), _bit(d, 74)
    a = B._c(d, f"op{ia}.n")
    fb, fc = _op(d, ib), _op(d, ic)
    cneg = B._c(d, f"op{ic}.neg") if not x else 0
    cnot = B._c(d, f"op{ic}.not") if x else 0
    p = []
    if clase == 0x27 and pp[0] != 7:   # IMAD.HI con acarreo: P = acarreo de hi(A*B) + C (+ P de entrada)
        _imad1(T0, a, fb, ("reg", RZ), 0, 1, signed, {}, p)
        sc = "~" if cnot else "-" if cneg else "+"
        terms, k = _terminos([(("reg", T0), "+"), (fc, sc)])
        return _suma_exacta(rd, pp, PT_, terms, [pin] if x else [], k, g, p, primero_en_t0=True)
    if pp[0] != 7:
        raise SinRegla("IMAD.WIDE con predicado de salida")
    if clase in (0x24, 0x27):
        hilo = clase == 0x27
        if not x:
            return _imad1(rd, a, fb, fc, cneg, hilo, signed, g, p)
        # A*B + C (o ~C = -C - 1) + P
        _imad1(T0, a, fb, fc, cnot, hilo, signed, {}, p)
        return _suma_sin_salida(rd, [(("reg", T0), "+")], [pin], -1 if cnot else 0, g, p, libres=(T2, T1))
    # IMAD.WIDE: {Rd+1:Rd} = A * B (64 bits, con o sin signo) + {C+1:C}
    if rd != RZ and rd >= 254:
        raise SinRegla("IMAD.WIDE en R254")
    if fc[0] == "reg":
        clo, chi = (fc, fc) if fc[1] == RZ else (fc, ("reg", fc[1] + 1))
    elif fc[0] == "const":
        clo, chi = fc, ("const", fc[1], fc[2] + 1)
    else:
        raise SinRegla("IMAD.WIDE con C inmediato")
    srcs = {a} | ({fb[1]} if fb[0] == "reg" else set())
    if not x and not cneg and _cero(clo) and _cero(chi):   # sin C: dos IMAD
        if rd == RZ:
            return []
        if rd not in srcs:
            _imad1(rd, a, fb, ("reg", RZ), 0, 0, signed, g, p)
            return _imad1(rd + 1, a, fb, ("reg", RZ), 0, 1, signed, g, p)
        if rd + 1 not in srcs:
            _imad1(rd + 1, a, fb, ("reg", RZ), 0, 1, signed, g, p)
            return _imad1(rd, a, fb, ("reg", RZ), 0, 0, signed, g, p)
    # parte alta en T1 (con la parte alta de C), parte baja en T0 y su acarreo a T1
    negado = cneg or cnot
    _imad1(T1, a, fb, chi if chi[0] != "imm" else ("reg", RZ), negado, 1, signed, {}, p)
    if negado:   # ~Chi = -Chi - 1
        p.append(codificar("Iadd32i", Dest=T1, SrcA=T1, Imm32=M32))
    _imad1(T0, a, fb, ("reg", RZ), 0, 0, signed, {}, p)
    s = _Suma(p, signo=1)
    s.ya_en_t0()
    terms, k = _terminos([(clo, "~" if negado else "+")], 1 if cneg else 0)
    for f, sg in terms:
        (s.mas if sg == "+" else s.mas_not)(f)
    if k:
        s.mas(("imm", k))
    if x:
        s.mas_pred(pin)
    if rd != RZ:
        p.append(codificar("MovR", Dest=rd, SrcA=T0, QuadMask=0xF, **g))
        p.append(codificar("MovR", Dest=rd + 1, SrcA=T1, QuadMask=0xF, **g))
    return p


# ---------- SHF

def baja_shf(d):
    """SHF.{L,R}.{S64,U64,S32,U32}[.W][.HI] Rd, A, s, C: funnel shift de {C:A}; sin .HI da la palabra baja, con .HI
    la alta. Maxwell: SHF.L da la alta y SHF.R la baja; las otras dos salen con SHL/SHR o con SHF sobre RZ."""
    g = _guarda(d)
    tipo, w, der, alto = _bits(d, 73, 2), _bit(d, 75), _bit(d, 76), _bit(d, 80)
    t64, signed = tipo in (0, 1), tipo in (0, 2)
    rd, a = B._c(d, "op0.n"), B._c(d, "op1.n")
    fs, fc = _op(d, 2), _op(d, 3)
    p = []
    ancho = 64 if t64 else 32
    if fs[0] == "imm":   # el desplazamiento se resuelve aquí
        n = fs[1] & (ancho - 1) if w else min(fs[1], ancho)
        return _shf_inm(rd, a, n, fc, t64, signed, der, alto, g, p)
    s = _a_reg(fs, p, T2)
    maxs = (3 if signed else 2) if t64 else 0
    if not der and not alto:   # palabra baja de {C:A} << s = A << s (0 desde 32)
        if t64 and w:
            return p + [codificar("ShfLR", Dest=rd, SrcA=RZ, SrcB=s, SrcC=a, MaxShift=2, M=1, **g)]
        return p + [codificar("ShlR", Dest=rd, SrcA=a, SrcB=s, M=w, **g)]
    if der and alto:   # palabra alta de {C:A} >> s = C >> s
        c = _a_reg(fc, p, T1)
        if t64 and w:   # (s & 63) puede pasar de 31: SHF.R de {signo:C}
            alta = RZ
            if signed:
                p.append(codificar("ShrI", Dest=T0, SrcA=c, Imm20=31, Signed=1, M=0))
                alta = T0
            return p + [codificar("ShfRR", Dest=rd, SrcA=c, SrcB=s, SrcC=alta, MaxShift=maxs, M=1, **g)]
        return p + [codificar("ShrR", Dest=rd, SrcA=c, SrcB=s, Signed=signed, M=w, **g)]
    c = _a_reg(fc, p, T1)
    op = "ShfLR" if not der else "ShfRR"
    if w:
        return p + [codificar(op, Dest=rd, SrcA=a, SrcB=s, SrcC=c, MaxShift=maxs, M=1, **g)]
    # sin .W el hardware satura el desplazamiento (32 o 64); Ryujinx da 0 desde ahí. Se satura antes y se pide el
    # desplazamiento largo de Maxwell, que sí da C (o A) al llegar a 32.
    # (con 64 bits Ryujinx ya da 0 desde 64, salvo SHF.R.S64, que debe dar el signo: se satura en 63)
    if not t64 or (der and signed):
        p.append(codificar("ImnmxI", Dest=T2, SrcA=s, Imm20=32 if not t64 else 63, Signed=0, SrcPred=7,
                           SrcPredInv=0))
        s = T2
    return p + [codificar(op, Dest=rd, SrcA=a, SrcB=s, SrcC=c, MaxShift=maxs or 2, M=0, **g)]


def _shf_inm(rd, a, n, fc, t64, signed, der, alto, g, p):
    if not der and not alto:
        return p + [codificar("ShlI", Dest=rd, SrcA=a, Imm20=min(n, 32), M=0, **g)]
    c = _a_reg(fc, p, T1)
    if der and alto:
        return p + [codificar("ShrI", Dest=rd, SrcA=c, Imm20=min(n, 32), Signed=signed, M=0, **g)]
    if n == 0:   # L.HI: C; R: A
        return p + [codificar("MovR", Dest=rd, SrcA=c if not der else a, QuadMask=0xF, **g)]
    if not der:   # palabra alta de {C:A} << n
        if n >= 64:
            return p + [codificar("MovR", Dest=rd, SrcA=RZ, QuadMask=0xF, **g)]
        if n >= 32:
            return p + [codificar("ShlI", Dest=rd, SrcA=a, Imm20=n - 32, M=0, **g)]
        return p + [codificar("ShfLI", Dest=rd, SrcA=a, SrcC=c, Imm6=n, MaxShift=0, M=0, **g)]
    # palabra baja de {C:A} >> n
    if n >= 32:
        if n == 32 or not t64:
            return p + [codificar("MovR", Dest=rd, SrcA=c, QuadMask=0xF, **g)]
        return p + [codificar("ShrI", Dest=rd, SrcA=c, Imm20=min(n - 32, 32), Signed=signed, M=0, **g)]
    return p + [codificar("ShfRI", Dest=rd, SrcA=a, SrcC=c, Imm6=n, MaxShift=0, M=0, **g)]


# ---------- LOP3 y FLO con predicado

def baja_lop3(d):
    """LOP3.LUT P, Rd, A, B, C, tabla, Q: Rd = tabla(A, B, C); P = (Rd != 0) OR Q (.PAND: AND Q)."""
    g = _guarda(d)
    pp, rd, a, fb, c, tabla, pq = _pred(d, 0), B._c(d, "op1.n"), B._c(d, "op2.n"), _op(d, 3), B._c(d, "op4.n"), \
        B._c(d, "op5.v") & 0xFF, _pred(d, 6)
    pand = _bit(d, 80)
    p = []
    if pp[0] == 7:
        op, cb = _flex(fb, "Lop3", p, T2)
        return p + [codificar(op, Dest=rd, SrcA=a, SrcC=c, Imm=tabla, **cb, **g)]
    if pq == (NOPT if not pand else PT_):   # Q no cambia nada: el predicado de LOP3 de Maxwell (Nz)
        b = _a_reg(fb, p, T2)
        return p + [codificar("Lop3R", Dest=rd, SrcA=a, SrcB=b, SrcC=c, Imm=tabla, DestPred=pp[0], PredicateOp=3,
                              **g)]
    res = rd if rd != RZ else T0
    op, cb = _flex(fb, "Lop3", p, T2)
    p.append(codificar(op, Dest=res, SrcA=a, SrcC=c, Imm=tabla, **cb, **(g if res == rd else {})))
    return p + [codificar("IsetpR", DestPred=pp[0], DestPredInv=7, SrcA=res, SrcB=RZ, IComp=B.COMP["NE"], Signed=0,
                          Bop=B.BOP["AND" if pand else "OR"], SrcPred=pq[0], SrcPredInv=pq[1], **g)]


def baja_flo(d):
    """FLO[.U32][.SH] Rd, P, B: P = no hay bit (Rd = 0xFFFFFFFF), con ISETP sobre el resultado (supuesto: nvdisasm no
    dice qué es P)."""
    g = _guarda(d)
    rd, pp, fb = B._c(d, "op0.n"), _pred(d, 1), _op(d, 2)
    p = []
    res = rd if (rd != RZ or pp[0] == 7) else T0
    op, cb = _flex(fb, "Flo", p, T2)
    signed, sh = _bit(d, 73), _bit(d, 74)
    if signed and sh:   # Ryujinx hace FLO.SH sin signo: FLO.S32 y luego i ^ 31 si i >= 0 (31 - i), -1 si no
        p.append(codificar(op, Dest=T1, Signed=1, Sh=0, NegB=B._c(d, "op2.not"), **cb))
        p.append(codificar("ShrI", Dest=T2, SrcA=T1, Imm20=31, Signed=1, M=0))
        p.append(codificar("Lop3I", Dest=res, SrcA=T1, Imm20=31, SrcC=T2, Imm=0xB4, **(g if res == rd else {})))
    else:
        p.append(codificar(op, Dest=res, Signed=signed, Sh=sh, NegB=B._c(d, "op2.not"), **cb,
                           **(g if res == rd else {})))
    if pp[0] != 7:
        p.append(codificar("IsetpI", DestPred=pp[0], DestPredInv=7, SrcA=res, Imm20=0xFFFFF, IComp=B.COMP["EQ"],
                           Signed=0, Bop=0, SrcPred=7, **g))
    return p


# ---------- BMSK, SGXT, BREV, IDP

def baja_bmsk(d):
    """BMSK Rd, A, B: Rd = ((1 << B) - 1) << A; sin .W A y B se saturan en 32, con .W se toman & 31."""
    g = _guarda(d)
    rd, a, fb, w = B._c(d, "op0.n"), B._c(d, "op1.n"), _op(d, 2), _bit(d, 75)
    p = []
    if fb[0] == "imm":
        n = fb[1] & 31 if w else min(fb[1], 32)
        p.append(codificar("Mov32i", Dest=T0, Imm32=(1 << n) - 1, QuadMask=0xF))
    else:
        p.append(codificar("Mov32i", Dest=T0, Imm32=M32, QuadMask=0xF))
        op, cb = _flex(fb, "Shl", p, T2)
        p.append(codificar(op, Dest=T0, SrcA=T0, M=w, **cb))
        p.append(codificar("LopR", Dest=T0, SrcA=RZ, SrcB=T0, Lop=3, NegB=1))
    return p + [codificar("ShlR", Dest=rd, SrcA=T0, SrcB=a, M=w, **g)]


def baja_sgxt(d):
    """SGXT[.U32][.W] Rd, A, B: extiende (con signo o con ceros) los B bits bajos de A; B se satura en 32 (.W: & 31);
    B = 0 da 0. Con desplazamientos: (A << (32 - B)) >> (32 - B)."""
    g = _guarda(d)
    rd, a, fb, signed, w = B._c(d, "op0.n"), B._c(d, "op1.n"), _op(d, 2), _bit(d, 73), _bit(d, 75)
    p = []
    if fb[0] == "imm":
        n = fb[1] & 31 if w else min(fb[1], 32)
        if n == 0:
            return [codificar("MovR", Dest=rd, SrcA=RZ, QuadMask=0xF, **g)]
        if n == 32:
            return [codificar("MovR", Dest=rd, SrcA=a, QuadMask=0xF, **g)]
        p.append(codificar("ShlI", Dest=T0, SrcA=a, Imm20=32 - n, M=0))
        return p + [codificar("ShrI", Dest=rd, SrcA=T0, Imm20=32 - n, Signed=signed, M=0, **g)]
    b = _a_reg(fb, p, T1)
    if w:
        p.append(codificar("LopI", Dest=T1, SrcA=b, Imm20=31, LogicOp=0))
    else:
        p.append(codificar("ImnmxI", Dest=T1, SrcA=b, Imm20=32, Signed=0, SrcPred=7, SrcPredInv=0))
    p.append(codificar("Iadd3I", Dest=T1, SrcA=T1, NegA=1, Imm20=32, SrcC=RZ))
    p.append(codificar("ShlR", Dest=T0, SrcA=a, SrcB=T1, M=0))
    return p + [codificar("ShrR", Dest=rd, SrcA=T0, SrcB=T1, Signed=signed, M=0, **g)]


def baja_brev(d):
    g = _guarda(d)
    rd, fb = B._c(d, "op0.n"), _op(d, 1)
    if fb[0] == "imm":
        return [codificar("Mov32i", Dest=rd, Imm32=int(f"{fb[1]:032b}"[::-1], 2), QuadMask=0xF, **g)]
    p = []
    b = _a_reg(fb, p, T0)
    return p + [codificar("ShrI", Dest=rd, SrcA=b, Imm20=0, Brev=1, Signed=0, M=0, **g)]


I2I_FMT = {"U8": 0, "U16": 1, "U32": 2, "S8": 4, "S16": 5, "S32": 6}


def baja_idp(d):
    """IDP.4A.{U8,S8}.{U8,S8} Rd, A, B, C: suma de los 4 productos byte a byte + C; IDP.2A.{LO,HI}.{U16,S16}.{U8,S8}:
    las dos mitades de A por los bytes 0-1 (LO) o 2-3 (HI) de B. Maxwell no tiene IDP: I2I saca cada byte o mitad y
    una IMAD acumula."""
    g = _guarda(d)
    rd, a, fb, c = B._c(d, "op0.n"), B._c(d, "op1.n"), _op(d, 2), B._c(d, "op3.n")
    sa, sb, dos, alto = _bit(d, 73), _bit(d, 74), _bit(d, 76), _bit(d, 77)
    cneg = B._c(d, "op3.neg")
    if fb[0] == "imm":
        raise SinRegla("IDP con inmediato")
    if dos:
        fa = "S16" if sa else "U16"
        partes = [(2 * i, i + (2 if alto else 0)) for i in range(2)]   # (byte de A, byte de B)
    else:
        if alto:
            raise SinRegla("IDP.4A con el bit 77")
        fa = "S8" if sa else "U8"
        partes = [(i, i) for i in range(4)]
    fbm = "S8" if sb else "U8"
    p = []
    acc, neg = c, cneg
    for j, (ba, bb) in enumerate(partes):
        p.append(codificar("I2iR", Dest=T0, SrcB=a, ISrcFmt=I2I_FMT[fa], IDstFmt=6, ByteSel=ba))
        if fb[0] == "reg":
            p.append(codificar("I2iR", Dest=T1, SrcB=fb[1], ISrcFmt=I2I_FMT[fbm], IDstFmt=6, ByteSel=bb))
        else:
            p.append(codificar("I2iC", Dest=T1, CbufSlot=fb[1], CbufOffset=fb[2], ISrcFmt=I2I_FMT[fbm], IDstFmt=6,
                               ByteSel=bb))
        ultimo = j == len(partes) - 1
        p.append(codificar("ImadR", Dest=rd if ultimo else T2, SrcA=T0, SrcB=T1, SrcC=acc, AvgMode=1 if neg else 0,
                           ASigned=1, BSigned=1, **(g if ultimo else {})))
        acc, neg = T2, 0
    return p


# ---------- conversiones

def baja_i2i(d):
    """I2I.{U8,S8,U16,S16}.S32.SAT Rd, B: B (con signo) saturado al rango del destino, con dos IMNMX con signo (el
    I2I.SAT de Ryujinx satura sin signo cuando el destino no lo tiene: -5 daría 255)."""
    g = _guarda(d)
    rd, fb = B._c(d, "op0.n"), _op(d, 1)
    lo, hi = [(0, 255), (-128, 127), (0, 65535), (-32768, 32767)][_bits(d, 76, 2)]
    if fb[0] == "imm":
        v = fb[1] - (1 << 32) if fb[1] & 0x80000000 else fb[1]
        return [codificar("Mov32i", Dest=rd, Imm32=max(lo, min(hi, v)) & M32, QuadMask=0xF, **g)]
    p = []
    b = _a_reg(fb, p, T0)
    p.append(codificar("ImnmxI", Dest=T0, SrcA=b, Imm20=lo & 0xFFFFF, Signed=1, SrcPred=7, SrcPredInv=1))   # máx
    return p + [codificar("ImnmxI", Dest=rd, SrcA=T0, Imm20=hi & 0xFFFFF, Signed=1, SrcPred=7, SrcPredInv=0, **g)]


RONDA = {0: 0, 1: 1, 2: 2, 3: 3}   # RN, RM, RP, RZ (bits 78-79 de I2F) = RoundMode de Maxwell


def _i2f_formato(d):
    """(ISrcFmt, DstFmt, RoundMode, ByteSel) de I2F (0x306/0xb06) o I2FP (0x245/0xa45)."""
    clase = int(d["op"], 16) & 0xFF
    con_signo = _bit(d, 74)
    tam = _bits(d, 84, 2)
    if clase == 0x45:   # I2FP.F32.{S32,U32}[.RZ]
        if _bits(d, 75, 3) != 2 or tam != 2 or _bits(d, 78, 2) not in (0, 3):
            raise SinRegla("I2FP ???")
        return (6 if con_signo else 2), 2, _bits(d, 78, 2), 0
    dst = _bits(d, 75, 2)   # 0x306: a F16/F32 desde 8-32 bits; 0x312: a F64, o desde 64 bits
    if (dst not in (1, 2) or tam == 3) if clase == 0x06 else (dst == 0 or (dst != 3 and tam != 3)):
        raise SinRegla("I2F ???")
    sel = _bits(d, 60, 2)
    if tam == 1:
        if sel & 2:
            raise SinRegla("I2F.16 con selector inválido")
        sel = 2 * sel   # .H1: desde el byte 2
    elif tam == 2:
        sel = 0
    elif tam == 3:
        sel = 0
    return tam + (4 if con_signo else 0), dst, _bits(d, 78, 2), sel


def baja_i2f(d):
    g = _guarda(d)
    isrc, dst, ronda, sel = _i2f_formato(d)
    rd, fb = B._c(d, "op0.n"), _op(d, 1)
    p = []
    op, cb = _flex(fb, "I2f", p, T0)
    return p + [codificar(op, Dest=rd, ISrcFmt=isrc, DstFmt=dst, RoundMode=ronda, ByteSel=sel, **cb, **g)]


FRND_RONDA = {0: 4, 1: 5, 2: 6, 3: 7}   # nada (RN), FLOOR, CEIL, TRUNC -> IntegerRound de Maxwell


def baja_frnd(d):
    """FRND (0x307: F16/F32; 0x313: F64; formato en los bits 75-76 y 84-85, redondeo en 78-79, FTZ en 80) -> F2F del
    mismo formato con redondeo a entero. 0x307 no está en la especificación."""
    g = _guarda(d)
    fmt, src = _bits(d, 75, 2), _bits(d, 84, 2)
    if fmt != src or (fmt != 3 if int(d["op"], 16) & 0xFF == 0x13 else fmt not in (1, 2)):
        raise SinRegla("FRND ???")   # 0x313 solo tiene nombre en F64; 0x307 en F16/F32 (nvdisasm no muestra el bit 74)
    fb = _op(d, 1)
    if fb[0] == "imm":
        raise SinRegla("FRND con inmediato")
    campos = dict(Dest=B._c(d, "op0.n"), DstFmt=fmt, SrcFmt=fmt, RoundMode=FRND_RONDA[_bits(d, 78, 2)], Ftz=_bit(d, 80),
                  AbsB=B._c(d, "op1.abs"), NegB=B._c(d, "op1.neg"), **g)
    if fb[0] == "reg":
        return [codificar("F2fR", SrcB=fb[1], **campos)]
    return [codificar("F2fC", CbufSlot=fb[1], CbufOffset=fb[2], **campos)]


def _medio(d, src):
    """Sh de Maxwell (mitad alta de un F16 en registro): bit 60 de la instrucción (.H1); el 61 no es válido. Con una
    constante o con F32/F64 nvdisasm no muestra selector: esos bits no cuentan."""
    if B._fuente(d, 1) != "reg" or src != 1:   # sin F16 en registro nvdisasm tampoco muestra selector
        return 0
    if _bit(d, 61):
        raise SinRegla("selector de mitad inválido")
    return _bit(d, 60)


def baja_f2i(d):
    """F2I[.FTZ][.NTZ].{U8..S64}.{F16,F32,F64}[.FLOOR/.CEIL/.TRUNC] (0x305; 0x311 con 64 bits): bit 72 con signo,
    75-76 tamaño (8/16/32/64), 77 NTZ, 78-79 redondeo, 80 FTZ, 84-85 formato de la fuente (los mismos códigos que
    Maxwell). Maxwell no tiene destino de 8 bits: F2I de 32 y IMNMX para saturar. .NTZ no tiene equivalente (Ryujinx
    no distingue NaN): se ignora."""
    g = _guarda(d)
    clase = int(d["op"], 16) & 0xFF
    signed, tam, src = _bit(d, 72), _bits(d, 75, 2), _bits(d, 84, 2)
    valido = (src in (1, 2) and tam < 3) if clase == 0x05 else ((src in (1, 2) and tam == 3) or src == 3)
    if not valido:
        raise SinRegla("F2I ???")
    rd, fb = B._c(d, "op0.n"), _op(d, 1)
    if fb[0] == "imm":
        raise SinRegla("F2I con inmediato")
    dstf = {0: 2, 1: 1, 2: 2, 3: 3}[tam] + (4 if signed else 0)
    campos = dict(IDstFmt=dstf, SrcFmt=src, RoundMode=_bits(d, 78, 2), Ftz=_bit(d, 80), Sh=_medio(d, src),
                  AbsB=B._c(d, "op1.abs"), NegB=B._c(d, "op1.neg"))
    p = []
    dest, gg = (rd, g) if tam else (T0, {})
    if fb[0] == "reg":
        p.append(codificar("F2iR", Dest=dest, SrcB=fb[1], **campos, **gg))
    else:
        p.append(codificar("F2iC", Dest=dest, CbufSlot=fb[1], CbufOffset=fb[2], **campos, **gg))
    if tam == 0:   # saturar a 8 bits
        if signed:
            p.append(codificar("ImnmxI", Dest=T0, SrcA=T0, Imm20=(-128) & 0xFFFFF, Signed=1, SrcPred=7,
                               SrcPredInv=1))
            p.append(codificar("ImnmxI", Dest=rd, SrcA=T0, Imm20=127, Signed=1, SrcPred=7, SrcPredInv=0, **g))
        else:
            p.append(codificar("ImnmxI", Dest=rd, SrcA=T0, Imm20=255, Signed=0, SrcPred=7, SrcPredInv=0, **g))
    return p


def baja_f2f(d):
    """F2F.{dst}.{src} (0x304: F16 <-> F32; 0x310: con F64): bits 75-76 destino, 84-85 fuente (F16/F32/F64 = 1/2/3,
    como Maxwell), 77 BF16 (Maxwell no lo tiene), 78-79 redondeo RN/RM/RP/RZ, 80 FTZ."""
    g = _guarda(d)
    clase = int(d["op"], 16) & 0xFF
    dst, src = _bits(d, 75, 2), _bits(d, 84, 2)
    if _bit(d, 77):
        raise SinRegla("F2F a BF16")
    if dst == 0 or src == 0 or dst == src or (clase == 0x04) == (3 in (dst, src)):
        raise SinRegla("F2F ???")
    fb = _op(d, 1)
    if fb[0] == "imm":
        raise SinRegla("F2F con inmediato")
    campos = dict(Dest=B._c(d, "op0.n"), DstFmt=dst, SrcFmt=src, RoundMode=_bits(d, 78, 2), Ftz=_bit(d, 80),
                  Sh=_medio(d, src), AbsB=B._c(d, "op1.abs"), NegB=B._c(d, "op1.neg"), Sat=0, **g)
    if fb[0] == "reg":
        return [codificar("F2fR", SrcB=fb[1], **campos)]
    return [codificar("F2fC", CbufSlot=fb[1], CbufOffset=fb[2], **campos)]


def baja_f2ip(d):
    raise SinRegla("F2IP: no se sabe cómo empaqueta (A, B y C); nvdisasm solo da el nombre")


REGLAS = {"IADD3": baja_iadd3, "ISETP": baja_isetp, "LEA": baja_lea, "IMAD": baja_imad, "SHF": baja_shf,
          "LOP3": baja_lop3, "FLO": baja_flo, "BMSK": baja_bmsk, "SGXT": baja_sgxt, "BREV": baja_brev,
          "IDP": baja_idp, "I2I": baja_i2i, "I2F": baja_i2f, "I2FP": baja_i2f, "FRND": baja_frnd,
          "F2I": baja_f2i, "F2F": baja_f2f, "F2IP": baja_f2ip}
# Todas las de aquí se comprueban con COMPROBAR (ejecutándolas o, las conversiones, por sus campos): la comprobación
# por papeles de sm86_baja.py (ROLES/FAMILIA) no sirve para secuencias de varias instrucciones.
ROLES = {}
FAMILIA = {}


# ---------- comprobación: ejecutar la instrucción SM86 y lo bajado (con la semántica de Ryujinx)

_SPEC = None


def _spec():
    global _SPEC
    if _SPEC is None:
        _SPEC = json.load(open(os.path.join(os.path.dirname(AQUI), "build", "sm86_spec.json")))
        extra = os.environ.get("SM86_SPEC_EXTRA")   # formas que aún no están en la especificación (para probar)
        if extra:
            _SPEC.update(json.load(open(extra)))
    return _SPEC


def mods_completos(d):
    """Los modificadores de todos los grupos de la especificación (decodificar() solo nombra el primero que cambia):
    se parte del nombre de cada grupo y se suman o quitan sus diferencias con el nombre base."""
    s = _spec()[d["op"]]
    base = set(s["nombre"].split(".")[1:])
    r = set(base)
    for t in s["modificadores"]:
        combo = sum(_bit(d, b) << j for j, b in enumerate(t["bits"]))
        n = t["nombres"].get(str(combo))
        if n is None:
            return None
        m = set(n.split(".")[1:])
        r |= m - base
        r -= base - m
    return r


_NOMBRES = {}


def mods_reales(d):
    """Los modificadores según nvdisasm (si está), o según la especificación."""
    k = (d["lo"], d["hi"])
    if k not in _NOMBRES:
        import sm86_oraculo as orc
        t = orc.desensamblar(*k) if orc.disponible() else None
        _NOMBRES[k] = set(sm86.partes(t)[1].split(".")[1:]) if t else mods_completos(d)
    return _NOMBRES[k]


def _regs_usados(d):
    """Los registros comunes que lee o escribe la instrucción SM86 (pares de 64 bits incluidos)."""
    r = set()
    for i, o in enumerate(sm86.partes(d["base"])[2]):
        if sm86.operando(o)["tipo"] == "reg":
            n = B._c(d, f"op{i}.n")
            r |= {n, n + 1} if n != RZ else set()
    return r


class Estado:
    """Registros, predicados, CC y bancos de constantes de un hilo."""
    BORDES = [0, 1, 2, 31, 32, 33, 63, 64, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFF, 0x100, 0xFFFF]

    def __init__(self, rnd):
        self.R = [self._valor(rnd) for _ in range(256)]
        for i in range(256):
            if rnd.random() < 0.15:
                self.R[i] = self.R[rnd.randrange(256)]
        self.R[RZ] = 0
        self.P = [rnd.random() < 0.5 for _ in range(7)] + [True]
        self.F = [rnd.random() < 0.5 for _ in range(4)]   # Z, N, C, V
        self.semilla = rnd.getrandbits(32)

    def _valor(self, rnd):
        x = rnd.random()
        if x < 0.3:
            return rnd.choice(self.BORDES)
        if x < 0.55:
            return rnd.randrange(70)
        return rnd.getrandbits(32)

    def copia(self):
        e = Estado.__new__(Estado)
        e.R, e.P, e.F, e.semilla = list(self.R), list(self.P), list(self.F), self.semilla
        return e

    def cb(self, banco, off):
        return self._valor(random.Random(hash((self.semilla, banco, off))))

    def r(self, n):
        return 0 if n == RZ else self.R[n]

    def w(self, n, v):
        if n != RZ:
            self.R[n] = v & M32

    def p(self, n, inv=False):
        return (True if n == 7 else self.P[n]) != bool(inv)

    def wp(self, n, v):
        if n != 7:
            self.P[n] = bool(v)


def _s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def _sx20(v):
    v &= 0xFFFFF
    return (v - 0x100000 if v & 0x80000 else v) & M32


def _cmp(comp, a, b, signed):
    if signed:
        a, b = _s32(a), _s32(b)
    return {0: False, 1: a < b, 2: a == b, 3: a <= b, 4: a > b, 5: a != b, 6: a >= b, 7: True}[comp]


def _bop(op, a, b):
    return [a and b, a or b, a != b][op]


def _lut(t, a, b, c):
    r = 0
    for i in range(32):
        idx = (a >> i & 1) << 2 | (b >> i & 1) << 1 | (c >> i & 1)
        r |= (t >> idx & 1) << i
    return r


def _msb(v, signed):
    v &= M32
    if signed and v & 0x80000000:
        v = ~v & M32
    return v.bit_length() - 1 if v else -1


def _brev(v):
    return int(f"{v & M32:032b}"[::-1], 2)


def _mx_b(op, c, e):
    """El operando B de una operación Maxwell según su forma."""
    if op.endswith("32i"):
        return c["Imm32"] & M32
    if op.endswith("Rc"):
        return e.r(c["SrcC"])
    if op.endswith("R"):
        return e.r(c["SrcB"])
    if op.endswith("I"):
        return _sx20(c["Imm20"])
    if op.endswith("C"):
        return e.cb(c["CbufSlot"], c["CbufOffset"] * 4)
    raise KeyError(op)


def _mx_c(op, c, e):
    if op.endswith("Rc"):
        return e.cb(c["CbufSlot"], c["CbufOffset"] * 4)
    return e.r(c.get("SrcC", RZ))


def _iext(e, comp, a, b, signed):
    """Comparación extendida (.X) de Ryujinx."""
    Z, C = e.F[0], e.F[2]
    if comp in (0, 7):
        return comp == 7
    eq = a == b
    if comp == 2:
        return eq and Z
    if comp == 5:
        return (not eq) or not Z
    sa, sb = (_s32(a), _s32(b)) if signed else (a, b)
    return {1: sa < sb or (eq and not C), 3: sa < sb or (eq and (Z or not C)),
            4: sa > sb or (eq and not Z and C), 6: sa > sb or (eq and C)}[comp]


def _shf_ryujinx(izq, maxs, a, b, cc, m):
    """EmitShf de Ryujinx (con el arreglo de TOTK para desplazar 0). Un desplazamiento de SPIR-V de 32 o más que
    no tape un select es indefinido: ValueError."""
    largo = maxs in (2, 3)
    tope = 64 if largo else 32
    if m:
        b &= tope - 1
    res = None
    if 0 < b < 32:
        res = ((cc << b) | (a >> (32 - b))) if izq else ((a >> b) | (cc << (32 - b)))
    if largo and b >= 32:
        sh = b - 32
        if sh < 32:
            res = (a << sh) if izq else ((_s32(cc) >> sh) if maxs == 3 else (cc >> sh))
    if b == 0:
        res = cc if izq else a
    if not m and b >= tope:
        res = 0
    if res is None:
        raise ValueError("SHF: desplazamiento indefinido en SPIR-V")
    return res & M32


def ejecutar_maxwell(palabras, e):
    """Ejecuta palabras Maxwell sobre el estado, como las traduce Ryujinx (solo las operaciones que usan las bajadas
    de este módulo)."""
    for w in palabras:
        op, c = leer(w)
        if not e.p(c.get("Pred", 7), c.get("PredInv", False)):
            continue
        fam = op[:-2] if op.endswith("Rc") else op.rstrip("RIC").replace("32i", "")
        if op == "Mov32i":
            e.w(c["Dest"], c["Imm32"])
        elif op == "MovR":
            e.w(c["Dest"], e.r(c["SrcA"]))
        elif op == "MovC":
            e.w(c["Dest"], e.cb(c["CbufSlot"], c["CbufOffset"] * 4))
        elif fam == "Iadd3":
            if c.get("Lrs") or c.get("Apart") or c.get("Bpart") or c.get("Cpart") or c.get("X"):
                raise ValueError("IADD3 con partes o X")
            a, b, cc = e.r(c["SrcA"]), _mx_b(op, c, e), e.r(c["SrcC"])
            r = (-a if c["NegA"] else a) + (-b if c["NegB"] else b) + (-cc if c["NegC"] else cc)
            e.w(c["Dest"], r)
        elif fam == "Iadd":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            if c.get("AvgMode") == 2:
                a = -a & M32
            if c.get("AvgMode") == 1:
                b = -b & M32
            r = a + b + (e.F[2] if c.get("X") else 0)
            if c.get("WriteCC") or c.get("X"):
                raise ValueError("IADD con CC (no lo usa este módulo)")
            e.w(c["Dest"], r)
        elif fam == "Imad":
            a, b, cc = e.r(c["SrcA"]), _mx_b(op, c, e), _mx_c(op, c, e)
            if c["AvgMode"] == 2:
                b = -b & M32
            if c["AvgMode"] == 1:
                cc = -cc & M32
            if c["Hilo"]:
                sa = _s32(a) if c["ASigned"] else a
                sb = _s32(b) if c["BSigned"] else b
                if c["ASigned"] != c["BSigned"]:   # Ryujinx: mulhi sin signo + corrección del que tiene signo
                    r = (a * b) >> 32
                    if c["ASigned"]:
                        r += b * (M32 if a & 0x80000000 else 0)
                    else:
                        r += a * (M32 if b & 0x80000000 else 0)
                else:
                    r = (sa * sb) >> 32
            else:
                r = a * b
            e.w(c["Dest"], r + cc)
        elif fam in ("Isetp", "Iset"):
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            r = _iext(e, c["IComp"], a, b, c["Signed"]) if c.get("X") else _cmp(c["IComp"], a, b, c["Signed"])
            pr = e.p(c["SrcPred"], c["SrcPredInv"])
            if fam == "Isetp":
                e.wp(c["DestPred"], _bop(c["Bop"], r, pr))
                e.wp(c["DestPredInv"], _bop(c["Bop"], not r, pr))
            else:
                v = _bop(c["Bop"], r, pr)
                e.w(c["Dest"], (0x3F800000 if c["BVal"] else M32) if v else 0)
        elif fam == "Sel":
            e.w(c["Dest"], e.r(c["SrcA"]) if e.p(c["SrcPred"], c["SrcPredInv"]) else _mx_b(op, c, e))
        elif fam == "Lop3":
            r = _lut(c["Imm"], e.r(c["SrcA"]), _mx_b(op, c, e), e.r(c["SrcC"]))
            if op == "Lop3R" and c["DestPred"] != 7:
                e.wp(c["DestPred"], [False, True, r == 0, r != 0][c["PredicateOp"]])
            e.w(c["Dest"], r)
        elif fam == "Lop":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            a, b = (~a if c["NegA"] else a) & M32, (~b if c["NegB"] else b) & M32
            lo = c.get("Lop", c.get("LogicOp"))
            r = [a & b, a | b, a ^ b, b][lo]
            if c.get("DestPred", 7) != 7:
                e.wp(c["DestPred"], [False, True, r == 0, r != 0][c["PredicateOp"]])
            e.w(c["Dest"], r)
        elif op in ("ShfLR", "ShfLI", "ShfRR", "ShfRI"):
            b = e.r(c["SrcB"]) if op.endswith("R") else c["Imm6"]
            e.w(c["Dest"], _shf_ryujinx(op.startswith("ShfL"), c["MaxShift"], e.r(c["SrcA"]), b, e.r(c["SrcC"]),
                                        c["M"]))
        elif fam == "Shl":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            if c["M"]:
                b &= 31
            e.w(c["Dest"], (a << b) if b < 32 else 0)
        elif fam == "Shr":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            if c["Brev"]:
                a = _brev(a)
            if c["M"]:
                b &= 31
            if b < 32:
                r = (_s32(a) >> b) if c["Signed"] else a >> b
            else:
                r = (_s32(a) >> 31) if c["Signed"] else 0
            e.w(c["Dest"], r)
        elif fam == "Imnmx":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            k = _s32 if c["Signed"] else (lambda v: v)
            mn, mx = (a, b) if k(a) <= k(b) else (b, a)
            e.w(c["Dest"], mn if e.p(c["SrcPred"], c["SrcPredInv"]) else mx)
        elif fam == "Flo":
            b = _mx_b(op, c, e)
            if c["NegB"]:
                b = ~b & M32
            if c["Sh"]:   # Ryujinx: FindLSB(BitfieldReverse(b)), sin mirar el signo
                r = (31 - (b.bit_length() - 1)) if b else -1
            else:
                r = _msb(b, c["Signed"])
            e.w(c["Dest"], r)
        elif op == "R2pI":
            v, mask = e.r(c["SrcA"]) >> (c["ByteSel"] * 8), _sx20(c["Imm20"])
            if not c["Ccpr"]:
                raise ValueError("R2P a predicados")
            for i in range(4):
                if mask >> i & 1:
                    e.F[i] = bool(v >> i & 1)
        elif fam == "I2i":
            b = _mx_b(op, c, e)
            src, sel = c["ISrcFmt"], c["ByteSel"]
            if src & 3 != 2:
                tam = 16 if src & 3 == 1 else 8
                v = b >> (sel * 8) & ((1 << tam) - 1)
                if src & 4 and v >> (tam - 1):
                    v -= 1 << tam
                b = v & M32
            if c["AbsB"] or c["NegB"] or c["Sat"]:
                raise ValueError("I2I con abs/neg/sat (no lo usa este módulo)")
            e.w(c["Dest"], b)
        elif fam == "Lea" or fam == "LeaHi":
            a, b = e.r(c["SrcA"]), _mx_b(op, c, e)
            s = c["ImmU5"]
            if op.startswith("LeaHi"):
                cc = e.r(c["SrcC"])
                x = ((cc << 32) | a) << s
                if c["NegA"]:
                    x = -x
                r = ((x >> 32) & M32) + b
            else:
                x = (a << s) & M32
                r = (-x if c["NegA"] else x) + b
            e.w(c["Dest"], r)
        else:
            raise ValueError(f"sin emulación: {op}")
    return e


def _v(d, i, e):
    t = B._fuente(d, i)
    if t == "reg":
        return e.r(B._c(d, f"op{i}.n"))
    if t == "imm":
        return B._c(d, f"op{i}.v") & M32
    if t == "const":
        banco, palabra = B._const(d, i)
        return e.cb(banco, palabra * 4)
    raise ValueError(t)


def _pv(d, i, e):
    n, inv = _pred(d, i)
    return e.p(n, inv)


def _cuenta_a_preds(d, e, i_p, i_q, total):
    c = total >> 32
    e.wp(_pred(d, i_p)[0], c >= 1)
    if i_q is not None:
        e.wp(_pred(d, i_q)[0], c >= 2)


def ref_sm86(d, e):
    """La instrucción SM86 ejecutada sobre e (con la semántica que se le supone; ver cada caso)."""
    if not e.p(*_guarda_tupla(d)):
        return e
    n = d["nombre"].split(".")[0]
    mods = mods_completos(d)
    if n == "IADD3":
        x = _bit(d, 74)
        tot = 0
        for i in (3, 4, 5):
            v = _v(d, i, e)
            if x:
                tot += (~v & M32) if B._c(d, f"op{i}.not") else v
            else:
                tot += ((~v & M32) + 1) if B._c(d, f"op{i}.neg") else v
        if x:
            tot += _pv(d, 6, e) + _pv(d, 7, e)
        e.w(B._c(d, "op0.n"), tot)
        _cuenta_a_preds(d, e, 1, 2, tot)
    elif n == "ISETP":
        partes = d["nombre"].split(".")
        comp, sig = ICOMP[partes[1]], "U32" not in mods
        bop = B.BOP[partes[-2] if "EX" in mods else partes[-1]]
        a, b = _v(d, 2, e), _v(d, 3, e)
        r = _cmp(comp, a, b, sig)
        if "EX" in mods and comp not in (0, 7):
            prev = _pv(d, 5, e)
            if comp == 2:
                r = a == b and prev
            elif comp == 5:
                r = a != b or prev
            else:
                estricta = {1: 1, 3: 1, 4: 4, 6: 4}[comp]
                r = _cmp(estricta, a, b, sig) or (a == b and prev)
        pc = _pv(d, 4, e)
        e.wp(B._c(d, "op0.n", 7), _bop(bop, r, pc))
        e.wp(B._c(d, "op1.n", 7), _bop(bop, not r, pc))
    elif n == "LEA":
        hi, x, rd, pp, ia, ib, fc, s, pin = _lea_campos(d)
        a, b = _v(d, ia, e), _v(d, ib, e)
        nega = B._c(d, f"op{ia}.not" if x else f"op{ia}.neg")
        negb = B._c(d, f"op{ib}.not" if x else f"op{ib}.neg")
        if hi:
            v64 = ((_v(d, 4, e) << 32) | a) << s
            if nega and not x:
                va = ((-v64) >> 32) & M32   # LEA.HI -A: la parte alta de -({C:A} << s)
                nega_suma = 0
            else:
                va = (v64 >> 32) & M32
                nega_suma = nega
        else:
            va, nega_suma = (a << s) & M32, nega
        tot = 0
        for v, ng in ((va, nega_suma), (b, negb)):
            tot += ((~v & M32) + (0 if x else 1)) if ng else v
        if x:
            tot += e.p(*pin)
        e.w(rd, tot)
        if pp[0] != 7:
            e.wp(pp[0], tot >> 32 != 0)
    elif n == "IMAD":
        clase, rd, pp, ia, ib, ic, pin = _imad_campos(d)
        signed, x = _bit(d, 73), _bit(d, 74)
        a, b = _v(d, ia, e), _v(d, ib, e)
        fc = _op(d, ic)
        cneg = B._c(d, f"op{ic}.neg") if not x else 0
        cnot = B._c(d, f"op{ic}.not") if x else 0
        sa, sb = (_s32(a), _s32(b)) if signed else (a, b)
        prod = sa * sb
        if clase == 0x25:
            if fc[0] == "reg":
                c64 = (e.r(fc[1] + 1 if fc[1] != RZ else RZ) << 32) | e.r(fc[1])
            else:
                c64 = (e.cb(fc[1], fc[2] * 4 + 4) << 32) | e.cb(fc[1], fc[2] * 4)
            if cneg:
                c64 = -c64
            if cnot:
                c64 = ~c64 & (2 ** 64 - 1)
            r = (prod + c64 + (e.p(*pin) if x else 0)) & (2 ** 64 - 1)
            if rd != RZ:
                e.w(rd, r)
                e.w(rd + 1, r >> 32)
        else:   # IMAD.HI: P = acarreo (sin signo, 32 bits) de hi(A*B) + C (+ P de entrada); -C = ~C + 1
            c = _v(d, ic, e)
            c = ((~c & M32) + 1) if cneg else (~c & M32) if cnot else c
            base = ((prod >> 32) if clase == 0x27 else prod) & M32
            tot = base + c + (e.p(*pin) if x else 0)
            e.w(rd, tot)
            if clase == 0x27:
                e.wp(pp[0], tot >> 32 != 0)
    elif n == "SHF":
        tipo, w, der, alto = _bits(d, 73, 2), _bit(d, 75), _bit(d, 76), _bit(d, 80)
        t64, signed = tipo in (0, 1), tipo in (0, 2)
        ancho = 64 if t64 else 32
        s = _v(d, 2, e)
        s = s & (ancho - 1) if w else min(s, ancho)
        v = (_v(d, 3, e) << 32) | e.r(B._c(d, "op1.n"))
        if der:
            if signed and v >> 63:
                v -= 1 << 64
            r = v >> s
        else:
            r = v << s
        r &= (1 << 64) - 1
        e.w(B._c(d, "op0.n"), r >> 32 if alto else r)
    elif n == "LOP3":
        r = _lut(B._c(d, "op5.v") & 0xFF, e.r(B._c(d, "op2.n")), _v(d, 3, e), e.r(B._c(d, "op4.n")))
        q = _pv(d, 6, e)
        pp = _pred(d, 0)[0]
        e.w(B._c(d, "op1.n"), r)
        e.wp(pp, (r != 0 and q) if _bit(d, 80) else (r != 0 or q))
    elif n == "FLO":
        b = _v(d, 2, e)
        if B._c(d, "op2.not"):
            b = ~b & M32
        i = _msb(b, _bit(d, 73))
        r = (31 - i if _bit(d, 74) else i) if i >= 0 else -1
        e.w(B._c(d, "op0.n"), r)
        e.wp(_pred(d, 1)[0], (r & M32) == M32)
    elif n == "BMSK":
        a, b, w = e.r(B._c(d, "op1.n")), _v(d, 2, e), _bit(d, 75)
        a, b = (a & 31, b & 31) if w else (min(a, 32), min(b, 32))
        e.w(B._c(d, "op0.n"), ((1 << b) - 1) << a)
    elif n == "SGXT":
        a, b, w = e.r(B._c(d, "op1.n")), _v(d, 2, e), _bit(d, 75)
        b = b & 31 if w else min(b, 32)
        if b == 0:
            r = 0
        elif b == 32:
            r = a
        else:
            r = a & ((1 << b) - 1)
            if _bit(d, 73) and r >> (b - 1):
                r -= 1 << b
        e.w(B._c(d, "op0.n"), r)
    elif n == "BREV":
        e.w(B._c(d, "op0.n"), _brev(_v(d, 1, e)))
    elif n == "IDP":
        a, b, c = e.r(B._c(d, "op1.n")), _v(d, 2, e), e.r(B._c(d, "op3.n"))
        dos = "2A" in mods
        alto = _bit(d, 77)
        sa, sb = _bit(d, 73), _bit(d, 74)

        def campo(v, i, tam, sig):
            x = v >> (i * tam) & ((1 << tam) - 1)
            return x - (1 << tam) if sig and x >> (tam - 1) else x
        if dos:
            r = sum(campo(a, i, 16, sa) * campo(b, i + (2 if alto else 0), 8, sb) for i in range(2))
        else:
            r = sum(campo(a, i, 8, sa) * campo(b, i, 8, sb) for i in range(4))
        e.w(B._c(d, "op0.n"), r + (-c if B._c(d, "op3.neg") else c))
    elif n == "I2I":
        lo, hi = [(0, 255), (-128, 127), (0, 65535), (-32768, 32767)][_bits(d, 76, 2)]
        e.w(B._c(d, "op0.n"), max(lo, min(hi, _s32(_v(d, 1, e)))))
    else:
        raise ValueError(f"sin referencia: {n}")
    return e


def _guarda_tupla(d):
    g = B._guarda(d)
    return g["Pred"], g["PredInv"]


def _comprobar_ejecutando(d, palabras, intentos=12):
    """Ejecuta la instrucción SM86 y lo bajado sobre los mismos estados al azar y compara registros y predicados.
    Una instrucción que use R252..R254 queda fuera del contrato (quien arma el programa los reserva): no se compara."""
    if _regs_usados(d) & set(TEMPS):
        return []
    errores = []
    rnd = random.Random(hash((d["lo"], d["hi"])))
    for k in range(intentos):
        e0 = Estado(rnd)
        if k < 2:   # la guarda, una vez verdadera y otra falsa
            n, inv = _guarda_tupla(d)
            if n != 7:
                e0.P[n] = (k == 0) != inv
        ref = ref_sm86(d, e0.copia())
        try:
            mx = ejecutar_maxwell(palabras, e0.copia())
        except ValueError as ex:
            return [str(ex)]
        for i in range(255):
            if i not in TEMPS and ref.R[i] != mx.R[i]:
                errores.append(f"R{i}: {mx.R[i]:#x} en vez de {ref.R[i]:#x}")
        for i in range(7):
            if ref.P[i] != mx.P[i]:
                errores.append(f"P{i}: {mx.P[i]} en vez de {ref.P[i]}")
        if errores:
            return errores[:4]
    return []


_INT = {"U8": 0, "U16": 1, "U32": 2, "U64": 3, "S8": 4, "S16": 5, "S32": 6, "S64": 7}
_FP = {"F16": 1, "F32": 2, "F64": 3}


def _comprobar_conversion(d, palabras):
    """Las conversiones no se ejecutan (redondeos de float): se leen los campos de la palabra que convierte y se
    comparan con el nombre que da nvdisasm (o la especificación): formatos, redondeo, FTZ, abs/neg y, en F2I a 8
    bits, la saturación que sigue."""
    errores = []
    n = d["nombre"].split(".")[0]
    fam = {"I2F": "I2f", "I2FP": "I2f", "FRND": "F2f", "F2F": "F2f", "F2I": "F2i"}[n]
    conv = [leer(w) for w in palabras if leer(w)[0].startswith(fam)]
    if len(conv) != 1:
        return [f"sin {fam}"]
    op, c = conv[0]
    m = mods_reales(d)
    g = B._guarda(d)
    ultimo = leer(palabras[-1])[1]
    if (ultimo.get("Pred"), bool(ultimo.get("PredInv"))) != (g["Pred"], bool(g["PredInv"])):
        errores.append("guarda")
    if ultimo.get("Dest") != B._c(d, "op0.n"):
        errores.append(f"Dest={ultimo.get('Dest')}")
    fb = _op(d, 1)
    if fb[0] == "reg" and c.get("SrcB") != fb[1]:
        errores.append(f"fuente R{c.get('SrcB')}")
    if fb[0] == "const" and (c.get("CbufSlot"), c.get("CbufOffset")) != fb[1:]:
        errores.append("constante")
    if "AbsB" in c and n != "I2F" and n != "I2FP" and (bool(c["AbsB"]), bool(c["NegB"])) != \
            (bool(B._c(d, "op1.abs")), bool(B._c(d, "op1.neg"))):
        errores.append("abs/neg")
    enteros = [t for t in m if t in _INT]
    flotantes = [t for t in m if t in _FP]
    ronda = 1 if "RM" in m else 2 if "RP" in m else 3 if "RZ" in m else 0
    if n in ("I2F", "I2FP"):
        isrc = _INT[enteros[0]] if enteros else 6
        dstf = _FP[flotantes[0]] if flotantes else 2
        if (c["ISrcFmt"], c["DstFmt"], c["RoundMode"]) != (isrc, dstf, ronda):
            errores.append(f"I2F {c['ISrcFmt']} {c['DstFmt']} {c['RoundMode']} ({sorted(m)})")
    elif n == "FRND":
        quiere = 5 if "FLOOR" in m else 6 if "CEIL" in m else 7 if "TRUNC" in m else 4
        f = _FP[flotantes[0]] if flotantes else 2
        if (c["RoundMode"], c["DstFmt"], c["SrcFmt"], bool(c["Ftz"])) != (quiere, f, f, "FTZ" in m):
            errores.append(f"F2F {c['RoundMode']} {c['DstFmt']} {c['SrcFmt']} {c['Ftz']}")
    elif n == "F2F":   # F2F.<destino>.<fuente>
        toks = [t for t in _nombre_ordenado(d) if t in _FP]
        if len(toks) != 2 or (c["DstFmt"], c["SrcFmt"]) != (_FP[toks[0]], _FP[toks[1]]):
            errores.append(f"F2F {c['DstFmt']} {c['SrcFmt']} ({toks})")
        if (c["RoundMode"], bool(c["Ftz"])) != (ronda, "FTZ" in m):
            errores.append("redondeo/FTZ")
    elif n == "F2I":
        tipo = enteros[0] if enteros else "S32"
        dstf = {"U8": 2, "S8": 6}.get(tipo, _INT[tipo])
        quiere = 1 if "FLOOR" in m else 2 if "CEIL" in m else 3 if "TRUNC" in m else 0
        src = _FP[flotantes[0]] if flotantes else 2
        if (c["IDstFmt"], c["SrcFmt"], c["RoundMode"], bool(c["Ftz"])) != (dstf, src, quiere, "FTZ" in m):
            errores.append(f"F2I {c['IDstFmt']} {c['SrcFmt']} {c['RoundMode']} {c['Ftz']} ({sorted(m)})")
        if tipo in ("U8", "S8"):
            topes = sorted(_s32(_sx20(leer(w)[1]["Imm20"])) for w in palabras if leer(w)[0] == "ImnmxI")
            if topes != ([-128, 127] if tipo == "S8" else [255]):
                errores.append(f"saturación {topes}")
    return errores


def _nombre_ordenado(d):
    """Los modificadores en orden (para F2F.<destino>.<fuente>): de nvdisasm si está."""
    import sm86_oraculo as orc
    t = orc.desensamblar(d["lo"], d["hi"]) if orc.disponible() else None
    return (sm86.partes(t)[1] if t else d["nombre"]).split(".")[1:]


COMPROBAR = {n: _comprobar_ejecutando for n in ("IADD3", "ISETP", "LEA", "IMAD", "SHF", "LOP3", "FLO", "BMSK",
                                                 "SGXT", "BREV", "IDP", "I2I")}
COMPROBAR.update({n: _comprobar_conversion for n in ("I2F", "I2FP", "FRND", "F2I", "F2F")})


# ---------- prueba propia (también los bits de los modificadores)

def probar(n, oraculo=False):
    spec = _spec()
    B._cargar_modulos()
    solo = os.environ.get("SOLO")
    ops = [k for k, s in spec.items() if s["nombre"].split(".")[0] in REGLAS and
           (not solo or s["nombre"].split(".")[0] in solo.split(","))]
    rnd = random.Random(11)
    hechas = bien = sin_regla = 0
    fallas, motivos = {}, {}
    orc = None
    if oraculo:
        import sm86_oraculo as orc
    for _ in range(n):
        k = rnd.choice(ops)
        s = spec[k]
        lo, hi = int(k, 16) | (rnd.randrange(16) << 12), int(s["hi"], 16)
        bits = [b for v in s["campos"].values() for b in v.values()]
        bits += [b for t in s["modificadores"] for b in t["bits"] if not (k.endswith("24") and b < 64)]
        for v in s.get("variantes", []):
            bits += v["encendidos"] + [b for c in v["campos"].values() for b in c.values()]
        if s["nombre"].startswith(("I2F", "IDP", "F2I", "F2F", "FRND")):
            bits += [60, 61, 72, 74, 75, 76, 77, 78, 79, 80, 84, 85]
        for b in set(bits):
            if rnd.random() < 0.5:
                lo, hi = (lo ^ (1 << b), hi) if b < 64 else (lo, hi ^ (1 << (b - 64)))
        d0 = sm86.decodificar(lo, hi, spec)
        if d0 and not d0.get("invalida") and rnd.random() < 0.5:   # predicados de salida en PT a menudo
            mapa = s["campos"] if d0["base"] == s["base"] else \
                next(v["campos"] for v in s["variantes"] if v["base"] == d0["base"])
            for i, o in enumerate(sm86.partes(d0["base"])[2]):
                if sm86.operando(o)["tipo"] == "pred" and i < 3:
                    for b in mapa.get(f"op{i}.n", {}).values():
                        lo, hi = (lo | (1 << b), hi) if b < 64 else (lo, hi | (1 << (b - 64)))
        if rnd.random() < 0.3:   # guarda PT a menudo
            lo |= 0x7000
        d = sm86.decodificar(lo, hi, spec)
        if not d or d.get("invalida"):
            continue
        if orc:
            t = orc.desensamblar(lo, hi)
            if t is None:
                continue
            nombre = sm86.partes(t)[1]
            mc = mods_completos(d)
            if not any("???" in x for x in mc) and \
                    set(nombre.split(".")[1:]) - {"MOV", "IADD", "SHL"} != mc - {"MOV", "IADD", "SHL"}:
                fallas.setdefault("modificadores vs nvdisasm", []).append(f"{nombre} / {sorted(mods_completos(d))}")
        if _regs_usados(d) & set(TEMPS):
            continue
        hechas += 1
        try:
            palabras = B.bajar(d)
        except SinRegla as ex:
            sin_regla += 1
            motivos[str(ex)] = motivos.get(str(ex), 0) + 1
            if orc and "???" not in nombre and "INVALID" not in t:   # nvdisasm le da nombre: falta de verdad
                fallas.setdefault("sin regla con nombre válido", []).append((t, str(ex)))
            continue
        errores = B.comprobar(d, palabras)
        if errores:
            fallas.setdefault(d["nombre"].split(".")[0], []).append((d["nombre"], hex(lo), hex(hi), errores))
        else:
            bien += 1
    print(f"{bien}/{hechas} bajadas iguales a la instrucción SM86; {sin_regla} sin regla")
    for m, c in sorted(motivos.items(), key=lambda x: -x[1]):
        print(f"  sin regla: {m}: {c}")
    for k, v in sorted(fallas.items(), key=lambda x: -len(x[1])):
        print(f"  {k}: {len(v)}; ej. {v[0]}")
        if k == "sin regla con nombre válido":
            por = {}
            for t, m in v:
                por.setdefault(m, []).append(t)
            for m, ts in sorted(por.items(), key=lambda x: -len(x[1])):
                print(f"      {m}: {len(ts)}; ej. {ts[0]}")
    return bien, hechas


if __name__ == "__main__":
    sys.modules["sm86_baja_enteros"] = sys.modules["__main__"]
    if sys.argv[1:2] == ["probar"]:
        probar(int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 2000, "--oraculo" in sys.argv)
    else:
        sys.exit(__doc__)
