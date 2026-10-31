"""Punto flotante escalar, NEON (Advanced SIMD) y sus cargas/guardados.

La Switch usa un Cortex-A57: ARMv8.0-A, sin media precisión aritmética, sin
productos punto ni atómicos LSE. Eso define qué se traduce aquí.
"""

from a64 import COND, R, RW, Unsupported, bits, lit, setr

FT = {0: 32, 1: 64, 3: 16}  # ftype → ancho


def S(n, w):
    return f"c->vr[{n}].{ {8: 'b', 16: 'h', 32: 's', 64: 'd'}[w] }[0]"


def wr(d, w, expr):
    """Escribe un escalar en Vd y pone en cero el resto del registro, como ARM."""
    return f"wrv{w}(c, {d}, {expr});"


def vfp_expand_imm(imm8, w):
    e = 8 if w == 32 else 11
    f = w - e - 1
    sign = imm8 >> 7
    b6 = (imm8 >> 6) & 1
    exp = ((b6 ^ 1) << (e - 1)) | (((1 << (e - 3)) - 1 if b6 else 0) << 2) | ((imm8 >> 4) & 3)
    frac = (imm8 & 0xF) << (f - 4)
    return (sign << (w - 1)) | (exp << f) | frac


def fp_scalar(i, x):
    """Grupo 'punto flotante escalar' (bits 28:24 = 11110 / 11111, bit 30 = 0)."""
    if bits(x, 29, 29):
        raise Unsupported("FP con S reservado")
    conversion = bits(x, 28, 24) == 0b11110 and (bits(x, 21, 21) == 0 or bits(x, 15, 10) == 0)
    if bits(x, 31, 31) and not conversion:  # en las conversiones el bit 31 es sf
        raise Unsupported("FP con M reservado")
    ftype = bits(x, 23, 22)
    rd, rn, rm = bits(x, 4, 0), bits(x, 9, 5), bits(x, 20, 16)

    if bits(x, 28, 24) == 0b11111:  # FMADD / FMSUB / FNMADD / FNMSUB
        if ftype not in (0, 1):
            raise Unsupported("FMA de media precisión")
        w = FT[ftype]
        ra, o1, o0 = bits(x, 14, 10), bits(x, 21, 21), bits(x, 15, 15)
        sign = lit(1 << (w - 1), 64 if w == 64 else 32)
        a, n, m = S(ra, w), S(rn, w), S(rm, w)
        if o1:
            a = f"({a} ^ {sign})"
        if o0 != o1:
            n = f"({n} ^ {sign})"
        i.lines = [wr(rd, w, f"fmuladd{w}({a}, {n}, {m})")]
        return

    if bits(x, 21, 21) == 0:  # conversión con punto fijo
        sf, rmode, opcode, scale = bits(x, 31, 31), bits(x, 20, 19), bits(x, 18, 16), bits(x, 15, 10)
        sf = bits(x, 31, 31)
        if ftype not in (0, 1) or (not sf and scale < 32):
            raise Unsupported("conversión de punto fijo inválida")
        fbits = 64 - scale
        w = FT[ftype]
        iw = 64 if sf else 32
        if rmode == 3 and opcode in (0, 1):  # FCVTZS / FCVTZU
            fn = f"f2{'u' if opcode else 's'}{iw}"
            i.lines = [setr(rd, f"({'uint' if iw == 64 else 'uint'}{iw}_t){fn}(fval{w}({S(rn, w)}), 3, 0x1p{fbits})", iw)]
        elif rmode == 0 and opcode in (2, 3):  # SCVTF / UCVTF
            src = f"({'uint' if opcode == 3 else 'int'}{iw}_t){R(rn)}"
            ty = "float" if w == 32 else "double"
            i.lines = [wr(rd, w, f"u{w}of(({ty})({ty})({src}) * ({ty})0x1p-{fbits})")]
        else:
            raise Unsupported("conversión de punto fijo inválida")
        return

    low = bits(x, 15, 10)
    if low == 0:  # FP <-> entero
        conv_int(i, x, ftype, rd, rn)
        return
    if bits(x, 14, 10) == 0b10000:  # 1 fuente
        opcode = bits(x, 20, 15)
        if ftype not in (0, 1, 3):
            raise Unsupported("tipo FP inválido")
        w = FT[ftype]
        if opcode >> 2 == 0b0001:  # FCVT entre precisiones
            to = bits(x, 16, 15)
            if to == 2 or to == ftype:
                raise Unsupported("FCVT inválido")
            tw = FT[to]
            suf = {16: "h", 32: "s", 64: "d"}
            i.lines = [wr(rd, tw, f"cvt_{suf[tw]}_{suf[w]}({S(rn, w)})")]
            return
        if w == 16:
            raise Unsupported("aritmética de media precisión (ARMv8.2)")
        sign = lit(1 << (w - 1), 64 if w == 64 else 32)
        a = S(rn, w)
        ops = {
            0b000000: a,
            0b000001: f"{a} & ~{sign}",
            0b000010: f"{a} ^ {sign}",
            0b000011: f"fsqrt{w}({a})",
            0b001000: f"frint{w}({a}, 0)",
            0b001001: f"frint{w}({a}, 1)",
            0b001010: f"frint{w}({a}, 2)",
            0b001011: f"frint{w}({a}, 3)",
            0b001100: f"frint{w}({a}, 4)",
            0b001110: f"frint{w}({a}, 0)",
            0b001111: f"frint{w}({a}, 0)",
        }
        if opcode not in ops:
            raise Unsupported("FP de 1 fuente no soportado")
        i.lines = [wr(rd, w, ops[opcode])]
        return
    if bits(x, 13, 10) == 0b1000:  # FCMP / FCMPE
        if ftype not in (0, 1) or bits(x, 15, 14) or bits(x, 2, 0):
            raise Unsupported("FCMP inválido")
        w = FT[ftype]
        zero = bits(x, 3, 3)  # con cero, el campo Rm se ignora
        b = "0" if zero else S(rm, w)
        i.lines = [f"{{ unsigned f = fcmp{w}({S(rn, w)}, {b}); c->n = f >> 3; c->z = f >> 2 & 1; c->c = f >> 1 & 1; c->v = f & 1; }}"]
        return
    if bits(x, 12, 10) == 0b100:  # FMOV (inmediato)
        if ftype not in (0, 1) or bits(x, 9, 5):
            raise Unsupported("FMOV inmediato inválido")
        w = FT[ftype]
        i.lines = [wr(rd, w, lit(vfp_expand_imm(bits(x, 20, 13), w), 64 if w == 64 else 32))]
        return
    if ftype not in (0, 1):
        raise Unsupported("FP de media precisión")
    w = FT[ftype]
    a, b = S(rn, w), S(rm, w)
    kind = bits(x, 11, 10)
    if kind == 0b01:  # FCCMP / FCCMPE
        cond, nzcv = bits(x, 15, 12), bits(x, 3, 0)
        i.lines = [f"{{ unsigned f = {COND[cond]} ? fcmp{w}({a}, {b}) : {nzcv}; "
                   f"c->n = f >> 3; c->z = f >> 2 & 1; c->c = f >> 1 & 1; c->v = f & 1; }}"]
    elif kind == 0b10:  # 2 fuentes
        opcode = bits(x, 15, 12)
        sign = lit(1 << (w - 1), 64 if w == 64 else 32)
        ops = {0: "fmul", 1: "fdiv", 2: "fadd", 3: "fsub", 4: "fmax", 5: "fmin", 6: "fmaxnm", 7: "fminnm"}
        if opcode in ops:
            e = f"{ops[opcode]}{w}({a}, {b})"
        elif opcode == 8:
            e = f"fmul{w}({a}, {b}) ^ {sign}"
        else:
            raise Unsupported("FP de 2 fuentes no soportado")
        i.lines = [wr(rd, w, e)]
    elif kind == 0b11:  # FCSEL
        i.lines = [wr(rd, w, f"{COND[bits(x, 15, 12)]} ? {a} : {b}")]
    else:
        raise Unsupported("FP no asignado")


