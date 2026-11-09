"""Bajada SM86 -> Maxwell de las texturas (TEX, TLD, TLD4, TXD, TXQ, TMML) y de las entradas/salidas del shader (IPA,
AL2P, ISBERD, OUT, KILL, PIXLD). Código propio (MIT). Lo carga tools/sm86_baja.py (_cargar_modulos); se prueba con
`SOLO=TEX,TLD,TLD4,TXD,TXQ,TMML,IPA,AL2P,ISBERD,OUT,KILL,PIXLD python3 tools/sm86_baja.py probar 3000`.

Cómo se averiguó (sin código ajeno): nvdisasm (tools/sm86_oraculo.py) para los campos y modificadores, y ptxas del kit
CUDA para la semántica: programas PTX con tex/tld4/txq/tex.grad compilados a sm_86 dicen qué valor queda en qué
registro antes de la textura y dónde quedan los componentes (los guarda un STG después).

Texturas en Ampere (los seis códigos de cada una: forma ligada 0xb6x, forma bindless 0x36x, forma de tres inmediatos):
- Destino partido: Rd (bits 16-23) y Rd2 (bits 64-71; nvdisasm lo escribe primero). De los componentes pedidos por la
  máscara (bits 72-75), en orden, los dos primeros van a Rd, Rd+1 y el tercero y cuarto a Rd2, Rd2+1. Con dos o menos,
  todo va a Rd. Maxwell TEX los deja todos seguidos desde Dest (como lee Ryujinx): si Rd2 != Rd+2 se arma con MOVs.
- Predicado de residencia (bits 81-83, PT si no se usa): Maxwell lo tiene (DestPred) pero Ryujinx no lo escribe.
- La textura ligada lee su mango de c[banco][off*4]: banco en los bits 54-58 y off (palabras, 14 bits) en 40-53. Maxwell
  lo lee de c[TextureBufferIndex][TidB*4] (TidB de 13 bits; el banco lo fija el estado de la GPU, NVN usa c[2] y el
  traductor de nxrecomp lo trata así). Si el banco es BANCO_TEXTURAS se baja a la forma ligada con TidB = off; si es
  otro, a la forma bindless con el mango cargado por MOV c[banco][off]: el traductor de Ryujinx (BindlessElimination)
  lo devuelve a ligada con ese banco (cbuf_slot en la salida). TMML no puede ir así (SinRegla).
- Bindless (0x36x con .B, bit 59): el mango (32 bits) va en un registro dentro de la lista de fuentes.
- Tipo (bits 61-63): 0 1D, 1 2D, 2 3D, 3 CUBE, 4 ARRAY_1D, 5 ARRAY_2D, 7 ARRAY_CUBE (Maxwell: 0 1D, 1 ARRAY_1D, 2 2D,
  3 ARRAY_2D, 4 3D, 6 CUBE, 7 ARRAY_CUBE).
- Fuentes de TEX/TLD/TLD4/TMML: la lista lógica es la de Maxwell, A = [índice de arreglo] + coordenadas y
  B = [mango bindless] + [lod (LB/LL)] + [desplazamientos AOFFI (PTP: dos)] + [DC] + [muestra MS]. Maxwell pone A desde
  Ra y B desde Rb. Ampere hace lo mismo si son 5 o más; con 4 o menos los reparte parejo: Ra lleva 1 (con 1 o 2 en
  total) o 2 (con 3 o 4) y Rb el resto. Ej.: 2D con LZ -> Ra=[x] Rb=[y]; 3D con LL -> Ra=[x,y] Rb=[z,lod];
  ARRAY_2D bindless con LL -> Ra=[i,x,y] Rb=[mango,lod]. Lo que en Maxwell quede partido se junta en TEMPS con MOVs.
- TXD y TXQ ponen las fuentes como Maxwell: TXD Ra=[mango] + coords + [empaquetado: índice de arreglo en los bits
  0-15 y desplazamientos en 16+], Rb=derivadas (dx.x, dy.x, dx.y, dy.y); TXQ Ra=[mango, lod]. Se copian tal cual.
- Desplazamientos empaquetados iguales que en Maxwell: TEX/TLD 4 bits por coordenada, TLD4 6 bits cada 8.
- Modificadores: TEX/TLD/TLD4/TXD con .F16 (resultado en medio float) no tienen equivalente: SinRegla. LOD de TEX
  (bits 87-89): 0 auto, 1 LZ, 2 LB, 3 LL (los de .LC y .FDV: SinRegla, Ryujinx ignora el clamp). TLD: LZ o LL.
  TLD4: componente R/G/B/A en 87-88, AOFFI/PTP en 76-77, DC 78. TXD: AOFFI 76, .LC 87 (SinRegla). TXQ: consulta en
  62-63 (DIMENSION, TEXTURE_TYPE, SAMPLER_POS). .SCR (bit 60) y las pistas de caché (.EF/.EL/.LU/.EU/.NA) no cambian
  el resultado y se dejan.

Entradas/salidas:
- IPA: a[] en los bits 64-71 (x4); modo PASS/CONSTANT/STATE (78-79) = Maxwell Pass/Constant/Sc; CENTROID/OFFSET
  (76-77) = Maxwell Msi; el registro de OFFSET (32-39) o su inmediato/constante (formas 0x926/0xb26, por un temporal)
  va en SrcC. Ampere no tiene IPA.MULTIPLY: el compilador multiplica por 1/w con FMUL, y PASS se lee igual que en
  Maxwell (Ryujinx ya lo emula multiplicando por gl_FragCoord.w). El predicado de los bits 81-83 tiene que ser PT.
- AL2P, ISBERD (con inmediato: IADD32I a un temporal), OUT (EMIT/CUT/EMIT_THEN_CUT), PIXLD: mismos campos que Maxwell.
- KILL Pq con guarda Pg: Maxwell KIL solo tiene la guarda; si las dos son de verdad se combinan en P0 con PSETP y P0
  se guarda y se repone con P2R/R2P (todo el PR en un temporal).
- ALD/AST no están en build/sm86_spec.json, así que el decodificador no los da y no hay regla. nvdisasm decodifica
  ALD (0x321) con Ra = RZ (o con .P/.PHYS): mismos campos que el ALD de Maxwell (ver docs/pendientes.md).
"""
import json
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import sm86  # noqa: E402
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, codificar, leer, _guarda  # noqa: E402

