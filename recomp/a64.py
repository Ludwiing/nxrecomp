"""Decodificador de AArch64 (A64) que traduce cada instrucción a C.

Cada instrucción se decodifica por su codificación (no por su alias), según el
manual de arquitectura de Arm. El resultado es un `Insn` con el tipo de control de
flujo y las líneas de C que implementan la instrucción sobre el contexto `c`
(registros x0-x30, sp y banderas NZCV) que define runtime/nxrt.h.

Lo que todavía no está cubierto (SIMD/FP, atómicos LSE, CRC32, autenticación de
punteros) se traduce a una llamada a nx_trap(), que detiene el programa con la
dirección y la palabra de la instrucción.
"""

from dataclasses import dataclass, field

M64 = (1 << 64) - 1


def bits(x, hi, lo):
    return (x >> lo) & ((1 << (hi - lo + 1)) - 1)


def sext(v, n):
    return v - (1 << n) if v & (1 << (n - 1)) else v


@dataclass
class Insn:
    pc: int
    word: int
    # op: sigue a pc+4 · b · bl · bcond · br · blr · ret · trap
    kind: str = "op"
    lines: list = field(default_factory=list)
    target: int | None = None
    cond: str | None = None   # expresión C de la condición (bcond)
    reg: str | None = None    # expresión C del registro destino (br, blr)
    ref: int | None = None    # dirección que la instrucción materializa (ADR/ADRP)
    adrp_reg: int | None = None


# ---------- registros ----------

def R(n, sp=False):
    if n == 31:
        return "c->sp" if sp else "0"
    return f"c->x[{n}]"


def RW(n, w, sp=False):
    r = R(n, sp)
    if r == "0":
        return "(uint64_t)0" if w == 64 else "(uint32_t)0"
    return r if w == 64 else f"(uint32_t){r}"


def T(w):
    return "uint64_t" if w == 64 else "uint32_t"


def ST(w):
    return "int64_t" if w == 64 else "int32_t"


def setr(n, expr, w=64, sp=False):
    """Escribe un registro. Con w=32 se trunca a 32 bits y se extiende con ceros, como en ARM."""
    if n == 31 and not sp:
        return f"(void)({expr});"
    dst = "c->sp" if n == 31 else f"c->x[{n}]"
    if w == 32:
        return f"{dst} = (uint32_t)({expr});"
    return f"{dst} = (uint64_t)({expr});"


def mask(w):
    return M64 if w == 64 else 0xFFFFFFFF


def lit(v, w):
    v &= mask(w)
    return f"0x{v:x}ull" if w == 64 else f"0x{v:x}u"


COND = ["c->z", "!c->z", "c->c", "!c->c", "c->n", "!c->n", "c->v", "!c->v",
        "(c->c && !c->z)", "!(c->c && !c->z)", "(c->n == c->v)", "(c->n != c->v)",
        "(!c->z && c->n == c->v)", "!(!c->z && c->n == c->v)", "1", "1"]


def shifted(reg_expr, stype, amt, w):
    if amt == 0:
        return reg_expr
    if stype == 0:
        return f"({T(w)})({reg_expr} << {amt})"
    if stype == 1:
        return f"({T(w)})({reg_expr} >> {amt})"
    if stype == 2:
        return f"({T(w)})(({ST(w)}){reg_expr} >> {amt})"
    return f"ror{w}({reg_expr}, {amt})"


def extended(n, option, amt, w):
    r = R(n)
    base = {
        0: f"(uint64_t)(uint8_t){r}",
        1: f"(uint64_t)(uint16_t){r}",
        2: f"(uint64_t)(uint32_t){r}",
        3: f"(uint64_t){r}",
        4: f"(uint64_t)(int64_t)(int8_t){r}",
        5: f"(uint64_t)(int64_t)(int16_t){r}",
        6: f"(uint64_t)(int64_t)(int32_t){r}",
        7: f"(uint64_t){r}",
    }[option]
    e = f"({base} << {amt})" if amt else base
    return f"({T(w)}){e}"


