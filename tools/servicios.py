#!/usr/bin/env python3
"""Lista los servicios de Horizon que nombra un juego y cuáles tiene nxrecomp.

    python3 tools/servicios.py <carpeta ExeFS, NSO o NRO...>

Busca en los módulos los nombres de servicio (cadenas como "acc:u0" o "fsp-srv") y los cruza con los
que atiende el runtime: implementados, respondidos por la tabla de stubs, o que faltan. Lo que falta
hace que el SDK aborte al pedirlo, así que conviene saberlo antes del primer arranque.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "recomp"))
from nso import load_module  # noqa: E402

# Servicios de Horizon que una aplicación puede abrir (de SwitchBrew).
CONOCIDOS = """
acc:u0 acc:u1 acc:aa acc:su am apm apm:p appletAE appletOE aoc:u arp:r audin:u audout:u audrec:u audren:u
audctl auddev bcat:u bcat:a bcat:m bcat:s bsd:u bsd:s bsd:a btm:u caps:a caps:c caps:su caps:u caps:ss csrng
ectx:aw erpt:c err:u fatal:u friends:u friends:v fsp-srv grc:c hid hid:dbg hid:sys hidbus hwopus irs lbl ldn:u
lm lp2p:app mii:e mii:u mm:u nfc:user nfp:user ngc:u ngct:u nifm:u nifm:a nifm:s nim:eca notif:a npns:u ns:am2
ns:ec ns:rid ns:rt ns:su ns:vm ns:web ns:ro nsd:u olsc:u ovln:rcv pctl pctl:a pctl:r pctl:s pdm:ntfy pdm:qry pl:u
pl:s prepo:a prepo:a2 prepo:s prepo:u psc:m psm set set:sys sfdnsres spl: ssl time:u time:a time:s vi:u vi:s vi:m
nvdrv nvdrv:a nvdrv:s nvdrv:t pm:dmnt ldr:ro ro:1 hshl:sys hshl:set mnpp:app ins:r ins:s
""".split()


def nombres_en(datos):
    hallados = set()
    for m in re.finditer(rb"(?<![\x21-\x7e])([a-zA-Z][a-zA-Z0-9-]{1,7}(?::[a-zA-Z0-9]{0,4})?)\x00", datos):
        s = m.group(1).decode()
        if s in CONOCIDOS:
            hallados.add(s)
    return hallados


def nombres_inmediatos(datos):
    """Nombres armados como una constante de 64 bits con MOVZ/MOVK (libnx, y el SDK con constexpr)."""
    import struct
    hallados, regs = set(), {}
    for (w,) in struct.iter_unpack("<I", datos[:len(datos) & ~3]):
        if w & 0xFF800000 in (0xD2800000, 0xF2800000):  # MOVZ / MOVK de 64 bits
            rd, hw, imm = w & 31, (w >> 21) & 3, (w >> 5) & 0xFFFF
            v = 0 if w & 0xFF800000 == 0xD2800000 else regs.get(rd, 0) & ~(0xFFFF << (hw * 16))
            v |= imm << (hw * 16)
            regs[rd] = v
            s = v.to_bytes(8, "little").rstrip(b"\0")
            if 2 <= len(s) <= 8 and b"\0" not in s and s.decode("latin-1") in CONOCIDOS:
                hallados.add(s.decode())
    return hallados


def del_runtime():
    """(implementados, stubs): los nombres que aparecen en la tabla de sm: y en la de stubs."""
    impl = set(re.findall(r'\{"([^"]+)", "[^"]+", \w+\}', open(os.path.join(ROOT, "runtime", "nxservices.c")).read()))
    otros = open(os.path.join(ROOT, "runtime", "nxsvc_otros.c")).read()
    stubs = set(re.findall(r'\{"([a-z][a-z0-9-]*:[a-z0-9]*|[a-z]{2,8})", \{', otros))
    impl |= set(re.findall(r'!strcmp\(nombre, "([^"]+)"\)', otros))
    return impl, stubs - impl


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    archivos = []
    for a in sys.argv[1:]:
        if os.path.isdir(a):
            archivos += [os.path.join(a, f) for f in sorted(os.listdir(a)) if not f.endswith(".npdm")]
        else:
            archivos.append(a)
    hallados = set()
    for f in archivos:
        raw = open(f, "rb").read()
        try:
            img = load_module(raw, os.path.basename(f))
            hallados |= nombres_en(bytes(img.data)) | nombres_inmediatos(bytes(img.data))
        except Exception:
            hallados |= nombres_en(raw) | nombres_inmediatos(raw)
    impl, stubs = del_runtime()
    faltan = sorted(hallados - impl - stubs)
    print(f"{len(hallados)} servicios nombrados en {len(archivos)} módulos")
    print("  implementados: " + " ".join(sorted(hallados & impl)))
    print("  con stubs:     " + (" ".join(sorted(hallados & stubs)) or "-"))
    print("  faltan:        " + (" ".join(faltan) or "-"))
    return 1 if faltan else 0


if __name__ == "__main__":
    sys.exit(main())
