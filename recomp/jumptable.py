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
MAX_BACK = 64       # instrucciones que se miran hacia atrás desde el BR
MAX_ENTRIES = 1024  # tope sin cota conocida
COND_HI, COND_HS, COND_LS, COND_LO = 0b1000, 0b0010, 0b1001, 0b0011
CALLER_SAVED = set(range(19)) | {30}


def _flow_stop(w):
    """Instrucciones que cortan un camino lineal: B, BL, BR, BLR, RET."""
    return bits(w, 30, 26) == 0b00101 or bits(w, 31, 25) == 0b1101011


def _uncond(w):
    """Después de B, BR o RET no se sigue de largo (BL sí: la llamada vuelve)."""
    return (bits(w, 30, 26) == 0b00101 and not bits(w, 31, 31)) or bits(w, 31, 25) == 0b1101011


def _is_call(w):
    return (bits(w, 30, 26) == 0b00101 and bits(w, 31, 31)) or (bits(w, 31, 25) == 0b1101011 and bits(w, 24, 21) == 1)


def _branch(w, q):
    """Destino y condición de B, B.cond, CBZ/CBNZ o TBZ/TBNZ (o None)."""
    if bits(w, 31, 26) == 0b000101:
        return (q + (sext(bits(w, 25, 0), 26) << 2)) & M64, None
    if bits(w, 31, 24) == 0b01010100 and not bits(w, 4, 4):
        return (q + (sext(bits(w, 23, 5), 19) << 2)) & M64, bits(w, 3, 0)
    if bits(w, 30, 25) in (0b011010, 0b011011):
        imm = sext(bits(w, 23, 5), 19) if bits(w, 25, 25) == 0 else sext(bits(w, 18, 5), 14)
        return (q + (imm << 2)) & M64, "cb"
    return None


def _cmp_imm(w):
    """CMP o SUBS Wn/Xn, #imm: devuelve (registro comparado, valor) o None."""
    if w & 0x7F800000 == 0x71000000:
        return bits(w, 9, 5), bits(w, 21, 10) << (12 if bits(w, 22, 22) else 0)
    return None


_branch_maps = {}


def _preds(mod, lo, hi):
    """Para cada dirección de [lo, hi), los saltos que llegan a ella: {destino: [(origen, condición)]}."""
    key = (id(mod), lo, hi)
    if key not in _branch_maps:
        m = {}
        for q in range(lo, hi, 4):
            if not mod.in_text(q):
                break
            b = _branch(mod.word(q), q)
            if b:
                m.setdefault(b[0], []).append((q, b[1]))
        _branch_maps[key] = m
    return _branch_maps[key]


def backward_path(mod, pc, lo):
    """Camino hacia atrás desde pc siguiendo el flujo de control: [(dirección, cómo se llegó)].

    De cada bloque se pasa a su único predecesor: la instrucción anterior si se sigue de largo
    ("cae"), o el salto que lleva a él ("salta", condición). Si hay más de uno, se para.
    """
    hi = pc + 0x4000
    preds = _preds(mod, lo, hi)
    path, seen, q = [], set(), pc
    while len(path) < MAX_BACK:
        prev = q - 4
        falls = prev >= lo and mod.in_text(prev) and not _uncond(mod.word(prev))
        jumps = preds.get(q, [])
        if falls and not jumps:
            step = (prev, "cae")
        elif not falls and len(jumps) == 1:
            step = (jumps[0][0], ("salta", jumps[0][1]))
        else:
            break
        if step[0] in seen:
            break
        seen.add(step[0])
        path.append(step)
        q = step[0]
    return path


def find_bound(mod, path):
    """La cota del índice en el camino: CMP #N y B.HI/B.HS sin saltar, o B.LS/B.LO saltando.

    Devuelve (registro, posición del CMP en el camino, cantidad de entradas) o None.
    """
    for i, (addr, how) in enumerate(path):
        w = mod.word(addr)
        if not (bits(w, 31, 24) == 0b01010100 and not bits(w, 4, 4)):
            continue
        c = bits(w, 3, 0)
        taken = how != "cae"
        if not taken and c in (COND_HI, COND_HS):
            extra = 1 if c == COND_HI else 0
        elif taken and c in (COND_LS, COND_LO):
            extra = 1 if c == COND_LS else 0
        else:
            continue
        for j in range(i + 1, min(i + 4, len(path))):
            cmp = _cmp_imm(mod.word(path[j][0]))
            if cmp:
                return cmp[0], j, cmp[1] + extra
    return None


