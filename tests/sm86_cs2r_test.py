#!/usr/bin/env python3
"""Regression for the proven CS2R-from-SRZ lowering (32- and 64-bit forms)."""

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import sm86  # noqa: E402
import sm86_baja as baja  # noqa: E402


def main():
    spec = json.load(open(os.path.join(ROOT, "build", "sm86_spec.json"), encoding="utf-8"))
    baja._cargar_modulos()

    # nvdisasm confirmed CS2R R0/R2, SRZ in the captured fragment shader.
    raw = int.from_bytes(bytes.fromhex("057800000000000000ff010000e20f00"), "little")
    for wide, expected_dests in ((True, (0, 1)), (False, (0,))):
        instruction = raw if wide else raw & ~(1 << 80)
        lo, hi = instruction & ((1 << 64) - 1), instruction >> 64
        decoded = sm86.decodificar(lo, hi, spec)
        words = baja.bajar(decoded)
        decoded_maxwell = [baja.leer(word) for word in words]
        if tuple(op[1].get("Dest") for op in decoded_maxwell) != expected_dests:
            raise SystemExit(f"destinos incorrectos para CS2R width={wide}: {decoded_maxwell}")
        if any(op[0] != "Mov32i" or op[1].get("Imm32") != 0 for op in decoded_maxwell):
            raise SystemExit(f"CS2R SRZ no quedó en cero: {decoded_maxwell}")

    # No convierte otras fuentes de sistema en cero silenciosamente.
    nonzero_sr = (raw & ~(0xFF << 72)) | (1 << 72)
    decoded = sm86.decodificar(nonzero_sr & ((1 << 64) - 1), nonzero_sr >> 64, spec)
    try:
        baja.bajar(decoded)
    except baja.SinRegla:
        pass
    else:
        raise SystemExit("CS2R desde un SR distinto de SRZ debe rechazarse")

    print("OK: CS2R SRZ 32/64 baja a MOV32I cero; otras fuentes se rechazan")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
