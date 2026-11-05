from pathlib import Path
import hashlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from compilar import compile_source


class CompileCacheTest(unittest.TestCase):
    def test_diagnostic_build_reuses_fresh_optimized_objects_only(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'answer.c'
            source.write_text('int answer(void) { return 42; }\n')
            obj, error = compile_source(str(source), folder, '2', 0)
            self.assertIsNone(error)
            obj = Path(obj)
            timestamp = obj.stat().st_mtime_ns
            stamp = Path(str(obj) + '.command').read_text()
            _, error = compile_source(str(source), folder, '0', 0, reuse_optimized=True)
            self.assertIsNone(error)
            self.assertEqual(obj.stat().st_mtime_ns, timestamp)
            self.assertEqual(Path(str(obj) + '.command').read_text(), stamp)
            source.write_text('int answer(void) { return 7; }\n')
            _, error = compile_source(str(source), folder, '0', 0, reuse_optimized=True)
            self.assertIsNone(error)
            self.assertNotEqual(Path(str(obj) + '.command').read_text(), stamp)

    def test_reuses_objects_and_preserves_good_object_on_compiler_error(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'answer.c'
            source.write_text('int answer(void) { return 42; }\n')
            obj, error = compile_source(str(source), folder, '0', 0)
            self.assertIsNone(error)
            obj = Path(obj)
            original = obj.read_bytes()
            timestamp = obj.stat().st_mtime_ns
            _, error = compile_source(str(source), folder, '0', 0)
            self.assertIsNone(error)
            self.assertEqual(obj.stat().st_mtime_ns, timestamp)

            source.write_text('int answer(void) { return invalid syntax; }\n')
            _, error = compile_source(str(source), folder, '0', 0)
            self.assertTrue(error)
            self.assertEqual(obj.read_bytes(), original)

            source.write_text('int answer(void) { return 7; }\n')
            _, error = compile_source(str(source), folder, '0', 0)
            self.assertIsNone(error)
            self.assertNotEqual(hashlib.sha256(obj.read_bytes()).digest(),
                                hashlib.sha256(original).digest())
            self.assertFalse(list(obj.parent.glob('.compilar-*')))


if __name__ == '__main__':
    unittest.main()
