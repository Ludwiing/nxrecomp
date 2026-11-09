"""Arma un programa Maxwell entero desde el área de código de un programa SM86 (Ampere, Switch 2), para que el
traductor de Ryujinx (totk_xlate) lo pase a SPIR-V. Código propio (MIT).

    python3 tools/sm86_programa.py armar <área.bin> <etapa> <salida.bin> [--c1 c1.bin] [--largo n]
    python3 tools/sm86_programa.py traducir <área.bin> <etapa> <dir> [--c1 c1.bin] [--largo n]
                                   arma y pasa el resultado por totk_xlate (TOTK_XLATE o el de totk-pc-recomp)

<etapa>: vs, ps, gs, tcs, tes o cs. El área es la de runtime/nvn/nvn2_program.h: en etapas gráficas empieza con la
cabecera SPH de 128 bytes; después las instrucciones de 16 bytes (`--largo`: bytes de instrucciones, sin la cabecera;
por omisión hasta el final del área). c1 es el de la sección de control (c1_off/c1_tam).

Lo que hace:
 1. Cabecera: la SPH de Ampere es de 32 palabras; la de Maxwell, de 20 (0x50 bytes). Las palabras 0-19 tienen el mismo
    diseño (tipo, versión, etapa, MRT, kills, memoria local, mapas de entradas IMAP en 6-17, salidas OMAP en 18-19:
    las mismas posiciones que lee Ryujinx en ShaderHeader.cs y que escribe Mesa para Turing/Ampere), así que se copian.
    Las palabras 20-31 no tienen lugar en Maxwell y no se sabe qué guardan (en los shaders vistos hasta ahora:
    ninguno; falta un juego real): si no son cero se avisa. Un shader de cómputo no tiene cabecera.
 2. Cada instrucción se decodifica (tools/sm86.py; las formas con descriptor uniforme que no están en la spec, con
    sm86_baja_saltos.crudo) y se baja con sm86_baja.bajar. Una instrucción que no se puede leer o bajar es un error
    con su dirección y su texto (nvdisasm, si está).
 3. Registros reservados: R188..R250 (URn), R251 (UPR, los predicados uniformes) y R252..R254 (los temporales
    sm86_baja.TEMPS). Si el shader usa alguno como registro común, error claro: no hay dónde ponerlos sin reasignar
    registros (eso no se hace todavía). El ancho de cada operando se estima por el nombre (.64/.WIDE/F64 = 2,
    .128 y texturas = 4) y por los bits crudos en la memoria.
 4. Saltos: los marcadores de sm86_baja_saltos.Reubicar se resuelven con el mapa instrucción SM86 -> primera palabra
    Maxwell. BRX (salto por tabla): la tabla de c1 (LDC Rn, c[0x1][Rx+base] antes del BRX, con su IMNMX.U32 de tope)
    se reescribe con desplazamientos Maxwell, y ese LDC se baja al patrón que Ryujinx sabe leer
    (SHR Rx 2, IMNMX.U32 imm, SHL 2, LDC c[1]: el mismo valor, Rx ya viene acotado y multiplicado por 4). El c1 reescrito sale en el resultado: el runtime tiene que enlazar
    ese c1, no el del juego.
 5. Palabras de control del planificador: una cada 3 instrucciones (antes de cada grupo), con un valor seguro
    (CONTROL: cada instrucción espera 15 ciclos, sin barreras); Ryujinx las salta. Se rellena con NOP hasta múltiplo
    de 3.
 6. Salida en el formato de totk_xlate (tools/xlate/Program.cs de totk-pc-recomp): gráfico = SPH de 0x50 bytes +
    instrucciones; cómputo = instrucciones. El job: {"key", "stage": "vs|ps|cs", "code": ruta, "c1": hex, "local":
    [x, y, z], ...}; responde "ok <key>" y deja <out>/<key>.spv y <key>.res.json.
"""
import json
import os
import struct
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, AQUI)
import sm86  # noqa: E402
import sm86_baja  # noqa: E402

sm86_baja._cargar_modulos()
import sm86_baja_saltos as saltos  # noqa: E402

RESERVADOS = range(188, 255)       # R188..R250 URn, R251 UPR, R252..R254 TEMPS
SPH_SM86, SPH_MAXWELL = 0x80, 0x50
# Palabra de control de Maxwell: tres ranuras de 21 bits (una por instrucción del grupo): espera 15 ciclos (bits 0-3),
# sin barrera de escritura ni de lectura (7 en los bits 5-7 y 8-10), sin esperar barreras ni reusar operandos.
_RANURA = 0xF | 7 << 5 | 7 << 8
CONTROL = _RANURA | _RANURA << 21 | _RANURA << 42
ETAPAS_SPH = {"vs": 1, "tcs": 2, "tes": 3, "gs": 4, "ps": 5}   # ShaderType de la SPH