def decode_bit_masks(n, imms, immr, immediate, w):
    """DecodeBitMasks() del manual de Arm. Devuelve (wmask, tmask) o None si no es válido."""
    combined = (n << 6) | (~imms & 0x3F)
    if combined == 0:
        return None
    length = combined.bit_length() - 1
    if length < 1:
        return None
    levels = (1 << length) - 1
    if immediate and (imms & levels) == levels:
        return None
    s = imms & levels
    r = immr & levels
    diff = (s - r) & 0x3F
    esize = 1 << length
    d = diff & levels
    emask = (1 << esize) - 1
    welem = (1 << (s + 1)) - 1
    telem = (1 << (d + 1)) - 1
    welem = ((welem >> r) | (welem << (esize - r))) & emask if r else welem
    wmask = tmask = 0
    for i in range(0, w, esize):
        wmask |= welem << i
        tmask |= telem << i
    return wmask & mask(w), tmask & mask(w)


def flags_add(w, a, b, carry, setf):
    """Suma con acarreo; con setf actualiza NZCV (AddWithCarry del manual)."""
    return f"awc{w}(c, {a}, {b}, {carry}, {1 if setf else 0})"


# ---------- decodificación ----------

def decode(pc, word):
    i = Insn(pc, word)
    op0 = bits(word, 28, 25)
    try:
        if op0 in (0b1000, 0b1001):
            dp_imm(i, word)
        elif op0 in (0b1010, 0b1011):
            branch_sys(i, word)
        elif op0 & 0b0101 == 0b0100:
            loadstore(i, word)
        elif op0 & 0b0111 == 0b0101:
            dp_reg(i, word)
        elif op0 & 0b0111 == 0b0111:
            import simd
            if bits(word, 28, 24) in (0b11110, 0b11111) and not bits(word, 30, 30):
                simd.fp_scalar(i, word)
            else:
                simd.advsimd(i, word)
        else:
            unimpl(i, "codificación no asignada")
    except Unsupported as e:
        unimpl(i, str(e))
    return i


class Unsupported(Exception):
    pass


def unimpl(i, why):
    i.kind = "trap"
    i.lines = [f'nx_trap(c, 0x{i.pc:x}ull, 0x{i.word:08x}u, "{why}");']


