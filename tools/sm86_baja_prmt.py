"""Bajada SM86 -> Maxwell de PRMT (permutación de bytes) sin el PRMT de Maxwell. Código propio (MIT). Lo carga
tools/sm86_baja.py (_cargar_modulos); se prueba con `python3 tools/sm86_baja_prmt.py probar 1500`, y en la GPU en
tests/sm86_programa_test.py.

Por qué: el Ryujinx que compila totk_xlate (build/xlate/ryujinx, InstEmit.cs) no implementa PrmtR, PrmtI, PrmtC ni
PrmtRc: solo deja un log, y el destino queda sin escribir. La regla anterior bajaba a PrmtR, y en la GPU un
`PRMT R5, R1, 0x1032, RZ` daba 0 en vez de 0x33441122, sin error.

Formas SM86 (nvdisasm): 0x216 selector y C en registro, 0x416 selector en registro y C inmediato, 0x616 selector en
registro y C constante, 0x816 selector inmediato (bits 32-63) y C en registro (64-71), 0xa16 selector constante. Rd
16-23, A 24-31. Modo en los bits 72-74 (0 normal, 1 F4E, 2 B4E, 4 ECL...).

Semántica del modo normal (PTX `prmt.b32`, la misma de Maxwell): los 8 bytes son A (0-3) y C (4-7); el nibble k del
selector elige el byte k del resultado: sus 3 bits bajos, cuál byte; el bit 3, replicar el bit de signo de ese byte en
los 8 bits. Solo cuentan los 16 bits bajos del selector.

Bajada (selector inmediato, modo normal): por cada byte k, BFE de 8 bits (o BFE con signo de 1 bit en el bit 7 del
byte, que da 0 o -1) del byte elegido a T1 y BFI de sus 8 bits en el byte k de T0; el último BFI escribe Rd. Rd se
escribe al final, así que puede ser A o C. Hasta 8 instrucciones.

Sin regla: selector en registro o constante (haría falta un PRMT general), los modos F4E, B4E, RC8, ECL, ECR y RC16.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm86_baja as B  # noqa: E402
from sm86_baja import RZ, TEMPS, SinRegla, codificar, _guarda  # noqa: E402
import sm86_baja_uniformes as U  # noqa: E402


def _bits(d, a, n=1):
    return (d["lo"] | d["hi"] << 64) >> a & ((1 << n) - 1)


def baja_prmt(d):
    codigo = int(d["op"], 16)
    if codigo != 0x816:
        raise SinRegla("PRMT con el selector en registro o constante (Ryujinx no tiene PRMT de Maxwell)")
    if _bits(d, 72, 3):
        raise SinRegla("PRMT con modo (F4E, B4E, ECL...)")
    g = _guarda(d)
    rd, ra, rc, sel = _bits(d, 16, 8), _bits(d, 24, 8), _bits(d, 64, 8), _bits(d, 32, 16)
    t0, t1 = TEMPS[0], TEMPS[1]
    out = []
    for k in range(4):
        n = sel >> 4 * k & 0xF
        fuente = ra if (n & 7) < 4 else rc
        pos = (n & 3) * 8
        if n & 8:   # el bit de signo del byte, repetido
            out.append(codificar("BfeI", Dest=t1, SrcA=fuente, Imm20=1 << 8 | pos + 7, Signed=1, Brev=0, **g))
        else:
            out.append(codificar("BfeI", Dest=t1, SrcA=fuente, Imm20=8 << 8 | pos, Signed=0, Brev=0, **g))
        out.append(codificar("BfiI", Dest=rd if k == 3 else t0, SrcA=t1, Imm20=8 << 8 | 8 * k, SrcC=t0, **g))
    return out


REGLAS = {"PRMT": baja_prmt}
ROLES = {}
FAMILIA = {"PRMT": "Bfe"}


# ---------- comprobar ejecutando (simulador de tools/sm86_baja_uniformes.py: BfeI y BfiI como Ryujinx)

def _ejecuta_sm86(d, hilos):
    g = d["guarda"]
    gn, ginv = (7 if g.endswith("PT") else int(g[-1])), g.startswith("!")
    sel = _bits(d, 32, 16)
    for h in hilos:
        if h["P"][gn] == ginv:
            continue
        a, c = h["R"][_bits(d, 24, 8)], h["R"][_bits(d, 64, 8)]
        bytes8 = [(a >> 8 * i) & 0xFF for i in range(4)] + [(c >> 8 * i) & 0xFF for i in range(4)]
        r = 0
        for k in range(4):
            n = sel >> 4 * k & 0xF
            b = bytes8[n & 7]
            if n & 8:
                b = 0xFF if b & 0x80 else 0
            r |= b << 8 * k
        U._w(h, _bits(d, 16, 8), r)


def comprobar_ejecutando(d, palabras):
    for semilla in range(4):
        a = U._estado(U._h(d["lo"], d["hi"], semilla))
        b = [{"R": h["R"][:], "P": h["P"][:]} for h in a]
        _ejecuta_sm86(d, a)
        try:
            U._ejecuta_maxwell(palabras, b)
        except (ValueError, KeyError) as e:
            return [f"Maxwell: {e}"]
        for i, (x, y) in enumerate(zip(a, b)):
            for t in TEMPS:
                y["R"][t] = x["R"][t]
            if x["R"] != y["R"] or x["P"] != y["P"]:
                r = next((r for r in range(256) if x["R"][r] != y["R"][r]), None)
                return [f"hilo {i} R{r}: {x['R'][r]:#x} vs {y['R'][r]:#x}" if r is not None else f"hilo {i} P"]
    return []


COMPROBAR = {"PRMT": comprobar_ejecutando}


def probar(n, semilla=86):
    """PRMT al azar (selector de 16 bits, registros, guarda y modo al azar), bajado y ejecutado contra la SM86."""
    import json
    import random
    import sm86
    spec = json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build",
                                       "sm86_spec.json")))
    claves = [k for k, s in spec.items() if s["nombre"].split(".")[0] == "PRMT"]
    rnd = random.Random(semilla)
    bien, sin, malas = 0, {}, {}
    for _ in range(n):
        k = rnd.choice(claves)
        s = spec[k]
        lo = int(k, 16) | rnd.randrange(16) << 12 | rnd.getrandbits(8) << 16 | rnd.getrandbits(8) << 24 | \
            rnd.getrandbits(32) << 32
        hi = int(s["hi"], 16) | rnd.getrandbits(8) | (rnd.getrandbits(3) << 8 if rnd.random() < 0.2 else 0)
        d = sm86.decodificar(lo, hi, spec)
        if not d or d.get("invalida"):
            continue
        if {_bits(d, 16, 8), _bits(d, 24, 8), _bits(d, 64, 8)} & set(TEMPS):
            continue   # los temporales de la bajada: sm86_programa rechaza un programa que los use
        try:
            palabras = B.bajar(d)
        except SinRegla as e:
            sin[str(e)] = sin.get(str(e), 0) + 1
            continue
        errores = comprobar_ejecutando(d, palabras)
        if errores:
            malas[f"{d['nombre']}: {errores[0]}"] = malas.get(f"{d['nombre']}: {errores[0]}", 0) + 1
        else:
            bien += 1
    return bien, sin, malas


if __name__ == "__main__":
    sys.modules["sm86_baja"] = B
    B._cargar_modulos()
    bien, sin, malas = probar(int(sys.argv[2]) if sys.argv[1:2] == ["probar"] and len(sys.argv) > 2 else 1500)
    print(f"{bien} iguales, {sum(sin.values())} sin regla, {sum(malas.values())} distintas")
    for k, v in sorted(malas.items(), key=lambda x: -x[1])[:8] + sorted(sin.items(), key=lambda x: -x[1])[:5]:
        print(f"  {v}: {k}")
    sys.exit(1 if malas else 0)