class Slicer:
    """Reconstruye el valor de un registro como expresión, hacia atrás por el camino del BR."""

    def __init__(self, mod, path, bound, entry):
        self.mod = mod
        self.path = path
        self.bound = bound
        self.entry = entry
        self.preds = _preds(mod, entry, (path[0][0] if path else entry) + 0x4000)

    def reg(self, r, pos=0, depth=0):
        if r == 31:
            return ("const", 0)
        if depth > 12:
            return ("var",)
        for i in range(pos, len(self.path)):
            if self.bound and r == self.bound[0] and i >= self.bound[1]:
                return ("var",)  # el valor que acota el CMP
            q = self.path[i][0]
            w = self.mod.word(q)
            if _is_call(w) and r in CALLER_SAVED:
                return ("var",)  # la llamada pudo cambiarlo
            e = self.effect(w, q, r, depth, lambda reg, i=i: self.reg(reg, i + 1, depth + 1), i)
            if e is not None:
                return e
        if self.bound and r == self.bound[0]:
            return ("var",)
        return self.merged(r, self.path[-1][0] if self.path else self.entry, depth)

    def merged(self, r, q, depth):
        """Valor de r antes de q juntando todos los caminos que llegan (el camino único se cortó en
        un bloque con varios predecesores, como la cabeza de un bucle). Los caminos que dan la vuelta
        sin cambiar r no aportan; si los demás coinciden, vale ese valor."""
        found, seen, work = [], set(), [q]
        while work and len(seen) < 20000:
            cur = work.pop()
            preds = [s for s, _ in self.preds.get(cur, [])]
            prev = cur - 4
            if prev >= self.entry and self.mod.in_text(prev) and not _uncond(self.mod.word(prev)):
                preds.append(prev)
            if not preds and cur <= self.entry:
                return ("var",)  # se llegó al inicio de la función sin encontrar el valor
            for p in preds:
                if p in seen:
                    continue
                seen.add(p)
                w = self.mod.word(p)
                if _is_call(w) and r in CALLER_SAVED:
                    return ("var",)
                e = self.effect(w, p, r, depth, lambda reg, p=p: self.merged(reg, p, depth + 1) if depth < 6 else ("var",), None)
                if e is None:
                    work.append(p)
                elif e == ("var",) or has_var(e):
                    return ("var",)
                else:
                    found.append(e)
        if work or not found or any(e != found[0] for e in found):
            return ("var",)
        return found[0]

    def forward(self, base, off, size, i, depth):
        """Busca hacia atrás el guardado en [base + off] que alimenta una carga (código -O0)."""
        for j in range(i + 1, len(self.path)):
            p = self.path[j][0]
            w = self.mod.word(p)
            if _is_call(w):
                return None
            d = decode(p, w)
            if d.kind == "op" and f"{'c->sp' if base == 31 else f'c->x[{base}]'} = " in " ".join(d.lines):
                return None  # la base cambió
            unscaled = w & 0x3BE00C00 == 0x38000000
            if (w & 0x3BC00000 == 0x39000000 or unscaled) and not bits(w, 26, 26) and bits(w, 9, 5) == base:
                s_off = sext(bits(w, 20, 12), 9) if unscaled else bits(w, 21, 10) << bits(w, 31, 30)
                if s_off == off and bits(w, 31, 30) == size:
                    return self.reg(bits(w, 4, 0), j + 1, depth + 1)
        return None

    def effect(self, w, q, r, depth, rec, pos):
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
            if rn in (29, 31) and opc == 1 and pos is not None:  # variable local: el último STR a esa ranura
                fwd = self.forward(rn, off, size, pos, depth)
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
    lo = min(entry, br_pc)
    path = backward_path(mod, br_pc, lo)
    bound = find_bound(mod, path)
    expr = Slicer(mod, path, bound, lo).reg(reg)
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
