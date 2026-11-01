#!/usr/bin/env python3
"""Genera runtime/mxisa.h (y tools/mxisa.json) con la codificación de las instrucciones de la GPU
Maxwell, a partir de las tablas del traductor de shaders de Ryujinx (MIT, third_party/LICENSE.ryujinx):
InstTable.cs (patrones de bits) e InstDecoders.cs (campos de cada instrucción y sus enums).

Uso: gen_maxwell.py <carpeta Decoders de Ryujinx.Graphics.Shader>
(nxnative la trae en dm/third_party/ryujinx/src/Ryujinx.Graphics.Shader/Decoders)."""
import json
import os
import re
import sys

src = sys.argv[1]
raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tabla = open(os.path.join(src, 'InstTable.cs')).read()
decod = open(os.path.join(src, 'InstDecoders.cs')).read()

# Enums: enum Nombre { Miembro = valor, ... }
enums = {}
for m in re.finditer(r'enum (\w+)\s*\{([^}]*)\}', decod):
    miembros = []
    for linea in m.group(2).split(','):
        linea = linea.strip()
        if not linea:
            continue
        nombre, valor = [s.strip() for s in linea.split('=')]
        miembros.append((nombre, int(valor, 0)))
    enums[m.group(1)] = miembros

# Campos: readonly struct InstX { ... public T Nombre => expr; }
structs = {}
for m in re.finditer(r'readonly struct Inst(\w+)\s*\{(.*?)\n    \}', decod, re.S):
    campos = []
    for c in re.finditer(r'public (\w+) (\w+) => (.*?);', m.group(2)):
        tipo, nombre, expr = c.groups()
        if '_opcode' not in expr:
            continue  # derivado de otros campos (Phys de Al2p/Ald)
        expr = re.sub(r'\((?:int|' + '|'.join(enums) + r')\)', '', expr)  # sin casts: todo es entero
        expr = expr.replace('_opcode', '(o)')
        campos.append((nombre, tipo, expr))
    structs[m.group(1)] = campos

# Patrones: Add("bits", InstName.X, InstEmit.Y, props)
patrones = []
for m in re.finditer(r'Add\("([01x]{64})",\s*InstName\.(\w+),\s*InstEmit\.(\w+)((?:,\s*[^)]*)?)\)', tabla):
    bits, nombre, emisor, props = m.groups()
    props = sorted(set(re.findall(r'InstProps\.(\w+)', props)) - {'None'})
    patrones.append((bits, nombre, emisor, props))

ops = sorted({p[2] for p in patrones})
for o in ops:  # TexsF16 y compañía comparten los campos de la versión de 32 bits
    if o not in structs and o.endswith('F16') and o[:-3] in structs:
        structs[o] = structs[o[:-3]]
faltan = [o for o in ops if o not in structs]
if faltan:
    print('sin estructura de campos:', faltan, file=sys.stderr)

h = ['// Generado por tools/gen_maxwell.py a partir del traductor de shaders de Ryujinx (MIT,',
     '// third_party/LICENSE.ryujinx). No editar a mano.',
     '#pragma once', '#include <stdint.h>', '']
for nombre, miembros in sorted(enums.items()):
    h.append('enum { ' + ', '.join(f'MX_{nombre}_{mn.lstrip("_")} = {v}' for mn, v in miembros) + ' };')
h.append('')
h.append('enum MxOp {\n    MXOP_INVALID,')
for o in ops:
    h.append(f'    MXOP_{o},')
h.append('    MXOP_COUNT\n};')
h.append('static const char *const mx_op_name[MXOP_COUNT] = {"invalid", ' + ', '.join(f'"{o}"' for o in ops) + '};')
h.append('')
h.append('// Las 14 primeras cifras de la instrucción eligen la operación: valor, máscara y cuántas "x" tiene.')
h.append('typedef struct { uint16_t value, mask; uint8_t xbits; uint16_t op; } MxPattern;')
h.append('static const MxPattern mx_patterns[] = {')
for bits, nombre, emisor, props in patrones:
    top = bits[:14]
    valor = int(top.replace('x', '0'), 2)
    mascara = int(''.join('0' if c == 'x' else '1' for c in top), 2)
    h.append(f'    {{0x{valor:04x}, 0x{mascara:04x}, {top.count("x")}, MXOP_{emisor}}},')
h.append('};')
h.append('')
for o in ops:
    for nombre, tipo, expr in structs.get(o, []):
        h.append(f'#define {o}_{nombre}(o) ((int64_t)({expr}))')
# Acceso genérico: mx_get(op, MXF_Campo, o); MX_NO si la instrucción no tiene ese campo.
nombres = sorted({n for o in ops for n, _, _ in structs.get(o, [])})
h.append('')
h.append('enum MxField {\n' + ''.join(f'    MXF_{n},\n' for n in nombres) + '    MXF_COUNT\n};')
h.append('#define MX_NO INT64_MIN')
h.append('static inline int64_t mx_get(int op, int f, uint64_t o) {')
h.append('    switch (op) {')
for o in ops:
    if not structs.get(o):
        continue
    h.append(f'    case MXOP_{o}:')
    h.append('        switch (f) {')
    for n, _, _ in structs[o]:
        h.append(f'        case MXF_{n}: return {o}_{n}(o);')
    h.append('        }')
    h.append('        return MX_NO;')
h.append('    }')
h.append('    return MX_NO;')
h.append('}')
open(os.path.join(raiz, 'runtime', 'mxisa.h'), 'w').write('\n'.join(h) + '\n')

datos = {'enums': enums,
         'patrones': [{'bits': b, 'nombre': n, 'op': e, 'props': p} for b, n, e, p in patrones],
         'campos': {o: [{'nombre': n, 'tipo': t, 'expr': x} for n, t, x in structs.get(o, [])] for o in ops}}
json.dump(datos, open(os.path.join(raiz, 'tools', 'mxisa.json'), 'w'), indent=1)
print(f'{len(patrones)} patrones, {len(ops)} operaciones, {len(enums)} enums')