def dp_imm(i, x):
    rd, rn = bits(x, 4, 0), bits(x, 9, 5)
    sf = bits(x, 31, 31)
    w = 64 if sf else 32
    kind = bits(x, 25, 23)
    if kind in (0b000, 0b001):  # ADR / ADRP
        imm = sext((bits(x, 23, 5) << 2) | bits(x, 30, 29), 21)
        if bits(x, 31, 31):
            val = ((i.pc & ~0xFFF) + (imm << 12)) & M64
            i.adrp_reg = rd
        else:
            val = (i.pc + imm) & M64
        i.ref = val
        i.lines = [setr(rd, f"0x{val:x}ull")]
    elif kind == 0b010:  # ADD/SUB (inmediato)
        op, s = bits(x, 30, 30), bits(x, 29, 29)
        imm = bits(x, 21, 10) << (12 if bits(x, 22, 22) else 0)
        a = RW(rn, w, sp=True)
        if s:
            b = lit(~imm if op else imm, w)
            expr = flags_add(w, a, b, 1 if op else 0, True)
        else:
            expr = f"{a} {'-' if op else '+'} {lit(imm, w)}"
        i.lines = [setr(rd, expr, w, sp=not s)]
    elif kind == 0b100:  # lógicas con inmediato
        opc, n = bits(x, 30, 29), bits(x, 22, 22)
        if not sf and n:
            raise Unsupported("inmediato lógico inválido")
        m = decode_bit_masks(n, bits(x, 15, 10), bits(x, 21, 16), True, w)
        if m is None:
            raise Unsupported("inmediato lógico inválido")
        imm = lit(m[0], w)
        a = RW(rn, w)
        if opc == 3:
            i.lines = [f"{{ {T(w)} r = {a} & {imm}; nz{w}(c, r); {setr(rd, 'r', w)} }}"]
        else:
            o = "&|^"[opc]
            i.lines = [setr(rd, f"{a} {o} {imm}", w, sp=True)]
    elif kind == 0b101:  # MOVN / MOVZ / MOVK
        opc, hw = bits(x, 30, 29), bits(x, 22, 21)
        if opc == 1 or (not sf and hw > 1):
            raise Unsupported("move wide inválido")
        sh = hw * 16
        imm = bits(x, 20, 5) << sh
        if opc == 0:
            i.lines = [setr(rd, lit(~imm, w), w)]
        elif opc == 2:
            i.lines = [setr(rd, lit(imm, w), w)]
        else:
            keep = lit(~(0xFFFF << sh), w)
            i.lines = [setr(rd, f"({RW(rd, w)} & {keep}) | {lit(imm, w)}", w)]
    elif kind == 0b110:  # SBFM / BFM / UBFM
        opc, n = bits(x, 30, 29), bits(x, 22, 22)
        immr, imms = bits(x, 21, 16), bits(x, 15, 10)
        if opc == 3 or n != sf or (not sf and (immr > 31 or imms > 31)):
            raise Unsupported("bitfield inválido")
        m = decode_bit_masks(n, imms, immr, False, w)
        if m is None:
            raise Unsupported("bitfield inválido")
        wm, tm = lit(m[0], w), lit(m[1], w)
        src = RW(rn, w)
        rot = f"ror{w}({src}, {immr})" if immr else src
        if opc == 2:  # UBFM
            i.lines = [setr(rd, f"({rot} & {wm}) & {tm}", w)]
        elif opc == 0:  # SBFM
            top = f"(({T(w)})0 - (({src} >> {imms}) & 1))"
            i.lines = [f"{{ {T(w)} bot = {rot} & {wm}; {T(w)} top = {top}; "
                       f"{setr(rd, f'(top & ~{tm}) | (bot & {tm})', w)} }}"]
        else:  # BFM
            dst = RW(rd, w)
            i.lines = [f"{{ {T(w)} d = {dst}; {T(w)} bot = (d & ~{wm}) | ({rot} & {wm}); "
                       f"{setr(rd, f'(d & ~{tm}) | (bot & {tm})', w)} }}"]
    elif kind == 0b111:  # EXTR
        if bits(x, 30, 29) or bits(x, 22, 22) != sf or bits(x, 21, 21):
            raise Unsupported("extract inválido")
        rm, lsb = bits(x, 20, 16), bits(x, 15, 10)
        if not sf and lsb > 31:
            raise Unsupported("extract inválido")
        if lsb == 0:
            i.lines = [setr(rd, RW(rm, w), w)]
        else:
            i.lines = [setr(rd, f"({RW(rm, w)} >> {lsb}) | ({RW(rn, w)} << {w - lsb})", w)]
    else:
        raise Unsupported("datos inmediatos (etiquetas MTE)")


SYSREG = {
    (3, 3, 13, 0, 2): "TPIDR_EL0",
    (3, 3, 13, 0, 3): "TPIDRRO_EL0",
    (3, 3, 14, 0, 0): "CNTFRQ_EL0",
    (3, 3, 14, 0, 1): "CNTPCT_EL0",
    (3, 3, 14, 0, 2): "CNTVCT_EL0",
    (3, 3, 0, 0, 7): "DCZID_EL0",
    (3, 3, 4, 2, 0): "NZCV",
    (3, 3, 4, 4, 0): "FPCR",
    (3, 3, 4, 4, 1): "FPSR",
    (3, 3, 0, 0, 1): "CTR_EL0",
}