# Banco de los mangos de textura que el traductor toma como el de Maxwell (TextureBufferIndex). Sin confirmar para
# NVN2: quien arma el programa lo cambia si el juego usa otro banco para todas sus texturas.
BANCO_TEXTURAS = 2
TEMPS_ORDEN = sorted(TEMPS)   # 252, 253, 254: seguidos, para vectores de fuentes

_SPEC = None


def _spec():
    global _SPEC
    if _SPEC is None:
        _SPEC = json.load(open(os.path.join(os.path.dirname(AQUI), "build", "sm86_spec.json")))
    return _SPEC


def _bits(d, ini, n):
    return ((d["lo"] | d["hi"] << 64) >> ini) & ((1 << n) - 1)


def fichas(d):
    """Todos los modificadores del nombre, de las tablas de la especificación grupo por grupo (d['nombre'] solo
    trae el primer grupo que cambia). None si alguna combinación no existe."""
    s = _spec()[d["op"]]
    base = set(s["nombre"].split(".")[1:])
    mas, menos = set(), set()
    v = d["lo"] | d["hi"] << 64
    for t in s["modificadores"]:
        combo = sum(((v >> b) & 1) << j for j, b in enumerate(t["bits"]))
        n = t["nombres"].get(str(combo))
        if n is None:
            return None
        partes = set(n.split(".")[1:])
        mas |= partes - base
        menos |= base - partes
    return (base | mas) - menos


def fichas_grupo(d, bit):
    """Los modificadores del nombre que da solo el grupo de bits que contiene `bit` (con el resto como en la forma
    base), o None si ese bit no cambia el nombre."""
    v = d["lo"] | d["hi"] << 64
    for t in _spec()[d["op"]]["modificadores"]:
        if bit in t["bits"]:
            n = t["nombres"].get(str(sum(((v >> b) & 1) << j for j, b in enumerate(t["bits"]))))
            return set(n.split(".")[1:]) if n else None
    return None


def _indefinida(d):
    f = fichas(d)
    if f is None or any("???" in x or "INVALID" in x for x in f):
        raise SinRegla(f"{d['nombre'].split('.')[0]}: forma indefinida (??? o INVALID)")


# ---------- texturas

CLASE = {0xb60: ("TEX", "ligada"), 0x361: ("TEX", "registro"), 0xb62: ("TEX", "tres"),
         0xb63: ("TLD4", "ligada"), 0x364: ("TLD4", "registro"), 0xb65: ("TLD4", "tres"),
         0xb66: ("TLD", "ligada"), 0x367: ("TLD", "registro"), 0xb68: ("TLD", "tres"),
         0xb69: ("TMML", "ligada"), 0x36a: ("TMML", "registro"), 0xb6b: ("TMML", "tres"),
         0xb6c: ("TXD", "ligada"), 0x36d: ("TXD", "registro"), 0xb6e: ("TXD", "tres"),
         0xb6f: ("TXQ", "ligada"), 0x370: ("TXQ", "registro"), 0xb71: ("TXQ", "tres")}
# Tipo de Ampere (bits 61-63) -> (Dim de Maxwell, coordenadas, arreglo)
DIM = {0: (0, 1, False), 1: (2, 2, False), 2: (4, 3, False), 3: (6, 3, False), 4: (1, 1, True), 5: (3, 2, True),
       7: (7, 3, True)}
DIM_VALIDOS = {"TEX": {0, 1, 2, 3, 4, 5, 7}, "TMML": {0, 1, 2, 3, 4, 5, 7}, "TLD": {0, 1, 2, 4, 5},
               "TLD4": {1, 3, 5, 7}, "TXD": {1, 5}}   # TXD 1D: ptxas lo pasa a 2D, su disposición no se pudo ver
MX = {"TEX": "Tex", "TLD": "Tld", "TLD4": "Tld4", "TXD": "Txd", "TMML": "Tmml", "TXQ": "Txq"}
TXQ_CONSULTA = {0: 1, 1: 2, 2: 5}   # DIMENSION, TEXTURE_TYPE, SAMPLER_POS -> TexQuery de Maxwell


