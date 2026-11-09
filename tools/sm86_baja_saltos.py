"""Bajada SM86 -> Maxwell del control de flujo y de la memoria. Código propio (MIT). Lo carga tools/sm86_baja.py
(_cargar_modulos); quien arma el programa entero es tools/sm86_programa.py.

    SOLO=BRA,BSSY,BSYNC,BREAK,CALL,RET,BAR,MEMBAR,LDS,STS,LDL,STL,ATOMS,LDG,STG,RED,ATOMG python3 tools/sm86_baja.py probar 3000
    python3 tools/sm86_baja_saltos.py probar [n]     n instrucciones al azar de estas familias, también las formas que
                                                     no están en build/sm86_spec.json (LDG.E, STG.E, ATOMG.E... con
                                                     descriptor uniforme), comparadas con el texto de nvdisasm

Saltos: el marcador de reubicación
----------------------------------
Una instrucción SM86 puede volverse varias Maxwell, así que los destinos se arreglan al armar el programa. Una regla
que salta devuelve, en vez de un int, un `Reubicar` (subclase de int: la palabra Maxwell con Imm24 = 0) con uno de:
  .sm86  = desplazamiento del destino en bytes SM86, relativo a la instrucción SM86 siguiente (como en Ampere:
           destino = dirección + 16 + desplazamiento);
  .local = índice de la palabra destino dentro de la misma bajada (len(palabras) = la instrucción SM86 siguiente);
  .brx   = {"reg": Rn, "imm": desplazamiento SM86}: salto indirecto; quien arma el programa busca la tabla en c1 y
           la reescribe (ver sm86_programa.py).
Quien arma el programa pone Imm24 (bits 20-43, igual en BRA, BRX, SSY, PBK y CAL) = destino Maxwell - (dirección de
la palabra + 8), que es como lo lee Ryujinx (InstOp.GetAbsoluteAddress).

Convergencia de Ampere
----------------------
BSSY/BSYNC/BREAK/WARPSYNC (y BMOV con barreras, YIELD, NANOSLEEP, DEPBAR, ERRBAR, CCTL) se bajan a NOP, no a
SSY/SYNC: en Maxwell SYNC salta al destino del SSY, en Ampere BSYNC sigue con la instrucción siguiente (espera a los
hilos, nada más) y BREAK solo saca hilos de la barrera; el salto de verdad es un BRA aparte. El traductor de Ryujinx
arma SPIR-V donde cada invocación es independiente y estructura el grafo de saltos por su cuenta, así que esas barreras
no cambian el resultado; SSY/SYNC solo serían equivalentes si cada BSYNC estuviera justo antes del destino del BSSY y
además obligarían a Ryujinx a resolver la pila de SYNC. Con NOP la semántica por hilo es exacta.
BRA.DIV ("salta si el warp está dividido") se toma como nunca (NOP) y BRA.CONV como siempre: se supone el warp
convergido, que es lo único que puede representar el SPIR-V que genera Ryujinx.

Dos predicados: BRA, CALL, RET... de Ampere tienen la guarda (bits 12-15) y un predicado propio (bits 87-90);
Maxwell solo uno. Si los dos están, sale `@!guarda BRA siguiente` y la instrucción con el otro. CAL de Maxwell no tiene
guarda: va igual, detrás de saltos.

CALL.REL / RET: Ampere pasa la dirección de vuelta en un registro (MOV R7, 0x120 ... RET.REL R4) y la pila no existe;
se baja a CAL / RET de Maxwell (pila del hardware, que Ryujinx traduce a funciones), suponiendo que RET vuelve a quien
llamó, que es lo que hace el compilador. El registro de RET se ignora (su valor es una dirección SM86, sin sentido en
Maxwell). CALL.ABS no se baja.

Memoria
-------
Los campos de direcciones no están en build/sm86_spec.json (salen como "otro"), y las formas con descriptor uniforme
(LDG.E/STG.E/ATOMG.E de 0x9xx, las que genera el compilador) ni siquiera están: se leen aquí de los bits crudos, con
el diseño medido con nvdisasm (comprobado por `probar`): Rd 16-23, Ra 24-31, Rb (dato) 32-39, Rc (CAS) 64-71,
desplazamiento con signo 40-63, tamaño 73-75 (U8, S8, U16, S16, 32, 64, 128: el mismo orden que LsSize de Maxwell),
dirección de 64 bits .E. El UR del descriptor de memoria (desc[URn], el c[0x0][0x118] de CUDA) solo da la política de
caché: se ignora. LD/ST genéricos se bajan a LDG/STG (dirección global): Ryujinx no implementa LD/ST de Maxwell.
ATOM/ATOMG/ATOMS.CAS no se bajan (Ryujinx no implementa AtomCas/AtomsCas: el shader saldría mal en silencio).
LDC lo baja sm86_baja_uniformes.py y KILL sm86_baja_texturas.py.
"""
import functools
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import sm86  # noqa: E402
from sm86_baja import PATRONES, TEMPS, SinRegla, codificar, leer, ur  # noqa: E402

RZ, PT = 255, 7


