"""Resolución de saltos indirectos (BR) que vienen de tablas de salto.

Un `switch` grande se compila como una tabla: el destino se calcula con el índice,
una carga de la tabla y una suma a una base. Aquí se reconstruye esa cuenta como
expresión, recorriendo hacia atrás las instrucciones que escriben el registro del
BR; se busca el CMP + B.HI/B.HS que acota el índice, y se lee la tabla del binario
para obtener todos los destinos.

Pasarse de destinos no rompe nada (es código que nunca se ejecuta); quedarse corto
sí, así que cuando no aparece la cota se leen entradas mientras den direcciones de
código válidas.
"""

from a64 import bits, decode, sext

M64 = (1 << 64) - 1
MAX_BACK = 32       # instrucciones que se miran hacia atrás desde el BR
MAX_ENTRIES = 1024  # tope sin cota conocida


def _flow_stop(w):
    """Instrucciones que cortan un camino lineal: B, BL, BR, BLR, RET."""
    return bits(w, 30, 26) == 0b00101 or bits(w, 31, 25) == 0b1101011


def _cmp_imm(w):
    """CMP o SUBS Wn/Xn, #imm: devuelve (registro comparado, valor) o None."""
    if w & 0x7F800000 == 0x71000000:
        return bits(w, 9, 5), bits(w, 21, 10) << (12 if bits(w, 22, 22) else 0)
    return None


def find_bound(mod, pc):
    """Busca hacia atrás un CMP #N seguido de B.HI / B.HS antes del BR.

    Devuelve (registro, pc del CMP, cantidad de entradas) o None.
    """
    cond = None
    q = pc - 4
    for _ in range(MAX_BACK):
        if not mod.in_text(q):
            break
        w = mod.word(q)
        if bits(w, 31, 24) == 0b01010100 and not bits(w, 4, 4):  # B.cond
            c = bits(w, 3, 0)
            if c in (0b1000, 0b0010):  # HI, HS
                cond = c
        elif _flow_stop(w):
            break
        elif cond is not None:
            cmp = _cmp_imm(w)
            if cmp:
                reg, n = cmp
                return reg, q, n + 1 if cond == 0b1000 else n
        q -= 4
    return None