def branch_sys(i, x):
    if bits(x, 30, 26) == 0b00101:  # B / BL
        i.target = (i.pc + (sext(bits(x, 25, 0), 26) << 2)) & M64
        i.kind = "bl" if bits(x, 31, 31) else "b"
    elif bits(x, 30, 25) == 0b011010:  # CBZ / CBNZ
        w = 64 if bits(x, 31, 31) else 32
        i.kind = "bcond"
        i.target = (i.pc + (sext(bits(x, 23, 5), 19) << 2)) & M64
        i.cond = f"{RW(bits(x, 4, 0), w)} {'!=' if bits(x, 24, 24) else '=='} 0"
    elif bits(x, 30, 25) == 0b011011:  # TBZ / TBNZ
        b = (bits(x, 31, 31) << 5) | bits(x, 23, 19)
        i.kind = "bcond"
        i.target = (i.pc + (sext(bits(x, 18, 5), 14) << 2)) & M64
        i.cond = f"(({R(bits(x, 4, 0))} >> {b}) & 1) {'!=' if bits(x, 24, 24) else '=='} 0"
    elif bits(x, 31, 24) == 0b01010100 and not bits(x, 4, 4):  # B.cond
        i.kind = "bcond"
        i.target = (i.pc + (sext(bits(x, 23, 5), 19) << 2)) & M64
        i.cond = COND[bits(x, 3, 0)]
    elif bits(x, 31, 24) == 0b11010100:  # excepciones
        opc, imm, ll = bits(x, 23, 21), bits(x, 20, 5), bits(x, 1, 0)
        if opc == 0 and ll == 1:
            i.lines = [f"nx_svc(c, 0x{imm:x});"]
        elif opc == 1 and ll == 0:
            unimpl(i, f"BRK #{imm:#x}")
        else:
            raise Unsupported("excepción no soportada")
    elif bits(x, 31, 22) == 0b1101010100:
        system(i, x)
    elif bits(x, 31, 25) == 0b1101011:  # saltos a registro
        opc, op2, op3, rn, op4 = bits(x, 24, 21), bits(x, 20, 16), bits(x, 15, 10), bits(x, 9, 5), bits(x, 4, 0)
        if op2 != 0x1F or op3 != 0 or op4 != 0:
            raise Unsupported("salto con autenticación de punteros")
        if opc == 0:
            i.kind, i.reg = "br", R(rn)
        elif opc == 1:
            i.kind, i.reg = "blr", R(rn)
        elif opc == 2:
            i.kind = "ret"
        else:
            raise Unsupported("salto a registro no soportado")
    else:
        raise Unsupported("rama/sistema no soportado")


def system(i, x):
    l, op0, op1 = bits(x, 21, 21), bits(x, 20, 19), bits(x, 18, 16)
    crn, crm, op2, rt = bits(x, 15, 12), bits(x, 11, 8), bits(x, 7, 5), bits(x, 4, 0)
    if l == 0 and op0 == 0 and crn == 0b0010 and rt == 31:  # hints: NOP, YIELD, BTI, PACIASP...
        i.lines = ["/* hint */"]
    elif l == 0 and op0 == 0 and crn == 0b0011 and rt == 31:  # barreras
        if op2 == 0b010:
            i.lines = ["c->excl_addr = ~0ull; /* CLREX */"]
        else:
            i.lines = ["__atomic_thread_fence(__ATOMIC_SEQ_CST);"]
    elif l == 0 and op0 == 0 and crn == 0b0100 and rt == 31:  # MSR (inmediato) a PSTATE
        i.lines = ["/* MSR PSTATE */"]
    elif op0 == 1 and l == 0:  # SYS: mantenimiento de cachés
        if op1 == 3 and crn == 7 and crm == 4 and op2 == 1:
            i.lines = [f"nx_dc_zva(c, {R(rt)});"]
        elif op1 == 3 and crn == 7 and (crm, op2) in ((5, 1), (10, 1), (11, 1), (14, 1)):
            i.lines = ["/* IC IVAU / DC CVAC / DC CVAU / DC CIVAC */"]
        else:
            raise Unsupported("instrucción SYS no permitida en EL0")
    elif op0 >= 2:
        name = SYSREG.get((op0, op1, crn, crm, op2))
        if name is None:
            raise Unsupported(f"registro de sistema S{op0}_{op1}_C{crn}_C{crm}_{op2}")
        if l:
            i.lines = [setr(rt, f"nx_mrs(c, NX_{name})")]
        else:
            i.lines = [f"nx_msr(c, NX_{name}, {R(rt)});"]
    else:
        raise Unsupported("instrucción de sistema no soportada")


