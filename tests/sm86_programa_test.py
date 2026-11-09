"""Prueba de punta a punta de la bajada de shaders de Switch 2: programas SM86 armados a mano (con la especificación
build/sm86_spec.json y comprobados contra nvdisasm), bajados a Maxwell con tools/sm86_programa.py, traducidos a
SPIR-V por totk_xlate y validados con spirv-val; el de cómputo además corre en la GPU (build/sm86_programa_vk) y se
compara con lo que tiene que dar. Código propio (MIT).

    python3 tests/sm86_programa_test.py [-v]     0 bien, 1 mal, 77 si falta totk_xlate o CUDA (nvdisasm)

Programas:
  cómputo (64 hilos): v = tid < 10 ? 3*tid : tid + 100 (salto con BSSY/BSYNC), + c3[0] (constante directa)
                      + c3[0x10 + 4*tid] (LDC con registro); compartida: STS v, BAR.SYNC, LDS v(63-tid);
                      un lazo con salto hacia atrás: s = 0 + 1 + ... + (tid-1); out[tid] = v + v(63-tid) + s (STG.E
                      a la dirección de c0[0x110], la convención de Ryujinx para memoria global -> buffer).
  fragmento:          SPH de 128 bytes (salida RT0), aritmética de punto flotante, FSETP y un salto adelante.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tools"))
VERBOSO = "-v" in sys.argv

SPEC = json.load(open(os.path.join(RAIZ, "build", "sm86_spec.json")))


def campo(lo, hi, clave, nombre, valor):
    """Pone un campo de la especificación (op1.n, op2.v...) en la instrucción (lo, hi)."""
    for k, b in SPEC[clave]["campos"][nombre].items():
        bit = (valor >> int(k)) & 1
        if b < 64:
            lo = lo & ~(1 << b) | bit << b
        else:
            hi = hi & ~(1 << (b - 64)) | bit << (b - 64)
    return lo, hi


def ins(plantilla, guarda=None, **campos):
    """Una instrucción desde una plantilla (lo, hi) del compilador, con sus campos cambiados por la spec."""
    lo, hi = plantilla
    clave = f"{lo & 0xFFF:#05x}"
    for nombre, valor in campos.items():
        lo, hi = campo(lo, hi, clave, nombre.replace("_", "."), valor)
    if guarda is not None:
        lo = lo & ~0xF000 | guarda << 12
    return lo, hi


def rel(plantilla, desp, guarda=None, bits=48):
    """Un salto (BRA/BSSY) con desplazamiento `desp` bytes desde la instrucción siguiente."""
    lo, hi = plantilla
    v = (desp >> 2) & ((1 << bits) - 1)
    x = (hi << 64 | lo) & ~(((1 << bits) - 1) << 34) | v << 34
    lo, hi = x & (1 << 64) - 1, x >> 64
    if guarda is not None:
        lo = lo & ~0xF000 | guarda << 12
    return lo, hi


# Plantillas: instrucciones tal como las deja el compilador de CUDA para SM86 (ver docs del módulo de saltos)
S2R = (0x0000000000057919, 0x000e220000002100)           # S2R R5, SR_TID.X
IADD3_I = (0x0000000404047810, 0x000fc80007ffe0ff)       # IADD3 R4, R4, 0x4, RZ
IADD3_R = (0x0000000404047210, 0x000fc80007ffe0ff)       # IADD3 R4, R4, R4, RZ
IADD3_C = (0x0000580003067a10, 0x000fe20007ffe0ff)       # IADD3 R6, R3, c[0x0][0x160], RZ
ISETP_R = (0x000000000000720c, 0x000fda0003f06270)       # ISETP.GE.AND P0, PT, R0, R0, PT
ISETP_I = (0x000000010000780c, 0x000fda0003f06270)       # ISETP.GE.AND P0, PT, R0, 0x1, PT
BRA = (0x0000000000007947, 0x000fea0003800000)
BSSY = (0x0000000000007945, 0x000fe20003800000)
BSYNC = (0x0000000000007941, 0x000fea0003800000)
MOV_C = (0x00000a0000017a02, 0x000fe40000000f00)         # MOV R1, c[0x0][0x28]
MOV_I = (0x0000012000077802, 0x000fe20000000f00)         # MOV R7, 0x120
LDC = (0x00c0000003037b82, 0x000e220000000800)           # LDC R3, c[0x3][R3]
STG = (0x0000000b06007986, 0x000fe8000c101904)           # STG.E [R6.64], R11
STS = (0x0000000005007388, 0x004fe80000000800)           # STS [R5], R0
LDS = (0x0000000004047984, 0x000e240000000800)           # LDS R4, [R4]
BAR = (0x0000000000007b1d, 0x000fec0000010000)           # BAR.SYNC.DEFER_BLOCKING 0x0
EXIT = (0x000000000000794d, 0x000fea0003800000)
FADD_I = (0xc0e00000060a7421, 0x000fe20000000000)        # FADD R10, R6, -7
FMUL_R = (0x00000006060a7220, 0x000fe20000400000)        # FMUL R10, R6, R6
FSETP_I = (0x3f8000000400780b, 0x001fda0003f04000)       # FSETP.GT.AND P0, PT, R4, 1, PT
RZ = 255


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def computo():
    """(instrucciones, textos esperados de nvdisasm)."""
    p = []

    def a(i, texto):
        p.append((i, texto))
    a(ins(S2R, op0_n=0), "S2R R0, SR_TID.X")
    a(ins(IADD3_R, op0_n=4, op3_n=0, op4_n=0, op5_n=RZ), "IADD3 R4, R0, R0, RZ")
    a(ins(IADD3_R, op0_n=4, op3_n=4, op4_n=4, op5_n=RZ), "IADD3 R4, R4, R4, RZ")          # 4*tid
    a(rel(BSSY, 0x60, bits=30), "BSSY B0, 0x70")                                            # 0x30 -> 0xa0
    a(ins(ISETP_I, op2_n=0, op3_v=10), "ISETP.GE.AND P0, PT, R0, 0xa, PT")
    a(rel(BRA, 0x20, guarda=0), "@P0 BRA 0x30")                                             # 0x50 -> 0x80
    a(ins(IADD3_R, op0_n=5, op3_n=0, op4_n=0, op5_n=0), "IADD3 R5, R0, R0, R0")             # 0x60: 3*tid
    a(rel(BRA, 0x10), "BRA 0x20")                                                           # 0x70 -> 0x90
    a(ins(IADD3_I, op0_n=5, op3_n=0, op4_v=100, op5_n=RZ), "IADD3 R5, R0, 0x64, RZ")        # 0x80
    a(BSYNC, "BSYNC B0")                                                                    # 0x90
    a(ins(IADD3_C, op0_n=5, op3_n=5, op4_banco=3, op4_off=0, op5_n=RZ), "IADD3 R5, R5, c[0x3][0x0], RZ")
    a(ins((LDC[0] & ~(0xFF << 24) | 4 << 24, LDC[1]), op0_n=7, op1_banco=3), "LDC R7, c[0x3][R4]")                                 # c3[4*tid]
    a(ins(IADD3_R, op0_n=5, op3_n=5, op4_n=7, op5_n=RZ), "IADD3 R5, R5, R7, RZ")
    a((0x0000000504007388, 0x004fe80000000800), "STS [R4], R5")
    a(BAR, "BAR.SYNC.DEFER_BLOCKING 0x0")
    a(ins(IADD3_I, op0_n=8, op3_n=4, op3_neg=1, op4_v=0xfc, op5_n=RZ), "IADD3 R8, -R4, 0xfc, RZ")
    a((0x0000000008097984, 0x000e240000000800), "LDS R9, [R8]")
    a(ins(IADD3_R, op0_n=5, op3_n=5, op4_n=9, op5_n=RZ), "IADD3 R5, R5, R9, RZ")
    a(ins(MOV_I, op0_n=10, op1_v=0), "MOV R10, 0x0")
    a(ins(MOV_I, op0_n=11, op1_v=0), "MOV R11, 0x0")
    lazo = len(p)
    a(ins(ISETP_R, op0_n=1, op2_n=11, op3_n=0), "ISETP.GE.AND P1, PT, R11, R0, PT")
    a(rel(BRA, 0x30, guarda=1), "@P1 BRA 0x40")                                             # @P1 BRA fuera
    a(ins(IADD3_R, op0_n=10, op3_n=10, op4_n=11, op5_n=RZ), "IADD3 R10, R10, R11, RZ")
    a(ins(IADD3_I, op0_n=11, op3_n=11, op4_v=1, op5_n=RZ), "IADD3 R11, R11, 0x1, RZ")
    a(rel(BRA, -(len(p) + 1 - lazo) * 16), None)                                            # BRA lazo
    a(ins(IADD3_R, op0_n=5, op3_n=5, op4_n=10, op5_n=RZ), "IADD3 R5, R5, R10, RZ")
    a(ins(MOV_C, op0_n=3, op1_banco=0, op1_off=0x114), "MOV R3, c[0x0][0x114]")
    a(ins(IADD3_C, op0_n=2, op3_n=4, op4_banco=0, op4_off=0x110, op5_n=RZ), "IADD3 R2, R4, c[0x0][0x110], RZ")
    a((0x0000000502007986, 0x000fe8000c101904), "STG.E [R2.64], R5")
    a(EXIT, "EXIT")
    a(rel(BRA, -0x10), None)                                                                # BRA a sí mismo
    return p


LOP3_I = (0x000000fc02037812, 0x000fe400078ec0ff)       # LOP3.LUT R3, R2, 0xfc, RZ, 0xc0, !PT
IMNMX_I = (0x0000000303037817, 0x000fca0003800000)      # IMNMX.U32 R3, R3, 0x3, PT
BRX = (0xfffffd8004007949, 0x000fea000383ffff)          # BRX R4 -0x280
CALL = (0x000003c000007944, 0x000fea0003c00000)         # CALL.REL.NOINC
RET = (0xfffff6f004007950, 0x000fea0003c3ffff)          # RET.REL.NODEC R4


def crudo(plantilla, **bits):
    """Cambia campos crudos: r16=Rd, r24=Ra... (registro de 8 bits en ese bit)."""
    lo, hi = plantilla
    for k, v in bits.items():
        b = int(k[1:])
        lo = lo & ~(0xFF << b) | v << b
    return lo, hi


def saltos():
    """Salto por tabla (BRX con su tabla en c1) y una subrutina (CALL.REL / RET.REL)."""
    p = []

    def a(i, texto):
        p.append((i, texto))
    a(ins(S2R, op0_n=0), "S2R R0, SR_TID.X")
    a(ins(LOP3_I, op1_n=6, op2_n=0, op3_v=3), "LOP3.LUT R6, R0, 0x3, RZ, 0xc0, !PT")
    a(crudo(IMNMX_I, r16=6, r24=6), "IMNMX.U32 R6, R6, 0x3, PT")
    a(ins(IADD3_R, op0_n=6, op3_n=6, op4_n=6, op5_n=RZ), "IADD3 R6, R6, R6, RZ")
    a(ins(IADD3_R, op0_n=6, op3_n=6, op4_n=6, op5_n=RZ), "IADD3 R6, R6, R6, RZ")
    a(crudo((LDC[0] & ~(0x1F << 54) | 1 << 54, LDC[1]), r16=6, r24=6), "LDC R6, c[0x1][R6]")
    a(rel(crudo(BRX, r24=6), -0x70), "BRX R6 -0x70")                                        # 0x60
    for k in range(4):                                                                      # 0x70, 0x90, 0xb0, 0xd0
        a(ins(IADD3_I, op0_n=5, op3_n=0, op4_v=0x10 * (k + 1), op5_n=RZ), f"IADD3 R5, R0, {0x10 * (k + 1):#x}, RZ")
        if k < 3:
            a(rel(BRA, (14 - len(p) - 1) * 16), None)                                       # BRA al CALL (0xe0)
    a(rel(CALL, (21 - 15) * 16), "CALL.REL.NOINC 0x70")                                     # 0xe0 -> 0x150
    a(ins(IADD3_R, op0_n=4, op3_n=0, op4_n=0, op5_n=RZ), "IADD3 R4, R0, R0, RZ")
    a(ins(IADD3_R, op0_n=4, op3_n=4, op4_n=4, op5_n=RZ), "IADD3 R4, R4, R4, RZ")
    a(ins(MOV_C, op0_n=3, op1_banco=0, op1_off=0x114), "MOV R3, c[0x0][0x114]")
    a(ins(IADD3_C, op0_n=2, op3_n=4, op4_banco=0, op4_off=0x110, op5_n=RZ), "IADD3 R2, R4, c[0x0][0x110], RZ")
    a((0x0000000502007986, 0x000fe8000c101904), "STG.E [R2.64], R5")
    a(EXIT, "EXIT")
    a(ins(IADD3_R, op0_n=5, op3_n=5, op4_n=5, op5_n=RZ), "IADD3 R5, R5, R5, RZ")            # 0x150: la subrutina
    a(crudo(RET, r24=8), None)                                                              # RET.REL.NODEC R8
    a(rel(BRA, -0x10), None)
    return p


C1_SALTOS = struct.pack("<4I", 0x70, 0x90, 0xb0, 0xd0)   # la tabla del BRX: direcciones SM86


def esperado_saltos(c3):
    return [2 * (t + 0x10 * (1 + (t & 3))) for t in range(64)]


def esperado_computo(c3):
    v = [(3 * t if t < 10 else t + 100) + c3[0] + c3[t] for t in range(64)]
    return [(v[t] + v[63 - t] + t * (t - 1) // 2) & 0xFFFFFFFF for t in range(64)]


def fragmento():
    p = []

    def a(i, texto):
        p.append((i, texto))
    a(ins(MOV_I, op0_n=0, op1_v=f32(1.0)), "MOV R0, 0x3f800000")
    a(ins(FADD_I, op0_n=1, op1_n=0, op2_v=f32(-0.5)), "FADD R1, R0, -0.5")
    a(ins(FMUL_R, op0_n=2, op1_n=1, op2_n=1), "FMUL R2, R1, R1")
    a(ins(FSETP_I, op0_n=0, op2_n=2, op3_v=f32(0.125)), "FSETP.GT.AND P0, PT, R2, 0.125, PT")
    a(rel(BRA, 0x10, guarda=0), None)                                                       # @P0: salta el MOV
    a(ins(MOV_I, op0_n=2, op1_v=0), "MOV R2, 0x0")
    a(ins(MOV_I, op0_n=3, op1_v=f32(1.0)), "MOV R3, 0x3f800000")
    a(EXIT, "EXIT")
    a(rel(BRA, -0x10), None)
    return p


def sph_fragmento():
    w = [0] * 32
    w[0] = 2 | (4 << 5) | (5 << 10)   # SPH tipo 2 (píxel), versión 4, etapa fragmento
    w[18] = 0xF                       # OMAP: RT0, xyzw
    return struct.pack("<32I", *w)


def comprobar_textos(prog, oraculo, nombre):
    malos = 0
    for k, ((lo, hi), quiere) in enumerate(prog):
        t = oraculo.desensamblar(lo, hi)
        if quiere is not None and t != quiere:
            print(f"MAL: {nombre} {k * 16:#x}: nvdisasm dice {t!r}, se armó para {quiere!r}")
            malos += 1
        elif VERBOSO:
            print(f"  {k * 16:#06x}  {t}")
    return malos


def main():
    import sm86_oraculo
    import sm86_programa as sp
    if not sp.totk_xlate() or not sm86_oraculo.disponible():
        print("(sin totk_xlate o sin CUDA no se prueba la bajada de shaders de Switch 2)")
        return 77
    spirv_val = shutil.which("spirv-val")
    ok = True
    tmp = tempfile.mkdtemp(prefix="sm86_programa_")
    c3 = [0x1000 + 7 * i for i in range(64)]
    casos = (("cómputo", computo(), b"", "cs", b"", esperado_computo),
             ("saltos", saltos(), b"", "cs", C1_SALTOS, esperado_saltos),
             ("fragmento", fragmento(), sph_fragmento(), "ps", b"", None))
    for nombre, prog, sph, etapa, c1, esperado in casos:
        if comprobar_textos(prog, sm86_oraculo, nombre):
            ok = False
            continue
        area = sph + b"".join(struct.pack("<QQ", lo, hi) for (lo, hi), _ in prog)
        try:
            p = sp.armar(area, etapa, c1)
        except sp.ErrorPrograma as e:
            print(f"MAL: {nombre}: {e}")
            ok = False
            continue
        extra = {"shared": 256} if etapa == "cs" else {"fout": {"0": "float"}}
        respuesta, spv, res = sp.traducir(p, etapa, tmp, clave=nombre.replace("ó", "o"), local=(64, 1, 1), extra=extra)
        if VERBOSO:
            print(f"  {nombre}: {len(prog)} instrucciones SM86 -> {len(p['codigo'])} bytes Maxwell; totk_xlate: {respuesta}")
            if res:
                print("  res.json:", json.dumps({k: res[k] for k in ("cbufs", "sbufs", "notes")}, ensure_ascii=False))
        if not respuesta.startswith("ok"):
            print(f"MAL: {nombre}: totk_xlate: {respuesta}")
            ok = False
            continue
        if res.get("notes"):
            print(f"aviso: {nombre}: notas del traductor: {res['notes']}")
        if spirv_val:
            v = subprocess.run([spirv_val, "--target-env", "vulkan1.2", spv], capture_output=True, text=True)
            if v.returncode:
                print(f"MAL: {nombre}: spirv-val: {v.stdout}{v.stderr}")
                ok = False
                continue
        if etapa == "cs":
            ok = correr_computo(nombre, spv, res, c3, p["c1"], esperado, tmp) and ok
        else:
            print(f"OK: fragmento SM86 -> Maxwell -> SPIR-V válido ({len(open(spv, 'rb').read())} bytes)")
    ok = errores_claros(sp) and ok
    shutil.rmtree(tmp, ignore_errors=True)
    return 0 if ok else 1


def errores_claros(sp):
    """Un shader que usa un registro reservado (R200, R253) o una instrucción sin regla da un error que lo nombra."""
    bien = True
    for prog, quiere in (([ins(IADD3_R, op0_n=200, op3_n=0, op4_n=0, op5_n=RZ), EXIT], "R200"),
                         ([ins(IADD3_R, op0_n=1, op3_n=253, op4_n=0, op5_n=RZ), EXIT], "R253"),
                         ([(0x0000000004bb7984, 0x000e240000000a00), EXIT], "R188")):   # LDS.64 R187, [R4]: R187-R188
        area = b"".join(struct.pack("<QQ", lo, hi) for lo, hi in prog)
        try:
            sp.armar(area, "cs")
            print(f"MAL: un shader con {quiere} se armó sin error")
            bien = False
        except sp.ErrorPrograma as e:
            if quiere not in str(e):
                print(f"MAL: el error no nombra {quiere}: {e}")
                bien = False
    if bien:
        print("OK: los registros reservados de la bajada (R188-R254) se rechazan con un error claro")
    return bien


def correr_computo(nombre, spv, res, c3, c1, esperado, tmp):
    """Corre el SPIR-V en la GPU con build/sm86_programa_vk: c0 = ceros (la dirección base del buffer 0 es 0),
    c3 = la tabla, el buffer de memoria global = salida."""
    runner = os.path.join(RAIZ, "build", "sm86_programa_vk")
    if not os.path.isfile(runner):
        print(f"MAL: falta {runner} (make build/sm86_programa_vk)")
        return False
    args = [runner, spv, "1"]
    salida = None
    for b in res["cbufs"]:
        datos = struct.pack("<64I", *c3) if b["slot"] == 3 else c1 if b["slot"] == 1 else bytes(4096)
        ruta = os.path.join(tmp, f"cb{b['slot']}.bin")
        open(ruta, "wb").write(datos + bytes(max(0, 4096 - len(datos))))
        args.append(f"{b['binding']}:u:{ruta}")
    for b in res["sbufs"]:
        ruta = os.path.join(tmp, f"sb{b['slot']}.bin")
        open(ruta, "wb").write(bytes(256))
        args.append(f"{b['binding']}:s:{ruta}")
        salida = ruta
    if salida is None:
        print(f"MAL: el cómputo no declara buffer de salida (res.json: {res['sbufs']})")
        return False
    r = subprocess.run(args, capture_output=True, text=True, timeout=60)
    import glob
    for icd in sorted(glob.glob("/usr/share/vulkan/icd.d/*.json")) if r.returncode == 77 else []:
        # la GPU de siempre no pudo (p. ej. sin memoria de video): otro controlador de Vulkan
        r = subprocess.run(args, capture_output=True, text=True, timeout=60,
                           env=dict(os.environ, VK_ICD_FILENAMES=icd, VK_DRIVER_FILES=icd))
        if r.returncode != 77:
            break
    if VERBOSO:
        print("  " + r.stderr.strip().replace("\n", "\n  "))
    if r.returncode == 77:
        print("(sin Vulkan no se corre el shader de cómputo bajado)")
        return True
    if r.returncode:
        print(f"MAL: sm86_programa_vk: {r.stdout}{r.stderr}")
        return False
    tiene = list(struct.unpack("<64I", open(salida, "rb").read()[:256]))
    quiere = esperado(c3)
    if tiene != quiere:
        malos = [t for t in range(64) if tiene[t] != quiere[t]]
        print(f"MAL: {nombre}: el cómputo bajado dio otra cosa en {len(malos)} hilos; ej. hilo {malos[0]}: "
              f"{tiene[malos[0]]:#x} en vez de {quiere[malos[0]]:#x}")
        return False
    que = {"cómputo": "saltos, lazo, LDC, compartida, BAR, STG", "saltos": "BRX con tabla en c1, CALL/RET"}[nombre]
    print(f"OK: {nombre} SM86 ({que}) -> Maxwell -> SPIR-V corre en la GPU y da lo esperado en los 64 hilos")
    return True


if __name__ == "__main__":
    sys.exit(main())