def _info(d):
    """Lo que hace falta de una textura SM86, leído de los bits."""
    op = d["lo"] & 0xFFF
    tipo, forma = CLASE[op]
    if forma == "tres":
        raise SinRegla(f"{tipo} con tres inmediatos (forma {op:#x}): significado sin confirmar")
    _indefinida(d)
    bindless = bool(_bits(d, 59, 1))
    if forma == "ligada" and bindless:
        raise SinRegla(f"{tipo}.B con banco y offset (forma {op:#x}): significado sin confirmar")
    i = {"tipo": tipo, "bindless": bindless, "rd": _bits(d, 16, 8), "rd2": _bits(d, 64, 8), "ra": _bits(d, 24, 8),
         "rb": RZ if tipo == "TXQ" else _bits(d, 32, 8), "mask": _bits(d, 72, 4), "nodep": _bits(d, 90, 1)}
    if not bindless:
        i["banco"], i["off"] = _bits(d, 54, 5), _bits(d, 40, 14)
    if tipo in ("TEX", "TLD", "TLD4", "TXD"):
        if _bits(d, 81, 3) != 7:
            raise SinRegla(f"{tipo} con predicado de residencia (sparse): Ryujinx no lo escribe")
        if _bits(d, 79, 2):
            raise SinRegla(f"{tipo}.F16: resultado en medio float, Maxwell TEX no lo tiene")
    if tipo == "TXQ":
        q = _bits(d, 62, 2)
        if q not in TXQ_CONSULTA:
            raise SinRegla("TXQ con consulta indefinida")
        i["consulta"] = TXQ_CONSULTA[q]
        return i
    dim = _bits(d, 61, 3)
    if dim not in DIM_VALIDOS[tipo]:
        raise SinRegla(f"{tipo} con tipo de textura {dim} (indefinido o sin disposición confirmada)")
    i["dim"], i["coords"], i["arreglo"] = DIM[dim]
    if tipo == "TEX":
        lod = _bits(d, 87, 3)
        if lod > 3:
            raise SinRegla("TEX con .LC/.FDV (clamp de LOD): Ryujinx no lo implementa")
        i.update(lod=lod, aoffi=_bits(d, 76, 1), ndv=_bits(d, 77, 1), dc=_bits(d, 78, 1))
    elif tipo == "TLD":
        lod = _bits(d, 87, 3)
        if lod not in (1, 3):
            raise SinRegla("TLD sin LZ ni LL")
        i.update(ll=lod == 3, aoffi=_bits(d, 76, 1), cl=_bits(d, 77, 1), ms=_bits(d, 78, 1))
    elif tipo == "TLD4":
        toff = _bits(d, 76, 2)
        if toff == 3:
            raise SinRegla("TLD4 con desplazamiento indefinido")
        i.update(toff=toff, dc=_bits(d, 78, 1), comp=_bits(d, 87, 2))
    elif tipo == "TXD":
        if _bits(d, 87, 1):
            raise SinRegla("TXD.LC (clamp de LOD): Ryujinx no lo implementa")
        i["aoffi"] = _bits(d, 76, 1)
    elif tipo == "TMML":
        i["ndv"] = _bits(d, 77, 1)
    return i


def _listas(i):
    """(A, B): papeles de la lista lógica de fuentes, como los lee Maxwell (A desde Ra, B desde Rb)."""
    t = i["tipo"]
    h = ["h"] if i["bindless"] else []
    if t == "TXQ":
        return h + ["lod"], []
    coords = ["x", "y", "z"][:i["coords"]]
    if t == "TXD":
        emp = ["emp"] if i["arreglo"] or i["aoffi"] else []
        return h + coords + emp, [f"d{k}" for k in range(2 * i["coords"])]
    a = (["ai"] if i["arreglo"] else []) + coords
    if t == "TEX":
        b = h + (["lod"] if i["lod"] in (2, 3) else []) + (["off"] if i["aoffi"] else []) + (["dc"] if i["dc"] else [])
    elif t == "TLD":
        b = h + (["lod"] if i["ll"] else []) + (["off"] if i["aoffi"] else []) + (["ms"] if i["ms"] else [])
    elif t == "TLD4":
        b = h + ["off", "off2"][:i["toff"]] + (["dc"] if i["dc"] else [])
    else:   # TMML
        b = h
    return a, b


def _corte_ampere(t, a, b):
    """Cuántos elementos de la lista lógica van desde Ra en Ampere."""
    if t in ("TXD", "TXQ"):
        return len(a)
    n = len(a) + len(b)
    return (1 if n <= 2 else 2) if n <= 4 else len(a)


def _reg_de(base, k):
    """Registro del elemento k de un vector que empieza en base; None si lee cero (RZ o más allá)."""
    return None if base == RZ or base + k >= RZ else base + k


def _base_directa(regs):
    """Registro base Maxwell que lee exactamente esos registros (None = cero), o None si no hay."""
    if all(r is None for r in regs):
        return RZ
    b = regs[0]
    if b is None:
        return None
    for k, r in enumerate(regs):
        if (r is None and b + k < RZ) or (r is not None and r != b + k):
            return None
    return b


def _mov(dest, fuente, g):
    return codificar("MovR", Dest=dest, SrcA=RZ if fuente is None else fuente, QuadMask=0xF, **g)


def _mascara(comps):
    return sum(1 << k for k in comps)


def _palabra_tex(i, bindless_mx, dest, mask, sa, sb, g):
    t = i["tipo"]
    op = MX[t] + ("B" if bindless_mx else "")
    c = dict(Dest=dest, SrcA=sa, WMask=mask, Nodep=i["nodep"], **g)
    if not bindless_mx:
        c["TidB"] = i["off"]
    if t == "TXQ":
        return codificar(op, TexQuery=i["consulta"], **c)
    c.update(SrcB=sb, Dim=i["dim"])
    if t == "TEX":
        lod = {"Lodb": i["lod"], "Aoffib": i["aoffi"]} if bindless_mx else {"Lod": i["lod"], "Aoffi": i["aoffi"]}
        return codificar(op, Dc=i["dc"], Ndv=i["ndv"], DestPred=7, **lod, **c)
    if t == "TLD":
        return codificar(op, Lod=i["ll"], Toff=i["aoffi"], Ms=i["ms"], Cl=i["cl"], DestPred=7, **c)
    if t == "TLD4":
        return codificar(op, TexComp=i["comp"], Toff=i["toff"], Dc=i["dc"], DestPred=7, **c)
    if t == "TXD":
        return codificar(op, Toff=i["aoffi"], DestPred=7, **c)
    return codificar(op, Ndv=i["ndv"], **c)   # TMML