LD = {0: "ld8", 1: "ld16", 2: "ld32", 3: "ld64"}
STF = {0: "st8", 1: "st16", 2: "st32", 3: "st64"}
SX = {0: "int8_t", 1: "int16_t", 2: "int32_t"}


def mem_op(size, opc, rt, a="a"):
    """Carga o guardado de un registro entero de 1, 2, 4 u 8 bytes en la dirección `a`."""
    if opc == 0:
        return f"{STF[size]}({a}, {R(rt)});"
    if opc == 1:
        return setr(rt, f"{LD[size]}({a})")
    if opc == 2:
        if size == 3:
            return "/* PRFM */"
        return setr(rt, f"(uint64_t)(int64_t)({SX[size]}){LD[size]}({a})")
    if size >= 2:
        raise Unsupported("carga con signo inválida")
    return setr(rt, f"(uint32_t)(int32_t)({SX[size]}){LD[size]}({a})", 32)


def loadstore(i, x):
    v = bits(x, 26, 26)
    rt, rn = bits(x, 4, 0), bits(x, 9, 5)
    base = R(rn, sp=True)

    if bits(x, 29, 24) in (0b001100, 0b001101) and not bits(x, 31, 31):  # LD1-LD4 / ST1-ST4
        import simd
        simd.ldst_struct(i, x)
        return
    if v and (bits(x, 29, 27) in (0b101, 0b111) or bits(x, 29, 27) == 0b011 and not bits(x, 24, 24)):  # B/H/S/D/Q
        import simd
        simd.ldst_v(i, x)
        return

    if bits(x, 29, 24) == 0b001000:  # exclusivas y adquirir/liberar
        if v:
            raise Unsupported("SIMD exclusivo")
        size, o2, l, o1, rs, o0 = bits(x, 31, 30), bits(x, 23, 23), bits(x, 22, 22), bits(x, 21, 21), bits(x, 20, 16), bits(x, 15, 15)
        rt2 = bits(x, 14, 10)
        if o1 == 0 and rt2 != 31 or (o2 == 1 or l) and rs != 31 or o2 == 1 and o0 == 0:
            raise Unsupported("exclusiva con campos reservados")
        if not l and o2 == 0 and rs in (rt, rt2) or not l and o2 == 0 and rs == rn and rn != 31:
            raise Unsupported("exclusiva UNPREDICTABLE")
        if o2 == 1 and o1 == 0:  # LDAR / STLR
            i.lines = [f"{{ uint64_t a = {base}; __atomic_thread_fence(__ATOMIC_SEQ_CST); {mem_op(size, l, rt)} }}"]
        elif o2 == 0 and o1 == 0:  # LDXR / LDAXR / STXR / STLXR
            if l:
                i.lines = [f"{{ uint64_t a = {base}; c->excl_addr = a; {mem_op(size, 1, rt)} }}"]
            else:
                i.lines = [f"{{ uint64_t a = {base}; if (c->excl_addr == a) {{ {mem_op(size, 0, rt)} "
                           f"{setr(rs, '0', 32)} }} else {{ {setr(rs, '1', 32)} }} c->excl_addr = ~0ull; }}"]
        elif o2 == 0 and o1 == 1 and size >= 2:  # LDXP / STXP
            s = size
            n = 1 << s
            if l:
                i.lines = [f"{{ uint64_t a = {base}; c->excl_addr = a; uint64_t v1 = {LD[s]}(a), v2 = {LD[s]}(a + {n}); "
                           f"{setr(rt, 'v1')} {setr(rt2, 'v2')} }}"]
            else:
                i.lines = [f"{{ uint64_t a = {base}; if (c->excl_addr == a) {{ {STF[s]}(a, {R(rt)}); "
                           f"{STF[s]}(a + {n}, {R(rt2)}); {setr(rs, '0', 32)} }} else {{ {setr(rs, '1', 32)} }} "
                           f"c->excl_addr = ~0ull; }}"]
        else:
            raise Unsupported("CAS (atómicos LSE)")
        return

    if bits(x, 29, 27) == 0b011 and bits(x, 25, 24) == 0:  # LDR (literal)
        if v:
            raise Unsupported("LDR literal SIMD")
        opc = bits(x, 31, 30)
        a = (i.pc + (sext(bits(x, 23, 5), 19) << 2)) & M64
        if opc == 0:
            i.lines = [setr(rt, f"ld32(0x{a:x}ull)")]
        elif opc == 1:
            i.lines = [setr(rt, f"ld64(0x{a:x}ull)")]
        elif opc == 2:
            i.lines = [setr(rt, f"(uint64_t)(int64_t)(int32_t)ld32(0x{a:x}ull)")]
        else:
            i.lines = ["/* PRFM literal */"]
        return

    if bits(x, 29, 27) == 0b101:  # pares: LDP / STP / LDPSW
        if v:
            raise Unsupported("LDP/STP de SIMD")
        opc, typ, l = bits(x, 31, 30), bits(x, 24, 23), bits(x, 22, 22)
        rt2 = bits(x, 14, 10)
        if opc == 3 or (opc == 1 and (not l or typ == 0)):
            raise Unsupported("par inválido")
        if l and rt == rt2 or typ in (1, 3) and rn != 31 and rn in (rt, rt2):
            raise Unsupported("par UNPREDICTABLE")
        s = 3 if opc == 2 else 2
        off = sext(bits(x, 21, 15), 7) << s
        n = 1 << s
        post, wb = typ == 1, typ in (1, 3)
        a = base if post else f"{base} + {off}"
        out = [f"uint64_t a = {a};"]
        if l:
            if opc == 1:
                out.append(f"uint64_t v1 = (uint64_t)(int64_t)(int32_t)ld32(a), v2 = (uint64_t)(int64_t)(int32_t)ld32(a + 4);")
            else:
                out.append(f"uint64_t v1 = {LD[s]}(a), v2 = {LD[s]}(a + {n});")
        else:
            out.append(f"{STF[s]}(a, {R(rt)}); {STF[s]}(a + {n}, {R(rt2)});")
        if wb:
            out.append(f"{base} = a{' + ' + str(off) if post else ''};")
        if l:
            out.append(f"{setr(rt, 'v1')} {setr(rt2, 'v2')}")
        i.lines = ["{ " + " ".join(out) + " }"]
        return

    if bits(x, 29, 27) == 0b111:  # registro único
        if v:
            raise Unsupported("LDR/STR de SIMD")
        size, opc = bits(x, 31, 30), bits(x, 23, 22)
        if bits(x, 25, 24) == 0b01:  # inmediato sin signo
            a = f"{base} + {bits(x, 21, 10) << size}"
            i.lines = [f"{{ uint64_t a = {a}; {mem_op(size, opc, rt)} }}"]
            return
        if bits(x, 25, 24) != 0:
            raise Unsupported("load/store no soportado")
        if bits(x, 21, 21) == 0:
            imm = sext(bits(x, 20, 12), 9)
            form = bits(x, 11, 10)  # 0 sin escalar, 1 post, 2 sin privilegios, 3 pre
            if size == 3 and opc == 2 and form != 0 or size == 2 and opc == 3:
                raise Unsupported("load/store inválido")
            if form in (1, 3) and rn == rt and rn != 31:
                raise Unsupported("writeback UNPREDICTABLE")
            if form == 1:
                i.lines = [f"{{ uint64_t a = {base}; {mem_op(size, opc, rt)} {base} = a + {imm}; }}"]
            elif form == 3:
                i.lines = [f"{{ uint64_t a = {base} + {imm}; {mem_op(size, opc, rt)} {base} = a; }}"]
            else:
                i.lines = [f"{{ uint64_t a = {base} + {imm}; {mem_op(size, opc, rt)} }}"]
            return
        if bits(x, 11, 10) == 0b10:  # desplazamiento con registro
            option, s = bits(x, 15, 13), bits(x, 12, 12)
            if option not in (2, 3, 6, 7):
                raise Unsupported("extensión inválida")
            off = extended(bits(x, 20, 16), option, size if s else 0, 64)
            i.lines = [f"{{ uint64_t a = {base} + {off}; {mem_op(size, opc, rt)} }}"]
            return
        raise Unsupported("atómicos LSE (LDADD, SWP...)")

    raise Unsupported("load/store no soportado")


