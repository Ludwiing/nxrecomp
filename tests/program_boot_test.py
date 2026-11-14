#!/usr/bin/env python3
"""Comprueba que el arranque no ejecuta dos veces los constructores del módulo."""
import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "recomp"))
from program import Program  # noqa: E402


def module(base, init, init_array, text=0x1000, dyn=None, starts=(), name="mod"):
    return SimpleNamespace(name=name, base=base, init=init, init_array=init_array, dyn=dyn or {},
                           eh_frame_starts=lambda: set(starts),
                           img=SimpleNamespace(text=(text, 0x1000), size=0x1000000))


p = Program.__new__(Program)
p.modules = [
    # Sports Resort main's DT_INIT materializes the start and end of its array.
    module(0x8000000, 0xD4BE80, [0x8D4C000], dyn={25: 0x4E41000, 27: 0x9230},
           starts={0xD4BE80, 0xD4BFF0}),
    # An SDK module can have both entries without dispatching the array itself.
    module(0x8100000, 0x120, [0x8100340], dyn={25: 0x2000, 27: 8},
           starts={0x120, 0x220}),
    module(0x8200000, 0, [0x8200560]),
    # rtld ya lo reemplaza el enlace del recompilador: sus constructores no se corren.
    module(0x8300000, 0x100, [0x8300200], name="rtld"),
]
p.text_ranges = []
p.word = lambda addr: {
    0x8D4BF7C: 0xD00207A0,  # ADRP x0, 0xce41000
    0x8D4BF80: 0x91000000,  # ADD x0, x0, #0
    0x8D4BFA8: 0xF00207F4,  # ADRP x20, 0xce4a000
    0x8D4BFAC: 0x9108C294,  # ADD x20, x20, #0x230 -> 0xce4a230
}.get(addr, 0)
p.symbols = {"nnMain": 0x8200000}
actual = p.boot()
expected = {
    # Solo se omite el array que el DT_INIT despacha internamente.
    "inits": [0x8D4BE80, 0x8100120, 0x8100340, 0x8200560],
    "main": 0x8200000,
    "entry": 0x8001000,
}
if actual != expected:
    raise SystemExit(f"MAL: boot() devolvió {actual!r}, se esperaba {expected!r}")
print("OK: boot() evita duplicar tablas despachadas por DT_INIT y ejecuta las demás")