def baja_textura(d):
    i = _info(d)
    g = _guarda(d)
    t = i["tipo"]
    a, b = _listas(i)
    corte = _corte_ampere(t, a, b)
    lista = a + b
    # registro Ampere de cada elemento de la lista lógica
    reg = {p: _reg_de(i["ra"], k) if k < corte else _reg_de(i["rb"], k - corte) for k, p in enumerate(lista)}
    usados = {r for r in reg.values() if r is not None} | {i["rd"], i["rd"] + 1, i["rd2"], i["rd2"] + 1}
    if usados & set(TEMPS):
        raise SinRegla(f"{t} usa R{TEMPS_ORDEN[0]}-R{TEMPS_ORDEN[-1]}, los temporales reservados de la bajada")
    bindless_mx = i["bindless"]
    mango = None
    if not i["bindless"] and i["banco"] != BANCO_TEXTURAS:
        if t == "TMML":
            raise SinRegla(f"TMML con el mango en c[{i['banco']:#x}] (no es el banco de texturas): el traductor no "
                           "resuelve TMML bindless")
        bindless_mx, mango = True, (i["banco"], i["off"])
        if t in ("TXD", "TXQ"):
            a = ["h"] + a
        else:
            b = ["h"] + b
    elif not i["bindless"] and i["off"] >= 0x2000:
        raise SinRegla("textura con offset de mango >= 0x2000 (TidB de Maxwell tiene 13 bits)")
    previas = []
    libres = list(TEMPS_ORDEN)
    leidos = set()
    bases = []
    for grupo in (a, b):
        regs = [reg.get(p) for p in grupo]
        base = None if "h" in grupo and mango else _base_directa(regs) if grupo else RZ
        if base is None:   # se junta en temporales seguidos
            if len(grupo) > len(libres):
                raise SinRegla(f"{t}: las fuentes necesitan más de {len(TEMPS)} temporales")
            base = libres[0]
            for k, p in enumerate(grupo):
                if p == "h" and mango:
                    off = mango[1]
                    if off >= 0x4000:
                        raise SinRegla("mango de textura fuera del alcance de MOV c[][]")
                    previas.append(codificar("MovC", Dest=libres[k], CbufSlot=mango[0], CbufOffset=off,
                                             QuadMask=0xF, **g))
                else:
                    previas.append(_mov(libres[k], regs[k], g))
            libres = libres[len(grupo):]
        else:
            leidos |= {base + k for k in range(len(grupo)) if base + k < RZ}
        bases.append(base)
    sa, sb = bases
    return previas + _destinos(i, lambda dest, m: _palabra_tex(i, bindless_mx, dest, m, sa, sb, g), g, libres,
                               leidos)


def _destinos(i, tex, g, libres, leidos):
    """La textura Maxwell (que escribe todo seguido desde Dest) con los destinos de Ampere (Rd para los dos primeros
    componentes, Rd2 para el resto)."""
    rd, rd2, mask = i["rd"], i["rd2"], i["mask"]
    comps = [k for k in range(4) if mask >> k & 1]
    n = len(comps)
    if n <= 2 or rd2 == RZ:
        return [tex(rd, mask if n <= 2 else _mascara(comps[:2]))]
    if rd == RZ:
        return [tex(rd2, _mascara(comps[2:]))]
    if rd2 == rd + 2:
        return [tex(rd, mask)]
    alto = {rd2 + k for k in range(n - 2)}
    bajo = {rd, rd + 1}
    if alto & bajo or max(alto | bajo) >= TEMPS_ORDEN[0]:
        raise SinRegla(f"{i['tipo']} con Rd y Rd2 encimados o en los temporales")
    # Una sola textura en Rd..Rd+n-1; lo de Rd+2.. va a Rd2 y lo pisado que no es destino se repone
    if rd + n - 1 < TEMPS_ORDEN[0]:
        pisados = [rd + 2 + k for k in range(n - 2)]
        guardar = [r for r in pisados if r not in alto]
        if len(guardar) <= len(libres):
            antes = [_mov(t, r, g) for r, t in zip(guardar, libres)]
            orden = range(n - 2) if rd2 < rd + 2 else range(n - 3, -1, -1)
            movs = [_mov(rd2 + k, rd + 2 + k, g) for k in orden]
            reponer = [_mov(r, t, g) for r, t in zip(guardar, libres)]
            return antes + [tex(rd, mask)] + movs + reponer
    # Si no, dos texturas (la misma lectura): una a Rd y otra a Rd2, en el orden que no pise las fuentes de la otra
    t1, t2 = tex(rd, _mascara(comps[:2])), tex(rd2, _mascara(comps[2:]))
    if not bajo & leidos:
        return [t1, t2]
    if not alto & leidos:
        return [t2, t1]
    raise SinRegla(f"{i['tipo']}: destinos partidos que pisan las fuentes y sin temporales libres")


# ---------- entradas y salidas

IPA_MODO = {0: 0, 1: 2, 2: 3}   # PASS, CONSTANT, STATE -> IpaOp Pass, Constant, Sc


def baja_ipa(d):
    _indefinida(d)
    g = _guarda(d)
    op = d["lo"] & 0xFFF
    if _bits(d, 81, 3) != 7:
        raise SinRegla("IPA con predicado en los bits 81-83 (sin equivalente en Maxwell)")
    modo, muestra = _bits(d, 78, 2), _bits(d, 76, 2)
    if modo not in IPA_MODO or muestra == 3:
        raise SinRegla("IPA con modo indefinido")
    if op == 0x926 and _bits(d, 91, 1):
        raise SinRegla("IPA a[UR+off]: atributo indexado por registro uniforme")
    previas = []
    c = RZ
    if muestra == 2:   # OFFSET: el desplazamiento de la muestra
        if op == 0x326:
            c = _bits(d, 32, 8)
        elif op == 0x926:
            c = TEMPS[0]
            previas.append(codificar("Mov32i", Dest=c, Imm32=_bits(d, 32, 32), QuadMask=0xF, **g))
        else:
            c = TEMPS[0]
            banco, off = B._const(d, 3)
            previas.append(codificar("MovC", Dest=c, CbufSlot=banco, CbufOffset=off, QuadMask=0xF, **g))
    return previas + [codificar("Ipa", Dest=_bits(d, 16, 8), SrcA=RZ, SrcB=RZ, SrcC=c, Imm10=_bits(d, 64, 8) << 2,
                                IpaOp=IPA_MODO[modo], Msi=muestra, SrcPred=7, **g)]


def baja_al2p(d):
    _indefinida(d)
    return [codificar("Al2p", Dest=_bits(d, 16, 8), SrcA=_bits(d, 24, 8), Imm11=_bits(d, 40, 11),
                      AlSize=_bits(d, 74, 2), Aio=_bits(d, 79, 1), DestPred=7, **_guarda(d))]