class ErrorPrograma(Exception):
    pass


_SPEC = None


def spec():
    global _SPEC
    if _SPEC is None:
        _SPEC = json.load(open(os.path.join(RAIZ, "build", "sm86_spec.json")))
    return _SPEC


def _texto(lo, hi):
    try:
        t = saltos._texto(lo, hi)
    except Exception:
        t = None
    return t or f"{lo:016x} {hi:016x}"


def decodificar(lo, hi):
    d = sm86.decodificar(lo, hi, spec())
    if d and d.get("invalida"):
        d = None
    return d or saltos.crudo(lo, hi)


ANCHO4 = ("TEX", "TLD", "TLD4", "TXD", "TMML", "TXQ", "SULD", "SUST", "SUATOM", "SURED", "IPA", "ALD", "AST", "OUT")


def _ancho(nombre):
    partes = nombre.split(".")
    if "128" in partes or partes[0] in ANCHO4:
        return 4
    if "64" in partes or "WIDE" in partes or "F64" in partes or partes[0] in ("DADD", "DMUL", "DFMA", "DSETP", "DMNMX"):
        return 2
    return 1


def registros(d):
    """{registro común: ancho} que la instrucción nombra (estimado; ver arriba)."""
    usados = {}
    nombre = d["nombre"]
    if d.get("base") and d.get("campos"):
        _, _, ops = sm86.partes(d["base"])
        for i, o in enumerate(ops):
            if sm86.operando(o)["tipo"] == "reg" and f"op{i}.n" in d["campos"]:
                usados[d["campos"][f"op{i}.n"]] = _ancho(nombre)
    fam = saltos.FAMILIA.get(nombre.split(".")[0])
    ancho = {5: 2, 6: 4}.get(saltos._bits(d, 73, 3), 1)
    campos = {"Ldg": ((16, ancho), (24, 2)), "Lds": ((16, ancho), (24, 1)), "Ldl": ((16, ancho), (24, 1)),
              "Stg": ((24, 2), (32, ancho)), "Sts": ((24, 1), (32, ancho)), "Stl": ((24, 1), (32, ancho)),
              "Atom": ((16, 2), (24, 2), (32, 2)), "Red": ((24, 2), (32, 2)), "Atoms": ((16, 2), (24, 1), (32, 2)),
              "Brx": ((24, 1),), "Ret": ((24, 2),)}.get(fam, ())
    if nombre.startswith("ST.") and not (int(d["op"], 16) & 0x800 and saltos._bits(d, 91)):
        campos = ((24, 2), (64, ancho))   # ST genérico sin descriptor: el dato va en 64-71
    for a, w in campos:   # los de memoria y saltos: de los bits crudos (la spec no tiene las direcciones)
        r = saltos._bits(d, a, 8)
        usados[r] = max(usados.get(r, 0), w)
    return {r: w for r, w in usados.items() if r != 255}


def _reservado(d):
    for r, w in registros(d).items():
        for k in range(w):
            if r + k in RESERVADOS:
                return r + k
    return None


def _sph(area, avisos, etapa):
    sph = area[:SPH_SM86]
    if len(sph) < SPH_SM86:
        raise ErrorPrograma("el área es más corta que la cabecera de 128 bytes")
    palabras = struct.unpack("<32I", sph)
    if any(palabras[20:]):
        avisos.append("SPH: palabras 20-31 no nulas (se descartan): " + " ".join(f"{w:08x}" for w in palabras[20:]))
    tipo = palabras[0] >> 10 & 0xF
    if etapa in ETAPAS_SPH and tipo != ETAPAS_SPH[etapa]:
        avisos.append(f"SPH: dice etapa {tipo}, se pidió {etapa}")
    return sph[:SPH_MAXWELL]


