"""El programa completo en memoria: los módulos cargados uno detrás de otro, enlazados.

Hace el trabajo del rtld de Horizon, pero antes de traducir: ubica los módulos,
lee su sección dinámica (por MOD0), aplica las reubicaciones y resuelve los símbolos
entre módulos. Así el traductor ve los valores reales de las vtables, la GOT y las
tablas de punteros. Reglas de enlace (las mismas que usa nxnative, que imitan a rtld):
 - un símbolo se busca en todos los módulos en orden de carga; gana la primera definición;
 - un símbolo local o con visibilidad no por defecto se enlaza a su propio módulo;
 - un símbolo débil que nadie define vale 0;
 - lo que nadie define queda como importación: una dirección propia que, si se llama,
   detiene el programa diciendo el nombre.
"""

import os
import hashlib
import struct
from collections import Counter

from nso import PAGE, FormatError, load_module, parse_npdm

M64 = (1 << 64) - 1
BASE = 0x08000000                 # inicio de la región de código de Horizon sin ASLR
IMPORT_BASE = 0x00FF00000000      # direcciones de las importaciones sin definir (fuera de todo módulo)
EXEFS_ORDER = ["rtld", "main"] + [f"subsdk{k}" for k in range(10)] + ["sdk"]

DT = {"NEEDED": 1, "PLTRELSZ": 2, "HASH": 4, "STRTAB": 5, "SYMTAB": 6, "RELA": 7, "RELASZ": 8, "STRSZ": 10,
      "INIT": 12, "FINI": 13, "JMPREL": 23, "INIT_ARRAY": 25, "INIT_ARRAYSZ": 27, "RELRSZ": 35, "RELR": 36,
      "GNU_HASH": 0x6FFFFEF5}
R_ABS64, R_GLOB_DAT, R_JUMP_SLOT, R_RELATIVE = 257, 1025, 1026, 1027
R_NAMES = {0: "NONE", 257: "ABS64", 1025: "GLOB_DAT", 1026: "JUMP_SLOT", 1027: "RELATIVE", 1028: "TLS_DTPMOD64",
           1029: "TLS_DTPREL64", 1030: "TLS_TPREL64", 1031: "TLSDESC"}


def align(v, a=PAGE):
    return (v + a - 1) & ~(a - 1)


class Symbol:
    __slots__ = ("name", "value", "info", "other", "shndx")

    def __init__(self, name, value, info, other, shndx):
        self.name, self.value, self.info, self.other, self.shndx = name, value, info, other, shndx

    @property
    def bind(self):
        return self.info >> 4  # 0 local, 1 global, 2 weak

    @property
    def is_func(self):
        return self.info & 0xF == 2

    @property
    def defined(self):
        return self.shndx != 0