def baja_isberd(d):
    """ISBERD Rd, [Ra+imm]: Maxwell no tiene inmediato (Ryujinx devuelve la dirección: Rd = Ra)."""
    _indefinida(d)
    g = _guarda(d)
    if _bits(d, 74, 2) or _bits(d, 78, 1):
        raise SinRegla("ISBERD con tamaño (.U16/.32) o .SKEW: sin equivalente comprobado en Maxwell")
    a, imm = _bits(d, 24, 8), _bits(d, 40, 16)
    previas = []
    if imm:
        previas.append(codificar("Iadd32i", Dest=TEMPS[0], SrcA=a, Imm32=imm, **g))
        a = TEMPS[0]
    return previas + [codificar("Isberd", Dest=_bits(d, 16, 8), SrcA=a, IBase=_bits(d, 76, 2), O=_bits(d, 79, 1),
                                **g)]


def baja_out(d):
    _indefinida(d)
    g = _guarda(d)
    op = d["lo"] & 0xFFF
    tipo = _bits(d, 78, 2)
    if tipo == 0 or (op == 0x924 and tipo == 2):
        raise SinRegla("OUT.FINAL o tipo indefinido")
    c = dict(Dest=_bits(d, 16, 8), SrcA=_bits(d, 24, 8), OutType=tipo, **g)
    if op == 0x324:
        return [codificar("OutR", SrcB=RZ if tipo == 2 else _bits(d, 32, 8), **c)]
    v = _bits(d, 32, 32)
    if B._cabe20(v):
        return [codificar("OutI", Imm20=v & 0xFFFFF, **c)]
    return [codificar("Mov32i", Dest=TEMPS[0], Imm32=v, QuadMask=0xF, **g), codificar("OutR", SrcB=TEMPS[0], **c)]


def baja_kill(d):
    """@Pg KILL Pq: descarta si Pg y Pq. Maxwell KIL solo tiene la guarda."""
    g = _guarda(d)
    q, qn = _bits(d, 87, 3), _bits(d, 90, 1)
    if (g["Pred"] == 7 and g["PredInv"]) or (q == 7 and qn):   # nunca
        return [codificar("Nop", Ccc=15, **g)]
    if q == 7:
        return [codificar("Kil", Ccc=15, **g)]
    if g["Pred"] == 7:
        return [codificar("Kil", Ccc=15, Pred=q, PredInv=qn)]
    t = TEMPS[0]
    return [codificar("P2rI", Dest=t, SrcA=RZ, Imm20=0x7F),
            codificar("Psetp", DestPred=0, DestPredInv=7, Src2Pred=g["Pred"], Src2PredInv=g["PredInv"],
                      Src1Pred=q, Src1PredInv=qn, SrcPred=7, BoolOpAB=0, BoolOpC=0),
            codificar("Kil", Ccc=15, Pred=0, PredInv=0),
            codificar("R2pI", SrcA=t, Imm20=0x7F)]


PIXLD_MODO = {1: 1, 2: 4, 3: 5}   # COVMASK, CENTROID_OFFSET, MY_INDEX -> PixMode de Maxwell


def baja_pixld(d):
    _indefinida(d)
    modo = _bits(d, 78, 3)
    if modo not in PIXLD_MODO:
        raise SinRegla("PIXLD sin modo o .INNER_COVERAGE: sin equivalente en Maxwell")
    return [codificar("Pixld", Dest=_bits(d, 16, 8), SrcA=RZ, Imm8=0, DestPred=_bits(d, 81, 3),
                      PixMode=PIXLD_MODO[modo], **_guarda(d))]


REGLAS = {"TEX": baja_textura, "TLD": baja_textura, "TLD4": baja_textura, "TXD": baja_textura, "TXQ": baja_textura,
          "TMML": baja_textura, "IPA": baja_ipa, "AL2P": baja_al2p, "ISBERD": baja_isberd, "OUT": baja_out,
          "KILL": baja_kill, "PIXLD": baja_pixld}
# Los usa la comprobación general; las de aquí tienen la suya (COMPROBAR), pero quedan como referencia del papel de
# cada operando de la forma base.
ROLES = {"AL2P": ({"Dest": 0, "SrcA": 1}, None), "PIXLD": ({"Dest": 0}, None), "IPA": ({"Dest": 0}, None)}
FAMILIA = {"TEX": "Tex", "TLD": "Tld", "TLD4": "Tld4", "TXD": "Txd", "TXQ": "Txq", "TMML": "Tmml", "IPA": "Ipa",
           "AL2P": "Al2p", "ISBERD": "Isberd", "OUT": "Out", "KILL": "Kil", "PIXLD": "Pixld"}


# ---------- comprobación, independiente de las reglas: los operandos salen del texto de la forma (spec) y los
# modificadores de sus tablas; lo Maxwell se lee con las tablas de Ryujinx y se ejecuta en símbolos.

_ENUM = B.ISA["enums"]


def _enum(nombre_enum, nombre):
    return dict(_ENUM[nombre_enum])[nombre]


def _camel(txt):
    """'ARRAY_2D' -> 'Array2d', '2D' -> '_2d', 'TEX_HEADER_DIMENSION' -> 'TexHeaderDimension'."""
    s = "".join(p[:1] + p[1:].lower() for p in txt.split("_"))
    return "_" + s.lower() if s[:1].isdigit() else s


DIM_TXT = {0: "1D", 1: "2D", 2: "3D", 3: "CUBE", 4: "ARRAY_1D", 5: "ARRAY_2D", 7: "ARRAY_CUBE"}
CONSULTA_TXT = {0: "TEX_HEADER_DIMENSION", 1: "TEX_HEADER_TEXTURE_TYPE", 2: "TEX_HEADER_SAMPLER_POS"}


