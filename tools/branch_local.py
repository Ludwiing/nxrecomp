"""Upgrade already generated C to preserve local BR targets and BR returns."""
from pathlib import Path
import re

BR = re.compile(r'^    (default: )?\{ NxFn fn = nx_buscar\((c->x\[\d+\])\); __attribute__\(\(musttail\)\) return fn\(c\); \}', re.M)


def prepare_tail(path):
    """Preserve the host caller across generated tail branches, idempotently."""
    path = Path(path)
    original = path.read_text()
    text = original.replace('const uint64_t lr0 = c->x[30];',
                            'const uint64_t lr0 = nx_enter(c);')
    text = re.sub(r'(?<!nx_tail\(c, lr0\); )__attribute__\(\(musttail\)\)',
                  'nx_tail(c, lr0); __attribute__((musttail))', text)
    if text == original:
        return False
    temporary = path.with_suffix('.tail.tmp')
    temporary.write_text(text)
    temporary.replace(path)
    return True


def prepare(path):
    path = Path(path)
    text = path.read_text()
    if not BR.search(text):
        return False
    chunks = re.split(r'(?=^void f_[0-9a-f]+\(Ctx \*c\) \{)', text, flags=re.M)
    changed = False
    for k, chunk in enumerate(chunks):
        if not chunk.startswith('void f_') or 'nx_local_dispatch:' in chunk:
            continue
        end = chunk.find('\n}\n')
        if end < 0:
            continue
        body, rest = chunk[:end], chunk[end:]
        if not BR.search(body):
            continue
        labels = re.findall(r'^L_([0-9a-f]+):', body, re.M)
        if not labels:
            continue
        body = body.replace('    const uint64_t lr0', '    uint64_t nx_local_target;\n    const uint64_t lr0', 1)
        body = BR.sub(lambda m: f'    {m[1] or ""}if ({m[2]} == lr0) return; nx_local_target = {m[2]}; goto nx_local_dispatch;', body)
        available = set(labels)
        body = re.sub(r'\{ __attribute__\(\(musttail\)\) return f_([0-9a-f]+)\(c\); \}',
                      lambda m: f'goto L_{m[1]};' if m[1] in available else m[0], body)
        body += '\nnx_local_dispatch:\n    switch (nx_local_target) {\n'
        body += ''.join(f'    case 0x{pc}ull: goto L_{pc};\n' for pc in labels)
        body += '    default: { NxFn fn = nx_buscar(nx_local_target); __attribute__((musttail)) return fn(c); }\n    }'
        chunks[k] = body + rest
        changed = True
    if changed:
        temporary = path.with_suffix('.br.tmp')
        temporary.write_text(''.join(chunks))
        temporary.replace(path)
    return changed