def conv_int(i, x, ftype, rd, rn):
    sf, rmode, opcode = bits(x, 31, 31), bits(x, 20, 19), bits(x, 18, 16)
    iw = 64 if sf else 32
    if opcode in (6, 7):  # FMOV entre registros generales y de FP
        if rmode == 0 and (sf, ftype) in ((0, 0), (1, 1)):
            w = 64 if sf else 32
            i.lines = [setr(rd, S(rn, w), w)] if opcode == 6 else [wr(rd, w, RW(rn, w))]
        elif rmode == 1 and sf and ftype == 2:  # FMOV Xd, Vn.D[1] / Vd.D[1], Xn
            i.lines = [setr(rd, f"c->vr[{rn}].d[1]")] if opcode == 6 else [f"c->vr[{rd}].d[1] = {R(rn)};"]
        else:
            raise Unsupported("FMOV inválido")
        return
    if ftype not in (0, 1):
        raise Unsupported("conversión de media precisión")
    w = FT[ftype]
    if opcode in (0, 1, 4, 5):  # FCVT{N,P,M,Z,A}{S,U}
        if opcode in (4, 5) and rmode:
            raise Unsupported("FCVTA inválido")
        mode = {0: 0, 1: 1, 2: 2, 3: 3}[rmode] if opcode < 4 else 4
        fn = f"f2{'u' if opcode & 1 else 's'}{iw}"
        i.lines = [setr(rd, f"(uint{iw}_t){fn}(fval{w}({S(rn, w)}), {mode}, 1.0)", iw)]
    elif opcode in (2, 3) and rmode == 0:  # SCVTF / UCVTF
        ty = "float" if w == 32 else "double"
        src = f"({'uint' if opcode == 3 else 'int'}{iw}_t){R(rn)}"
        i.lines = [wr(rd, w, f"u{w}of(({ty})({src}))")]
    else:
        raise Unsupported("conversión FP/entero inválida")


