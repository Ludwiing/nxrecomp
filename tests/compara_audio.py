#!/usr/bin/env python3
"""Compara un WAV grabado por el runtime con las muestras PCM de 16 bits que tendría que contener.

    python3 tests/compara_audio.py grabado.wav referencia.bin

El silencio de antes depende del tiempo real, así que se alinea por la primera muestra no nula.
Cada canal tiene que contener la referencia muestra por muestra.
"""
import struct
import sys

wav, ref_path = sys.argv[1], sys.argv[2]
d = open(wav, "rb").read()
ch = struct.unpack_from("<H", d, 22)[0]
n = struct.unpack_from("<I", d, 40)[0] // 2
s = struct.unpack_from(f"<{n}h", d, 44)
raw = open(ref_path, "rb").read()
ref = struct.unpack(f"<{len(raw) // 2}h", raw[: len(raw) // 2 * 2])
r0 = next(i for i, v in enumerate(ref) if v)
for c in range(ch):
    x = s[c::ch]
    first = next((i for i, v in enumerate(x) if v), None)
    if first is None:
        sys.exit(f"canal {c}: silencio")
    seg = x[first - r0: first - r0 + len(ref)]
    if list(seg) != list(ref):
        same = sum(a == b for a, b in zip(seg, ref))
        sys.exit(f"canal {c}: {same} de {len(ref)} muestras coinciden")
print(f"{ch} canales idénticos a la referencia ({len(ref)} muestras)")