class LoadedModule:
    def __init__(self, img, base):
        self.img = img
        self.name = img.name
        self.base = base
        self.dyn = {}
        self.syms = []
        self.mod0 = None
        self.dinamico = False      # NRO de nn::ro::LoadModule (Program modulos_ro)
        self.init = 0
        self.init_array = []
        self._parse_dynamic()

    def u32(self, off):
        return struct.unpack_from("<I", self.img.data, off)[0]

    def u64(self, off):
        return struct.unpack_from("<Q", self.img.data, off)[0]

    def cstr(self, off):
        end = self.img.data.index(0, off)
        return self.img.data[off:end].decode(errors="replace")

    def eh_frame_starts(self):
        """Inicios de función de la tabla de búsqueda de .eh_frame_hdr (MOD0 +0x10/+0x14), relativos al módulo.

        Las excepciones de C++ necesitan un FDE por función, así que la tabla nombra casi todas, también las que solo
        se alcanzan por punteros (callbacks, vtables armadas en ejecución) y que el recorrido desde BL no encuentra."""
        if self.mod0 is None or self.mod0 + 0x18 > self.img.size:
            return set()
        lo, hi = (self.mod0 + struct.unpack_from("<i", self.img.data, self.mod0 + k)[0] for k in (0x10, 0x14))
        if not (0 <= lo < hi <= self.img.size) or hi - lo < 12 or self.img.data[lo] != 1:
            return set()
        d = self.img.data

        def leer(pos, enc):
            """Un valor DWARF codificado con enc en pos: (valor, posición siguiente); None si no se sabe leer."""
            fmt = {0x03: "<I", 0x0B: "<i", 0x04: "<Q", 0x0C: "<q"}.get(enc & 0x0F)
            if fmt is None or pos + struct.calcsize(fmt) > hi:
                return None, pos
            v = struct.unpack_from(fmt, d, pos)[0]
            base = {0x00: 0, 0x10: pos, 0x30: lo}.get(enc & 0x70)
            return (None if base is None else v + base), pos + struct.calcsize(fmt)

        ptr_enc, count_enc, table_enc = d[lo + 1], d[lo + 2], d[lo + 3]
        _, pos = leer(lo + 4, ptr_enc)
        n, pos = leer(pos, count_enc)
        out = set()
        for _ in range(n or 0):
            inicio, pos = leer(pos, table_enc)
            _, pos = leer(pos, table_enc)
            if inicio is None:
                break
            out.add(inicio)
        return out

    def _parse_dynamic(self):
        if self.img.size < 8:
            return
        mod0 = self.u32(4)
        if mod0 + 8 > self.img.size or self.img.data[mod0:mod0 + 4] != b"MOD0":
            return
        self.mod0 = mod0
        dyn_rel = struct.unpack_from("<i", self.img.data, mod0 + 4)[0]
        if not dyn_rel:
            return
        p = mod0 + dyn_rel
        while p + 16 <= self.img.size:
            tag, val = struct.unpack_from("<qQ", self.img.data, p)
            if tag == 0:
                break
            self.dyn.setdefault(tag, val)
            p += 16
        nsyms = self._symbol_count()
        st, ss = self.dyn.get(DT["SYMTAB"]), self.dyn.get(DT["STRTAB"])
        if st is not None and ss is not None:
            for k in range(nsyms):
                name, info, other, shndx, value, _ = struct.unpack_from("<IBBHQQ", self.img.data, st + 24 * k)
                self.syms.append(Symbol(self.cstr(ss + name), value, info, other, shndx))
        self.init = self.dyn.get(DT["INIT"], 0)
        ia, iasz = self.dyn.get(DT["INIT_ARRAY"]), self.dyn.get(DT["INIT_ARRAYSZ"], 0)
        if ia is not None:
            self.init_array = [ia + 8 * k for k in range(iasz // 8)]  # se leen después de reubicar

    def _symbol_count(self):
        if DT["HASH"] in self.dyn:
            return self.u32(self.dyn[DT["HASH"]] + 4)  # nchain
        if DT["GNU_HASH"] in self.dyn:
            h = self.dyn[DT["GNU_HASH"]]
            nbuckets, symoffset, bloom_size = self.u32(h), self.u32(h + 4), self.u32(h + 8)
            buckets = h + 16 + 8 * bloom_size
            last = max((self.u32(buckets + 4 * k) for k in range(nbuckets)), default=0)
            if last < symoffset:
                return symoffset
            chains = buckets + 4 * nbuckets
            while not self.u32(chains + 4 * (last - symoffset)) & 1:
                last += 1
            return last + 1
        return 0

    def relocations(self):
        out = []
        for tab, size in ((DT["RELA"], DT["RELASZ"]), (DT["JMPREL"], DT["PLTRELSZ"])):
            if tab in self.dyn:
                for p in range(self.dyn[tab], self.dyn[tab] + self.dyn.get(size, 0), 24):
                    off, info, addend = struct.unpack_from("<QQq", self.img.data, p)
                    out.append((off, info & 0xFFFFFFFF, info >> 32, addend))
        if DT["RELR"] in self.dyn:  # reubicaciones relativas empaquetadas
            where = None
            for p in range(self.dyn[DT["RELR"]], self.dyn[DT["RELR"]] + self.dyn.get(DT["RELRSZ"], 0), 8):
                v = self.u64(p)
                if not v & 1:
                    where = v
                    out.append((where, R_RELATIVE, 0, None))
                    where += 8
                else:
                    for b in range(63):
                        if v >> (b + 1) & 1:
                            out.append((where + 8 * b, R_RELATIVE, 0, None))
                    where += 8 * 63
        return out


class Program:
    def __init__(self, images, base=BASE, npdm=None, modulos_ro=()):
        """images: los módulos que rtld carga al arrancar. modulos_ro: NRO que el juego carga después con
        nn::ro::LoadModule (volcados con NX_VOLCAR_NRO); van detrás, a una dirección fija que el runtime devuelve en
        ldr:ro MapManualLoadModuleMemory, y su init no corre al arrancar sino dentro de LoadModule."""
        self.modules = []
        addr = base
        for img in images:
            self.modules.append(LoadedModule(img, addr))
            addr += align(img.size + img.bss)
        for img in modulos_ro:
            m = LoadedModule(img, addr)
            m.dinamico = True
            self.modules.append(m)
            addr += align(img.size + img.bss)
        self.end = addr
        self.npdm = npdm
        self.imports = []          # nombres sin definir, en el orden de su dirección de stub
        self._import_index = {}
        self.unsupported = Counter()
        self.symbols = {}          # nombre -> dirección (primera definición global en orden de carga)
        self.globales = {}         # lo mismo, solo de los módulos del arranque (la búsqueda de rtld, LookupGlobalAuto)
        for m in self.modules:
            for s in m.syms:
                if s.defined and s.bind in (1, 2) and (s.other & 3) == 0 and s.name:
                    self.symbols.setdefault(s.name, m.base + s.value)
                    if not m.dinamico:
                        self.globales.setdefault(s.name, m.base + s.value)
        self._link()
        self.text_ranges = [(m.base + m.img.text[0], m.base + m.img.text[0] + m.img.text[1]) for m in self.modules]
        self.data_ranges = [(m.base + o, m.base + o + s) for m in self.modules for (o, s) in (m.img.ro, m.img.rw)]

    # ---------- enlace ----------
    def _import(self, name):
        if name not in self._import_index:
            self._import_index[name] = len(self.imports)
            self.imports.append(name)
        return IMPORT_BASE + 16 * self._import_index[name]

    def bind(self, m, s):
        if s.defined and (s.bind == 0 or (s.other & 3) != 0):
            return m.base + s.value
        v = self.symbols.get(s.name)
        if v is not None:
            return v
        if s.defined:
            return m.base + s.value
        if s.bind == 2:
            return 0
        return self._import(s.name)

    def _link(self):
        for m in self.modules:
            for off, typ, symi, addend in m.relocations():
                if typ == R_RELATIVE:
                    val = m.base + (addend if addend is not None else m.u64(off))
                elif typ in (R_ABS64, R_GLOB_DAT, R_JUMP_SLOT):
                    s = m.syms[symi]
                    val = self.bind(m, s) + (addend if typ != R_JUMP_SLOT else 0)
                elif typ == 0:
                    continue
                else:
                    self.unsupported[R_NAMES.get(typ, str(typ))] += 1
                    continue
                struct.pack_into("<Q", m.img.data, off, val & M64)
            m.init_array = [m.u64(p) for p in m.init_array]

    # ---------- acceso por dirección (lo que usan el traductor y jumptable) ----------
    def module_at(self, a):
        for m in self.modules:
            if m.base <= a < m.base + m.img.size:
                return m
        return None

    def read(self, a, n):
        m = self.module_at(a)
        if m is None or a + n > m.base + m.img.size:
            raise ValueError("fuera de la imagen")
        return bytes(m.img.data[a - m.base:a - m.base + n])

    # word e in_text se llaman millones de veces con un juego grande: el módulo y el rango de texto
    # de la última consulta quedan a mano, y las palabras se leen de una vista de 32 bits.
    _ultimo = (0, 0, None)  # (desde, hasta, palabras)

    def word(self, a):
        lo, hi, w = self._ultimo
        if not (lo <= a < hi) or a & 3:
            m = self.module_at(a)
            if m is None or a + 4 > m.base + m.img.size:
                raise ValueError("fuera de la imagen")
            if a & 3:
                return struct.unpack("<I", self.read(a, 4))[0]
            if not hasattr(m, "_palabras"):
                n = len(m.img.data) & ~3
                m._palabras = memoryview(bytes(m.img.data[:n])).cast("I")
            self._ultimo = lo, hi, w = (m.base, m.base + (len(m.img.data) & ~3), m._palabras)
        return w[(a - lo) >> 2]

    _texto = (0, 0)

    def in_text(self, a):
        if a & 3:
            return False
        lo, hi = self._texto
        if lo <= a < hi:
            return True
        for lo, hi in self.text_ranges:
            if lo <= a < hi:
                self._texto = (lo, hi)
                return True
        return False

    def entry_points(self):
        """Inicios seguros: la entrada de cada módulo, DT_INIT, init_array, funciones exportadas y las de .eh_frame_hdr."""
        out = set()
        for m in self.modules:
            out.add(m.base + m.img.text[0])
            out.update(m.base + a for a in m.eh_frame_starts() if self.in_text(m.base + a) and not a & 3)
            if m.init:
                out.add(m.base + m.init)
            out.update(a for a in m.init_array if self.in_text(a))
            out.update(m.base + s.value for s in m.syms if s.defined and s.is_func and self.in_text(m.base + s.value))
        return out

    def boot(self):
        """Cómo arranca: con nnMain (juego con SDK) o por la entrada del primer módulo (homebrew)."""
        inits = []
        for m in self.modules:
            # nxrecomp ya enlazó y reubicó los módulos; ejecutar rtld de nuevo
            # vuelve a inicializar el cargador dinámico y rompe su estado TLS.
            if m.name == "rtld" or getattr(m, "dinamico", False):   # el init de un módulo de nn::ro lo llama LoadModule
                continue
            if m.init:
                inits.append(m.base + m.init)
            if m.init_array and not self._init_dispatches_array(m):
                # Algunos módulos de Horizon recorren la tabla dentro de
                # DT_INIT; otros tienen DT_INIT independiente y necesitan que
                # el loader ejecute también sus constructores.
                inits.extend(a for a in m.init_array if a)
        return {"inits": inits, "main": self.symbols.get("nnMain"), "entry": self.modules[0].base + self.modules[0].img.text[0]}

    # La entrada de hilo del SDK guarda su ThreadType en TLR+0x1F8:
    # mrs x1, tpidrro_el0 / add x1, x1, #0x1f8 / str x0, [x1]
    # (runtime/nxkern.c reconoce la misma firma en los trampolines creados por SVC).
    FIRMA_ENTRADA_HILO = struct.pack("<3I", 0xD53BD061, 0x9107E021, 0xF9000020)

    def thread_hook_gots(self, ventana=40):
        """Entradas de la GOT de los hooks de nnmusl al iniciar y al terminar un hilo (TLS por hilo).

        Tras la firma de la entrada de hilo del SDK, los dos primeros pares ADRP + LDR (64 bits, offset
        sin signo) que leen de una misma página dan las dos entradas. Devuelve ((inicio, fin), avisos);
        (0, 0) si no hay firma o si dos entradas de hilo no coinciden (no se adivina cuál vale).
        Idea tomada de Hookshot (nx2/standalone.py thread_hooks, GPL-3.0); el código es propio."""
        encontrados, avisos = set(), []
        for m in self.modules:
            datos = m.img.data
            ini, tam = m.img.text
            k = datos.find(self.FIRMA_ENTRADA_HILO, ini, ini + tam)
            while k >= 0:
                if k % 4 == 0:
                    par = self._hooks_tras_firma(m, k + 12, min(ini + tam, k + 12 + 4 * ventana))
                    if par:
                        encontrados.add(par)
                k = datos.find(self.FIRMA_ENTRADA_HILO, k + 4, ini + tam)
        if len(encontrados) == 1:
            return encontrados.pop(), avisos
        if len(encontrados) > 1:
            avisos.append("entradas de hilo con hooks distintos: " +
                          ", ".join(f"0x{a:x}/0x{b:x}" for a, b in sorted(encontrados)) + "; no se usa ninguno")
        return (0, 0), avisos

    @staticmethod
    def _hooks_tras_firma(m, desde, hasta):
        paginas, gots = {}, []
        for off in range(desde, hasta - 3, 4):
            w = struct.unpack_from("<I", m.img.data, off)[0]
            pc = m.base + off
            if w & 0x9F000000 == 0x90000000:  # ADRP Xd, página
                imm = ((w >> 5) & 0x7FFFF) << 2 | (w >> 29) & 3
                if imm & 1 << 20:
                    imm -= 1 << 21
                paginas[w & 31] = (pc & ~0xFFF) + (imm << 12)
            elif w & 0xFFC00000 == 0xF9400000 and (w >> 5) & 31 in paginas:  # LDR Xt, [Xn, #imm12*8]
                gots.append(paginas.pop((w >> 5) & 31) + ((w >> 10) & 0xFFF) * 8)
                if len(gots) == 2:
                    return tuple(gots)
        return None

    # Lo que rtld publica en los módulos al terminar de enlazarlos: recorre su lista de módulos, busca en cada uno
    # estos símbolos (binding global o débil) y escribe en ellos la dirección de su propio estado de nn::ro.
    # Visto en el rtld de Switch 2 (bucle tras enlazar con los cinco nombres en su .rodata); el recompilador no
    # ejecuta rtld (boot), así que el runtime repite esas escrituras (runtime/nxsvc_ldr.c nx_rtld_publicar_ro).
    # El orden fija el índice que usa el runtime.
    RTLD_RO = ("_ZN2nn2ro6detail15g_pAutoLoadListE",
               "_ZN2nn2ro6detail17g_pManualLoadListE",
               "_ZN2nn2ro6detail14g_pRoDebugFlagE",
               "_ZN2nn2ro6detail34g_pLookupGlobalAutoFunctionPointerE",
               "_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE")

    def rtld_ro_vars(self):
        """[(índice en RTLD_RO, dirección de la variable)] de cada módulo que define una de ellas."""
        out = []
        for m in self.modules:
            if m.name == "rtld":
                continue
            for s in m.syms:
                if s.defined and s.bind in (1, 2) and s.name in self.RTLD_RO:
                    out.append((self.RTLD_RO.index(s.name), m.base + s.value))
        return sorted(out)

    def _init_dispatches_array(self, m):
        """Detecta si DT_INIT materializa el rango de su DT_INIT_ARRAY."""
        if not m.init or not m.init_array or DT["INIT_ARRAY"] not in m.dyn:
            return False
        from a64 import bits, decode

        array_lo = m.base + m.dyn[DT["INIT_ARRAY"]]
        array_hi = array_lo + m.dyn.get(DT["INIT_ARRAYSZ"], 0)
        ends = [m.base + start for start in m.eh_frame_starts() if start > m.init]
        end = min(ends, default=m.base + m.img.size)
        found_lo = found_hi = False
        pc = m.base + m.init
        while pc + 4 <= end:
            insn = decode(pc, self.word(pc))
            if insn.ref is not None:
                refs = [insn.ref]
                if insn.adrp_reg is not None:
                    for k in range(1, 9):
                        q = pc + 4 * k
                        if q >= end:
                            break
                        word = self.word(q)
                        if bits(word, 31, 22) == 0b1001000100 and bits(word, 9, 5) == insn.adrp_reg:
                            refs.append(insn.ref + bits(word, 21, 10))
                            break
                found_lo |= array_lo in refs
                found_hi |= array_hi in refs
                if found_lo and found_hi:
                    return True
            pc += 4
        return False

    def summary(self):
        lines = []
        for m in self.modules:
            lines.append(f"  {m.name:8s} 0x{m.base:x}  .text {m.img.text[1]:#x}  .rodata {m.img.ro[1]:#x}  "
                         f".data {m.img.rw[1]:#x}  .bss {m.img.bss:#x}  {len(m.syms)} símbolos")
        lines.append(f"  {len(self.imports)} importaciones sin definir"
                     + (f": {', '.join(self.imports[:8])}{'...' if len(self.imports) > 8 else ''}" if self.imports else ""))
        if self.unsupported:
            lines.append("  [RED] reubicaciones no soportadas: " + ", ".join(f"{k} ×{v}" for k, v in self.unsupported.items()))
        return "\n".join(lines)


def modulos_ro_de(carpeta):
    """Los NRO volcados por el runtime (NX_VOLCAR_NRO=<carpeta>, <sha256>.nro), en orden de nombre: así cada uno
    recibe siempre la misma dirección. Un archivo que no es NRO o cuyo SHA-256 no es su nombre se ignora con aviso."""
    out = []
    if not carpeta or not os.path.isdir(carpeta):
        return out
    for nombre in sorted(os.listdir(carpeta)):
        if not nombre.endswith(".nro"):
            continue
        raw = open(os.path.join(carpeta, nombre), "rb").read()
        sha = hashlib.sha256(raw).hexdigest()
        if sha != nombre[:-4]:
            print(f"[RED] módulo de nn::ro {nombre}: su SHA-256 es {sha}; se ignora")
            continue
        try:
            img = load_module(raw, "ro_" + sha[:16])
        except FormatError as e:
            print(f"[RED] módulo de nn::ro {nombre}: {e}; se ignora")
            continue
        if img.kind != "nro":
            print(f"[RED] módulo de nn::ro {nombre}: no es un NRO; se ignora")
            continue
        img.sha256 = bytes.fromhex(sha)
        out.append(img)
    return out


def load_program(paths, base=BASE, modulos_ro=()):
    """Carga un ExeFS (carpeta con main, sdk, subsdk*... y main.npdm) o una lista de módulos."""
    npdm = None
    if len(paths) == 1 and os.path.isdir(paths[0]):
        d = paths[0]
        files = [os.path.join(d, n) for n in EXEFS_ORDER if os.path.isfile(os.path.join(d, n))]
        if not files:
            raise FormatError(f"{d}: no hay módulos de ExeFS (main, sdk, subsdk*...)")
        if os.path.isfile(os.path.join(d, "main.npdm")):
            npdm = parse_npdm(open(os.path.join(d, "main.npdm"), "rb").read())
    else:
        files = paths
    images = [load_module(open(f, "rb").read(), os.path.splitext(os.path.basename(f))[0]) for f in files]
    return Program(images, base, npdm, modulos_ro)
