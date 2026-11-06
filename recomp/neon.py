"""Advanced SIMD (NEON) de ARMv8.0: vectores y sus formas escalares.

Cada operación se traduce a un bucle sobre los elementos. Dentro del bucle los
valores son `uint64_t` con los bits del elemento (a de Vn, b de Vm, dd de Vd); las
lecturas con signo se hacen con sx(). El resultado se guarda truncado al tamaño
del elemento en R, que empieza en cero, así que las formas de 64 bits y las
escalares dejan en cero la parte alta del registro como manda ARM.
"""

from a64 import COND, R, RW, Unsupported, bits, lit, setr  # noqa: F401

LANE = {8: "b", 16: "h", 32: "s", 64: "d"}
UT = {8: "uint8_t", 16: "uint16_t", 32: "uint32_t", 64: "uint64_t"}
STY = {8: "int8_t", 16: "int16_t", 32: "int32_t", 64: "int64_t"}
ONES = {8: "0xFFull", 16: "0xFFFFull", 32: "0xFFFFFFFFull", 64: "~0ull"}


def sx(v, es):
    """Valor con signo de un elemento de `es` bits guardado en un uint64_t."""
    return f"(int64_t)({STY[es]})({v})"


def ux(v, es):
    return f"(uint64_t)({UT[es]})({v})"


def ext(v, es, uns):
    return ux(v, es) if uns else sx(v, es)


def sat(expr, es, uns):
    return f"{'usat' if uns else 'ssat'}({expr}, {es})"


def block(i, lines):
    i.lines = ["{ " + " ".join(lines) + " }"]


def lanes(i, d, n, m, es, count, expr, *, res_es=None, read_d=False, a_off=0, b_off=0, a_es=None, b_es=None,
          keep_d=False, dst_off=0):
    """R[dst_off + e] = expr para e en [0, count).

    a y b se leen de Vn[a_off + e] y Vm[b_off + e] con su tamaño (a_es, b_es; por
    defecto es); dd de Vd[dst_off + e] con el tamaño del resultado.
    keep_d: R empieza como Vd (para escribir solo una mitad, como en XTN2).
    """
    res_es = res_es or es
    a_es = a_es or es
    b_es = b_es or es
    out = [f"V128 A = c->vr[{n}];"]
    if m is not None:
        out.append(f"V128 B = c->vr[{m}];")
    if read_d or keep_d:
        out.append(f"V128 D = c->vr[{d}];")
    out.append(f"V128 Rr = {'D' if keep_d else '{0}'};")
    body = [f"uint64_t a = A.{LANE[a_es]}[{a_off} + e];"]
    if m is not None:
        body.append(f"uint64_t b = B.{LANE[b_es]}[{b_off} + e];")
    if read_d:
        body.append(f"uint64_t dd = D.{LANE[res_es]}[{dst_off} + e];")
    body.append(f"Rr.{LANE[res_es]}[{dst_off} + e] = ({UT[res_es]})({expr});")
    out.append(f"for (int e = 0; e < {count}; e++) {{ {' '.join(body)} }}")
    out.append(f"c->vr[{d}] = Rr;")
    block(i, out)


def pairwise(i, d, n, m, es, count, expr, res_es=None):
    """R[e] = expr(a, b) sobre pares consecutivos de la concatenación Vm:Vn."""
    res_es = res_es or es
    half = count // 2 if m is not None else count
    out = [f"V128 A = c->vr[{n}];"]
    out.append(f"V128 B = c->vr[{m}];" if m is not None else "V128 B = A;")
    out.append("V128 Rr = {0};")
    L = LANE[es]
    out.append(f"for (int e = 0; e < {count}; e++) {{ V128 *S = e < {half} ? &A : &B; int k = 2 * (e < {half} ? e : e - {half}); "
               f"uint64_t a = S->{L}[k], b = S->{L}[k + 1]; Rr.{LANE[res_es]}[e] = ({UT[res_es]})({expr}); }}")
    out.append(f"c->vr[{d}] = Rr;")
    block(i, out)


# ---------- operaciones de tres registros iguales ----------

def int_same(op, u, es):
    """Expresión de una operación entera de 'three same' (o None si no existe)."""
    a, b = ext("a", es, u), ext("b", es, u)
    ones = ONES[es]
    return {
        0b00000: f"((i128){a} + {b}) >> 1",
        0b00001: sat(f"(i128){a} + {b}", es, u),
        0b00010: f"((i128){a} + {b} + 1) >> 1",
        0b00100: f"((i128){a} - {b}) >> 1",
        0b00101: sat(f"(i128){a} - {b}", es, u),
        0b00110: f"{a} > {b} ? {ones} : 0",
        0b00111: f"{a} >= {b} ? {ones} : 0",
        0b01000: f"vshl(a, b, {es}, {u}, 0, 0)",
        0b01001: f"vshl(a, b, {es}, {u}, 0, 1)",
        0b01010: f"vshl(a, b, {es}, {u}, 1, 0)",
        0b01011: f"vshl(a, b, {es}, {u}, 1, 1)",
        0b01100: f"{a} > {b} ? a : b",
        0b01101: f"{a} < {b} ? a : b",
        0b01110: f"(uint64_t)({a} > {b} ? (i128){a} - {b} : (i128){b} - {a})",
        0b01111: f"dd + (uint64_t)({a} > {b} ? (i128){a} - {b} : (i128){b} - {a})",
        0b10000: "a - b" if u else "a + b",
        0b10001: f"a == b ? {ones} : 0" if u else f"(a & b) ? {ones} : 0",
        0b10010: "dd - a * b" if u else "dd + a * b",
        0b10011: "pmul(a, b, 8)" if u else "a * b",
        0b10110: f"sqdmulh(a, b, {es}, {u})",
    }.get(op)


FP_SAME = {
    # (U, a, opcode & 7): (nombre del helper, por pares)
    (0, 0, 0): ("fmaxnm", 0), (0, 0, 1): ("fmla", 0), (0, 0, 2): ("fadd", 0), (0, 0, 3): ("fmulx", 0),
    (0, 0, 4): ("fcmeq", 0), (0, 0, 6): ("fmax", 0), (0, 0, 7): ("frecps", 0),
    (0, 1, 0): ("fminnm", 0), (0, 1, 1): ("fmls", 0), (0, 1, 2): ("fsub", 0), (0, 1, 6): ("fmin", 0),
    (0, 1, 7): ("frsqrts", 0),
    (1, 0, 0): ("fmaxnm", 1), (1, 0, 2): ("fadd", 1), (1, 0, 3): ("fmul", 0), (1, 0, 4): ("fcmge", 0),
    (1, 0, 5): ("facge", 0), (1, 0, 6): ("fmax", 1), (1, 0, 7): ("fdiv", 0),
    (1, 1, 0): ("fminnm", 1), (1, 1, 2): ("fabd", 0), (1, 1, 4): ("fcmgt", 0), (1, 1, 5): ("facgt", 0),
    (1, 1, 6): ("fmin", 1),
}