def _operandos(d):
    """Los operandos de la forma con sus valores: [(tipo, valor)] en el orden del texto de nvdisasm."""
    _, _, ops = sm86.partes(d["base"])
    r = []
    for k, o in enumerate(ops):
        t = sm86.operando(o)["tipo"]
        c = d["campos"]
        r.append((t, c.get(f"op{k}.n", 7 if t == "pred" else 0) if t in ("reg", "pred") else c.get(f"op{k}.v")))
    return r


class _Maquina:
    """Ejecuta palabras Maxwell en símbolos: cada registro vale ('R', n) al empezar; RZ y más allá valen 0."""

    def __init__(self):
        self.r = {}
        self.lecturas = []   # por cada textura: (operación, campos, {papel: valor})

    def val(self, n):
        return 0 if n >= RZ else self.r.get(n, ("R", n))

    def pon(self, n, v):
        if n < RZ:
            self.r[n] = v


def _fuentes_maxwell(op, c, m):
    """Qué lee el traductor de Ryujinx de una textura Maxwell, en su orden (InstEmitTexture.cs): {papel: valor}."""
    fam = op[:-1] if op.endswith("B") else op
    fam = {"Tex": "TEX", "Tld": "TLD", "Tld4": "TLD4", "Txd": "TXD", "Txq": "TXQ", "Tmml": "TMML"}[fam]
    bl = op.endswith("B")
    ra, rb = [c["SrcA"]], [c.get("SrcB", RZ)]

    def A():
        v = m.val(ra[0])
        ra[0] += 1
        return v

    def Bf():
        v = m.val(rb[0])
        rb[0] += 1
        return v
    f = {}
    if fam == "TXQ":
        if bl:
            f["h"] = A()
        f["lod"] = A()
        return fam, f
    dimn = dict((v, k) for k, v in _ENUM["TexDim"])[c["Dim"]]
    coords = {"_1d": 1, "Array1d": 1, "_2d": 2, "Array2d": 2, "_3d": 3, "Array3d": 3, "Cube": 3, "ArrayCube": 3}[dimn]
    arreglo = dimn.startswith("Array")
    xyz = ["x", "y", "z"]
    if fam == "TXD":
        if bl:
            f["h"] = A()
        for k in range(coords):
            f[xyz[k]] = A()
        emp = A()
        if arreglo or c["Toff"]:
            f["emp"] = emp
        for k in range(2 * coords):
            f[f"d{k}"] = Bf()
        return fam, f
    if arreglo:
        f["ai"] = A()
    if bl:
        f["h"] = Bf()
    for k in range(coords):
        f[xyz[k]] = A()
    if fam == "TEX":
        lod = c["Lodb"] if bl else c["Lod"]
        if lod in (2, 3, 6, 7):
            f["lod"] = Bf()
        if c["Aoffib"] if bl else c["Aoffi"]:
            f["off"] = Bf()
        if c["Dc"]:
            f["dc"] = Bf()
    elif fam == "TLD":
        if c["Lod"]:
            f["lod"] = Bf()
        if c["Toff"]:
            f["off"] = Bf()
        if c["Ms"]:
            f["ms"] = Bf()
    elif fam == "TLD4":
        if c["Toff"] in (1, 2):
            f["off"] = Bf()
        if c["Toff"] == 2:
            f["off2"] = Bf()
        if c["Dc"]:
            f["dc"] = Bf()
    return fam, f


def _ejecuta(palabras, g):
    """Corre las palabras; devuelve (máquina, errores de guarda u operación desconocida)."""
    m = _Maquina()
    errores = []
    for w in palabras:
        op, c = leer(w)
        if (c.get("Pred", 7), bool(c.get("PredInv"))) != (g["Pred"], bool(g["PredInv"])):
            errores.append(f"{op} con otra guarda")
        if op == "MovR":
            m.pon(c["Dest"], m.val(c["SrcA"]))
        elif op == "Mov32i":
            m.pon(c["Dest"], ("imm", c["Imm32"]))
        elif op == "MovC":
            m.pon(c["Dest"], ("c", c["CbufSlot"], c["CbufOffset"] * 4))
        elif op == "Iadd32i":
            m.pon(c["Dest"], ("suma", m.val(c["SrcA"]), c["Imm32"]))
        elif op in ("Tex", "TexB", "Tld", "TldB", "Tld4", "Tld4B", "Txd", "TxdB", "Txq", "TxqB", "Tmml", "TmmlB"):
            fam, f = _fuentes_maxwell(op, c, m)
            m.lecturas.append((op, c, f))
            if c["Dest"] != RZ:
                for k, comp in enumerate(x for x in range(4) if c["WMask"] >> x & 1):
                    m.pon(c["Dest"] + k, ("comp", comp))
        else:
            m.lecturas.append((op, c, None))
    return m, errores


