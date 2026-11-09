"""Bajada SM86 -> Maxwell de lo uniforme (registros URn y predicados UPn) y de LDC (constante con registro).
Código propio (MIT). Lo carga tools/sm86_baja.py (_cargar_modulos); se prueba con
`SOLO=ULDC,UMOV,UPLOP3,VOTEU,S2UR,R2UR,REDUX,LDC python3 tools/sm86_baja.py probar 3000`.

Dónde vive lo uniforme en Maxwell (que no tiene camino uniforme):
- URn (UR0..UR62) vive en el registro común R(188+n) (sm86_baja.ur); URZ (UR63) es RZ. Como URZ no ocupa registro,
  R251 (188+63) queda libre y es el registro de predicados uniformes, UPR.
- UPn (UP0..UP6) es el bit n de UPR; UPT es la constante verdadera (no se guarda; escribir en UPT no hace nada).
  Los bits 8..14 de UPR son de paso: ahí se guarda Pn mientras una guarda uniforme lo toma prestado (ver abajo).
  El resto de UPR no se usa. Quien arma el programa reserva R188..R251 y los temporales (sm86_baja.TEMPS).
- Guarda uniforme (@UPn / @!UPn, la de UMOV, ULDC, S2UR y UPLOP3: los bits 12-15 nombran un UP): Maxwell no tiene
  predicado libre (P0..P6 son los P0..P6 de SM86), así que se toma prestado Pn, el del mismo número:
      P2R UPR, PR, UPR, 1<<n, B1     guarda Pn en el bit 8+n de UPR
      R2P PR, UPR, 1<<n              Pn = UPn
      @[!]Pn ...                     el cuerpo, con la guarda normal
      R2P PR, UPR, 1<<n, B1          Pn vuelve
  Nada del cuerpo lee ni escribe un P (las instrucciones uniformes solo usan UR y UP), así que el préstamo no se ve.
- Leer UPn como valor: BFE.U32 T, UPR, (1, n) da 0/1; con eso LOP3 calcula y BFI deja el bit en UPR.

Lo que todavía no se baja (SinRegla con el motivo):
- cx[URn][off] (banco elegido por un registro uniforme, bit 91): en Turing/Ampere es una constante sin enlace
  (bindless): el UR lleva un mango del runtime, no un número de banco. Ver docs/pendientes.md.
- REDUX (reducción de todo el warp a un UR): Maxwell no la tiene; con SHFL en mariposa solo es exacta con el warp
  entero activo.
- R2UR con predicado de salida o .OR (OR de todos los hilos).
- Las formas con UR como operando de instrucciones comunes y UIADD3/ULOP3/USEL/USHF/UFLO/UPOPC/ULEA/UISETP no están
  en build/sm86_spec.json: el decodificador no las reconoce todavía (no hay regla posible sin regenerar la spec).
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, codificar, leer, ur, _c, _const, _const_off, _guarda, _mods  # noqa: E402

UPR = B.UR_BASE + 63   # R251: predicados uniformes (bit n = UPn)
SIN_CX = "cx[UR][off]: banco por registro uniforme (bindless), falta apoyo del runtime"


# ---------- guarda uniforme

def _guarda_u(d):
    """(n, invertida) de la guarda de una instrucción del camino uniforme: los bits 12-15 nombran UPn."""
    g = d["guarda"]
    return (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")


def _con_guarda_u(d, cuerpo):
    """Las palabras de cuerpo(guarda) con la guarda uniforme de d; @UPn toma prestado Pn (ver arriba)."""
    n, inv = _guarda_u(d)
    if n == 7:
        return cuerpo({"Pred": 7, "PredInv": inv})   # @!UPT: no se ejecuta nunca, igual que @!PT en Maxwell
    m = 1 << n
    return ([codificar("P2rI", Dest=UPR, SrcA=UPR, Imm20=m, ByteSel=1),
             codificar("R2pI", SrcA=UPR, Imm20=m)]
            + cuerpo({"Pred": n, "PredInv": inv})
            + [codificar("R2pI", SrcA=UPR, Imm20=m, ByteSel=1)])


def _lee_up(n, dest, g):
    """dest = UPn como 0/1."""
    return codificar("BfeI", Dest=dest, SrcA=UPR, Imm20=(1 << 8) | n, **g)


def _pon_up(n, fuente, g):
    """UPn = bit 0 de fuente."""
    return codificar("BfiI", Dest=UPR, SrcA=fuente, Imm20=(1 << 8) | n, SrcC=UPR, **g)


# ---------- reglas

def baja_umov(d):
    """UMOV URd, imm32 -> MOV32I R(188+d), imm32."""
    return _con_guarda_u(d, lambda g: [codificar("Mov32i", Dest=ur(_c(d, "op0.n")), Imm32=_c(d, "op1.v") & 0xFFFFFFFF,
                                                 QuadMask=0xF, **g)])


TAM = {"U8": 0, "S8": 1, "U16": 2, "S16": 3, "64": 5}   # LsSize de Maxwell (= los bits 73-75 de SM86); sin nada: 32


def baja_uldc(d):
    """ULDC[.U8/.S8/.U16/.S16/.64] URd, c[b][off]: 32 bits -> MOV c[][]; 64 -> dos MOV; los chicos -> LDC con RZ."""
    m = _mods(d)
    if any(x.startswith("INVALID") for x in m):
        raise SinRegla("ULDC inválido")
    if _c(d, "op1.banco_reg"):
        raise SinRegla(SIN_CX)
    rd = _c(d, "op0.n")
    tam = next((TAM[x] for x in m if x in TAM), 4)
    banco = _c(d, "op1.banco")
    if tam == 4 or tam == 5:
        banco, w = _const(d, 1)
        if tam == 5 and (w & 1 or (rd & 1 and rd != 63)):
            raise SinRegla("ULDC.64 sin alinear")
        destinos = [ur(rd)] + ([RZ if rd == 63 else ur(rd + 1)] if tam == 5 else [])
        return _con_guarda_u(d, lambda g: [codificar("MovC", Dest=r, CbufSlot=banco, CbufOffset=w + k, QuadMask=0xF,
                                                     **g) for k, r in enumerate(destinos)])
    off = _const_off(d, 1)
    if tam >= 2 and off & 1:
        raise SinRegla("ULDC.16 sin alinear")

    def cuerpo(g):   # LDC suma el offset con signo (16 bits): de 0x8000 para arriba, la base va a un temporal
        if off < 0x8000:
            return [codificar("Ldc", Dest=ur(rd), SrcA=RZ, CbufSlot=banco, CbufOffset=off, LsSize=tam, **g)]
        return [codificar("Mov32i", Dest=TEMPS[0], Imm32=0x8000, QuadMask=0xF, **g),
                codificar("Ldc", Dest=ur(rd), SrcA=TEMPS[0], CbufSlot=banco, CbufOffset=off - 0x8000, LsSize=tam,
                          **g)]
    return _con_guarda_u(d, cuerpo)


def baja_s2ur(d):
    """S2UR URd, SR_x -> S2R R(188+d), SR_x (mismo número de registro especial, bits 72-79)."""
    return _con_guarda_u(d, lambda g: [codificar("S2r", Dest=ur(_c(d, "op0.n")), SReg=d["hi"] >> 8 & 0xFF, **g)])


def baja_r2ur(d):
    """R2UR URd, Rs -> MOV R(188+d), Rs. El compilador solo la usa con valores iguales en todos los hilos; si no lo
    fueran, cada hilo se queda con el suyo (en Ampere el UR tendría uno solo)."""
    if "OR" in _mods(d):
        raise SinRegla("R2UR.OR: OR de todos los hilos")
    if _c(d, "op0.n", 7) != 7:
        raise SinRegla("R2UR con predicado de salida")
    return [codificar("MovR", Dest=ur(_c(d, "op1.n")), SrcA=_c(d, "op2.n"), QuadMask=0xF, **_guarda(d))]


def baja_redux(d):
    raise SinRegla("REDUX: reducción del warp a UR; Maxwell no la tiene (SHFL en mariposa solo con el warp entero)")


VOTO = {"ALL": 0, "ANY": 1, "EQ": 2}


def baja_voteu(d):
    """VOTEU.modo URd, UPu, Pp (guarda P común): URd = ballot(Pp); UPu = todos / alguno / todos iguales.
    VOTE de Maxwell da el ballot; UPu sale de ballots, sin predicado: ANY = ballot(p) != 0, ALL = ballot(!p) == 0,
    EQ = min(ballot(p), ballot(!p)) == 0 (los dos ballots no se pisan); ISET da -1/0 y BFI lo deja en UPR."""
    g = _guarda(d)
    modo = d["nombre"].split(".")[1]
    if modo not in VOTO:
        raise SinRegla("VOTEU " + modo)
    rd, up, p, pn = _c(d, "op0.n"), _c(d, "op1.n", 7), _c(d, "op2.n", 7), _c(d, "op2.not")
    if rd == 63 and up == 7:
        return [codificar("Nop", Ccc=15, **g)]
    b = ur(rd) if rd != 63 else TEMPS[0]
    t = TEMPS[1]
    out = [codificar("Vote", Dest=b, VoteMode=1, SrcPred=p, SrcPredInv=pn, VpDest=7, **g)]
    if up == 7:
        return out
    if modo == "ANY":
        out.append(codificar("IsetR", Dest=t, SrcA=b, SrcB=RZ, IComp=5, Signed=0, SrcPred=7, **g))
    else:
        out.append(codificar("Vote", Dest=t, VoteMode=1, SrcPred=p, SrcPredInv=not pn, VpDest=7, **g))
        if modo == "EQ":
            out.append(codificar("ImnmxR", Dest=t, SrcA=b, SrcB=t, Signed=0, SrcPred=7, **g))
        out.append(codificar("IsetR", Dest=t, SrcA=t, SrcB=RZ, IComp=2, Signed=0, SrcPred=7, **g))
    return out + [_pon_up(up, t, g)]


def _lut_invierte(lut, k):
    """La tabla con la entrada k (0 = A, 1 = B, 2 = C) negada."""
    bit = 4 >> k
    return sum(1 << i for i in range(8) if lut >> (i ^ bit) & 1)


def baja_uplop3(d):
    """UPLOP3.LUT UPu, UPv, UPa, UPb, UPc, lut_u, lut_v: cada UP de entrada (distinto) va a un temporal como 0/1
    (BFE de UPR); las negaciones y UPT se meten en la tabla (UPT entra como RZ con la entrada negada); cada salida es
    un LOP3 con su tabla y un BFI en UPR. Si hay tres entradas distintas y dos salidas, la primera salida pisa el
    temporal de una entrada que no es esa salida y después se vuelve a leer de UPR (ese bit no cambió)."""
    entradas = []   # (registro o None para UPT, negada)
    for i in (2, 3, 4):
        n = _c(d, f"op{i}.n", 7)
        entradas.append((n, bool(_c(d, f"op{i}.not")) ^ (n == 7)))
    salidas = [(n, lut) for n, lut in ((_c(d, "op0.n", 7), _c(d, "op5.v") & 0xFF),
                                         (_c(d, "op1.n", 7), _c(d, "op6.v") & 0xFF)) if n != 7]

    def cuerpo(g):
        if not salidas:
            return [codificar("Nop", Ccc=15, **g)]
        temp = {}
        out = []
        for n, _ in entradas:
            if n != 7 and n not in temp:
                temp[n] = TEMPS[len(temp)]
                out.append(_lee_up(n, temp[n], g))
        for j, (u, lut) in enumerate(salidas):
            regs = []
            for k, (n, neg) in enumerate(entradas):
                regs.append(RZ if n == 7 else temp[n])
                if neg:
                    lut = _lut_invierte(lut, k)
            libres = [t for t in TEMPS if t not in temp.values()]
            pisa = None
            if libres:
                dest = libres[0]
            elif j == len(salidas) - 1:
                dest = TEMPS[0]   # LOP3 lee antes de escribir: la última salida puede pisar una entrada
            else:
                pisa = next(n for n in temp if n != u)
                dest = temp[pisa]
            out.append(codificar("Lop3R", Dest=dest, SrcA=regs[0], SrcB=regs[1], SrcC=regs[2], Imm=lut, **g))
            out.append(_pon_up(u, dest, g))
            if pisa is not None:
                out.append(_lee_up(pisa, temp[pisa], g))
        return out
    return _con_guarda_u(d, cuerpo)


def _ldc_campos(d):
    """Campos de LDC (0xb82) desde los bits crudos: la especificación de LDC no tiene Ra ni el offset (los mezcló como
    off_reg). Posiciones confirmadas con nvdisasm: Rd 16-23, Ra 24-31, offset 38-53 (con signo), banco 54-58, tamaño
    73-75, modo 78-79 (1 .IL, 2 .IS, 3 .ISL)."""
    lo, hi = d["lo"], d["hi"]
    return {"rd": lo >> 16 & 0xFF, "ra": lo >> 24 & 0xFF, "off": lo >> 38 & 0xFFFF, "banco": lo >> 54 & 0x1F,
            "tam": hi >> 9 & 7, "modo": hi >> 14 & 3}


def baja_ldc(d):
    """LDC[.tam][.IL/.IS/.ISL] Rd, c[b][Ra+off] -> LDC de Maxwell: mismos campos y misma semántica (offset de 16 bits
    con signo sumado a Ra; .IS/.ISL toman el banco extra de Ra[31:16])."""
    f = _ldc_campos(d)
    if f["tam"] > 5:
        raise SinRegla("LDC de tamaño inválido")
    return [codificar("Ldc", Dest=f["rd"], SrcA=f["ra"], CbufSlot=f["banco"], CbufOffset=f["off"], LsSize=f["tam"],
                      AddressMode=f["modo"], **_guarda(d))]


REGLAS = {"UMOV": baja_umov, "ULDC": baja_uldc, "S2UR": baja_s2ur, "R2UR": baja_r2ur, "REDUX": baja_redux,
          "VOTEU": baja_voteu, "UPLOP3": baja_uplop3, "LDC": baja_ldc}
# La comprobación de estas no es por campos (ROLES): es COMPROBAR, que ejecuta las dos versiones.
ROLES = {}
FAMILIA = {"UMOV": "Mov32i", "ULDC": "MovC", "S2UR": "S2r", "R2UR": "MovR", "VOTEU": "Vote", "UPLOP3": "Lop3R",
           "LDC": "Ldc"}


# ---------- comprobar: ejecutar la instrucción SM86 (su semántica) y lo bajado (la de Maxwell según Ryujinx) sobre el
# mismo estado al azar, en un warp de 32 hilos, y comparar registros, predicados y UP. No usa nada de las reglas.

HILOS = 32
M32 = 0xFFFFFFFF


def _h(*xs):
    """Un número de 32 bits fijo para esos valores (contenido de las constantes y de los registros especiales)."""
    v = 0x811C9DC5
    for x in xs:
        v = ((v ^ (x & 0xFFFFFFFFFFFF)) * 0x01000193) & 0xFFFFFFFFFFFF
    return (v ^ v >> 29) & M32


def _cb(banco, palabra):
    return _h(1, banco, palabra & 0x3FFFFFFF)


def _sr(n):
    return _h(2, n)


def _s(x, bits=32):
    x &= (1 << bits) - 1
    return x - (1 << bits) if x >> (bits - 1) else x


def _chico(valor, tam, byte):
    """Ryujinx ExtractSmallInt: U8/S8/U16/S16 de la palabra, empezando en el byte dado."""
    ancho = 8 if tam < 2 else 16
    v = valor >> (byte * 8) & ((1 << ancho) - 1)
    return _s(v, ancho) & M32 if tam & 1 else v


_ESTADOS = []


def _estado(k):
    """Una copia del estado al azar número k (de 64, armados una vez: armarlos por instrucción es lo lento)."""
    if not _ESTADOS:
        rnd = random.Random(86)
        for _ in range(64):
            uni = [rnd.getrandbits(32) for _ in range(64)]
            hilos = []
            for _ in range(HILOS):
                r = [rnd.getrandbits(32) for _ in range(256)]
                r[B.UR_BASE:B.UR_BASE + 64] = uni   # URn y UPR: iguales en todo el warp
                r[RZ] = 0
                hilos.append({"R": r, "P": [rnd.random() < 0.5 for _ in range(7)] + [True]})
            _ESTADOS.append(hilos)
    return [{"R": h["R"][:], "P": h["P"][:]} for h in _ESTADOS[k % 64]]


def _w(h, reg, v):
    if reg != RZ:
        h["R"][reg] = v & M32


def _wp(h, p, v):
    if p != 7:
        h["P"][p] = bool(v)


# -- Maxwell, como lo traduce Ryujinx

def _lop3(a, b, c, lut):
    r = 0
    for i in range(8):
        if lut >> i & 1:
            r |= (a if i & 4 else ~a) & (b if i & 2 else ~b) & (c if i & 1 else ~c)
    return r & M32


def _campo_bits(imm):
    """(posición, tamaño) de BFE/BFI, con el recorte en el bit 31 que hace Ryujinx."""
    pos, tam = imm & 0xFF, imm >> 8 & 0xFF
    return pos, min(tam, max(0, 32 - pos))


def _ejecuta_maxwell(palabras, hilos):
    for w in palabras:
        op, c = leer(w)
        activos = [h for h in hilos if h["P"][c.get("Pred", 7)] != bool(c.get("PredInv"))]
        if op == "Vote":
            votos = {id(h): h["P"][c["SrcPred"]] != bool(c["SrcPredInv"]) for h in activos}
            ballot = sum(1 << i for i, h in enumerate(hilos) if votos.get(id(h)))
            votos = list(votos.values())
            res = (all(votos), any(votos), all(votos) or not any(votos))[c["VoteMode"]]
            for h in activos:
                _wp(h, c["VpDest"], res)
                _w(h, c["Dest"], ballot)
            continue
        for h in activos:
            R, P = h["R"], h["P"]
            if op == "Mov32i":
                _w(h, c["Dest"], c["Imm32"])
            elif op == "MovR":
                _w(h, c["Dest"], R[c["SrcA"]])
            elif op == "MovC":
                _w(h, c["Dest"], _cb(c["CbufSlot"], c["CbufOffset"]))
            elif op == "S2r":
                _w(h, c["Dest"], _sr(c["SReg"]))
            elif op == "Ldc":
                tam, slot, a = c["LsSize"], c["CbufSlot"], R[c["SrcA"]]
                if c["AddressMode"] in (2, 3):
                    slot, a = slot + (a >> 16), a & 0xFFFF
                addr = (a + _s(c["CbufOffset"], 16)) & M32
                for k in range(2 if tam == 5 else 1):
                    if c["Dest"] + k == RZ:
                        break
                    v = _cb(slot, (addr >> 2) + k)
                    _w(h, c["Dest"] + k, _chico(v, tam, addr & 3) if tam < 4 else v)
            elif op == "P2rI":
                m, sh = (c["Imm20"] & 0xFF) << (8 * c["ByteSel"]), 8 * c["ByteSel"]
                ins = sum(1 << (i + sh) for i in range(7) if P[i])
                _w(h, c["Dest"], (R[c["SrcA"]] & ~m) | (ins & m))
            elif op == "R2pI":
                for i in range(7):
                    if c["Imm20"] >> i & 1:
                        P[i] = bool(R[c["SrcA"]] >> (i + 8 * c["ByteSel"]) & 1)
            elif op == "BfeI":
                pos, tam = _campo_bits(c["Imm20"])
                v = R[c["SrcA"]] >> pos & ((1 << tam) - 1)
                _w(h, c["Dest"], _s(v, tam) if c["Signed"] and tam else v)
            elif op == "BfiI":
                pos, tam = _campo_bits(c["Imm20"])
                m = ((1 << tam) - 1) << pos
                _w(h, c["Dest"], (R[c["SrcC"]] & ~m) | (R[c["SrcA"]] << pos & m))
            elif op == "Lop3R":
                _w(h, c["Dest"], _lop3(R[c["SrcA"]], R[c["SrcB"]], R[c["SrcC"]], c["Imm"]))
            elif op == "IsetR":
                a, b = R[c["SrcA"]], R[c["SrcB"]]
                if c["Signed"]:
                    a, b = _s(a), _s(b)
                r = [False, a < b, a == b, a <= b, a > b, a != b, a >= b, True][c["IComp"]]
                p = P[c["SrcPred"]] != bool(c["SrcPredInv"])
                r = [r and p, r or p, r != p, r][c["Bop"]]
                _w(h, c["Dest"], M32 if r else 0)
            elif op == "ImnmxR":
                a, b = R[c["SrcA"]], R[c["SrcB"]]
                ka, kb = (_s(a), _s(b)) if c["Signed"] else (a, b)
                menor = a if ka <= kb else b
                mayor = b if ka <= kb else a
                _w(h, c["Dest"], menor if P[c["SrcPred"]] != bool(c["SrcPredInv"]) else mayor)
            elif op == "Nop":
                pass
            else:
                raise ValueError(f"el simulador no conoce {op}")


# -- SM86, desde los campos decodificados

def _ur_w(h, n, v):
    if n != 63:
        h["R"][B.UR_BASE + n] = v & M32


def _up(h, n):
    return True if n == 7 else bool(h["R"][UPR] >> n & 1)


def _up_w(h, n, v):
    if n != 7:
        h["R"][UPR] = (h["R"][UPR] & ~(1 << n)) | (int(bool(v)) << n)


def _ejecuta_sm86(d, hilos):
    nombre = d["nombre"].split(".")[0]
    m = _mods(d)
    uniforme = nombre in ("UMOV", "ULDC", "S2UR", "UPLOP3")
    g = d["guarda"]
    gn, ginv = (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")
    activos = [h for h in hilos if (_up(h, gn) if uniforme else h["P"][gn]) != ginv]
    if nombre == "VOTEU":
        p, pn = _c(d, "op2.n", 7), bool(_c(d, "op2.not"))
        votos = {id(h): h["P"][p] != pn for h in activos}
        ballot = sum(1 << i for i, h in enumerate(hilos) if votos.get(id(h)))
        v = list(votos.values())
        res = {"ALL": all(v), "ANY": any(v), "EQ": all(v) or not any(v)}[d["nombre"].split(".")[1]]
        for h in activos:
            _ur_w(h, _c(d, "op0.n"), ballot)
            _up_w(h, _c(d, "op1.n", 7), res)
        return
    for h in activos:
        if nombre == "UMOV":
            _ur_w(h, _c(d, "op0.n"), _c(d, "op1.v"))
        elif nombre == "S2UR":
            _ur_w(h, _c(d, "op0.n"), _sr(d["hi"] >> 8 & 0xFF))
        elif nombre == "R2UR":
            _ur_w(h, _c(d, "op1.n"), h["R"][_c(d, "op2.n")])
        elif nombre == "ULDC":
            off = _c(d, "op1.off") | _c(d, "op1.off_reg") << 15   # c[b][off]: 16 bits sin signo
            b, rd = _c(d, "op1.banco"), _c(d, "op0.n")
            v = _cb(b, off >> 2)
            if "64" in m:
                _ur_w(h, rd, v)
                if rd != 63:
                    _ur_w(h, rd + 1, _cb(b, (off >> 2) + 1))
            else:
                tam = {"U8": 0, "S8": 1, "U16": 2, "S16": 3}
                t = next((tam[x] for x in m if x in tam), None)
                _ur_w(h, rd, v if t is None else _chico(v, t, off & 3))
        elif nombre == "UPLOP3":
            ent = [_up(h, _c(d, f"op{i}.n", 7)) != bool(_c(d, f"op{i}.not")) for i in (2, 3, 4)]
            idx = ent[0] << 2 | ent[1] << 1 | ent[2]
            r0, r1 = _c(d, "op5.v") >> idx & 1, _c(d, "op6.v") >> idx & 1
            _up_w(h, _c(d, "op0.n", 7), r0)
            _up_w(h, _c(d, "op1.n", 7), r1)
        elif nombre == "LDC":
            # LDC de SM86: Rd = lo[16:24], Ra = lo[24:32], offset con signo lo[38:54], banco lo[54:59], tamaño
            # hi[9:12] (bits 73-75), modo hi[14:16] (78-79); leídos del texto de nvdisasm, no de la regla
            lo, hi = d["lo"], d["hi"]
            rd, a, slot = lo >> 16 & 0xFF, h["R"][lo >> 24 & 0xFF], lo >> 54 & 0x1F
            tam, modo = hi >> 9 & 7, hi >> 14 & 3
            if modo in (2, 3):
                slot, a = slot + (a >> 16), a & 0xFFFF
            addr = (a + _s(lo >> 38, 16)) & M32
            for k in range(2 if tam == 5 else 1):
                if rd + k == RZ:
                    break
                v = _cb(slot, (addr >> 2) + k)
                _w(h, rd + k, _chico(v, tam, addr & 3) if tam < 4 else v)


def comprobar_ejecutando(d, palabras):
    errores = []
    for semilla in range(8):   # los UP son iguales en todo el warp: varios estados para ver más combinaciones
        base = _h(d["lo"], d["hi"], semilla)
        a, b = _estado(base), _estado(base)
        _ejecuta_sm86(d, a)
        try:
            _ejecuta_maxwell(palabras, b)
        except (ValueError, KeyError) as e:
            return [f"Maxwell: {e}"]
        for i, (x, y) in enumerate(zip(a, b)):
            for t in TEMPS:   # los temporales no cuentan; de UPR, solo los bits de UP0..UP6
                y["R"][t] = x["R"][t]
            if x["R"][UPR] & 0x7F == y["R"][UPR] & 0x7F:
                y["R"][UPR] = x["R"][UPR]
            if x["R"] == y["R"] and x["P"] == y["P"]:
                continue
            for r in range(256):
                if r in TEMPS:
                    continue
                vx, vy = (x["R"][r] & 0x7F, y["R"][r] & 0x7F) if r == UPR else (x["R"][r], y["R"][r])
                if vx != vy:
                    errores.append(f"hilo {i} R{r}: {vx:#x} vs {vy:#x}")
            if x["P"] != y["P"]:
                errores.append(f"hilo {i} P: {x['P']} vs {y['P']}")
            if errores:
                return errores[:4]
    return errores


COMPROBAR = {k: comprobar_ejecutando for k in REGLAS}