def fp_expr(name, w, a="a", b="b", dd="dd"):
    U = UT[w]
    A, B, D = f"({U}){a}", f"({U}){b}", f"({U}){dd}"
    sign = {16: "0x8000u", 32: "0x80000000u", 64: "0x8000000000000000ull"}[w]
    ones = ONES[w]
    absm = f"~({U}){sign}"
    return {
        "fmla": f"fmuladd{w}({D}, {A}, {B})",
        "fmls": f"fmuladd{w}({D}, {A} ^ {sign}, {B})",
        "fcmeq": f"fcmp{w}({A}, {B}) == 6 ? {ones} : 0",
        "fcmge": f"(fcmp{w}({A}, {B}) & 2) && fcmp{w}({A}, {B}) != 3 ? {ones} : 0",
        "fcmgt": f"fcmp{w}({A}, {B}) == 2 ? {ones} : 0",
        "facge": f"(fcmp{w}({A} & {absm}, {B} & {absm}) & 2) && fcmp{w}({A} & {absm}, {B} & {absm}) != 3 ? {ones} : 0",
        "facgt": f"fcmp{w}({A} & {absm}, {B} & {absm}) == 2 ? {ones} : 0",
    }.get(name, f"{name}{w}({A}, {B})")


# ---------- punto de entrada ----------

def decode(i, x):
    q, u = bits(x, 30, 30), bits(x, 29, 29)
    rd, rn, rm = bits(x, 4, 0), bits(x, 9, 5), bits(x, 20, 16)
    size = bits(x, 23, 22)
    scalar = bits(x, 28, 28)
    if bits(x, 31, 31) or (scalar and not q):
        raise Unsupported("NEON no asignado")

    if x & 0xFF3E0C00 == 0x4E280800 or x & 0xFFE08C00 == 0x5E000000 or x & 0xFFFE0C00 == 0x5E280800:
        return crypto(i, x, rd, rn, rm)
    if bits(x, 24, 24) == 0 and bits(x, 21, 21) and bits(x, 10, 10):  # tres iguales
        return three_same(i, x, q, u, size, rd, rn, rm, scalar)
    if bits(x, 24, 24) == 0 and not bits(x, 21, 21) and bits(x, 15, 15) and bits(x, 10, 10):  # tres iguales extra
        return three_same_extra(i, x, q, u, size, rd, rn, rm, scalar)
    if bits(x, 24, 24) == 0 and bits(x, 21, 21) and bits(x, 11, 10) == 0:  # tres distintos
        return three_diff(i, x, q, u, size, rd, rn, rm, scalar)
    if bits(x, 24, 24) == 0 and bits(x, 22, 21) == 0b10 and bits(x, 15, 14) == 0 and bits(x, 10, 10):
        return three_same_fp16(i, x, q, u, rd, rn, rm, scalar)
    if bits(x, 24, 24) == 0 and bits(x, 22, 17) == 0b111100 and bits(x, 11, 10) == 0b10:  # dos registros FP16
        return two_misc_fp(i, bits(x, 16, 12), q, u, size, rd, rn, scalar, half=True)
    if bits(x, 24, 24) == 0 and bits(x, 21, 17) == 0b10000 and bits(x, 11, 10) == 0b10:
        return two_misc(i, x, q, u, size, rd, rn, scalar)
    if bits(x, 24, 24) == 0 and bits(x, 21, 17) == 0b11000 and bits(x, 11, 10) == 0b10:
        return across(i, x, q, u, size, rd, rn, scalar)
    if bits(x, 24, 21) == 0 and bits(x, 15, 15) == 0 and bits(x, 10, 10):
        return copy(i, x, q, u, rd, rn, scalar)
    if bits(x, 24, 19) == 0b100000 and bits(x, 10, 10) and not scalar:
        return modified_imm(i, x, q, u, rd)
    if bits(x, 24, 23) == 0b10 and bits(x, 10, 10) and bits(x, 22, 19):
        return shift_imm(i, x, q, u, rd, rn, scalar)
    if bits(x, 24, 24) and bits(x, 10, 10) == 0:
        return by_element(i, x, q, u, size, rd, rn, scalar)
    if not scalar and bits(x, 29, 24) == 0b001110 and bits(x, 21, 21) == 0 and bits(x, 15, 15) == 0 and bits(x, 11, 10) == 0b10:
        return permute(i, x, q, size, rd, rn, rm)
    if not scalar and bits(x, 29, 24) == 0b101110 and bits(x, 23, 21) == 0 and bits(x, 15, 15) == 0 and not bits(x, 10, 10):
        return ext_op(i, x, q, rd, rn, rm)
    if not scalar and bits(x, 29, 24) == 0b001110 and bits(x, 23, 21) == 0 and bits(x, 15, 15) == 0 and bits(x, 11, 10) == 0:
        return tbl(i, x, q, rd, rn, rm)
    raise Unsupported("NEON no soportado (¿cripto?)")


def three_same_extra(i, x, q, u, size, rd, rn, rm, scalar):
    """Tres iguales "extra" (ARMv8.1/8.2, Switch 2): SQRDMLAH/SQRDMLSH y SDOT/UDOT."""
    op = bits(x, 14, 11)
    if op in (0b0000, 0b0001) and u and size in (1, 2):  # SQRDMLAH / SQRDMLSH
        es = 8 << size
        count = 1 if scalar else (128 if q else 64) // es
        return lanes(i, rd, rn, rm, es, count, f"sqrdmlah(dd, a, b, {es}, {op})", read_d=True)
    if op == 0b0010 and size == 2 and not scalar:  # SDOT / UDOT
        return lanes(i, rd, rn, rm, 32, 4 if q else 2, f"dd + dot4(a, b, {u})", read_d=True)
    raise Unsupported("tres iguales extra no soportado (FCMLA, FCADD, USDOT...)")


def three_same_fp16(i, x, q, u, rd, rn, rm, scalar):
    """Tres iguales de media precisión (ARMv8.2 FP16): la misma tabla que FP de 32/64 bits."""
    key = (u, bits(x, 23, 23), bits(x, 13, 11))
    if key not in FP_SAME:
        raise Unsupported("FP16 vectorial no asignado")
    name, pw = FP_SAME[key]
    if scalar and (pw or name in ("fmla", "fmls", "fmaxnm", "fminnm", "fadd", "fsub", "fmul", "fdiv", "fmax", "fmin")):
        raise Unsupported("FP16 escalar no asignado")
    count = 1 if scalar else 8 if q else 4
    if pw:
        return pairwise(i, rd, rn, rm, 16, count, fp_expr(name, 16))
    return lanes(i, rd, rn, rm, 16, count, fp_expr(name, 16), read_d=name in ("fmla", "fmls"))