def advsimd(i, x):
    import neon
    neon.decode(i, x)


# ---------- cargas y guardados de registros V ----------

LANE = {8: "b", 16: "h", 32: "s", 64: "d"}


def vload(w, rt, a):
    if w == 128:
        return f"c->vr[{rt}] = ldq({a});"
    return f"wrv{w}(c, {rt}, ld{w}({a}));"


def vstore(w, rt, a):
    if w == 128:
        return f"stq({a}, c->vr[{rt}]);"
    return f"st{w}({a}, c->vr[{rt}].{LANE[w]}[0]);"


def ldst_v(i, x):
    """LDR/STR/LDP/STP y LDR literal de registros B, H, S, D y Q."""
    from a64 import extended, sext
    rt, rn = bits(x, 4, 0), bits(x, 9, 5)
    base = R(rn, sp=True)
    cls = bits(x, 29, 27)
    if cls == 0b011:  # literal
        opc = bits(x, 31, 30)
        if opc == 3:
            raise Unsupported("LDR literal SIMD inválido")
        a = (i.pc + (sext(bits(x, 23, 5), 19) << 2)) & (2**64 - 1)
        i.lines = [vload(32 << opc, rt, f"0x{a:x}ull")]
        return
    if cls == 0b101:  # pares
        opc, typ, l, rt2 = bits(x, 31, 30), bits(x, 24, 23), bits(x, 22, 22), bits(x, 14, 10)
        if opc == 3:
            raise Unsupported("par SIMD inválido")
        if l and rt == rt2:
            raise Unsupported("par UNPREDICTABLE")
        w = 32 << opc
        n = w // 8
        off = sext(bits(x, 21, 15), 7) * n
        post, wb = typ == 1, typ in (1, 3)
        out = [f"uint64_t a = {base if post else f'{base} + {off}'};"]
        if l:
            ty = "V128" if w == 128 else f"uint{w}_t"
            ld = "ldq" if w == 128 else f"ld{w}"
            out.append(f"{ty} v1 = {ld}(a), v2 = {ld}(a + {n});")
        else:
            out.append(f"{vstore(w, rt, 'a')} {vstore(w, rt2, f'a + {n}')}")
        if wb:
            out.append(f"{base} = a{f' + {off}' if post else ''};")
        if l:
            if w == 128:
                out.append(f"c->vr[{rt}] = v1; c->vr[{rt2}] = v2;")
            else:
                out.append(f"wrv{w}(c, {rt}, v1); wrv{w}(c, {rt2}, v2);")
        i.lines = ["{ " + " ".join(out) + " }"]
        return
    if cls != 0b111:
        raise Unsupported("load/store SIMD no soportado")
    size, opc = bits(x, 31, 30), bits(x, 23, 22)
    if opc & 2:
        if size:
            raise Unsupported("load/store SIMD inválido")
        w, l, scale = 128, opc & 1, 4
    else:
        w, l, scale = 8 << size, opc, size
    op = (lambda a: vload(w, rt, a)) if l else (lambda a: vstore(w, rt, a))
    if bits(x, 25, 24) == 0b01:
        i.lines = [f"{{ uint64_t a = {base} + {bits(x, 21, 10) << scale}; {op('a')} }}"]
        return
    if bits(x, 21, 21) == 0:
        imm = sext(bits(x, 20, 12), 9)
        form = bits(x, 11, 10)
        if form == 2:
            raise Unsupported("LDTR de SIMD no existe")
        if form == 1:
            i.lines = [f"{{ uint64_t a = {base}; {op('a')} {base} = a + {imm}; }}"]
        elif form == 3:
            i.lines = [f"{{ uint64_t a = {base} + {imm}; {op('a')} {base} = a; }}"]
        else:
            i.lines = [f"{{ uint64_t a = {base} + {imm}; {op('a')} }}"]
        return
    if bits(x, 11, 10) == 0b10:
        option, s = bits(x, 15, 13), bits(x, 12, 12)
        if option not in (2, 3, 6, 7):
            raise Unsupported("extensión inválida")
        off = extended(bits(x, 20, 16), option, scale if s else 0, 64)
        i.lines = [f"{{ uint64_t a = {base} + {off}; {op('a')} }}"]
        return
    raise Unsupported("load/store SIMD no soportado")


