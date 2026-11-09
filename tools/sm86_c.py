"""Genera runtime/nvn/sm86_tablas.h (las tablas del decodificador SM86 en C) desde build/sm86_spec.json.

    python3 tools/sm86_c.py [build/sm86_spec.json] [runtime/nvn/sm86_tablas.h]

Es la misma especificación que usa tools/sm86.py, pasada a arrays estáticos para runtime/nvn/sm86_dec.c:

- sm86_ops[]: una entrada por código de operación (bits 0-11), ordenada por código; sm86_indice[código] = entrada + 1
  (0 = código desconocido).
- sm86_campos[] / sm86_tramos[]: cada campo (enum Sm86Campo: operando × tipo) con sus tramos de bits seguidos
  (bit de la instrucción, bit del valor, ancho); un campo de bits sueltos queda en varios tramos.
- sm86_mods[] / sm86_mod_nombres[]: cada grupo de bits de modificadores (siempre seguidos) con su tabla de
  2^ancho índices de nombre (SM86_SIN_NOMBRE = combinación que nvdisasm no acepta).
- sm86_vars[]: las variantes (modificadores que cambian la cantidad de operandos) con su propio mapa de campos.
- sm86_cadenas[] / sm86_cadena_ini[]: todos los nombres y formas base, en una sola tabla.

El .h generado se versiona; su cabecera lleva el sha256 de la especificación con que se hizo.
"""
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm86   # noqa: E402  (solo partes(), para contar operandos)

# Tipos de campo, en el orden del enum; un campo es operando × tipo
TIPOS = ("n", "v", "h", "neg", "abs", "not", "suf", "forma", "banco", "off", "banco_reg", "off_reg", "otro", "extra")
SIN_NOMBRE = 0xFFFF


def campo_id(nombre, n_ops):
    op, _, tipo = nombre.partition(".")
    i = int(op[2:])
    if not op.startswith("op") or tipo not in TIPOS or i >= n_ops:
        sys.exit(f"campo desconocido en la especificación: {nombre}")
    return i * len(TIPOS) + TIPOS.index(tipo)


def tramos_de(bits):
    """{bit del valor: bit de la instrucción} -> [(bit de la instrucción, bit del valor, ancho)] en tramos seguidos."""
    pares = sorted((int(k), b) for k, b in bits.items())
    tramos = []
    for k, b in pares:
        if tramos:
            b0, k0, w = tramos[-1]
            if k == k0 + w and b == b0 + w and w < 64:
                tramos[-1] = (b0, k0, w + 1)
                continue
        tramos.append((b, k, 1))
    return tramos


def filas(valores, por_fila=16, ancho=0):
    """Los valores en filas de C, separados por comas."""
    v = [str(x).rjust(ancho) for x in valores]
    return "\n".join("\t" + ",".join(v[i:i + por_fila]) + "," for i in range(0, len(v), por_fila))


def cadena_c(s):
    r = []
    for ch in s:
        if ch in '"\\':
            r.append("\\" + ch)
        elif 32 <= ord(ch) < 127:
            r.append(ch)
        else:
            for byte in ch.encode():
                r.append(f"\\{byte:03o}")
    return '"' + "".join(r) + '\\0"'