class Slicer:
    """Reconstruye el valor de un registro como expresión, hacia atrás desde un pc."""

    def __init__(self, mod, bound, entry):
        self.mod = mod
        self.bound = bound
        self.entry = entry

    def reg(self, r, pc, depth=0):
        if r == 31:
            return ("const", 0)
        if depth > 12:
            return ("var",)
        q = pc - 4
        for _ in range(MAX_BACK):
            if not self.mod.in_text(q):
                break
            if self.bound and r == self.bound[0] and q < self.bound[1] + 4:
                return ("var",)  # el valor que acota el CMP
            w = self.mod.word(q)
            if _flow_stop(w):
                break
            e = self.effect(w, q, r, depth)
            if e is not None:
                return e
            q -= 4
        if self.bound and r == self.bound[0]:
            return ("var",)
        return self.invariant(r, pc, depth)

    def forward(self, base, off, size, q, depth):
        """Busca hacia atrás el guardado en [base + off] que alimenta una carga (código -O0)."""
        p = q - 4
        for _ in range(2 * MAX_BACK):
            if not self.mod.in_text(p):
                return None
            w = self.mod.word(p)
            if _flow_stop(w):
                return None
            i = decode(p, w)
            if i.kind == "op" and f"{'c->sp' if base == 31 else f'c->x[{base}]'} = " in " ".join(i.lines):
                return None  # la base cambió
            unscaled = w & 0x3BE00C00 == 0x38000000
            if (w & 0x3BC00000 == 0x39000000 or unscaled) and not bits(w, 26, 26) and bits(w, 9, 5) == base:
                s_off = sext(bits(w, 20, 12), 9) if unscaled else bits(w, 21, 10) << bits(w, 31, 30)
                if s_off == off and bits(w, 31, 30) == size:
                    return self.reg(bits(w, 4, 0), p, depth + 1)
            p -= 4
        return None

    def invariant(self, r, pc, depth):
        """Valores que el compilador sacó del bucle (la dirección de la tabla, por ejemplo):
        si todas las escrituras de r en la función dan la misma constante, vale esa."""
        found = None
        for q in range(self.entry, pc, 4):
            w = self.mod.word(q)
            i = decode(q, w)
            if i.kind != "op" or f"c->x[{r}] = " not in " ".join(i.lines):
                continue
            e = self.effect(w, q, r, depth + 1)
            if e is None or has_var(e):
                return ("var",)
            try:
                v = evaluate(self.mod, e, 0)
            except ValueError:
                return ("var",)
            if found is not None and v != found:
                return ("var",)
            found = v
        return ("const", found) if found is not None else ("var",)

    def effect(self, w, q, r, depth):
        """Si la instrucción en q escribe r, devuelve su valor como expresión (si no, None)."""
        rd, rn, rm = bits(w, 4, 0), bits(w, 9, 5), bits(w, 20, 16)
        i = decode(q, w)
        # La traducción a C dice con certeza qué registros escribe la instrucción.
        if i.kind != "op" or f"c->x[{r}] = " not in " ".join(i.lines):
            return None
        if i.ref is not None:  # ADR / ADRP
            return ("const", i.ref)
        if rd != r:  # escribe r de otra forma (writeback, segundo registro de un par)
            return ("var",)
        sf = bits(w, 31, 31)
        rec = lambda reg: self.reg(reg, q, depth + 1)  # noqa: E731
        wrap = (lambda e: e) if sf else (lambda e: ("ext", e, 2))
        if w & 0x7F800000 == 0x11000000:  # ADD inmediato
            imm = bits(w, 21, 10) << (12 if bits(w, 22, 22) else 0)
            return wrap(("add", rec(rn), ("const", imm)))
        if w & 0x7F800000 == 0x51000000:  # SUB inmediato
            imm = bits(w, 21, 10) << (12 if bits(w, 22, 22) else 0)
            return wrap(("add", rec(rn), ("const", -imm & M64)))
        if w & 0x7F200000 == 0x0B000000 and bits(w, 23, 22) == 0:  # ADD con LSL
            return wrap(("add", rec(rn), ("shl", rec(rm), bits(w, 15, 10))))
        if w & 0x7FE00000 == 0x0B200000:  # ADD extendido
            return wrap(("add", rec(rn), ("shl", ("ext", rec(rm), bits(w, 15, 13)), bits(w, 12, 10))))
        if w & 0x7F2003E0 == 0x2A0003E0 and bits(w, 15, 10) == 0:  # MOV (ORR con XZR)
            return wrap(rec(rm))
        if w & 0x7F800000 == 0x13000000 and bits(w, 21, 16) == 0:  # SBFM: SXTB / SXTH / SXTW
            imms = bits(w, 15, 10)
            if imms in (7, 15, 31):
                return ("ext", rec(rn), {7: 4, 15: 5, 31: 6}[imms])
        if w & 0x7F800000 == 0x53000000:  # UBFM: UXTB / UXTH / LSL
            immr, imms = bits(w, 21, 16), bits(w, 15, 10)
            width = 64 if sf else 32
            if immr == 0 and imms in (7, 15):
                return ("ext", rec(rn), {7: 0, 15: 1}[imms])
            if imms + 1 == immr:
                return wrap(("shl", rec(rn), width - immr))
        if w & 0x3B200C00 == 0x38200800 and not bits(w, 26, 26):  # LDR con registro de desplazamiento
            size, opc, option, s = bits(w, 31, 30), bits(w, 23, 22), bits(w, 15, 13), bits(w, 12, 12)
            if opc == 0:
                return None
            addr = ("add", rec(rn), ("shl", ("ext", rec(rm), option), size if s else 0))
            e = ("load", addr, 1 << size, opc >= 2)
            return ("ext", e, 2) if opc == 3 else e
        unscaled = w & 0x3B200C00 == 0x38000000
        if (w & 0x3B000000 == 0x39000000 or unscaled) and not bits(w, 26, 26):  # LDR / LDUR con inmediato
            size, opc = bits(w, 31, 30), bits(w, 23, 22)
            if opc == 0:
                return None
            off = sext(bits(w, 20, 12), 9) if unscaled else bits(w, 21, 10) << size
            if rn in (29, 31) and opc == 1:  # variable local: el valor del último STR a esa ranura
                fwd = self.forward(rn, off, size, q, depth)
                if fwd is not None:
                    return fwd if size == 3 else ("ext", fwd, {0: 0, 1: 1, 2: 2}[size])
            e = ("load", ("add", rec(rn), ("const", off & M64)), 1 << size, opc >= 2)
            return ("ext", e, 2) if opc == 3 else e
        if w & 0xBF000000 == 0x18000000:  # LDR literal
            a = (q + (sext(bits(w, 23, 5), 19) << 2)) & M64
            return ("load", ("const", a), 8 if bits(w, 30, 30) else 4, False)
        return ("var",)


def has_var(e):
    return e[0] == "var" or any(has_var(x) for x in e[1:] if isinstance(x, tuple))


EXT = {0: (8, False), 1: (16, False), 2: (32, False), 3: (64, False), 4: (8, True), 5: (16, True), 6: (32, True), 7: (64, True)}


def evaluate(mod, e, k):
    t = e[0]
    if t == "const":
        return e[1] & M64
    if t == "var":
        return k & M64
    if t == "add":
        return (evaluate(mod, e[1], k) + evaluate(mod, e[2], k)) & M64
    if t == "shl":
        return (evaluate(mod, e[1], k) << e[2]) & M64
    if t == "ext":
        width, signed = EXT[e[2]]
        v = evaluate(mod, e[1], k) & ((1 << width) - 1)
        return (sext(v, width) & M64) if signed else v
    if t == "load":
        a = evaluate(mod, e[1], k)
        v = int.from_bytes(mod.read(a, e[2]), "little")
        return (sext(v, 8 * e[2]) & M64) if e[3] else v
    raise ValueError(t)


def resolve(mod, br_pc, reg, entry):
    """Destinos posibles de `BR reg` en br_pc, o [] si no se reconoce una tabla."""
    bound = find_bound(mod, br_pc)
    expr = Slicer(mod, bound, min(entry, br_pc)).reg(reg, br_pc)
    if expr == ("var",):
        return []
    if not has_var(expr):  # destino fijo (un veneer, por ejemplo)
        try:
            t = evaluate(mod, expr, 0)
        except ValueError:
            return []
        return [t] if mod.in_text(t) else []
    count = bound[2] if bound else MAX_ENTRIES
    out = []
    for k in range(min(count, MAX_ENTRIES)):
        try:
            t = evaluate(mod, expr, k)
        except ValueError:
            break
        if not mod.in_text(t):
            if bound:
                return []  # la expresión no era una tabla después de todo
            break
        out.append(t)
    return sorted(set(out))