class Reubicar(int):
    """Palabra Maxwell de salto con el destino por arreglar (ver arriba)."""

    def __new__(cls, palabra, sm86=None, local=None, brx=None):
        o = int.__new__(cls, palabra)
        o.sm86, o.local, o.brx = sm86, local, brx
        return o

    def corrida(self, n):
        """La misma, con el índice local corrido n lugares (cuando se le anteponen n palabras)."""
        return Reubicar(int(self), self.sm86, None if self.local is None else self.local + n, self.brx)


# ---------- bits crudos

def _bits(d, a, n=1):
    return ((d["hi"] << 64 | d["lo"]) >> a) & ((1 << n) - 1)


def _sx(v, n):
    return v - (1 << n) if v >> (n - 1) & 1 else v


def _op(d):
    return int(d["op"], 16) if isinstance(d["op"], str) else d["op"]


def _guarda(d):
    g = d["guarda"]
    return (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")


def _rel50(d):
    """Desplazamiento de BRA / BRX / CALL.REL: bits 34-81 con signo, en bytes (en BRA los bits 32-33 son
    .U/.DIV/.CONV)."""
    return _sx(_bits(d, 34, 48), 48) << 2


def _cal():
    """CAL de Maxwell (no tiene guarda: codificar() pondría Pred)."""
    return int(PATRONES["Cal"]["bits"].replace("x", "0"), 2)


def _nop():
    return codificar("Nop", Ccc=15)


# ---------- predicados: la guarda y el propio (bits 87-90)

def _condiciones(d, propio=True):
    """[(predicado, negado)] que no son PT; None si alguna es !PT (la instrucción no se ejecuta nunca)."""
    res = []
    for p, inv in [_guarda(d)] + ([(_bits(d, 87, 3), _bits(d, 90))] if propio else []):
        if p == 7:
            if inv:
                return None
            continue
        res.append((p, bool(inv)))
    return res


def _condicionada(d, hacer, predicable=True, propio=True):
    """hacer(pred, negado) -> palabras; con la guarda y el predicado propio combinados (ver arriba)."""
    cs = _condiciones(d, propio)
    if cs is None:
        return [_nop()]
    if predicable and len(cs) <= 1:
        return hacer(*(cs[0] if cs else (7, False)))
    antes = cs[:-1] if predicable else cs
    cuerpo = hacer(*cs[-1]) if predicable else hacer(7, False)
    n = len(antes) + len(cuerpo)
    saltos = [Reubicar(codificar("Bra", Ccc=15, Pred=p, PredInv=not inv), local=n) for p, inv in antes]
    return saltos + [w.corrida(len(saltos)) if isinstance(w, Reubicar) else w for w in cuerpo]


# ---------- control de flujo

def baja_bra(d):
    if _bits(d, 85, 2):
        raise SinRegla("BRA.INC/.DEC")
    if _bits(d, 91):
        raise SinRegla("BRA con UR")
    modo = _bits(d, 32, 2)   # 0, 1 .U, 2 .DIV, 3 .CONV
    if modo == 2:
        return [_nop()]
    rel = _rel50(d)
    return _condicionada(d, lambda p, inv: [Reubicar(codificar("Bra", Ccc=15, Pred=p, PredInv=inv), sm86=rel)])


def baja_brx(d):
    if _bits(d, 85, 2):
        raise SinRegla("BRX.INC/.DEC")
    r = _bits(d, 24, 8)
    info = {"reg": r, "imm": _rel50(d)}
    return _condicionada(d, lambda p, inv: [Reubicar(codificar("Brx", SrcA=r, Ccc=15, Pred=p, PredInv=inv),
                                                     brx=info)])


def baja_call(d):
    if not d["nombre"].startswith("CALL.REL") or _op(d) != 0x944 or _bits(d, 91):
        raise SinRegla("CALL.ABS, con registro o con UR")
    rel = _rel50(d)
    return _condicionada(d, lambda p, inv: [Reubicar(_cal(), sm86=rel)], predicable=False)


def baja_ret(d):
    return _condicionada(d, lambda p, inv: [codificar("Ret", Ccc=15, Pred=p, PredInv=inv)])


def baja_sin_efecto(d):
    """Convergencia, espera y caché: sin efecto en el modelo por hilo del traductor (ver arriba)."""
    return [_nop()]


def baja_bmov(d):
    """BMOV R, B (guardar una barrera de convergencia): R = 0; BMOV B, x: nada. Las barreras no existen aquí."""
    if _op(d) == 0x355:
        g, inv = _guarda(d)
        return [codificar("MovR", Dest=_bits(d, 16, 8), SrcA=RZ, QuadMask=0xF, Pred=g, PredInv=inv)]
    return [_nop()]


def baja_bar(d):
    """BAR.SYNC (cualquier forma: barrera e hilos inmediatos o en registro, .DEFER_BLOCKING, .SYNCALL) -> BAR.SYNC.
    Ryujinx solo implementa SYNC (una barrera del grupo); ARV/RED/SCAN no se bajan."""
    op = _op(d)
    if _bits(d, 77, 2):
        raise SinRegla("BAR.ARV/RED/SCAN")
    g, inv = _guarda(d)
    if op in (0xb1d, 0x91d):
        return [codificar("Bar", BarOp=0, AFixBar=1, Imm12=_bits(d, 54, 4), Pred=g, PredInv=inv)]
    return [codificar("Bar", BarOp=0, SrcA=_bits(d, 32, 8), Pred=g, PredInv=inv)]


MEMBAR = {"CTA": 0, "SM": 0, "GPU": 1, "SYS": 2, "VC": 3}


def baja_membar(d):
    ambito = [MEMBAR[x] for x in d["nombre"].split(".")[1:] if x in MEMBAR]
    if not ambito:
        raise SinRegla(d["nombre"])
    g, inv = _guarda(d)
    return [codificar("Membar", Membar=ambito[-1], Pred=g, PredInv=inv)]


# ---------- memoria

def _ur_carga(d):
    """El URn sumado a la dirección de LDS / LDL (bit 91; n en los bits 32-37), o None."""
    return _bits(d, 32, 6) if _bits(d, 91) else None


def _dir32(d, g, previas, escala=0, urn=None):
    """La dirección de 32 bits (memoria compartida o local): Ra, o un temporal = (Ra << escala) + URn."""
    ra = _bits(d, 24, 8)
    if not escala and urn is None:
        return ra
    base = ur(urn) if urn is not None else RZ
    if ra == RZ:
        if base == RZ:
            return RZ
        return base
    previas.append(codificar("IscaddR", Dest=TEMPS[2], SrcA=ra, SrcB=base, Imm5=escala, Pred=g[0], PredInv=g[1]))
    return TEMPS[2]


def _desp24(d):
    """Los 24 bits del desplazamiento (con signo, como en Maxwell). Con RZ como base nvdisasm lo muestra sin signo
    (dirección absoluta): una de 8 MB o más no tiene sentido en memoria local o compartida y no se baja."""
    if _bits(d, 24, 8) == RZ and _bits(d, 63):
        raise SinRegla("dirección absoluta de 8 MB o más")
    return _bits(d, 40, 24)


def _tam(d):
    t = _bits(d, 73, 3)
    if t == 7:
        raise SinRegla("tamaño inválido")
    return t


def _global(d, sin_ur=76):
    """Dirección global: (Ra, 64 bits). Las formas 0x9xx llevan el descriptor uniforme (se ignora); si el UR es un
    desplazamiento de la dirección (bit `sin_ur` en 0: el 76 en cargas y guardados, el 71 en los atómicos, donde el
    76 es del tamaño), no se baja."""
    op = _op(d)
    if op & 0x800 and _bits(d, 91):   # forma con UR (descriptor o desplazamiento)
        if not _bits(d, 72):
            raise SinRegla("dirección global sin .E de 64 bits")
        if not _bits(d, sin_ur):
            raise SinRegla("dirección global + UR")
        e = _bits(d, 90)
    else:
        e = _bits(d, 72)
    return _bits(d, 24, 8), e


def _pred_carga_libre(d):
    """LDG/ATOMG tienen un predicado de salida (81-83) y LDG uno de entrada (64-67): solo sin usar (PT)."""
    if _bits(d, 81, 3) != 7:
        raise SinRegla("predicado de salida")


def _generica(d):
    """LD / ST genéricos sin descriptor (0x980 / 0x385 sin el bit 91): desplazamiento de 32 bits en 32-63 (el ST
    lleva el dato en 64-71) y .E (64 bits) en el 72. None si es la forma con descriptor (la de LDG / STG)."""
    if _op(d) & 0x800 and _bits(d, 91):
        return None
    off = _sx(_bits(d, 32, 32), 32)
    if not -(1 << 23) <= off < (1 << 23) or (_bits(d, 24, 8) == RZ and off < 0):
        raise SinRegla("desplazamiento de LD/ST de más de 24 bits")
    return off & 0xFFFFFF, _bits(d, 72)


def baja_ldg(d):
    """LDG / LD (genérica: se toma como global)."""
    g, inv = _guarda(d)
    if d["nombre"].startswith("LDG"):
        _pred_carga_libre(d)
        if _bits(d, 64, 4):
            raise SinRegla("LDG con predicado de entrada")
    else:
        gen = _generica(d)
        if gen:
            return [codificar("Ldg", Dest=_bits(d, 16, 8), SrcA=_bits(d, 24, 8), Imm24=gen[0], LsSize=_tam(d),
                              E=gen[1], Pred=g, PredInv=inv)]
    ra, e = _global(d)
    return [codificar("Ldg", Dest=_bits(d, 16, 8), SrcA=ra, Imm24=_desp24(d), LsSize=_tam(d), E=e, Pred=g,
                      PredInv=inv)]


def baja_stg(d):
    """STG / ST (genérica: se toma como global)."""
    g, inv = _guarda(d)
    if d["nombre"].startswith("ST."):
        gen = _generica(d)
        if gen:
            return [codificar("Stg", Dest=_bits(d, 64, 8), SrcA=_bits(d, 24, 8), Imm24=gen[0], LsSize=_tam(d),
                              E=gen[1], Pred=g, PredInv=inv)]
    ra, e = _global(d)
    return [codificar("Stg", Dest=_bits(d, 32, 8), SrcA=ra, Imm24=_desp24(d), LsSize=_tam(d), E=e, Pred=g,
                      PredInv=inv)]


def baja_lds(d):
    g = _guarda(d)
    previas = []
    a = _dir32(d, g, previas, _bits(d, 78, 2) and [0, 2, 3, 4][_bits(d, 78, 2)], _ur_carga(d))
    return previas + [codificar("Lds", Dest=_bits(d, 16, 8), SrcA=a, Imm24=_desp24(d), LsSize=_tam(d),
                                Pred=g[0], PredInv=g[1])]


def baja_sts(d):
    if _op(d) & 0x800 or _bits(d, 91):
        raise SinRegla("STS + UR")
    g = _guarda(d)
    previas = []
    a = _dir32(d, g, previas, [0, 2, 3, 4][_bits(d, 78, 2)])
    return previas + [codificar("Sts", Dest=_bits(d, 32, 8), SrcA=a, Imm24=_desp24(d), LsSize=_tam(d),
                                Pred=g[0], PredInv=g[1])]


def baja_ldl(d):
    g = _guarda(d)
    previas = []
    a = _dir32(d, g, previas, 0, _ur_carga(d))
    return previas + [codificar("Ldl", Dest=_bits(d, 16, 8), SrcA=a, Imm24=_desp24(d), LsSize=_tam(d),
                                Pred=g[0], PredInv=g[1])]


def baja_stl(d):
    if _op(d) & 0x800 or _bits(d, 91):
        raise SinRegla("STL + UR")
    g = _guarda(d)
    return [codificar("Stl", Dest=_bits(d, 32, 8), SrcA=_bits(d, 24, 8), Imm24=_desp24(d), LsSize=_tam(d),
                      Pred=g[0], PredInv=g[1])]


def _cabe20(v):
    return -(1 << 19) <= v < (1 << 19)


def _atom_op(d, bits=4):
    op = _bits(d, 87, bits)
    if op > 8:
        raise SinRegla(f"operación atómica {op}")
    return op


def baja_atomg(d):
    """ATOMG / ATOM (genérica: global) -> ATOM de Maxwell (sin CAS: Ryujinx no implementa AtomCas)."""
    if _op(d) in (0x3a9, 0x38b, 0x9a9, 0x98b):
        raise SinRegla("ATOM.CAS: Ryujinx no implementa AtomCas")
    _pred_carga_libre(d)
    tam = _bits(d, 73, 4)
    if tam > 5:
        raise SinRegla(f"tamaño atómico {tam}")
    ra, e = _global(d, 71)
    off = _sx(_desp24(d), 24)
    if not _cabe20(off):
        raise SinRegla("desplazamiento de más de 20 bits")
    g, inv = _guarda(d)
    return [codificar("Atom", Dest=_bits(d, 16, 8), SrcA=ra, SrcB=_bits(d, 32, 8), Imm20=off & 0xFFFFF, Size=tam,
                      Op=_atom_op(d), E=e, Pred=g, PredInv=inv)]


def baja_red(d):
    if not (_op(d) & 0x800):
        raise SinRegla("RED sin descriptor")
    tam = _bits(d, 73, 4)
    if tam > 5:
        raise SinRegla(f"tamaño atómico {tam}")
    if not _bits(d, 91):   # RED de 0x98e: bit 91 = dirección .64 con el descriptor; sin él, 32 bits
        e = 0
        if not _bits(d, 72):
            raise SinRegla("RED sin .E")
        ra = _bits(d, 24, 8)
    else:
        ra, e = _global(d, 71)
    off = _sx(_desp24(d), 24)
    if not _cabe20(off):
        raise SinRegla("desplazamiento de más de 20 bits")
    op = _bits(d, 87, 3)
    g, inv = _guarda(d)
    return [codificar("Red", SrcB=_bits(d, 32, 8), SrcA=ra, Imm20=off & 0xFFFFF, RedSize=tam, RedOp=op, E=e, Pred=g,
                      PredInv=inv)]


def baja_atoms(d):
    if _op(d) & 0x800 or _bits(d, 91):
        raise SinRegla("ATOMS + UR")
    if _op(d) == 0x38d:
        raise SinRegla("ATOMS.CAS: Ryujinx no implementa AtomsCas")
    tam = _bits(d, 73, 2)
    if tam == 3:
        raise SinRegla("tamaño inválido")
    off = _sx(_desp24(d), 24)
    if off & 3 or not -(1 << 21) <= off >> 2 < (1 << 21):
        raise SinRegla("desplazamiento de ATOMS no cabe en Imm22 (palabras)")
    g = _guarda(d)
    previas = []
    a = _dir32(d, g, previas, [0, 2, 3, 4][_bits(d, 78, 2)])
    return previas + [codificar("Atoms", Dest=_bits(d, 16, 8), SrcA=a, SrcB=_bits(d, 32, 8),
                                Imm22=(off >> 2) & 0x3FFFFF, AtomsSize=tam, AtomOp=_atom_op(d), Pred=g[0],
                                PredInv=g[1])]


REGLAS = {"BRA": baja_bra, "BRX": baja_brx, "CALL": baja_call, "RET": baja_ret,
          "BSSY": baja_sin_efecto, "BSYNC": baja_sin_efecto, "BREAK": baja_sin_efecto, "WARPSYNC": baja_sin_efecto,
          "YIELD": baja_sin_efecto, "NANOSLEEP": baja_sin_efecto, "DEPBAR": baja_sin_efecto,
          "ERRBAR": baja_sin_efecto, "CCTL": baja_sin_efecto, "BMOV": baja_bmov, "BAR": baja_bar,
          "MEMBAR": baja_membar,
          "LDG": baja_ldg, "LD": baja_ldg, "STG": baja_stg, "ST": baja_stg, "LDS": baja_lds, "STS": baja_sts,
          "LDL": baja_ldl, "STL": baja_stl, "ATOMG": baja_atomg, "ATOM": baja_atomg, "RED": baja_red,
          "ATOMS": baja_atoms}
ROLES = {}     # todo lo de aquí se comprueba con COMPROBAR (abajo)
FAMILIA = {"BRA": "Bra", "BRX": "Brx", "CALL": "Cal", "RET": "Ret", "BAR": "Bar", "MEMBAR": "Membar",
           "LDG": "Ldg", "LD": "Ldg", "STG": "Stg", "ST": "Stg", "LDS": "Lds", "STS": "Sts", "LDL": "Ldl", "STL": "Stl",
           "ATOMG": "Atom", "ATOM": "Atom", "RED": "Red", "ATOMS": "Atoms"}
for _k in ("BSSY", "BSYNC", "BREAK", "WARPSYNC", "YIELD", "NANOSLEEP", "DEPBAR", "ERRBAR", "CCTL"):
    FAMILIA[_k] = "Nop"

# Formas que no están en build/sm86_spec.json y que el compilador sí genera (con descriptor uniforme): para que
# sm86_programa.py las pueda leer (crudo()).
CRUDAS = {0x981: "LDG", 0x986: "STG", 0x985: "ST", 0x9a8: "ATOMG", 0x9a9: "ATOMG", 0x98a: "ATOM", 0x98b: "ATOM",
          0x987: "STL", 0x988: "STS", 0x98c: "ATOMS"}


def crudo(lo, hi):
    """Una instrucción de CRUDAS como la daría sm86.decodificar (sin campos: las reglas leen los bits)."""
    op = lo & 0xFFF
    if op not in CRUDAS:
        return None
    g = lo >> 12 & 0xF
    return {"op": f"{op:#05x}", "nombre": CRUDAS[op], "guarda": ("!" if g & 8 else "") + ("PT" if g & 7 == 7 else
            f"P{g & 7}"), "campos": {}, "base": CRUDAS[op], "lo": lo, "hi": hi}


# ---------- comprobar: contra el texto de nvdisasm (independiente de los bits que leen las reglas)

@functools.lru_cache(maxsize=None)
def _texto(lo, hi):
    import sm86_oraculo
    if not sm86_oraculo.disponible():
        return None
    return sm86_oraculo.desensamblar(lo, hi)


def _num(t):
    t = t.replace("+-", "-")
    return int(t, 16) if not t.startswith("-") else -int(t[1:], 16)


def _memoria(t):
    """Un operando de memoria de nvdisasm: {'reg', 'ancho' (.64/.U32/''), 'escala', 'ur', 'off'}."""
    t = re.sub(r"^desc\[UR\d+\]", "", t.strip())
    m = re.fullmatch(r"\[(RZ|R\d+)?(\.64|\.U32)?(\.X4|\.X8|\.X16)?(?:\+(UR\d+|URZ))?(?:\+?(-?0x[0-9a-f]+|\+-0x[0-9a-f]+))?\]",
                     t.replace("+-", "+-"))
    if not m:
        m = re.fullmatch(r"\[(?:(RZ|R\d+)(\.64|\.U32)?(\.X4|\.X8|\.X16)?)?(?:\+?(UR\d+))?(?:\+?(-?0x[0-9a-f]+))?\]",
                         t.replace("+-", "-"))
    if not m:
        return None
    reg = 255 if m.group(1) in (None, "RZ") else int(m.group(1)[1:])
    off = m.group(5)
    return {"reg": reg, "ancho": m.group(2) or "", "escala": {"": 0, ".X4": 2, ".X8": 3, ".X16": 4}[m.group(3) or ""],
            "ur": None if not m.group(4) or m.group(4) == "URZ" else int(m.group(4)[2:]),
            "off": _num(off.replace("+-", "-")) if off else 0}


def _reg(t):
    t = t.strip()
    return 255 if t == "RZ" else int(t[1:]) if re.fullmatch(r"R\d+", t) else None


TAM = {"U8": 0, "S8": 1, "U16": 2, "S16": 3, "64": 5, "128": 6}
TAM_ATOM = {"S32": 1, "64": 2, "F32": 3, "F16x2": 4, "S64": 5}
OPS_ATOM = {"ADD": 0, "MIN": 1, "MAX": 2, "INC": 3, "DEC": 4, "AND": 5, "OR": 6, "XOR": 7, "EXCH": 8}


def _predicados_maxwell(palabras):
    """Lo que hace falta para ejecutar la última palabra: [(pred, negado)] de los saltos locales que la saltean (como
    condición de paso: el salto `@!P BRA siguiente` pide P) y su propia guarda."""
    conds = []
    for w in palabras[:-1]:
        op, c = leer(w)
        if op == "Bra" and isinstance(w, Reubicar) and w.local == len(palabras):
            conds.append((c["Pred"], not c["PredInv"]))
    op, c = leer(palabras[-1])
    if c.get("Pred", 7) != 7:
        conds.append((c["Pred"], bool(c["PredInv"])))
    return sorted(conds)


def _condiciones_texto(texto, nombre_base):
    """Las condiciones (guarda y predicado propio) según el texto de nvdisasm."""
    pred, nombre, ops = sm86.partes(texto)
    conds = []
    if pred:
        conds.append((7 if pred.endswith("PT") else int(pred[-1]), pred.startswith("@!")))
    if ops and re.fullmatch(r"!?(P\d|PT)", ops[0]):
        p = ops[0]
        conds.append((7 if p.endswith("PT") else int(p[-1]), p.startswith("!")))
    if any(p == 7 and inv for p, inv in conds):
        return None
    return sorted(c for c in conds if c[0] != 7)


def comprobar(d, palabras):
    """Diferencias entre la bajada y el texto de nvdisasm (si no está CUDA: solo la operación y la guarda)."""
    try:
        return _comprobar(d, palabras)
    except (ValueError, KeyError, IndexError, StopIteration, TypeError, AttributeError) as e:
        return [f"no pude comparar con el texto ({type(e).__name__}: {e})"]


def _comprobar(d, palabras):
    nombre = d["nombre"].split(".")[0]
    texto = _texto(d["lo"], d["hi"])
    principal = palabras[-1]
    op, c = leer(principal)
    fam = FAMILIA.get(nombre)
    errores = []
    if texto is None:   # sin oráculo
        if fam and fam != "Nop" and not op.startswith(fam) and op != "Nop":
            errores.append(f"operación {op}")
        return errores
    pred, nom_txt, ops = sm86.partes(texto)
    mods = set(nom_txt.split(".")[1:])
    conds = _condiciones_texto(texto, nombre)
    if conds is None or (nombre == "BRA" and "DIV" in mods):
        def nunca(w):
            o2, c2 = leer(w)
            return o2 == "Nop" or (c2.get("Pred") == 7 and c2.get("PredInv"))
        return [] if all(nunca(w) for w in palabras) else [f"no se ejecuta nunca y salió {op}"]
    if nombre == "BMOV":   # BMOV R, B -> R = 0; el resto, nada
        quiere = ("MovR", _reg(ops[0]), RZ) if _reg(ops[0]) is not None else ("Nop", None, None)
        return [] if (op, c.get("Dest"), c.get("SrcA")) == quiere else [f"BMOV {op} {c}"]
    if fam == "Nop":
        return [] if op == "Nop" else [f"operación {op}"]
    if fam is None:
        return [f"sin familia para {nombre}"]
    if not op.startswith(fam):
        return [f"operación {op} (texto: {texto})"]
    if nombre != "CALL" and _predicados_maxwell(palabras) != conds:
        errores.append(f"condiciones {_predicados_maxwell(palabras)} vs {conds}")
    if nombre == "CALL" and _predicados_maxwell(palabras) != conds:
        errores.append(f"condiciones de CAL {_predicados_maxwell(palabras)} vs {conds}")
    if nombre in ("BRA", "CALL"):
        destino = _num(ops[-1].split()[0])   # nvdisasm lo muestra absoluto (la instrucción está en 0)
        if not isinstance(principal, Reubicar) or principal.sm86 is None or principal.sm86 + 0x10 != destino:
            errores.append(f"destino {getattr(principal, 'sm86', None)} vs {destino - 0x10}")
    if nombre == "BRX":
        m = re.fullmatch(r"(R\d+|RZ)(?:\s+(-?0x[0-9a-f]+))?", ops[-1].split(" (")[0].strip())
        if not m or c["SrcA"] != _reg(m.group(1)) or principal.brx["imm"] != _num(m.group(2) or "0x0"):
            errores.append(f"BRX {ops[-1]} vs R{c['SrcA']} {principal.brx}")
    if nombre == "BAR":
        if c["BarOp"] != 0:
            errores.append("BarOp")
    if nombre == "MEMBAR":
        quiere = [MEMBAR[x] for x in mods if x in MEMBAR]
        if [c["Membar"]] != quiere[-1:]:
            errores.append(f"MEMBAR {c['Membar']} vs {quiere}")
    if fam in ("Ldg", "Stg", "Lds", "Sts", "Ldl", "Stl", "Atom", "Red", "Atoms"):
        errores += _comprobar_memoria(nombre, fam, mods, ops, palabras, c)
    return errores


def _comprobar_memoria(nombre, fam, mods, ops, palabras, c):
    errores = []
    mem_i = next(i for i, o in enumerate(ops) if "[" in o)
    m = _memoria(ops[mem_i])
    if m is None:
        return [f"no entiendo {ops[mem_i]}"]
    regs = [_reg(o) for o in ops if _reg(o) is not None]
    # la dirección: directa, o un temporal = (Ra << escala) + UR
    if c["SrcA"] in TEMPS:
        _, c2 = leer(palabras[-2])
        if (c2["SrcA"], c2["Imm5"], c2["SrcB"]) != (m["reg"], m["escala"], ur(m["ur"]) if m["ur"] is not None else RZ):
            errores.append(f"dirección calculada {c2} vs {m}")
    else:
        quiere = m["reg"] if not m["escala"] or m["reg"] == RZ else None
        if m["ur"] is not None:
            quiere = ur(m["ur"]) if m["reg"] == RZ and not m["escala"] else None
        if c["SrcA"] != quiere:
            errores.append(f"SrcA={c['SrcA']} vs {m}")
    off = c.get("Imm24", c.get("Imm20"))
    if fam == "Atoms":
        off = _sx(c["Imm22"], 22) * 4
    elif "Imm24" in c:
        off = _sx(c["Imm24"], 24)
    else:
        off = _sx(c["Imm20"], 20)
    if off != m["off"]:
        errores.append(f"desplazamiento {off} vs {m['off']}")
    if fam in ("Ldg", "Stg", "Atom", "Red") and bool(c.get("E")) != (m["ancho"] == ".64" or (fam != "Red" and "E" in mods and m["ancho"] != ".U32")):
        errores.append(f"E={c.get('E')} vs {m['ancho']} {sorted(mods)}")
    if fam in ("Ldg", "Stg", "Lds", "Sts", "Ldl", "Stl"):
        quiere = next((TAM[x] for x in mods if x in TAM), 4)
        if c["LsSize"] != quiere:
            errores.append(f"tamaño {c['LsSize']} vs {quiere}")
        dato = regs[0] if fam in ("Ldg", "Lds", "Ldl") else regs[-1]
        if fam in ("Ldg", "Lds", "Ldl") and len(ops) and _reg(ops[0]) is None:   # LDG P0, Rd, [...]
            dato = _reg(ops[1])
        if c["Dest"] != dato:
            errores.append(f"dato R{c['Dest']} vs R{dato}")
    else:
        quiere_op = next((OPS_ATOM[x] for x in mods if x in OPS_ATOM), None)
        tam = next((TAM_ATOM[x] for x in mods if x in TAM_ATOM), 0)
        if fam == "Atoms":
            tam = {"S32": 1, "64": 2}.get(next((x for x in mods if x in ("S32", "64")), ""), 0)
            if (c["AtomOp"], c["AtomsSize"]) != (quiere_op, tam):
                errores.append(f"ATOMS {c['AtomOp']}/{c['AtomsSize']} vs {quiere_op}/{tam}")
        elif fam == "Atom":
            if (c["Op"], c["Size"]) != (quiere_op, tam):
                errores.append(f"ATOM {c['Op']}/{c['Size']} vs {quiere_op}/{tam}")
        else:
            if (c["RedOp"], c["RedSize"]) != (quiere_op, tam):
                errores.append(f"RED {c['RedOp']}/{c['RedSize']} vs {quiere_op}/{tam}")
        rs = [_reg(o) for o in ops[mem_i + 1:]]
        if c["SrcB"] != rs[0]:
            errores.append(f"operando R{c['SrcB']} vs {rs}")
        if fam != "Red":
            dest = [_reg(o) for o in ops[:mem_i] if _reg(o) is not None]
            if c["Dest"] != dest[-1]:
                errores.append(f"destino R{c['Dest']} vs {dest}")
    return errores


COMPROBAR = {k: comprobar for k in REGLAS}


# ---------- probar (también las formas que no están en la spec)

# (código, parte alta típica, bits que se cambian al azar): la parte alta es la que deja el compilador
MUESTRAS = [
    (0x947, 0x000fea0003800000, list(range(12, 16)) + list(range(32, 82)) + [85, 86, 87, 88, 89, 90]),
    (0x949, 0x000fea000383ffff, list(range(12, 16)) + list(range(24, 32)) + list(range(34, 64)) + [87, 88, 89, 90]),
    (0x944, 0x000fea0003c00000, list(range(12, 16)) + list(range(34, 82)) + [86, 87, 88, 89, 90]),
    (0x950, 0x000fea0003c3ffff, list(range(12, 16)) + list(range(24, 32)) + [85, 86, 87, 88, 89, 90]),
    (0x945, 0x000fe20003800000, list(range(12, 16)) + list(range(16, 20)) + list(range(34, 64)) + [87, 88, 89, 90]),
    (0x941, 0x000fea0003800000, list(range(12, 20)) + [87, 88, 89, 90]),
    (0x942, 0x000fea0003800000, list(range(12, 20)) + [87, 88, 89, 90]),
    (0x948, 0x000fea0003800000, list(range(12, 16)) + list(range(32, 64)) + [87, 88, 89, 90]),
    (0xb1d, 0x000fec0000010000, list(range(12, 16)) + list(range(54, 58)) + [79, 80]),
    (0x91d, 0x000fec0000010000, list(range(12, 16)) + list(range(32, 40)) + list(range(54, 58)) + [80]),
    (0x992, 0x000fec0000000000, list(range(12, 16)) + list(range(76, 81))),
    (0x981, 0x000ea2000c1e1900, list(range(12, 32)) + list(range(40, 64)) + [73, 74, 75, 77, 78, 79, 84, 85, 86, 90]),
    (0x381, 0x0000000000001e0900, list(range(12, 32)) + list(range(40, 64)) + [72, 73, 74, 75, 84, 85, 86]),
    (0x986, 0x000fe8000c101904, list(range(12, 16)) + list(range(24, 64)) + [73, 74, 75, 77, 78, 84, 85, 86, 90]),
    (0x386, 0x000fe80000000900, list(range(12, 16)) + list(range(24, 64)) + [72, 73, 74, 75, 84, 85, 86]),
    (0x984, 0x000e240000000800, list(range(12, 32)) + list(range(32, 38)) + list(range(40, 64)) + [73, 74, 75, 78, 79, 91]),
    (0x388, 0x004fe80000004800, list(range(12, 16)) + list(range(24, 64)) + [73, 74, 75, 78, 79]),
    (0x983, 0x000ee20000100800, list(range(12, 32)) + list(range(32, 38)) + list(range(40, 64)) + [73, 74, 75, 84, 85, 86, 91]),
    (0x387, 0x000fe80000100800, list(range(12, 16)) + list(range(24, 64)) + [73, 74, 75, 84, 85, 86]),
    (0x38c, 0x0003e80000000000, list(range(12, 64)) + [73, 74, 78, 79, 87, 88, 89, 90]),
    (0x3a8, 0x000ea800001ee100, list(range(12, 64)) + [72, 73, 74, 75, 76, 87, 88, 89, 90]),
    (0x98e, 0x000fe8000c10e184, list(range(12, 16)) + list(range(24, 64)) + [73, 74, 75, 76, 87, 88, 89]),
    (0x980, 0x000ea20000001900, list(range(12, 64)) + [72, 73, 74, 75, 84, 85, 86]),
    (0x385, 0x000fe80000000900, list(range(12, 16)) + list(range(24, 72)) + [72, 73, 74, 75, 84, 85, 86]),
    (0x985, 0x000fe8000c101904, list(range(12, 16)) + list(range(24, 64)) + [73, 74, 75, 84, 85, 86, 90]),
    (0x38a, 0x000ea800001ee100, list(range(12, 64)) + [72, 73, 74, 75, 76, 87, 88, 89, 90]),
]


def probar(n, semilla=11):
    import json
    import random
    import sm86_baja
    sm86_baja._cargar_modulos()
    spec = json.load(open(os.path.join(os.path.dirname(AQUI), "build", "sm86_spec.json")))
    rnd = random.Random(semilla)
    hechas = bien = sin_regla = invalidas = 0
    fallas, motivos = {}, {}
    candidatas = []
    for _ in range(n):
        op, hi, bits = rnd.choice(MUESTRAS)
        lo = op | 0x7000
        for b in bits:
            if rnd.random() < 0.3:
                if b < 64:
                    lo ^= 1 << b
                else:
                    hi ^= 1 << (b - 64)
        candidatas.append((lo, hi))
    import concurrent.futures as cf
    with cf.ThreadPoolExecutor(16) as ex:
        textos = list(ex.map(lambda c: _texto(*c), candidatas))
    for (lo, hi_), texto in zip(candidatas, textos):
        if not texto or "INVALID" in texto or "???" in texto:
            invalidas += 1
            continue
        d = sm86.decodificar(lo, hi_, spec) or crudo(lo, hi_)
        if not d or d.get("invalida"):
            invalidas += 1
            continue
        hechas += 1
        try:
            palabras = sm86_baja.bajar(d)
        except SinRegla as e:
            sin_regla += 1
            motivos.setdefault(f"{d['nombre'].split('.')[0]}: {e}", []).append(texto)
            continue
        errores = comprobar(d, palabras)
        if errores:
            fallas.setdefault(d["nombre"].split(".")[0], []).append((texto, errores))
        else:
            bien += 1
    print(f"{bien}/{hechas} bajadas iguales al texto de nvdisasm; {sin_regla} sin regla (a propósito); "
          f"{invalidas} combinaciones inválidas descartadas")
    for k, v in sorted(motivos.items(), key=lambda x: -len(x[1]))[:12]:
        print(f"  sin regla {k}: {len(v)}; ej. {v[0]}")
    for k, v in sorted(fallas.items(), key=lambda x: -len(x[1]))[:12]:
        print(f"  MAL {k}: {len(v)}; ej. {v[0]}")
    return bien, hechas


if __name__ == "__main__":
    if sys.argv[1:2] == ["probar"]:
        sys.modules.setdefault("sm86_baja_saltos", sys.modules["__main__"])
        b, h = probar(int(sys.argv[2]) if len(sys.argv) > 2 else 2000)
        sys.exit(0 if b == h else 1)
    sys.exit(__doc__)
