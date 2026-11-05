from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from dispatch_table import prepare


class DispatchTableTest(unittest.TestCase):
    def test_preserves_mapping_misses_and_static_extensions(self):
        for extension in (False, True):
            with self.subTest(extension=extension), tempfile.TemporaryDirectory() as folder:
                source = Path(folder) / 'programa.c'
                source.write_text('''#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef void (*NxFn)(void*);
static void f_10(void *c) {} static void f_20(void *c) {} static void f_30(void *c) {}
static uint64_t destino_malo;
NxFn nx_buscar(uint64_t a) {
    switch (a) {
    case 0x10ull: return f_10;
    case 0x20ull: return f_20;
    case 0x30ull: return f_30;
    }
    destino_malo = a;
    return 0;
}
''' + ('''NxFn nx_extra_lookup(uint64_t a) { return a == 0x40 ? f_10 : 0; }
''' if extension else '') + '''int main(void) {
    assert(nx_buscar(0x10) == f_10 && nx_buscar(0x20) == f_20 && nx_buscar(0x30) == f_30);
    assert(!nx_buscar(0) && !nx_buscar(0x21) && !nx_buscar(0x99));
    assert(destino_malo == 0x99);
''' + ('assert(nx_buscar(0x40) == f_10);' if extension else 'assert(!nx_buscar(0x40));') + '\n}')
                self.assertTrue(prepare(source))
                self.assertFalse(prepare(source))
                executable = Path(folder) / 'test'
                subprocess.run(['clang', '-O2', str(source), '-o', str(executable)], check=True)
                subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