def generar(spec_ruta, salida):
    crudo = open(spec_ruta, "rb").read()
    sha = hashlib.sha256(crudo).hexdigest()
    spec = json.loads(crudo)

    cadenas, indice_cad = [], {}

    def cad(s):
        if s not in indice_cad:
            indice_cad[s] = len(cadenas)
            cadenas.append(s)
        return indice_cad[s]

    n_ops_max = 0
    for s in spec.values():
        n_ops_max = max(n_ops_max, s["n_ops"], *(len(sm86.partes(v["base"])[2]) for v in s["variantes"]))
        for c in list(s["campos"]) + [c for v in s["variantes"] for c in v["campos"]]:
            n_ops_max = max(n_ops_max, int(c.split(".")[0][2:]) + 1)

    ops, campos, tramos, mods, mod_nombres, vars_ = [], [], [], [], [], []
    max_campos = 0

    def poner_campos(dic):
        nonlocal max_campos
        ini = len(campos)
        for nombre, bits in dic.items():
            t = tramos_de(bits)
            campos.append((campo_id(nombre, n_ops_max), len(t), len(tramos)))
            tramos.extend(t)
        max_campos = max(max_campos, len(dic))
        return ini, len(dic)

    for op_txt in sorted(spec, key=lambda x: int(x, 16)):
        s = spec[op_txt]
        codigo = int(op_txt, 16)
        assert codigo < 0x1000 and op_txt == f"{codigo:#05x}", op_txt
        nombre = cad(s["nombre"])
        forma = cad(s["base"])
        c_ini, c_n = poner_campos(s["campos"])
        m_ini = len(mods)
        for t in s["modificadores"]:
            b = t["bits"]
            assert b == list(range(b[0], b[0] + len(b))) and len(b) <= 8, (op_txt, b)
            mods.append((b[0], len(b), len(mod_nombres)))
            for combo in range(1 << len(b)):
                n = t["nombres"].get(str(combo))
                mod_nombres.append(SIN_NOMBRE if n is None else cad(n))
        v_ini = len(vars_)
        for v in s["variantes"]:
            enc = list(v["encendidos"])
            assert 1 <= len(enc) <= 2, (op_txt, enc)
            vc_ini, vc_n = poner_campos(v["campos"])
            vars_.append((enc + [0] * (2 - len(enc)), len(enc), vc_ini, vc_n, cad(v["base"]),
                          len(sm86.partes(v["base"])[2])))
        ops.append((codigo, nombre, forma, c_ini, c_n, s["n_ops"], m_ini, len(s["modificadores"]), v_ini,
                    len(s["variantes"])))

    assert len(cadenas) < SIN_NOMBRE and len(campos) < 0x10000 and len(tramos) < 0x10000
    assert len(vars_) < 256 and len(mods) < 0x10000 and len(mod_nombres) < 0x10000 and n_ops_max * len(TIPOS) < 256
    assert all(len(c) < 1000 for c in cadenas)

    o = []
    w = o.append
    w("/* Tablas del decodificador SM86 (Ampere, Switch 2) para runtime/nvn/sm86_dec.c.")
    w(" * GENERADO con tools/sm86_c.py desde build/sm86_spec.json; no se edita a mano.")
    w(f" * sha256 de la especificación: {sha}")
    w(f" * {len(ops)} códigos, {len(campos)} campos en {len(tramos)} tramos, {len(mods)} grupos de modificadores")
    w(f" * ({len(mod_nombres)} combinaciones), {len(vars_)} variantes, {len(cadenas)} cadenas.")
    w(" * Licencia MIT. */")
    w("#ifndef SM86_TABLAS_H")
    w("#define SM86_TABLAS_H")
    w("")
    w(f'#define SM86_SPEC_SHA256 "{sha}"')
    w(f"#define SM86_N_OPERANDOS {n_ops_max}        /* operandos con campos: op0..op{n_ops_max - 1} */")
    w(f"#define SM86_N_TIPOS {len(TIPOS)}")
    w(f"#define SM86_N_CAMPOS {n_ops_max * len(TIPOS)}          /* SM86_N_OPERANDOS * SM86_N_TIPOS */")
    w(f"#define SM86_TABLAS_MAX_CAMPOS {max_campos}   /* la mayor cantidad de campos de una forma */")
    w(f"#define SM86_N_CADENAS {len(cadenas)}")
    w(f"#define SM86_SIN_NOMBRE 0x{SIN_NOMBRE:X}")
    w("")
    w("/* Tipo de campo: lo que el bit cambia del operando (ver _campo_operando() en tools/sm86.py) */")
    w("enum Sm86Tipo {")
    for t in TIPOS:
        w(f"\tSM86_T_{t.upper()},")
    w("};")
    w("")
    w("/* Campo = operando * SM86_N_TIPOS + tipo; SM86_OP3_NEG es \"op3.neg\" */")
    w("enum Sm86Campo {")
    for i in range(n_ops_max):
        w("\t" + " ".join(f"SM86_OP{i}_{t.upper()}," for t in TIPOS))
    w("};")
    w("")
    w("#endif /* SM86_TABLAS_H */")
    w("")
    w("#if defined(SM86_TABLAS_DATOS) && !defined(SM86_TABLAS_DATOS_H)")
    w("#define SM86_TABLAS_DATOS_H")
    w("")
    w("static const char *const sm86_tipo_nombres[SM86_N_TIPOS] = {")
    w("\t" + ", ".join(f'"{t}"' for t in TIPOS) + ",")
    w("};")
    w("")
    w("/* {bit de la instrucción, bit del valor, ancho} */")
    w(f"static const Sm86Tramo sm86_tramos[{len(tramos)}] = {{")
    w(filas((f"{{{b},{k},{a}}}" for b, k, a in tramos), 12))
    w("};")
    w("")
    w("/* {campo, tramos, primer tramo} */")
    w(f"static const Sm86CampoDef sm86_campos[{len(campos)}] = {{")
    w(filas((f"{{{c},{n},{i}}}" for c, n, i in campos), 10))
    w("};")
    w("")
    w("/* {primer bit, ancho, primer índice en sm86_mod_nombres} */")
    w(f"static const Sm86ModDef sm86_mods[{len(mods)}] = {{")
    w(filas((f"{{{b},{a},{i}}}" for b, a, i in mods), 8))
    w("};")
    w("")
    w(f"static const uint16_t sm86_mod_nombres[{len(mod_nombres)}] = {{")
    w(filas(mod_nombres, 20))
    w("};")
    w("")
    w("/* {{bits encendidos}, cuántos, primer campo, campos, forma, operandos} */")
    w(f"static const Sm86VarDef sm86_vars[{max(len(vars_), 1)}] = {{")
    w(filas((f"{{{{{e[0]},{e[1]}}},{n},{ci},{cn},{f},{no}}}" for e, n, ci, cn, f, no in vars_), 4)
      if vars_ else "\t{{0,0},0,0,0,0,0},")
    w("};")
    w("")
    w("/* {código, nombre base, forma base, primer campo, campos, operandos, primer grupo de modificadores, grupos,")
    w("    primera variante, variantes} */")
    w(f"static const Sm86OpDef sm86_ops[{len(ops)}] = {{")
    w(filas((("{0x%03x,%d,%d,%d,%d,%d,%d,%d,%d,%d}" % x) for x in ops), 4))
    w("};")
    w("")
    w("/* código de operación (bits 0-11) -> entrada de sm86_ops + 1; 0 = desconocido */")
    w("static const uint16_t sm86_indice[0x1000] = {")
    w(filas((f"[0x{op[0]:03x}]={i + 1}" for i, op in enumerate(ops)), 8))
    w("};")
    w("")
    ini, pos = [], 0
    for c in cadenas:
        ini.append(pos)
        pos += len(c.encode()) + 1
    w(f"static const uint32_t sm86_cadena_ini[{len(cadenas)}] = {{")
    w(filas(ini, 14))
    w("};")
    w("")
    w(f"static const char sm86_cadenas[{pos + 1}] =   /* + el NUL final del literal */")
    linea = ""
    for c in cadenas:
        lit = cadena_c(c)
        if linea and len(linea) + len(lit) > 110:
            w("\t" + linea)
            linea = ""
        linea += lit
    w("\t" + linea + ";")
    w("")
    w("#endif /* SM86_TABLAS_DATOS */")
    texto = "\n".join(o) + "\n"
    open(salida, "w").write(texto)
    print(f"{salida}: {len(texto)} bytes; {len(ops)} códigos, {len(campos)} campos, {len(tramos)} tramos, "
          f"{len(mods)} grupos de modificadores ({len(mod_nombres)} combinaciones), {len(vars_)} variantes, "
          f"{len(cadenas)} cadenas ({pos} bytes); spec {sha[:16]}")


def main():
    a = sys.argv[1:]
    if a[:1] in (["-h"], ["--help"]):
        sys.exit(__doc__)
    generar(a[0] if a else "build/sm86_spec.json", a[1] if len(a) > 1 else "runtime/nvn/sm86_tablas.h")


if __name__ == "__main__":
    main()
