#!/usr/bin/env python3
"""Generate static functions omitted by an existing translation, without rebuilding it."""
import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'recomp'))
import nxrecomp as nx
from program import load_program


def generate(exefs, folder, hints):
    folder = Path(folder)
    known = {int(s, 16) for s in re.findall(r'void f_([0-9a-f]+)', (folder / 'nxgame.h').read_text())}
    requested = {int(line.strip(), 0) for line in Path(hints).read_text().splitlines() if line.strip()}
    program = load_program([str(exefs)])
    if any(not program.in_text(a) or a & 3 for a in requested):
        raise ValueError('Hint is not an aligned address in executable code')
    pending = list(requested - known)
    extra = set(pending)
    while pending:
        address = pending.pop()
        def found(target):
            if target not in known and target not in extra:
                extra.add(target)
                pending.append(target)
        nx.walk(program, address, known | extra, sorted(known | extra), found)
    output = ['#include "nxgame.h"']
    output += [f'void f_{a:x}(Ctx *c);' for a in sorted(extra)]
    for address in sorted(extra):
        nx.emit_function(program, address, known | extra, output, {'br_tabla': set(), 'br_indirecto': set()})
    output.append('NxFn nx_extra_lookup(uint64_t address) { switch (address) {')
    output += [f'case 0x{a:x}ull: return f_{a:x};' for a in sorted(extra)]
    output += ['default: return 0;', '} }']
    path = folder / 'pistas_extra.c'
    temporary = path.with_suffix('.tmp')
    temporary.write_text('\n'.join(output) + '\n')
    temporary.replace(path)
    print(f'{path}: {len(extra)} additional static functions', flush=True)
    return path


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exefs')
    parser.add_argument('folder')
    parser.add_argument('hints')
    args = parser.parse_args()
    generate(args.exefs, args.folder, args.hints)
