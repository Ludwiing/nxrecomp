"""Replace generated giant switches with a sorted static lookup table.

Same address/function pairs, with bounded compiler work and optional statically
compiled hint extensions. No interpreter or runtime compilation is involved.
"""
from pathlib import Path
import re


MARKER = 'NxFn nx_buscar(uint64_t a) {\n    switch (a) {\n'
DISPATCH_OLD = 'void nx_dispatch(Ctx *c, uint64_t a) { nx_buscar(a)(c); }'
DISPATCH_NEW = ('void nx_dispatch(Ctx *c, uint64_t a) { if (__builtin_expect(nx_core_pending, 0)) nx_core_check(); '
                'nx_buscar(a)(c); }')


def prepare(path):
    path = Path(path)
    if not path.exists():
        return False
    text = path.read_text()
    # Cada llamada indirecta es un punto de cesión del núcleo virtual (nxkern.c): una lectura si nadie espera.
    if DISPATCH_OLD in text:
        text = text.replace(DISPATCH_OLD, DISPATCH_NEW)
        if MARKER not in text:
            temporary = path.with_suffix('.dispatch.tmp')
            temporary.write_text(text)
            temporary.replace(path)
            return True
    marker = MARKER
    if marker not in text:
        return False
    before, rest = text.split(marker, 1)
    cases, after = rest.split('    }\n    destino_malo = a;', 1)
    pairs = re.findall(r'    case (0x[0-9a-f]+)ull: return (f_[0-9a-f]+);\n', cases)
    if ''.join(f'    case {a}ull: return {f};\n' for a, f in pairs) != cases:
        raise ValueError('Generated dispatcher format not recognized')
    addresses = [int(a, 16) for a, _ in pairs]
    if addresses != sorted(set(addresses)):
        raise ValueError('Dispatcher addresses must be unique and sorted')
    generated = 'extern NxFn nx_extra_lookup(uint64_t) __attribute__((weak));\nextern NxFn nx_hle_lookup(uint64_t) __attribute__((weak));\n'
    generated += 'static const struct { uint64_t address; NxFn function; } nx_lookup[] = {\n'
    generated += ''.join(f'    {{{a}ull, {f}}},\n' for a, f in pairs)
    generated += '''};
NxFn nx_buscar(uint64_t a) {
    size_t lo = 0, hi = sizeof nx_lookup / sizeof nx_lookup[0];
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (nx_lookup[mid].address < a) lo = mid + 1;
        else hi = mid;
    }
    if (lo < sizeof nx_lookup / sizeof nx_lookup[0] && nx_lookup[lo].address == a)
        return nx_lookup[lo].function;
    if (nx_hle_lookup) {
        NxFn fn = nx_hle_lookup(a);
        if (fn) return fn;
    }
    if (nx_extra_lookup) {
        NxFn fn = nx_extra_lookup(a);
        if (fn) return fn;
    }
    destino_malo = a;'''
    temporary = path.with_suffix('.dispatch.tmp')
    temporary.write_text(before + generated + after)
    temporary.replace(path)
    return True
