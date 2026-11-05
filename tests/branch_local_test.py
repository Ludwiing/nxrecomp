from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from branch_local import prepare, prepare_tail


class BranchLocalTest(unittest.TestCase):
    def test_preserves_host_caller_and_local_assembly_labels(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'funciones_000.c'
            source.write_text('''#include <stdint.h>
#include <assert.h>
typedef struct { uint64_t x[31]; } Ctx;
typedef void (*NxFn)(Ctx*);
NxFn nx_buscar(uint64_t a) { assert(!"Unexpected dispatch"); return 0; }
void f_10(Ctx *c) {
    const uint64_t lr0 = c->x[30];
L_10:
    c->x[9] = c->x[30]; c->x[30] = 0; c->x[10] = 0x20;
L_14:
    if (!c->x[0]) goto L_20;
L_18:
    { NxFn fn = nx_buscar(c->x[10]); __attribute__((musttail)) return fn(c); }
L_20:
    c->x[0] += 7;
L_24:
    { NxFn fn = nx_buscar(c->x[9]); __attribute__((musttail)) return fn(c); }
}
int main(void) {
    Ctx c = {.x = {[0] = 35, [30] = 0x100}};
    f_10(&c); assert(c.x[0] == 42);
    c = (Ctx){.x = {[0] = 0, [30] = 0x100}};
    f_10(&c); assert(c.x[0] == 7);
}
''')
            self.assertTrue(prepare(source))
            self.assertFalse(prepare(source))
            executable = Path(folder) / 'test'
            subprocess.run(['clang', '-O2', str(source), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_tail_fragment_keeps_caller_after_an_inner_call(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'funciones_000.c'
            source.write_text('''#include "nxrt.h"
#include <assert.h>
void f_20(Ctx *c);
void f_30(Ctx *c);
void f_40(Ctx *c);
void f_50(Ctx *c);
NxFn nx_buscar(uint64_t a) { assert(a == 0x20); return f_20; }
void f_10(Ctx *c) {
    const uint64_t lr0 = c->x[30];
    c->x[9] = c->x[30];
    c->x[30] = 0x14;
    f_30(c);
    { NxFn fn = nx_buscar(0x20); __attribute__((musttail)) return fn(c); }
}
void f_20(Ctx *c) {
    const uint64_t lr0 = c->x[30];
    c->x[30] = c->x[9];
    if (c->x[30] != lr0) { NxFn fn = nx_buscar(c->x[30]); __attribute__((musttail)) return fn(c); }
    c->x[0] += 7;
    return;
}
void f_30(Ctx *c) {
    const uint64_t lr0 = c->x[30];
    assert(lr0 == 0x14); c->x[0] += 35;
}
void f_40(Ctx *c) {
    const uint64_t lr0 = c->x[30];
    assert(lr0 == 0x100);
    if (!c->x[1]) return;
    --c->x[1]; ++c->x[0]; c->x[30] = 0x44;
    { __attribute__((musttail)) return f_50(c); }
}
void f_50(Ctx *c) {
    const uint64_t lr0 = c->x[30];
    assert(lr0 == 0x100); c->x[30] = 0x54;
    { __attribute__((musttail)) return f_40(c); }
}
int main(void) {
    Ctx c = {.x = {[30] = 0x100}};
    f_10(&c); assert(c.x[0] == 42); assert(!c.tail_pending);
    c.x[30] = 0x14; f_30(&c); assert(c.x[0] == 77);
    c = (Ctx){.x = {[1] = 1000000, [30] = 0x100}};
    f_40(&c); assert(c.x[0] == 1000000); assert(!c.tail_pending);
}
''')
            self.assertTrue(prepare_tail(source))
            self.assertFalse(prepare_tail(source))
            executable = Path(folder) / 'test'
            runtime = Path(__file__).resolve().parents[1] / 'runtime'
            for optimization in ['-O0', '-O2']:
                subprocess.run(['clang', optimization, '-I'+str(runtime), str(source), '-o', str(executable)], check=True)
                subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