def three_same(i, x, q, u, size, rd, rn, rm, scalar):
    op = bits(x, 15, 11)
    if op >= 0b11000:  # punto flotante
        sz, a = size & 1, size >> 1
        key = (u, a, op & 7)
        if key not in FP_SAME or (sz and not q):
            raise Unsupported("FP vectorial no asignado")
        name, pw = FP_SAME[key]
        w = 64 if sz else 32
        if scalar and (pw or name in ("fmla", "fmls", "fmaxnm", "fminnm", "fadd", "fsub", "fmul", "fdiv", "fmax", "fmin")):
            raise Unsupported("FP escalar no asignado")
        count = 1 if scalar else (128 if q else 64) // w
        if pw:
            return pairwise(i, rd, rn, rm, w, count, fp_expr(name, w))
        return lanes(i, rd, rn, rm, w, count, fp_expr(name, w), read_d=name in ("fmla", "fmls"))
    if op == 0b00011:  # lógicas
        if scalar:
            raise Unsupported("lógica escalar no existe")
        a, b, dd = "a", "b", "dd"
        expr = {
            (0, 0): "a & b", (0, 1): "a & ~b", (0, 2): "a | b", (0, 3): "a | ~b",
            (1, 0): "a ^ b", (1, 1): "(dd & a) | (~dd & b)", (1, 2): "(dd & ~b) | (a & b)", (1, 3): "(dd & b) | (a & ~b)",
        }[(u, size)]
        return lanes(i, rd, rn, rm, 64, 2 if q else 1, expr, read_d=(u and size > 0))
    es = 8 << size
    if size == 3 and not q:
        raise Unsupported("elementos de 64 bits en vector de 64")
    if op in (0b10100, 0b10101, 0b10111):  # por pares
        if scalar:
            raise Unsupported("por pares escalar")
        if op == 0b10111:
            if u:
                raise Unsupported("ADDP con U")
            expr = "a + b"
        else:
            if size == 3:
                raise Unsupported("por pares de 64 bits")
            a, b = ext("a", es, u), ext("b", es, u)
            expr = f"{a} > {b} ? a : b" if op == 0b10100 else f"{a} < {b} ? a : b"
        return pairwise(i, rd, rn, rm, es, (128 if q else 64) // es, expr)
    expr = int_same(op, u, es)
    if expr is None:
        raise Unsupported("tres iguales no asignado")
    no64 = {0b00000, 0b00010, 0b00100, 0b01100, 0b01101, 0b01110, 0b01111, 0b10010, 0b10011, 0b10110}
    if size == 3 and op in no64:
        raise Unsupported("operación sin elementos de 64 bits")
    if op == 0b10011 and u and size:
        raise Unsupported("PMUL solo de 8 bits")
    if op == 0b10110 and size not in (1, 2):
        raise Unsupported("SQDMULH solo de 16 y 32 bits")
    if scalar:
        ok = {0b00001, 0b00101, 0b01001, 0b01011, 0b10110}
        if not (op in ok or (size == 3 and op in (0b00110, 0b00111, 0b01000, 0b01010, 0b10000, 0b10001))):
            raise Unsupported("tres iguales escalar no asignado")
    count = 1 if scalar else (128 if q else 64) // es
    lanes(i, rd, rn, rm, es, count, expr, read_d=op in (0b01111, 0b10010))


def three_diff(i, x, q, u, size, rd, rn, rm, scalar):
    op = bits(x, 15, 12)
    es = 8 << size
    if scalar:
        if u or op not in (0b1001, 0b1011, 0b1101) or size not in (1, 2):
            raise Unsupported("tres distintos escalar no asignado")
        return long_op(i, op, 0, es, rd, rn, rm, 0, 1)
    if op == 0b1110:  # PMULL
        if u or size not in (0, 3):
            raise Unsupported("PMULL inválido")
        if size == 3:
            block(i, [f"unsigned __int128 p = pmul128(c->vr[{rn}].d[{q}], c->vr[{rm}].d[{q}]);",
                      f"c->vr[{rd}].q = p;"])
            return
        return lanes(i, rd, rn, rm, 8, 8, "pmul(a, b, 8)", res_es=16, a_off=8 * q, b_off=8 * q)
    if size == 3 or op == 0b1111:
        raise Unsupported("tres distintos inválido")
    if u and op in (0b1001, 0b1011, 0b1101):
        raise Unsupported("tres distintos no asignado")
    if op in (0b1001, 0b1011, 0b1101) and size not in (1, 2):
        raise Unsupported("SQDMLAL solo de 16 y 32 bits")
    count = 64 // es
    if op in (0b0100, 0b0110):  # ADDHN / SUBHN (con R: redondeo)
        sign = "-" if op == 0b0110 else "+"
        rnd = f" + ((i128)1 << {es - 1})" if u else ""
        expr = f"(uint64_t)(((i128)a {sign} (i128)b{rnd}) >> {es})"
        return lanes(i, rd, rn, rm, es, count, expr, a_es=2 * es, b_es=2 * es, res_es=es,
                     keep_d=bool(q), dst_off=count * q)
    return long_op(i, op, u, es, rd, rn, rm, q, count)


def long_op(i, op, u, es, rd, rn, rm, q, count):
    """Operaciones largas: el resultado tiene el doble de bits que los elementos de origen."""
    a, b = ext("a", es, u), ext("b", es, u)
    w = 2 * es
    wide_a = op in (0b0001, 0b0011)  # SADDW / SSUBW: Vn ya es ancho
    if wide_a:
        a = ext("a", w, u)
    absd = f"({a} > {b} ? (i128){a} - {b} : (i128){b} - {a})"
    dbl = f"ssat(2 * (i128){a} * {b}, {w})"
    expr = {
        0b0000: f"(i128){a} + {b}", 0b0001: f"(i128){a} + {b}",
        0b0010: f"(i128){a} - {b}", 0b0011: f"(i128){a} - {b}",
        0b0101: f"dd + (uint64_t){absd}", 0b0111: f"(uint64_t){absd}",
        0b1000: f"dd + (uint64_t)((i128){a} * {b})", 0b1010: f"dd - (uint64_t)((i128){a} * {b})",
        0b1100: f"(i128){a} * {b}",
        0b1001: f"ssat((i128){sx('dd', w)} + {sx(dbl, w)}, {w})",
        0b1011: f"ssat((i128){sx('dd', w)} - {sx(dbl, w)}, {w})",
        0b1101: dbl,
    }[op]
    off = count * q if count > 1 else 0
    lanes(i, rd, rn, rm, es, count, f"(uint64_t)({expr})", res_es=w, a_es=w if wide_a else es,
          a_off=0 if wide_a else off, b_off=off, read_d=op in (0b0101, 0b1000, 0b1010, 0b1001, 0b1011))


def two_misc(i, x, q, u, size, rd, rn, scalar):
    op = bits(x, 16, 12)
    es = 8 << size
    if op >= 0b01100 and (op >= 0b10110 or op in (0b01100, 0b01101, 0b01110, 0b01111)) and \
            not (op in (0b10010, 0b10011, 0b10100)):
        return two_misc_fp(i, op, q, u, size, rd, rn, scalar)
    count = 1 if scalar else (128 if q else 64) // es
    if op in (0b10010, 0b10100):  # XTN, SQXTUN, SQXTN, UQXTN (estrechan)
        if size == 3 or (op == 0b10010 and not u and scalar):
            raise Unsupported("estrechamiento inválido")
        w = 2 * es
        if op == 0b10010 and not u:
            expr = "a"
        elif op == 0b10010:
            expr = sat(sx("a", w), es, 1)
        else:
            expr = sat(ext("a", w, u), es, u)
        n = 1 if scalar else 64 // es
        return lanes(i, rd, rn, None, es, n, expr, a_es=w, keep_d=bool(q) and not scalar, dst_off=0 if scalar else n * q)
    if op == 0b10011:  # SHLL
        if not u or scalar or size == 3:
            raise Unsupported("SHLL inválido")
        n = 64 // es
        return lanes(i, rd, rn, None, es, n, f"a << {es}", res_es=2 * es, a_off=n * q)
    if size == 3 and not q and not scalar:
        raise Unsupported("64 bits en vector de 64")
    ones = ONES[es]
    a = ext("a", es, False)
    if op in (0b00000, 0b00001):  # REV64 / REV32 / REV16
        group = {(0, 0): 64, (1, 0): 32, (0, 1): 16}.get((u, op))
        if group is None or es >= group or scalar:
            raise Unsupported("REV inválido")
        per = group // es
        return lanes(i, rd, rn, None, es, (128 if q else 64) // es, "a", a_off=f"(e ^ {per - 1}) - e")
    if op in (0b00010, 0b00110):  # SADDLP / UADDLP / SADALP / UADALP
        if size == 3 or scalar:
            raise Unsupported("suma larga por pares inválida")
        acc = "dd + " if op == 0b00110 else ""
        n = (128 if q else 64) // (2 * es)
        out = [f"V128 A = c->vr[{rn}], D = c->vr[{rd}], Rr = {{0}};",
               f"for (int e = 0; e < {n}; e++) {{ uint64_t a = A.{LANE[es]}[2 * e], b = A.{LANE[es]}[2 * e + 1], "
               f"dd = D.{LANE[2 * es]}[e]; Rr.{LANE[2 * es]}[e] = ({UT[2 * es]})({acc}(uint64_t)((i128){ext('a', es, u)} + {ext('b', es, u)})); }}",
               f"c->vr[{rd}] = Rr;"]
        return block(i, out)
    if op == 0b00011:  # SUQADD / USQADD
        e = sat(f"(i128){sx('dd', es)} + {ux('a', es)}", es, 0) if not u else sat(f"(i128){ux('dd', es)} + {sx('a', es)}", es, 1)
        return lanes(i, rd, rn, None, es, count, e, read_d=True)
    if op == 0b00100:  # CLS / CLZ
        if size == 3 or scalar:
            raise Unsupported("CLS/CLZ inválido")
        e = f"clz64({ux('a', es)}) - {64 - es}" if u else f"cls64((uint64_t){sx('a', es)}) - {64 - es}"
        return lanes(i, rd, rn, None, es, count, e)
    if op == 0b00101:  # CNT / NOT / RBIT
        if scalar or (size and not (u and size == 1)):
            raise Unsupported("CNT/NOT/RBIT inválido")
        e = {(0, 0): "popcnt8(a)", (1, 0): "~a", (1, 1): "rbit32((uint32_t)a) >> 24"}[(u, size)]
        return lanes(i, rd, rn, None, 8, (128 if q else 64) // 8, e)
    if op == 0b00111:  # SQABS / SQNEG
        e = sat(f"-(i128){a}", es, 0) if u else sat(f"{a} < 0 ? -(i128){a} : (i128){a}", es, 0)
        return lanes(i, rd, rn, None, es, count, e)
    if op in (0b01000, 0b01001, 0b01010, 0b01011):
        if scalar and size != 3:
            raise Unsupported("comparación escalar solo de 64 bits")
        e = {
            (0b01000, 0): f"{a} > 0 ? {ones} : 0", (0b01000, 1): f"{a} >= 0 ? {ones} : 0",
            (0b01001, 0): f"{a} == 0 ? {ones} : 0", (0b01001, 1): f"{a} <= 0 ? {ones} : 0",
            (0b01010, 0): f"{a} < 0 ? {ones} : 0",
            (0b01011, 0): f"{a} < 0 ? -(uint64_t)a : a", (0b01011, 1): "-(uint64_t)a",
        }.get((op, u))
        if e is None:
            raise Unsupported("comparación con cero no asignada")
        return lanes(i, rd, rn, None, es, count, e)
    raise Unsupported("dos registros no asignado")


def two_misc_fp(i, op, q, u, size, rd, rn, scalar, half=False):
    sz, a = size & 1, size >> 1
    w = 16 if half else 64 if sz else 32
    if half:
        sz = 0
        if op in (0b10110, 0b10111) or op == 0b11100 and a:
            raise Unsupported("FP16 de dos registros no asignado")
    if op in (0b10110, 0b10111):  # FCVTN / FCVTXN / FCVTL
        if a:
            raise Unsupported("conversión no asignada")
        if op == 0b10110:
            if u and not sz:
                raise Unsupported("FCVTXN solo desde double")
            if scalar and not u:
                raise Unsupported("FCVTN escalar no existe")
            src, dst = (64, 32) if sz else (32, 16)
            fn = "cvt_s_d_odd" if u else ("cvt_s_d" if sz else "cvt_h_s")
            n = 1 if scalar else 64 // dst
            return lanes(i, rd, rn, None, dst, n, f"{fn}(a)", a_es=src, keep_d=bool(q) and not scalar,
                         dst_off=0 if scalar else n * q)
        if u or scalar:
            raise Unsupported("FCVTL inválido")
        src, dst = (32, 64) if sz else (16, 32)
        fn = "cvt_d_s" if sz else "cvt_s_h"
        n = 64 // src
        return lanes(i, rd, rn, None, src, n, f"{fn}(a)", res_es=dst, a_off=n * q)
    if sz and not q and not scalar:
        raise Unsupported("double en vector de 64")
    count = 1 if scalar else (128 if q else 64) // w
    U = UT[w]
    A = f"({U})a"
    sign = {16: "0x8000u", 32: "0x80000000u", 64: "0x8000000000000000ull"}[w]
    ones = ONES[w]
    zero = f"({U})0"
    rint = {(0, 0, 0b11000): 0, (0, 0, 0b11001): 2, (0, 1, 0b11000): 1, (0, 1, 0b11001): 3,
            (1, 0, 0b11000): 4, (1, 0, 0b11001): 0, (1, 1, 0b11001): 0}
    cvt = {(0, 0, 0b11010): 0, (0, 0, 0b11011): 2, (0, 0, 0b11100): 4, (0, 1, 0b11010): 1, (0, 1, 0b11011): 3}
    key = (u, a, op)
    if key in rint:
        if scalar:
            raise Unsupported("FRINT escalar de SIMD no existe")
        e = f"frint{w}({A}, {rint[key]})"
    elif (0, a, op) in cvt:
        fn = f"f2{'u' if u else 's'}{w}"
        e = f"{fn}(fval{w}({A}), {cvt[(0, a, op)]}, 1.0)"
    elif op == 0b11101 and not a:  # SCVTF / UCVTF
        ty = "float" if w == 32 else "double"
        it = f"{'uint' if u else 'int'}{w}_t"
        e = f"{'u' if u else 's'}cvtf_h(({it})a, 0)" if w == 16 else f"u{w}of(({ty})({it})a)"
    elif key == (0, 1, 0b01100):
        e = f"fcmp{w}({A}, {zero}) == 2 ? {ones} : 0"
    elif key == (0, 1, 0b01101):
        e = f"fcmp{w}({A}, {zero}) == 6 ? {ones} : 0"
    elif key == (0, 1, 0b01110):
        e = f"fcmp{w}({A}, {zero}) == 8 ? {ones} : 0"
    elif key == (1, 1, 0b01100):
        e = f"(fcmp{w}({A}, {zero}) & 2) && fcmp{w}({A}, {zero}) != 3 ? {ones} : 0"
    elif key == (1, 1, 0b01101):
        e = f"(fcmp{w}({A}, {zero}) == 6 || fcmp{w}({A}, {zero}) == 8) ? {ones} : 0"
    elif key == (0, 1, 0b01111) and not scalar:
        e = f"{A} & ~({U}){sign}"
    elif key == (1, 1, 0b01111) and not scalar:
        e = f"{A} ^ {sign}"
    elif key == (1, 1, 0b11111) and not scalar:
        e = f"fsqrt{w}({A})"
    elif key == (0, 1, 0b11101):
        e = f"frecpe({A}, {w})"
    elif key == (1, 1, 0b11101):
        e = f"frsqrte({A}, {w})"
    elif key == (0, 1, 0b11111) and scalar:
        e = f"frecpx({A}, {w})"
    elif key in ((0, 1, 0b11100), (1, 1, 0b11100)) and not sz and not scalar:
        e = "urecpe((uint32_t)a)" if not u else "ursqrte((uint32_t)a)"
    else:
        raise Unsupported("FP de dos registros no asignado")
    lanes(i, rd, rn, None, w, count, e)


def across(i, x, q, u, size, rd, rn, scalar):
    op = bits(x, 16, 12)
    if scalar and not u and not size & 1 and op in (0b01100, 0b01101, 0b01111):  # FP16 por pares escalar
        if op == 0b01101 and size >> 1:
            raise Unsupported("FADDP de half no asignado")
        name = {0b01100: "fminnm" if size >> 1 else "fmaxnm", 0b01101: "fadd", 0b01111: "fmin" if size >> 1 else "fmax"}[op]
        return lanes(i, rd, rn, None, 16, 1, fp_expr(name, 16, b=f"c->vr[{rn}].h[1]"))
    if not scalar and not u and not size & 1 and op in (0b01100, 0b01111):  # FMAXNMV... de half
        name = {0b01100: "fminnm" if size >> 1 else "fmaxnm", 0b01111: "fmin" if size >> 1 else "fmax"}[op]
        # Reduce() de Arm: mitades recursivas, o sea un árbol de pares
        out = [f"V128 A = c->vr[{rn}];", f"uint16_t t[8]; for (int e = 0; e < {8 if q else 4}; e++) t[e] = A.h[e];"]
        for n in ((8, 4, 2) if q else (4, 2)):
            out.append(f"for (int e = 0; e < {n // 2}; e++) t[e] = {name}16(t[2 * e], t[2 * e + 1]);")
        out.append(f"wrv16(c, {rd}, t[0]);")
        return block(i, out)
    if scalar:  # por pares escalar: ADDP, FADDP, FMAXP...
        if op == 0b11011 and not u:
            if size != 3:
                raise Unsupported("ADDP escalar solo de 64 bits")
            return lanes(i, rd, rn, None, 64, 1, f"a + c->vr[{rn}].d[1]")
        if not u or op not in (0b01100, 0b01101, 0b01111) or (op == 0b01101 and size >> 1):
            raise Unsupported("por pares escalar no asignado")
        w = 64 if size & 1 else 32
        name = {0b01100: "fminnm" if size >> 1 else "fmaxnm", 0b01101: "fadd", 0b01111: "fmin" if size >> 1 else "fmax"}[op]
        return lanes(i, rd, rn, None, w, 1, fp_expr(name, w, b=f"c->vr[{rn}].{LANE[w]}[1]"))
    if op in (0b01100, 0b01111):  # FMAXNMV / FMINNMV / FMAXV / FMINV
        if not u or size & 1 or not q:
            raise Unsupported("reducción FP inválida")
        name = {0b01100: "fminnm" if size >> 1 else "fmaxnm", 0b01111: "fmin" if size >> 1 else "fmax"}[op]
        block(i, [f"V128 A = c->vr[{rn}];",
                  f"uint32_t lo = {name}32(A.s[0], A.s[1]), hi = {name}32(A.s[2], A.s[3]);",
                  f"wrv32(c, {rd}, {name}32(lo, hi));"])
        return
    es = 8 << size
    if size == 3 or (size == 2 and not q):
        raise Unsupported("reducción inválida")
    n = (128 if q else 64) // es
    a = ext("A." + LANE[es] + "[e]", es, u)
    if op == 0b00011:  # SADDLV / UADDLV
        acc, res, init, w = "s += " + a, "s", "i128 s = 0;", 2 * es
    elif op == 0b11011 and not u:
        acc, res, init, w = "s += A." + LANE[es] + "[e]", "s", "uint64_t s = 0;", es
    elif op in (0b01010, 0b11010):
        cmp = ">" if op == 0b01010 else "<"
        init = f"i128 s = {ext('A.' + LANE[es] + '[0]', es, u)};"
        acc, res, w = f"if ({a} {cmp} s) s = {a}", "s", es
    else:
        raise Unsupported("reducción no asignada")
    block(i, [f"V128 A = c->vr[{rn}];", init, f"for (int e = 0; e < {n}; e++) {acc};",
              f"wrv{w}(c, {rd}, (uint64_t){res});"])


def copy(i, x, q, op, rd, rn, scalar):
    imm5, imm4 = bits(x, 20, 16), bits(x, 14, 11)
    k = (imm5 & -imm5).bit_length() - 1 if imm5 else 4
    if k > 3:
        raise Unsupported("copia inválida")
    es = 8 << k
    idx = imm5 >> (k + 1)
    L = LANE[es]
    if scalar:
        if op or imm4:
            raise Unsupported("copia escalar inválida")
        return block(i, [f"wrv{es}(c, {rd}, c->vr[{rn}].{L}[{idx}]);"])
    if op:  # INS (elemento)
        if not q:
            raise Unsupported("INS inválido")
        return block(i, [f"c->vr[{rd}].{L}[{idx}] = c->vr[{rn}].{L}[{imm4 >> k}];"])
    n = (128 if q else 64) // es
    if imm4 == 0b0000:  # DUP (elemento)
        if es == 64 and not q:
            raise Unsupported("DUP inválido")
        return block(i, [
            f"uint64_t v = c->vr[{rn}].{L}[{idx}]; V128 Rr = {{0}};",
            f"for (int e = 0; e < {n}; e++) Rr.{L}[e] = ({UT[es]})v;", f"c->vr[{rd}] = Rr;"])
    if imm4 == 0b0001:  # DUP (general)
        if es == 64 and not q:
            raise Unsupported("DUP inválido")
        return block(i, [f"uint64_t v = {R(rn)}; V128 Rr = {{0}};",
                         f"for (int e = 0; e < {n}; e++) Rr.{L}[e] = ({UT[es]})v;", f"c->vr[{rd}] = Rr;"])
    if imm4 == 0b0011:  # INS (general)
        if not q:
            raise Unsupported("INS inválido")
        return block(i, [f"c->vr[{rd}].{L}[{idx}] = ({UT[es]}){R(rn)};"])
    if imm4 == 0b0101:  # SMOV
        if es == 64 or (es == 32 and not q):
            raise Unsupported("SMOV inválido")
        return block(i, [setr(rd, f"(uint64_t){sx(f'c->vr[{rn}].{L}[{idx}]', es)}", 64 if q else 32)])
    if imm4 == 0b0111:  # UMOV
        if (q and es != 64) or (not q and es == 64):
            raise Unsupported("UMOV inválido")
        return block(i, [setr(rd, f"c->vr[{rn}].{L}[{idx}]", 64 if q else 32)])
    raise Unsupported("copia no asignada")


def expand_imm(op, cmode, imm8):
    r = lambda v, w, n: sum(v << (w * k) for k in range(n))  # noqa: E731
    c3 = cmode >> 1
    if c3 == 0:
        return r(imm8, 32, 2)
    if c3 == 1:
        return r(imm8 << 8, 32, 2)
    if c3 == 2:
        return r(imm8 << 16, 32, 2)
    if c3 == 3:
        return r(imm8 << 24, 32, 2)
    if c3 == 4:
        return r(imm8, 16, 4)
    if c3 == 5:
        return r(imm8 << 8, 16, 4)
    if c3 == 6:
        return r((imm8 << 8) | 0xFF, 32, 2) if not cmode & 1 else r((imm8 << 16) | 0xFFFF, 32, 2)
    if not cmode & 1:
        if not op:
            return r(imm8, 8, 8)
        return sum((0xFF if imm8 >> k & 1 else 0) << (8 * k) for k in range(8))
    a, b, cdefgh = imm8 >> 7, (imm8 >> 6) & 1, imm8 & 0x3F
    if not op:
        v = a << 31 | (b ^ 1) << 30 | (0x1F if b else 0) << 25 | cdefgh << 19
        return r(v, 32, 2)
    return a << 63 | (b ^ 1) << 62 | (0xFF if b else 0) << 54 | cdefgh << 48


def modified_imm(i, x, q, op, rd):
    cmode, o2 = bits(x, 15, 12), bits(x, 11, 11)
    if o2:  # FMOV (vector, half) de ARMv8.2
        if op or cmode != 0b1111:
            raise Unsupported("inmediato con o2 no asignado")
        from simd import vfp_expand_imm
        h = vfp_expand_imm(bits(x, 18, 16) << 5 | bits(x, 9, 5), 16)
        v = h * 0x0001000100010001
        return block(i, [f"V128 Rr = {{0}}; Rr.d[0] = 0x{v:x}ull;" + (f" Rr.d[1] = 0x{v:x}ull;" if q else ""),
                         f"c->vr[{rd}] = Rr;"])
    imm8 = bits(x, 18, 16) << 5 | bits(x, 9, 5)
    if cmode == 0b1111 and op and not q:
        raise Unsupported("FMOV double en vector de 64")
    imm = expand_imm(op, cmode, imm8)
    if cmode < 0b1100 and cmode & 1:  # ORR / BIC
        expr = f"dd | 0x{imm:x}ull" if not op else f"dd & ~0x{imm:x}ull"
        return lanes(i, rd, rd, None, 64, 2 if q else 1, expr, read_d=True)
    invert = op and cmode < 0b1110
    v = (~imm & (2**64 - 1)) if invert else imm
    block(i, [f"V128 Rr = {{0}}; Rr.d[0] = 0x{v:x}ull;" + (f" Rr.d[1] = 0x{v:x}ull;" if q else ""),
              f"c->vr[{rd}] = Rr;"])


def shift_imm(i, x, q, u, rd, rn, scalar):
    immh, immb, op = bits(x, 22, 19), bits(x, 18, 16), bits(x, 15, 11)
    hb = immh.bit_length() - 1
    es = 8 << hb
    val = immh << 3 | immb
    rsh, lsh = 2 * es - val, val - es
    count = 1 if scalar else (128 if q else 64) // es
    narrow = op in (0b10000, 0b10001, 0b10010, 0b10011)
    if es == 64 and not q and not scalar and not narrow and op != 0b10100:
        raise Unsupported("64 bits en vector de 64")
    if scalar and op in (0b00000, 0b00010, 0b00100, 0b00110, 0b01000, 0b01010) and es != 64:
        raise Unsupported("desplazamiento escalar solo de 64 bits")
    a = ext("a", es, u)
    ones = ONES[es]
    if op in (0b00000, 0b00010, 0b00100, 0b00110):  # [S|U][R]SHR / [S|U][R]SRA
        rnd = f" + ((i128)1 << {rsh - 1})" if op & 0b00100 else ""
        e = f"(uint64_t)(((i128){a}{rnd}) >> {rsh})"
        if op & 0b00010:
            e = f"dd + {e}"
        return lanes(i, rd, rn, None, es, count, e, read_d=bool(op & 0b00010))
    if op == 0b01000 and u:  # SRI
        m = f"({ones} >> {rsh})" if rsh < 64 else "0"
        sh = f"({ux('a', es)} >> {rsh})" if rsh < 64 else "0"
        return lanes(i, rd, rn, None, es, count, f"(dd & ~{m}) | {sh}", read_d=True)
    if op == 0b01010:  # SHL / SLI
        if u:
            return lanes(i, rd, rn, None, es, count, f"(dd & ~({ones} << {lsh})) | (a << {lsh})", read_d=True)
        return lanes(i, rd, rn, None, es, count, f"a << {lsh}")
    if op in (0b01100, 0b01110):  # SQSHLU / SQSHL / UQSHL (inmediato)
        if op == 0b01100 and not u:
            raise Unsupported("SQSHLU sin U")
        src = sx("a", es) if op == 0b01100 else a
        return lanes(i, rd, rn, None, es, count, sat(f"(i128){src} * ((i128)1 << {lsh})", es, u))
    if narrow:
        if hb == 3:
            raise Unsupported("estrechamiento de 128 bits")
        w = 2 * es
        src = sx("a", w) if (op in (0b10000, 0b10001) and u) else ext("a", w, u)
        rnd = f" + ((i128)1 << {rsh - 1})" if op & 1 else ""
        v = f"(((i128){src}{rnd}) >> {rsh})"
        if op in (0b10000, 0b10001) and not u:  # SHRN / RSHRN
            if scalar:
                raise Unsupported("SHRN escalar no existe")
            e = f"(uint64_t){v}"
        elif op in (0b10000, 0b10001):  # SQSHRUN / SQRSHRUN
            e = sat(v, es, 1)
        else:
            e = sat(v, es, u)
        n = 1 if scalar else 64 // es
        return lanes(i, rd, rn, None, es, n, e, a_es=w, keep_d=bool(q) and not scalar, dst_off=0 if scalar else n * q)
    if op == 0b10100:  # SSHLL / USHLL
        if hb == 3 or scalar:
            raise Unsupported("SSHLL inválido")
        n = 64 // es
        return lanes(i, rd, rn, None, es, n, f"(uint64_t)({a} * ((i128)1 << {lsh}))", res_es=2 * es, a_off=n * q)
    if op in (0b11100, 0b11111):  # SCVTF / UCVTF / FCVTZS / FCVTZU con punto fijo
        if es == 8:
            raise Unsupported("punto fijo de 8 bits")
        fb = rsh
        if es == 16:  # half (ARMv8.2)
            if op == 0b11100:
                e = f"{'u' if u else 's'}cvtf_h(({'uint' if u else 'int'}16_t)a, {fb})"
            else:
                e = f"f2{'u' if u else 's'}16(fval16((uint16_t)a), 3, 0x1p{fb})"
            return lanes(i, rd, rn, None, 16, count, e)
        if op == 0b11100:
            ty = "float" if es == 32 else "double"
            e = f"u{es}of(({ty})({'uint' if u else 'int'}{es}_t)a * ({ty})0x1p-{fb})"
        else:
            e = f"f2{'u' if u else 's'}{es}(fval{es}(({UT[es]})a), 3, 0x1p{fb})"
        return lanes(i, rd, rn, None, es, count, e)
    raise Unsupported("desplazamiento inmediato no asignado")


def by_element(i, x, q, u, size, rd, rn, scalar):
    op, l, mbit, h = bits(x, 15, 12), bits(x, 21, 21), bits(x, 20, 20), bits(x, 11, 11)
    rm4 = bits(x, 19, 16)
    fp = op in (0b0001, 0b0101, 0b1001) and not (u and op != 0b1001)
    if fp and size == 0:  # FMLA/FMLS/FMUL/FMULX por elemento de half (ARMv8.2): índice H:L:M, Vm de 0 a 15
        b = f"c->vr[{rm4}].h[{h << 2 | l << 1 | mbit}]"
        name = {0b0001: "fmla", 0b0101: "fmls", 0b1001: "fmulx" if u else "fmul"}[op]
        count = 1 if scalar else 8 if q else 4
        return lanes(i, rd, rn, None, 16, count, fp_expr(name, 16, b=b), read_d=name in ("fmla", "fmls"))
    if fp:
        if not size >> 1:
            raise Unsupported("FP por elemento inválido")
        sz = size & 1
        w = 64 if sz else 32
        if sz and (l or (not q and not scalar)):
            raise Unsupported("FP por elemento inválido")
        idx = h if sz else h << 1 | l
        m = mbit << 4 | rm4
        b = f"c->vr[{m}].{LANE[w]}[{idx}]"
        name = {0b0001: "fmla", 0b0101: "fmls", 0b1001: "fmulx" if u else "fmul"}[op]
        count = 1 if scalar else (128 if q else 64) // w
        return lanes(i, rd, rn, None, w, count, fp_expr(name, w, b=b), read_d=name in ("fmla", "fmls"))
    if op == 0b1110 and size == 2 and not scalar:  # SDOT / UDOT por elemento (ARMv8.2): un grupo de 4 bytes
        b = f"c->vr[{mbit << 4 | rm4}].s[{h << 1 | l}]"
        return lanes(i, rd, rn, None, 32, 4 if q else 2, f"dd + dot4(a, {b}, {u})", read_d=True)
    if size not in (1, 2):
        raise Unsupported("por elemento inválido")
    es = 8 << size
    if size == 1:
        idx, m = h << 2 | l << 1 | mbit, rm4
    else:
        idx, m = h << 1 | l, mbit << 4 | rm4
    b = f"c->vr[{m}].{LANE[es]}[{idx}]"
    count = 1 if scalar else (128 if q else 64) // es
    if op in (0b0000, 0b0100, 0b1000) and not scalar:  # MLA / MLS / MUL
        if op == 0b1000 and u or op != 0b1000 and not u:
            raise Unsupported("por elemento no asignado")
        e = {0b0000: f"dd + a * {b}", 0b0100: f"dd - a * {b}", 0b1000: f"a * {b}"}[op]
        return lanes(i, rd, rn, None, es, count, e, read_d=op != 0b1000)
    if op in (0b1101, 0b1111) and u:  # SQRDMLAH / SQRDMLSH por elemento (ARMv8.1)
        return lanes(i, rd, rn, None, es, count, f"sqrdmlah(dd, a, {b}, {es}, {op >> 1 & 1})", read_d=True)
    if op in (0b1100, 0b1101):  # SQDMULH / SQRDMULH
        if u:
            raise Unsupported("por elemento no asignado")
        return lanes(i, rd, rn, None, es, count, f"sqdmulh(a, {b}, {es}, {op & 1})")
    long = {0b0010: 0b1000, 0b0110: 0b1010, 0b1010: 0b1100, 0b0011: 0b1001, 0b0111: 0b1011, 0b1011: 0b1101}
    if op in long:
        if u and op in (0b0011, 0b0111, 0b1011):
            raise Unsupported("por elemento no asignado")
        if scalar and op not in (0b0011, 0b0111, 0b1011):
            raise Unsupported("por elemento escalar no asignado")
        n = 1 if scalar else 64 // es
        off = 0 if scalar else n * q
        # se reutiliza long_op con Vm reemplazado por el elemento elegido
        tmp = type(i)(i.pc, i.word)
        long_op(tmp, long[op], u, es, rd, rn, 0, q if not scalar else 0, n)
        line = tmp.lines[0].replace("V128 B = c->vr[0];", "").replace(f"B.{LANE[es]}[{off} + e]", b)
        i.lines = [line]
        return
    raise Unsupported("por elemento no asignado")


def permute(i, x, q, size, rd, rn, rm):
    op = bits(x, 14, 12)
    es = 8 << size
    if size == 3 and not q:
        raise Unsupported("permutación inválida")
    n = (128 if q else 64) // es
    L = LANE[es]
    if op in (0b001, 0b101):  # UZP1 / UZP2
        part = op >> 2
        body = f"Rr.{L}[e] = e < {n // 2} ? A.{L}[2 * e + {part}] : B.{L}[2 * (e - {n // 2}) + {part}];"
    elif op in (0b010, 0b110):  # TRN1 / TRN2
        part = op >> 2
        body = f"Rr.{L}[e] = (e & 1) ? B.{L}[(e & ~1) + {part}] : A.{L}[e + {part}];"
    elif op in (0b011, 0b111):  # ZIP1 / ZIP2
        base = (op >> 2) * n // 2
        body = f"Rr.{L}[e] = (e & 1) ? B.{L}[{base} + e / 2] : A.{L}[{base} + e / 2];"
    else:
        raise Unsupported("permutación no asignada")
    block(i, [f"V128 A = c->vr[{rn}], B = c->vr[{rm}], Rr = {{0}};", f"for (int e = 0; e < {n}; e++) {{ {body} }}",
              f"c->vr[{rd}] = Rr;"])


def ext_op(i, x, q, rd, rn, rm):
    pos = bits(x, 14, 11)
    if not q and pos > 7:
        raise Unsupported("EXT inválido")
    nb = 16 if q else 8
    block(i, [f"V128 A = c->vr[{rn}], B = c->vr[{rm}], Rr = {{0}};",
              f"for (int e = 0; e < {nb}; e++) {{ int k = e + {pos}; Rr.b[e] = k < {nb} ? A.b[k] : B.b[k - {nb}]; }}",
              f"c->vr[{rd}] = Rr;"])


def tbl(i, x, q, rd, rn, rm):
    ln, tbx = bits(x, 14, 13) + 1, bits(x, 12, 12)
    nb = 16 if q else 8
    regs = [(rn + k) % 32 for k in range(ln)]
    out = ["uint8_t T[64];"]
    out += [f"memcpy(T + {16 * k}, &c->vr[{r}], 16);" for k, r in enumerate(regs)]
    out.append(f"V128 I = c->vr[{rm}], D = c->vr[{rd}], Rr = {{0}};")
    miss = "D.b[e]" if tbx else "0"
    out.append(f"for (int e = 0; e < {nb}; e++) Rr.b[e] = I.b[e] < {16 * ln} ? T[I.b[e]] : {miss};")
    out.append(f"c->vr[{rd}] = Rr;")
    block(i, out)


def crypto(i, x, rd, rn, rm):
    """AES, SHA1 y SHA256 (extensión criptográfica, presente en el Cortex-A57 de la Switch)."""
    if x & 0xFF3E0C00 == 0x4E280800:  # AES
        op = bits(x, 16, 12)
        if bits(x, 23, 22) or op not in (0b00100, 0b00101, 0b00110, 0b00111):
            raise Unsupported("AES no asignado")
        if op in (0b00100, 0b00101):  # AESE / AESD
            return block(i, [f"V128 s = c->vr[{rd}]; s.q ^= c->vr[{rn}].q;",
                             f"aes_round(s.b, {op & 1}); c->vr[{rd}] = s;"])
        return block(i, [f"V128 s = c->vr[{rn}]; aes_mix(s.b, {op & 1}); c->vr[{rd}] = s;"])
    if x & 0xFFE08C00 == 0x5E000000:  # SHA de tres registros
        op = bits(x, 14, 12)
        if op <= 2:  # SHA1C / SHA1P / SHA1M
            return block(i, [f"V128 X = c->vr[{rd}];", f"uint32_t y = c->vr[{rn}].s[0];",
                             f"sha1_hash(X.s, y, c->vr[{rm}].s, {op});", f"c->vr[{rd}] = X;"])
        if op == 3:  # SHA1SU0
            return block(i, [f"V128 a = c->vr[{rd}], b = c->vr[{rn}], m = c->vr[{rm}], r;",
                             "r.d[0] = a.d[1]; r.d[1] = b.d[0]; r.q ^= a.q ^ m.q;", f"c->vr[{rd}] = r;"])
        if op in (4, 5):  # SHA256H / SHA256H2
            a, b = (rd, rn) if op == 4 else (rn, rd)
            return block(i, [f"V128 X = c->vr[{a}], Y = c->vr[{b}];", f"sha256_hash(X.s, Y.s, c->vr[{rm}].s);",
                             f"c->vr[{rd}] = {'X' if op == 4 else 'Y'};"])
        if op == 6:  # SHA256SU1
            return block(i, [
                f"V128 o1 = c->vr[{rd}], o2 = c->vr[{rn}], o3 = c->vr[{rm}], T0, r;",
                "T0.s[0] = o2.s[1]; T0.s[1] = o2.s[2]; T0.s[2] = o2.s[3]; T0.s[3] = o3.s[0];",
                "uint32_t t1[2] = {o3.s[2], o3.s[3]};",
                "for (int e = 0; e < 4; e++) { uint32_t el = e < 2 ? t1[e] : r.s[e - 2];"
                " el = rorc32(el, 17) ^ rorc32(el, 19) ^ (el >> 10); r.s[e] = el + o1.s[e] + T0.s[e]; }",
                f"c->vr[{rd}] = r;"])
        raise Unsupported("SHA no asignado")
    op = bits(x, 16, 12)  # SHA de dos registros
    if op == 0:  # SHA1H
        return block(i, [f"wrv32(c, {rd}, rol32(c->vr[{rn}].s[0], 30));"])
    if op == 1:  # SHA1SU1
        return block(i, [f"V128 a = c->vr[{rd}], b = c->vr[{rn}], T, r;",
                         "T.s[0] = a.s[0] ^ b.s[1]; T.s[1] = a.s[1] ^ b.s[2]; T.s[2] = a.s[2] ^ b.s[3]; T.s[3] = a.s[3];",
                         "for (int e = 0; e < 4; e++) r.s[e] = rol32(T.s[e], 1);",
                         "r.s[3] ^= rol32(T.s[0], 2);", f"c->vr[{rd}] = r;"])
    if op == 2:  # SHA256SU0
        return block(i, [f"V128 a = c->vr[{rd}], b = c->vr[{rn}], r;",
                         "uint32_t T[4] = {a.s[1], a.s[2], a.s[3], b.s[0]};",
                         "for (int e = 0; e < 4; e++) { uint32_t el = T[e];"
                         " r.s[e] = (rorc32(el, 7) ^ rorc32(el, 18) ^ (el >> 3)) + a.s[e]; }", f"c->vr[{rd}] = r;"])
    raise Unsupported("SHA no asignado")