def comprobar_textura(d, palabras):
    errores = []
    nombre = d["nombre"].split(".")[0]
    f = fichas(d)
    ops = _operandos(d)
    regs = [v for t, v in ops if t == "reg"]
    imms = [v for t, v in ops if t == "imm"]
    rd2, rd, ra = regs[0], regs[1], regs[2]
    rb = regs[3] if len(regs) > 3 else RZ
    mask = imms[-1]
    bindless = "B" in (fichas_grupo(d, 59) or ())
    banco, off = (imms[0], imms[1]) if not bindless else (None, None)
    g = _guarda(d)
    m, e = _ejecuta(palabras, g)
    errores += e
    texs = [x for x in m.lecturas if x[2] is not None]
    otras = [x[0] for x in m.lecturas if x[2] is None]
    if otras:
        errores.append(f"operaciones de más: {otras}")
    if not texs:
        return errores + ["sin textura"]
    # lista lógica de Ampere (A desde Ra, B desde Rb; reparto parejo con 4 o menos salvo TXD/TXQ)
    if nombre == "TXQ":
        dim_txt, coords, arreglo = None, 0, False
    else:
        dim_txt = DIM_TXT[_bits(d, 61, 3)]
        coords = {"1D": 1, "2D": 2, "3D": 3, "CUBE": 3}[dim_txt.replace("ARRAY_", "")]
        arreglo = dim_txt.startswith("ARRAY")
    xyz = ["x", "y", "z"][:coords]
    hb = ["h"] if bindless else []
    lod_f = "LB" if "LB" in f else "LL" if "LL" in f else "LZ" if "LZ" in f else None
    if nombre == "TXQ":
        pa, pb = hb + ["lod"], []
    elif nombre == "TXD":
        pa = hb + xyz + (["emp"] if arreglo or "AOFFI" in f else [])
        pb = [f"d{k}" for k in range(2 * coords)]
    else:
        pa = (["ai"] if arreglo else []) + xyz
        pb = list(hb)
        if nombre == "TEX":
            pb += (["lod"] if lod_f in ("LB", "LL") else []) + (["off"] if "AOFFI" in f else []) + \
                (["dc"] if "DC" in f else [])
        elif nombre == "TLD":
            pb += (["lod"] if lod_f == "LL" else []) + (["off"] if "AOFFI" in f else []) + (["ms"] if "MS" in f else [])
        elif nombre == "TLD4":
            pb += (["off"] if "AOFFI" in f else []) + (["off", "off2"] if "PTP" in f else []) + \
                (["dc"] if "DC" in f else [])
    if nombre not in ("TXD", "TXQ") and len(pa) + len(pb) <= 4:
        todo = pa + pb
        corte = {1: 1, 2: 1, 3: 2, 4: 2}[len(todo)]
        pa, pb = todo[:corte], todo[corte:]
    quiere = {}
    for base, papeles in ((ra, pa), (rb, pb)):
        for k, p in enumerate(papeles):
            quiere[p] = 0 if base == RZ or base + k >= RZ else ("R", base + k)
    for op, c, fu in texs:
        if not op.startswith(FAMILIA[nombre]) or (nombre == "TLD" and op.startswith("Tld4")):
            errores.append(f"operación {op}")
            continue
        q = dict(quiere)
        if op.endswith("B") and not bindless:
            q["h"] = ("c", banco, off * 4)
            if banco == BANCO_TEXTURAS:
                errores.append("bindless con el banco de texturas")
        elif not op.endswith("B"):
            if bindless:
                errores.append("textura ligada para una bindless")
            elif c.get("TidB") != off or banco != BANCO_TEXTURAS:
                errores.append(f"mango TidB={c.get('TidB')} vs c[{banco:#x}][{off:#x}]")
        for p, v in q.items():
            if fu.get(p) != v:
                errores.append(f"{op}: fuente {p} = {fu.get(p)} y debía ser {v}")
        if c["Nodep"] != ("NODEP" in f):
            errores.append("NODEP")
        if nombre == "TXQ":
            if c["TexQuery"] != _enum("TexQuery", _camel(CONSULTA_TXT[_bits(d, 62, 2)])):
                errores.append("consulta de TXQ")
            continue
        if c["Dim"] != _enum("TexDim", _camel(dim_txt)):
            errores.append(f"tipo {c['Dim']} vs {dim_txt}")
        bl = op.endswith("B")
        if nombre == "TEX":
            lod = c["Lodb"] if bl else c["Lod"]
            if lod != (_enum("Lod", _camel(lod_f)) if lod_f else 0):
                errores.append(f"LOD {lod} vs {lod_f}")
            if (c["Aoffib"] if bl else c["Aoffi"]) != ("AOFFI" in f) or c["Dc"] != ("DC" in f) or \
                    c["Ndv"] != ("NDV" in f):
                errores.append("AOFFI/DC/NDV")
        elif nombre == "TLD":
            if c["Lod"] != (lod_f == "LL") or c["Toff"] != ("AOFFI" in f) or c["Ms"] != ("MS" in f) or \
                    c["Cl"] != ("CL" in f):
                errores.append("LL/AOFFI/MS/CL")
        elif nombre == "TLD4":
            comp = [x for x in "RGBA" if x in fichas_grupo(d, 87)]
            if c["TexComp"] != _enum("TexComp", comp[0]) or c["Dc"] != ("DC" in f) or \
                    c["Toff"] != (1 if "AOFFI" in f else 2 if "PTP" in f else 0):
                errores.append("componente/DC/desplazamiento de TLD4")
        elif nombre == "TXD":
            if c["Toff"] != ("AOFFI" in f):
                errores.append("AOFFI")
        elif nombre == "TMML" and c["Ndv"] != ("NDV" in f):
            errores.append("NDV")
    # destinos: los componentes pedidos, dos en Rd y el resto en Rd2; los demás registros como estaban
    comps = [k for k in range(4) if mask >> k & 1]
    final = {}
    for k, comp in enumerate(comps):
        r = (rd + k) if k < 2 else (rd2 + k - 2)
        if (rd if k < 2 else rd2) != RZ and r < RZ:
            final[r] = ("comp", comp)
    for r in range(TEMPS_ORDEN[0]):
        if m.val(r) != final.get(r, ("R", r)):
            errores.append(f"R{r} = {m.val(r)} y debía ser {final.get(r, ('R', r))}")
    return errores


