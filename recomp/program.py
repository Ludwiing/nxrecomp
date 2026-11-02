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
    def __init__(self, images, base=BASE, npdm=None):
        self.modules = []
        addr = base
        for img in images:
            self.modules.append(LoadedModule(img, addr))
            addr += align(img.size + img.bss)
        self.end = addr
        self.npdm = npdm
        self.imports = []          # nombres sin definir, en el orden de su dirección de stub
        self._import_index = {}
        self.unsupported = Counter()
        self.symbols = {}          # nombre -> dirección (primera definición global en orden de carga)
        for m in self.modules:
            for s in m.syms:
                if s.defined and s.bind in (1, 2) and (s.other & 3) == 0 and s.name:
                    self.symbols.setdefault(s.name, m.base + s.value)
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
        """Inicios seguros: la entrada de cada módulo, DT_INIT, init_array y funciones exportadas."""
        out = set()
        for m in self.modules:
            out.add(m.base + m.img.text[0])
            if m.init:
                out.add(m.base + m.init)
            out.update(a for a in m.init_array if self.in_text(a))
            out.update(m.base + s.value for s in m.syms if s.defined and s.is_func and self.in_text(m.base + s.value))
        return out

    def boot(self):
        """Cómo arranca: con nnMain (juego con SDK) o por la entrada del primer módulo (homebrew)."""
        inits = []
        for m in self.modules:
            if m.init:
                inits.append(m.base + m.init)
            else:
                inits.extend(a for a in m.init_array if a)
        return {"inits": inits, "main": self.symbols.get("nnMain"), "entry": self.modules[0].base + self.modules[0].img.text[0]}

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


def load_program(paths, base=BASE):
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
    return Program(images, base, npdm)