MULTI = {0b0000: (1, 4), 0b0010: (4, 1), 0b0100: (1, 3), 0b0110: (3, 1), 0b0111: (1, 1), 0b1000: (1, 2), 0b1010: (2, 1)}


def ldst_struct(i, x):
    """LD1-LD4 / ST1-ST4: varias estructuras, un carril, o replicado (LDnR)."""
    q, l, post = bits(x, 30, 30), bits(x, 22, 22), bits(x, 23, 23)
    rm, rn, rt = bits(x, 20, 16), bits(x, 9, 5), bits(x, 4, 0)
    if not post and rm:
        raise Unsupported("estructura SIMD inválida")
    base = R(rn, sp=True)
    datasize = 128 if q else 64
    out = [f"uint64_t a0 = {base}, a = a0;"]

    if bits(x, 24, 24) == 0:  # varias estructuras
        opcode, size = bits(x, 15, 12), bits(x, 11, 10)
        if opcode not in MULTI or bits(x, 21, 21):
            raise Unsupported("LD/ST múltiple inválido")
        rpt, selem = MULTI[opcode]
        if size == 3 and not q and selem > 1:
            raise Unsupported("LD/ST múltiple inválido")
        esize = 8 << size
        elems, eb, lane = datasize // esize, esize // 8, LANE[esize]
        nregs = rpt + selem - 1
        regs = [(rt + k) % 32 for k in range(nregs)]
        total = rpt * selem * datasize // 8
        for k, r in enumerate(regs):
            out.append(f"V128 r{k} = {'{0}' if l else f'c->vr[{r}]'};")
        for r in range(rpt):
            body = []
            for s in range(selem):
                k = r + s
                body.append(f"r{k}.{lane}[e] = ld{esize}(a);" if l else f"st{esize}(a, r{k}.{lane}[e]);")
                body.append(f"a += {eb};")
            out.append(f"for (int e = 0; e < {elems}; e++) {{ {' '.join(body)} }}")
        if l:
            out += [f"c->vr[{r}] = r{k};" for k, r in enumerate(regs)]
    else:  # un carril o replicado
        opcode, s, size, rbit = bits(x, 15, 13), bits(x, 12, 12), bits(x, 11, 10), bits(x, 21, 21)
        selem = ((opcode & 1) << 1 | rbit) + 1
        scale = opcode >> 1
        replicate = False
        if scale == 3:
            if not l or s:
                raise Unsupported("LDnR inválido")
            scale, replicate = size, True
        elif scale == 0:
            index = q << 3 | s << 2 | size
        elif scale == 1:
            if size & 1:
                raise Unsupported("carril inválido")
            index = q << 2 | s << 1 | size >> 1
        else:
            if size & 2 or (size & 1 and s):
                raise Unsupported("carril inválido")
            if size & 1:
                index, scale = q, 3
            else:
                index = q << 1 | s
        esize = 8 << scale
        eb, lane = esize // 8, LANE[esize]
        total = selem * eb
        for k in range(selem):
            r = (rt + k) % 32
            if replicate:
                out.append(f"{{ uint{esize}_t v = ld{esize}(a); V128 t = {{0}}; "
                           f"for (int e = 0; e < {datasize // esize}; e++) t.{lane}[e] = v; c->vr[{r}] = t; }}")
            elif l:
                out.append(f"c->vr[{r}].{lane}[{index}] = ld{esize}(a);")
            else:
                out.append(f"st{esize}(a, c->vr[{r}].{lane}[{index}]);")
            out.append(f"a += {eb};")
    if post:
        out.append(f"{base} = a0 + {total if rm == 31 else R(rm)};")
    i.lines = ["{ " + " ".join(out) + " }"]
