"""Bajada acotada de SULD.P Ampere a Maxwell para superficies 2D.

Implementación propia para nxrecomp. La superficie se representa en Maxwell
como SULD.P; el traductor de Maxwell ya convierte esa operación a lectura de
imagen Vulkan. Solo se acepta el formato directo 2D observado y se rechazan
bindings distintos de cero, modos o campos que este camino no transporta.
"""
import sys

import sm86_baja as B
from sm86_baja import SinRegla, codificar, leer


def _bits(d, start, width):
    raw = (d["lo"] | (d["hi"] << 64))
    return (raw >> start) & ((1 << width) - 1)


def baja_suld(d):
    if d["lo"] & 0xFFF != 0xB97:
        raise SinRegla("opcode SULD distinto de 0xb97")

    dim = _bits(d, 61, 3)
    descriptor_buffer = _bits(d, 54, 5)
    descriptor_offset = _bits(d, 40, 14)
    dst = _bits(d, 16, 8)
    coord = _bits(d, 24, 8)
    channels = _bits(d, 72, 4)

    # Las palabras 2 y 3 identifican el modo SULD.P directo admitido. Solo
    # cambia el channel mask (bits 72..75) y la guarda normal de la instrucción.
    word2 = (d["hi"] & 0xFFFFFFFF)
    word3 = (d["hi"] >> 32) & 0xFFFFFFFF
    if (word2 & ~0xF00) != 0x001E0000 or word3 != 0x000F6200:
        raise SinRegla("SULD.P con formato/modificadores no admitidos")
    if dim != 3:
        raise SinRegla(f"SULD.P dimensión {dim}; solo 2D está implementada")
    if descriptor_buffer != 0:
        raise SinRegla(f"SULD.P usa el descriptor buffer {descriptor_buffer}; solo se transporta el 0")
    if not channels:
        raise SinRegla("SULD.P sin canales")
    if dst == 255 or coord > 253:
        raise SinRegla("SULD.P usa un registro reservado o no tiene dos coordenadas consecutivas")
    if descriptor_offset > 0x1FFF:
        raise SinRegla("SULD.P descriptor offset fuera del alcance Maxwell")

    g = B._guarda(d)
    # Maxwell SULD.P toma las coordenadas x/y en SrcA/SrcA+1. TidB conserva
    # el desplazamiento del descriptor en el buffer de imágenes.
    return [codificar("Suld", Dest=dst, SrcA=coord, Clamp=0,
                      TidB=descriptor_offset, Dim=dim, DestPred2=0,
                      CacheOp=0, Rgba=channels, **g)]


def comprobar_suld(d, palabras):
    errores = []
    if len(palabras) != 1:
        return [f"SULD.P produjo {len(palabras)} instrucciones Maxwell"]
    op, c = leer(palabras[0])
    esperado = {
        "Dest": _bits(d, 16, 8), "SrcA": _bits(d, 24, 8),
        "Pred": B._guarda(d)["Pred"], "Dim": 3, "TidB": _bits(d, 40, 14),
        "Rgba": _bits(d, 72, 4), "DestPred2": 0,
    }
    if op != "Suld":
        errores.append(f"operación Maxwell {op}, se esperaba Suld")
    for campo, valor in esperado.items():
        if c.get(campo) != valor:
            errores.append(f"{campo}={c.get(campo)} vs {valor}")
    g = B._guarda(d)
    if (c.get("Pred"), bool(c.get("PredInv"))) != (g["Pred"], g["PredInv"]):
        errores.append("guarda distinta")
    return errores


REGLAS = {"SULD": baja_suld}
COMPROBAR = {"SULD": comprobar_suld}


def probar():
    # Captura real de Mario Tennis Fever, shader 44e263c8…, offset 0x140:
    # SULD.P.2D.R.IGN R3, [R4], descriptor-buffer 0, offset 8.
    d = {"lo": 0x6000080004038B97, "hi": 0x000F6200001E0100,
         "nombre": "SULD.P.2D.EF.INVALID0.IGN", "guarda": "!P0"}
    B._cargar_modulos()
    salida = B.bajar(d)
    errores = comprobar_suld(d, salida)
    if errores:
        raise AssertionError("; ".join(errores))
    errores_generales = B.comprobar(d, salida)
    if errores_generales:
        raise AssertionError("comprobación integrada: " + "; ".join(errores_generales))
    _, c = leer(salida[0])
    assert (c["Dest"], c["SrcA"], c["Dim"], c["TidB"], c["Rgba"]) == (3, 4, 3, 8, 1)

    for cambio, razon in (({"lo": d["lo"] ^ (1 << 61)}, "dimensión"),
                          ({"lo": d["lo"] | (1 << 54)}, "buffer")):
        mal = {**d, **cambio}
        try:
            B.bajar(mal)
        except SinRegla:
            pass
        else:
            raise AssertionError(f"debió rechazar {razon}")
    print("OK: SULD.P.2D del shader MTF baja a Maxwell Suld; dimensiones y buffers fuera de alcance se rechazan")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "probar":
        probar()
    else:
        raise SystemExit("uso: sm86_baja_suld.py probar")