def dp_reg(i, x):
    sf = bits(x, 31, 31)
    w = 64 if sf else 32
    rd, rn, rm = bits(x, 4, 0), bits(x, 9, 5), bits(x, 20, 16)

    if bits(x, 28, 24) == 0b01010:  # lógicas con registro desplazado
        opc, st, n, amt = bits(x, 30, 29), bits(x, 23, 22), bits(x, 21, 21), bits(x, 15, 10)
        if not sf and amt >= 32:
            raise Unsupported("desplazamiento inválido")
        b = shifted(RW(rm, w), st, amt, w)
        if n:
            b = f"({T(w)})~{b}"
        a = RW(rn, w)
        if opc == 3:
            i.lines = [f"{{ {T(w)} r = {a} & {b}; nz{w}(c, r); {setr(rd, 'r', w)} }}"]
        else:
            i.lines = [setr(rd, f"{a} {'&|^'[opc]} {b}", w)]
        return

    if bits(x, 28, 24) == 0b01011:  # suma/resta con registro
        op, s = bits(x, 30, 30), bits(x, 29, 29)
        if bits(x, 21, 21) == 0:  # desplazado
            st, amt = bits(x, 23, 22), bits(x, 15, 10)
            if st == 3 or (not sf and amt > 31):
                raise Unsupported("desplazamiento inválido")
            a, b, dsp = RW(rn, w), shifted(RW(rm, w), st, amt, w), False
        else:  # extendido
            if bits(x, 23, 22) or bits(x, 12, 10) > 4:
                raise Unsupported("extendido inválido")
            a = RW(rn, w, sp=True)
            b = extended(rm, bits(x, 15, 13), bits(x, 12, 10), w)
            dsp = not s
        if s:
            expr = flags_add(w, a, f"({T(w)})~{b}" if op else b, 1 if op else 0, True)
        else:
            expr = f"{a} {'-' if op else '+'} {b}"
        i.lines = [setr(rd, expr, w, sp=dsp)]
        return

    top = bits(x, 28, 21)
    if top == 0b11010000:  # ADC / SBC
        if bits(x, 15, 10):
            raise Unsupported("ADC/SBC inválido")
        op, s = bits(x, 30, 30), bits(x, 29, 29)
        b = f"({T(w)})~{RW(rm, w)}" if op else RW(rm, w)
        i.lines = [setr(rd, flags_add(w, RW(rn, w), b, "c->c", s), w)]
        return

    if top == 0b11010010:  # CCMN / CCMP
        op, cond, nzcv = bits(x, 30, 30), bits(x, 15, 12), bits(x, 3, 0)
        if not bits(x, 29, 29) or bits(x, 10, 10) or bits(x, 4, 4):
            raise Unsupported("comparación condicional inválida")
        b = lit(rm, w) if bits(x, 11, 11) else RW(rm, w)
        if op:
            b = f"({T(w)})~{b}"
        i.lines = [f"if ({COND[cond]}) (void){flags_add(w, RW(rn, w), b, 1 if op else 0, True)}; "
                   f"else {{ c->n = {nzcv >> 3 & 1}; c->z = {nzcv >> 2 & 1}; c->c = {nzcv >> 1 & 1}; c->v = {nzcv & 1}; }}"]
        return

    if top == 0b11010100:  # CSEL / CSINC / CSINV / CSNEG
        op, cond, o2 = bits(x, 30, 30), bits(x, 15, 12), bits(x, 11, 10)
        if bits(x, 29, 29) or o2 > 1:
            raise Unsupported("selección condicional inválida")
        b = RW(rm, w)
        alt = {(0, 0): b, (0, 1): f"{b} + 1", (1, 0): f"~{b}", (1, 1): f"({T(w)})0 - {b}"}[(op, o2)]
        i.lines = [setr(rd, f"{COND[cond]} ? {RW(rn, w)} : ({T(w)})({alt})", w)]
        return

    if top == 0b11010110:
        opcode = bits(x, 15, 10)
        a, b = RW(rn, w), RW(rm, w)
        if bits(x, 30, 30) == 0:  # 2 fuentes
            ops = {
                0b000010: f"udiv{w}({a}, {b})",
                0b000011: f"sdiv{w}({a}, {b})",
                0b001000: f"{a} << ({b} & {w - 1})",
                0b001001: f"{a} >> ({b} & {w - 1})",
                0b001010: f"({T(w)})(({ST(w)}){a} >> ({b} & {w - 1}))",
                0b001011: f"ror{w}({a}, {b} & {w - 1})",
            }
            if opcode not in ops or bits(x, 29, 29):
                raise Unsupported("CRC32 o procesamiento de 2 fuentes no soportado")
            i.lines = [setr(rd, ops[opcode], w)]
        else:  # 1 fuente
            if bits(x, 20, 16) or bits(x, 29, 29):
                raise Unsupported("PAC u otra instrucción de 1 fuente")
            if opcode == 0:
                e = f"rbit{w}({a})"
            elif opcode == 1:
                e = f"rev16_{w}({a})"
            elif opcode == 2:
                e = "rev32_64(" + a + ")" if sf else f"__builtin_bswap32({a})"
            elif opcode == 3 and sf:
                e = f"__builtin_bswap64({a})"
            elif opcode == 4:
                e = f"clz{w}({a})"
            elif opcode == 5:
                e = f"cls{w}({a})"
            else:
                raise Unsupported("procesamiento de 1 fuente no soportado")
            i.lines = [setr(rd, e, w)]
        return

    if bits(x, 28, 24) == 0b11011:  # 3 fuentes
        op31, o0, ra = bits(x, 23, 21), bits(x, 15, 15), bits(x, 14, 10)
        if bits(x, 30, 29) or op31 in (2, 6) and ra != 31:
            raise Unsupported("multiplicación inválida")
        sign = "-" if o0 else "+"
        if op31 == 0:
            e = f"{RW(ra, w)} {sign} ({T(w)})({RW(rn, w)} * {RW(rm, w)})"
            i.lines = [setr(rd, e, w)]
        elif sf and op31 in (1, 5):  # SMADDL / UMADDL
            ty = "int32_t" if op31 == 1 else "uint32_t"
            prod = f"(uint64_t)((int64_t)({ty}){R(rn)} * (int64_t)({ty}){R(rm)})" if op31 == 1 else \
                f"((uint64_t)(uint32_t){R(rn)} * (uint64_t)(uint32_t){R(rm)})"
            i.lines = [setr(rd, f"{RW(ra, 64)} {sign} {prod}")]
        elif sf and op31 == 2 and not o0:
            i.lines = [setr(rd, f"smulh({R(rn)}, {R(rm)})")]
        elif sf and op31 == 6 and not o0:
            i.lines = [setr(rd, f"umulh({R(rn)}, {R(rm)})")]
        else:
            raise Unsupported("multiplicación no soportada")
        return

    raise Unsupported("procesamiento con registros no soportado")