def comprobar_io(d, palabras):
    """IPA, AL2P, ISBERD, OUT y PIXLD: campos Maxwell contra los operandos y modificadores de la forma."""
    nombre = d["nombre"].split(".")[0]
    f = fichas(d)
    ops = _operandos(d)
    g = _guarda(d)
    m, errores = _ejecuta(palabras, g)
    op, c = leer(palabras[-1])
    if not op.startswith(FAMILIA[nombre]):
        return errores + [f"operación {op}"]
    regs = [v for t, v in ops if t == "reg"]
    if nombre == "OUT" and d["op"] == "0x324":   # la forma base no tiene Rd ni Rb (???): de los bits
        regs = [_bits(d, 16, 8), regs[0], _bits(d, 32, 8)]
    if c["Dest"] != regs[0]:
        errores.append(f"Dest={c['Dest']} vs {regs[0]}")
    if nombre == "IPA":
        modo = [x for x in ("PASS", "CONSTANT", "STATE") if x in f]
        if c["IpaOp"] != _enum("IpaOp", {"PASS": "Pass", "CONSTANT": "Constant", "STATE": "Sc"}[modo[0]]):
            errores.append("modo de IPA")
        if c["Msi"] != (1 if "CENTROID" in f else 2 if "OFFSET" in f else 0) or c["Idx"] or c["Sat"]:
            errores.append("muestra/Idx/Sat de IPA")
        if c["Imm10"] != _bits(d, 64, 8) * 4:
            errores.append(f"atributo {c['Imm10']:#x}")
        if "OFFSET" in f:
            t, v = ops[3]
            quiere = ("R", v) if t == "reg" and v != RZ else 0 if t == "reg" else ("imm", v & 0xFFFFFFFF) \
                if t == "imm" else ("c", d["campos"]["op3.banco"], B._const_off(d, 3))
            if m.val(c["SrcC"]) != quiere:
                errores.append(f"desplazamiento de muestra {m.val(c['SrcC'])} vs {quiere}")
    elif nombre == "AL2P":
        imm = _bits(d, 40, 11)   # la especificación deja el bit de signo (50) entre los raros
        tam = 1 if "64" in f else 2 if "96" in f else 3 if "128" in f else 0
        if c["SrcA"] != regs[1] or c["Imm11"] != imm or c["AlSize"] != tam or c["Aio"] != ("O" in f) or \
                c["DestPred"] != 7:
            errores.append("AL2P")
    elif nombre == "ISBERD":
        base = 1 if "PATCH" in f else 2 if "PRIM" in f else 3 if "ATTR" in f else 0
        # [Ra+imm]: la especificación no separa el registro ni el inmediato del operando de memoria
        ra, imm = _bits(d, 24, 8), _bits(d, 40, 16)
        fuente = ("R", ra) if ra != RZ else 0
        quiere = ("suma", fuente, imm) if imm else fuente
        if m.val(c["SrcA"]) != quiere or c["IBase"] != base or c["O"] != ("O" in f):
            errores.append(f"ISBERD fuente {m.val(c['SrcA'])} vs {quiere}")
    elif nombre == "OUT":
        tipo = 3 if "EMIT_THEN_CUT" in f else 1 if "EMIT" in f else 2 if "CUT" in f else 0
        if c["SrcA"] != regs[1] or c["OutType"] != tipo:
            errores.append("OUT")
        t, v = ("reg", regs[2]) if d["op"] == "0x324" else ops[2]
        if t == "imm":
            v = _bits(d, 32, 32)   # la especificación deja el bit 63 (signo) entre los raros
            ok = ("Imm20" in c and (c["Imm20"] | (0xFFF00000 if c["Imm20"] & 0x80000 else 0)) == v) or \
                ("SrcB" in c and m.val(c["SrcB"]) == ("imm", v))
        else:
            ok = c["SrcB"] == (RZ if tipo == 2 else v)
        if not ok:
            errores.append("operando B de OUT")
    elif nombre == "PIXLD":
        modo = {"COVMASK": "Covmask", "CENTROID_OFFSET": "CentroidOffset", "MY_INDEX": "MyIndex"}
        quiere = [_enum("PixMode", v) for k, v in modo.items() if k in f]
        if [c["PixMode"]] != quiere or c["DestPred"] != [v for t, v in ops if t == "pred"][0]:
            errores.append("PIXLD")
    return errores


def comprobar_kill(d, palabras):
    """Para los 128 valores de P0..P6: descarta lo mismo que Ampere y deja los predicados como estaban."""
    g = _guarda(d)
    ops = _operandos(d)
    q = ops[0][1]
    qn = d["campos"].get("op0.not", 0)
    errores = []
    for pr in range(128):
        def p(n, inv):
            return bool(((pr >> n) & 1) if n < 7 else 1) != bool(inv)
        quiere = p(g["Pred"], g["PredInv"]) and p(q, qn)
        preds = [(pr >> n) & 1 for n in range(7)]
        regs = {}
        mata = False
        for w in palabras:
            op, c = leer(w)

            def pv(n, inv=0):
                return bool(preds[n] if n < 7 else 1) != bool(inv)
            if not pv(c.get("Pred", 7), c.get("PredInv", 0)):
                continue
            if op == "Kil":
                mata = True
                break
            if op == "P2rI":
                regs[c["Dest"]] = sum(preds[n] << n for n in range(7)) & c["Imm20"]
            elif op == "R2pI":
                for n in range(7):
                    if c["Imm20"] >> n & 1:
                        preds[n] = (regs.get(c["SrcA"], 0) >> n) & 1
            elif op == "Psetp":
                if c["BoolOpAB"] != 0 or c["BoolOpC"] != 0:
                    return [f"PSETP con otra operación {c['BoolOpAB']}/{c['BoolOpC']}"]
                v = pv(c["Src2Pred"], c["Src2PredInv"]) and pv(c["Src1Pred"], c["Src1PredInv"]) and \
                    pv(c["SrcPred"], c["SrcPredInv"])
                if c["DestPred"] < 7:
                    preds[c["DestPred"]] = int(v)
                if c["DestPredInv"] < 7:
                    preds[c["DestPredInv"]] = int(not v)
            elif op != "Nop":
                return [f"operación {op}"]
        if mata != quiere:
            errores.append(f"PR={pr:#x}: descarta {mata} y debía {quiere}")
        elif not mata and preds != [(pr >> n) & 1 for n in range(7)]:
            errores.append(f"PR={pr:#x}: los predicados no quedan como estaban")
        if len(errores) > 3:
            break
    return errores


COMPROBAR = {n: comprobar_textura for n in ("TEX", "TLD", "TLD4", "TXD", "TXQ", "TMML")}
COMPROBAR.update({n: comprobar_io for n in ("IPA", "AL2P", "ISBERD", "OUT", "PIXLD")})
COMPROBAR["KILL"] = comprobar_kill
