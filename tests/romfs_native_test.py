import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from romfs_native import convert, name_hash, NONE


class RomfsNativeTest(unittest.TestCase):
    def test_files_tree_hashes_source_and_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'archive.img'
            # root -> directory + root file; directory -> unicode file.
            names = [b'', b'sub', b'root.bin', 'niño.bin'.encode()]
            content = [b'first contents' * 100, bytes(range(256)) * 8]
            raw = bytearray(32)
            first = len(raw); raw.extend(content[0])
            second = len(raw); raw.extend(content[1])
            index = len(raw)
            values = [(1, 2, 0), (3, 1, 0), (first, len(content[0]), 1), (second, len(content[1]), 1)]
            pool = b''
            for (a, b, kind), name in zip(values, names):
                raw.extend(struct.pack('<QQIHBB', a, b, len(pool), len(name), kind, 0)); pool += name
            raw.extend(pool)
            struct.pack_into('<8sQQQ', raw, 0, b'DMROMFS1', index, len(raw) - index, 4)
            source.write_bytes(raw)
            digest = hashlib.sha256(raw).digest()
            output = convert(source)
            data = output.read_bytes()
            h, dh, dhs, dt, dts, fh, fhs, ft, fts, fd = struct.unpack_from('<10Q', data)
            self.assertEqual(h, 80)
            root = struct.unpack_from('<6I', data, dt)
            directory = struct.unpack_from('<6I', data, dt + root[2])
            self.assertEqual(directory[0], 0)
            for parent, name, expected in [(0, names[2], content[0]), (root[2], names[3], content[1])]:
                bucket = name_hash(parent, name, fhs // 4)
                offset = struct.unpack_from('<I', data, fh + 4 * bucket)[0]
                while offset != NONE:
                    p, sibling, off, size, nxt, length = struct.unpack_from('<IIQQII', data, ft + offset)
                    if p == parent and data[ft + offset + 32:ft + offset + 32 + length] == name:
                        self.assertEqual(data[fd + off:fd + off + size], expected)
                        break
                    offset = nxt
                else:
                    self.fail('File missing from name hash chain')
            self.assertEqual(hashlib.sha256(source.read_bytes()).digest(), digest)
            before = output.stat().st_mtime_ns
            self.assertEqual(convert(source), output)
            self.assertEqual(output.stat().st_mtime_ns, before)
            # An updated source invalidates the converted cache.
            with source.open('r+b') as f:
                f.seek(first); f.write(b'other contents')
            convert(source)
            self.assertNotEqual(output.stat().st_mtime_ns, before)


if __name__ == '__main__':
    unittest.main()