def _direccion(k):
    """Dirección (desde el comienzo de las instrucciones) de la instrucción Maxwell número k."""
    return (k // 3) * 32 + 8 + (k % 3) * 8


def _escribe_rel(w, rel):
    if not -(1 << 23) <= rel < (1 << 23):
        raise ErrorPrograma(f"salto de {rel} bytes no cabe en 24 bits")
    return (int(w) & ~(0xFFFFFF << 20)) | ((rel & 0xFFFFFF) << 20)


def _brx(i, dbrx, ds, c1):
    """El LDC de la tabla del BRX y su bajada al patrón que reconoce Ryujinx: (índice del LDC, palabras, base,
    cuántas entradas)."""
    reg = dbrx["reg"]
    ldc = None
    for j in range(i - 1, max(-1, i - 16), -1):
        d = ds[j]
        if d["nombre"].startswith("LDC") and saltos._bits(d, 16, 8) == reg:
            ldc = j
            break
    if ldc is None:
        raise ErrorPrograma(f"BRX R{reg} en {i * 16:#x}: no encuentro el LDC de su tabla")
    d = ds[ldc]
    banco, base, rx = saltos._bits(d, 54, 5), saltos._sx(saltos._bits(d, 38, 16), 16), saltos._bits(d, 24, 8)
    if banco != 1 or saltos._bits(d, 91) or saltos._bits(d, 78, 2) or saltos._bits(d, 73, 3) != 4:
        raise ErrorPrograma(f"BRX en {i * 16:#x}: la tabla no es un LDC de 32 bits de c[0x1] (banco {banco})")
    if d["guarda"] != "PT":
        raise ErrorPrograma(f"BRX en {i * 16:#x}: el LDC de la tabla tiene guarda")
    tope = None
    for j in range(ldc - 1, max(-1, ldc - 16), -1):
        d = ds[j]
        if d["nombre"].startswith("IMNMX") and ".U32" in d["nombre"] and sm86_baja._fuente(d, 2) == "imm":
            tope = d["campos"].get("op2.v") & 0xFFFFFFFF
            break
    if tope is None or tope > 255:
        raise ErrorPrograma(f"BRX en {i * 16:#x}: no encuentro el IMNMX.U32 con el tope de la tabla")
    if base < 0 or base + 4 * (tope + 1) > len(c1):
        raise ErrorPrograma(f"BRX en {i * 16:#x}: la tabla c1[{base:#x}..] se sale de c1 ({len(c1)} bytes)")
    T = sm86_baja.TEMPS[0]
    previas = [sm86_baja.codificar("ShrI", Dest=T, SrcA=rx, Imm20=2),
               sm86_baja.codificar("ImnmxI", Dest=T, SrcA=T, Imm20=tope, Signed=0, SrcPred=7),
               sm86_baja.codificar("ShlI", Dest=T, SrcA=T, Imm20=2),
               sm86_baja.codificar("Ldc", Dest=reg, SrcA=T, CbufSlot=1, CbufOffset=base & 0xFFFF, LsSize=4)]
    return ldc, previas, base, tope + 1


def armar(area, etapa, c1=b"", largo=None):
    """{'codigo': bytes para totk_xlate, 'c1': bytes (reescrito si hay BRX), 'avisos': [...], 'mapa': [primera
    instrucción Maxwell de cada instrucción SM86]}."""
    grafica = etapa != "cs"
    avisos = []
    cab = _sph(area, avisos, etapa) if grafica else b""
    inicio = SPH_SM86 if grafica else 0
    fin = len(area) if largo is None else inicio + largo
    if (fin - inicio) % 16 or fin > len(area):
        raise ErrorPrograma(f"largo de instrucciones {fin - inicio} (tiene que ser múltiplo de 16 y caber en el área)")
    crudas = [struct.unpack_from("<QQ", area, a) for a in range(inicio, fin, 16)]
    ds, bajadas = [], []
    for i, (lo, hi) in enumerate(crudas):
        d = decodificar(lo, hi)
        if not d:
            raise ErrorPrograma(f"{i * 16:#06x}: instrucción desconocida: {_texto(lo, hi)}")
        r = _reservado(d)
        if r is not None:
            raise ErrorPrograma(f"{i * 16:#06x}: usa R{r}, reservado para los uniformes y temporales de la bajada "
                                f"(R188-R254): {_texto(lo, hi)}")
        try:
            bajadas.append(list(sm86_baja.bajar(d)))
        except sm86_baja.SinRegla as e:
            raise ErrorPrograma(f"{i * 16:#06x}: sin regla ({e}): {_texto(lo, hi)}")
        ds.append(d)
    # BRX: patrón de Ryujinx delante y tabla de c1 reescrita (después de saber dónde cae cada cosa)
    c1_nuevo = bytearray(c1)
    tablas = []
    for i, ws in enumerate(bajadas):
        for k, w in enumerate(ws):
            if isinstance(w, saltos.Reubicar) and w.brx is not None:
                ldc, previas, base, n = _brx(i, w.brx, ds, c1)
                bajadas[ldc] = previas
                tablas.append((i, k, base, n, w.brx["imm"]))
    mapa, n = [], 0
    for ws in bajadas:
        mapa.append(n)
        n += len(ws)
    mapa.append(n)   # el final: destino de un salto a la instrucción que sigue a la última
    palabras = []
    for i, ws in enumerate(bajadas):
        for k, w in enumerate(ws):
            yo = mapa[i] + k
            if isinstance(w, saltos.Reubicar) and w.brx is None:
                if w.sm86 is not None:
                    destino = i * 16 + 16 + w.sm86
                    if destino % 16 or not 0 <= destino <= len(crudas) * 16:
                        raise ErrorPrograma(f"{i * 16:#06x}: salto a {destino:#x}, fuera del programa")
                    a = mapa[destino // 16]
                else:
                    a = mapa[i] + w.local
                w = _escribe_rel(w, _direccion(a) - (_direccion(yo) + 8))
            elif isinstance(w, saltos.Reubicar):
                w = _escribe_rel(w, 0)
            palabras.append(int(w))
    for i, k, base, cuantos, imm in tablas:   # la tabla: destino SM86 -> desplazamiento desde BRX + 8 en Maxwell
        yo = mapa[i] + k
        for e in range(cuantos):
            viejo = struct.unpack_from("<i", c1, base + 4 * e)[0]
            destino = i * 16 + 16 + imm + viejo
            if destino % 16 or not 0 <= destino < len(crudas) * 16:
                raise ErrorPrograma(f"BRX en {i * 16:#x}: entrada {e} de la tabla apunta a {destino:#x}")
            struct.pack_into("<i", c1_nuevo, base + 4 * e, _direccion(mapa[destino // 16]) - (_direccion(yo) + 8))
    nop = sm86_baja.codificar("Nop", Ccc=15)
    while len(palabras) % 3:
        palabras.append(nop)
    salida = bytearray(cab)
    for g in range(0, len(palabras), 3):
        salida += struct.pack("<4Q", CONTROL, *palabras[g:g + 3])
    return {"codigo": bytes(salida), "c1": bytes(c1_nuevo), "avisos": avisos, "mapa": mapa}


def totk_xlate():
    """La ruta de totk_xlate: TOTK_XLATE, o el de ~/Projects/totk-pc-recomp (como app/nx2linux/motor.py)."""
    r = os.environ.get("TOTK_XLATE") or os.path.expanduser("~/Projects/totk-pc-recomp/build/xlate/totk_xlate")
    return r if os.path.isfile(r) and os.access(r, os.X_OK) else None


def traducir(programa, etapa, directorio, clave="sm86", local=(1, 1, 1), extra=None):
    """Pasa un programa armado por totk_xlate --serve. Devuelve (respuesta, ruta .spv, res.json como dict o None)."""
    exe = totk_xlate()
    if not exe:
        raise ErrorPrograma("no está totk_xlate (TOTK_XLATE)")
    os.makedirs(directorio, exist_ok=True)
    ruta = os.path.join(directorio, clave + ".code")
    open(ruta, "wb").write(programa["codigo"])
    job = {"key": clave, "stage": etapa, "debug": False, "code": ruta,
           "c1": programa["c1"].hex()}
    if etapa == "cs":
        job["local"] = list(local)
    job.update(extra or {})
    r = subprocess.run([exe, "--serve"], input=json.dumps({"job": job, "out": directorio}) + "\n",
                       capture_output=True, text=True, timeout=120)
    respuesta = (r.stdout.strip().splitlines() or [r.stderr.strip()])[0]
    spv = os.path.join(directorio, clave + ".spv")
    res = os.path.join(directorio, clave + ".res.json")
    return respuesta, spv, json.load(open(res)) if os.path.isfile(res) and respuesta.startswith("ok") else None


def main():
    a = sys.argv[1:]
    if len(a) < 4 or a[0] not in ("armar", "traducir"):
        sys.exit(__doc__)
    c1 = open(a[a.index("--c1") + 1], "rb").read() if "--c1" in a else b""
    largo = int(a[a.index("--largo") + 1], 0) if "--largo" in a else None
    try:
        p = armar(open(a[1], "rb").read(), a[2], c1, largo)
    except ErrorPrograma as e:
        sys.exit(f"sm86_programa: {e}")
    for x in p["avisos"]:
        print("aviso:", x, file=sys.stderr)
    if a[0] == "armar":
        open(a[3], "wb").write(p["codigo"])
        if c1:
            open(a[3] + ".c1", "wb").write(p["c1"])
        print(f"{len(p['mapa']) - 1} instrucciones SM86 -> {len(p['codigo'])} bytes Maxwell en {a[3]}")
    else:
        respuesta, spv, _ = traducir(p, a[2], a[3])
        print(respuesta, spv)
        sys.exit(0 if respuesta.startswith("ok") else 1)


if __name__ == "__main__":
    main()
