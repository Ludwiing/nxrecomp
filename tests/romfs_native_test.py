import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from romfs_native import convert, ftp_listing, name_hash, NONE


class RomfsNativeTest(unittest.TestCase):
    def test_ftp_listing_requires_at_least_one_list_entry(self):
        # An empty/blank game file named .listing is not proof of an FTP LIST reply.
        self.assertFalse(ftp_listing(b'.listing', b''))
        self.assertFalse(ftp_listing(b'.listing', b' \r\n\t\n'))
        self.assertTrue(ftp_listing(b'.listing', b'-rw-r--r-- 1 ftp ftp 12 Jan 01 1970 data.bin\r\n'))

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

    def test_ftp_listing_left_out(self):
        # Switch Sports Resort (04008e001f098000): its DMROMFS1 dump came over FTP with
        # wget, which leaves a .listing in every directory. The game enumerates
        # Parameter/<Type>/, cuts every name at the first '.', and loads the resource:
        # ".listing" gives an empty name, the load returns null, and the game reads
        # null+0x150 (SIGSEGV at 0x150). The converted RomFS must not list it.
        listing = (b'dr-xr-xr-x 1 ftp ftp 0 Jan 01  1970 AI\r\n'
                   b'-r--r--r-- 1 ftp ftp 488 Jan 01  1970 Body00.bgyml\r\n')
        other = b'not an FTP reply\n'
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'archive.img'
            # root -> Parameter; Parameter -> .listing, Body00.bgyml, sub;
            # sub -> .listing that is not FTP output (kept: it is game data).
            names = [b'', b'Parameter', b'.listing', b'Body00.bgyml', b'sub', b'.listing']
            content = {2: listing, 3: b'body' * 40, 5: other}
            raw = bytearray(32)
            where = {}
            for k, data in content.items():
                where[k] = len(raw); raw.extend(data)
            index = len(raw)
            values = [(1, 1, 0), (2, 3, 0), (where[2], len(listing), 1), (where[3], 160, 1),
                      (5, 1, 0), (where[5], len(other), 1)]
            pool = b''
            for (a, b, kind), name in zip(values, names):
                raw.extend(struct.pack('<QQIHBB', a, b, len(pool), len(name), kind, 0)); pool += name
            raw.extend(pool)
            struct.pack_into('<8sQQQ', raw, 0, b'DMROMFS1', index, len(raw) - index, len(names))
            source.write_bytes(raw)
            output = convert(source)
            data = output.read_bytes()
            h, dh, dhs, dt, dts, fh, fhs, ft, fts, fd = struct.unpack_from('<10Q', data)

            def files_of(directory_offset):
                first_file = struct.unpack_from('<6I', data, dt + directory_offset)[3]
                found, offset = [], first_file
                while offset != NONE:
                    p, sibling, off, size, nxt, length = struct.unpack_from('<IIQQII', data, ft + offset)
                    self.assertEqual(p, directory_offset)
                    found.append((data[ft + offset + 32:ft + offset + 32 + length], data[fd + off:fd + off + size]))
                    offset = sibling
                return found

            root = struct.unpack_from('<6I', data, dt)
            parameter = root[2]
            self.assertEqual(files_of(parameter), [(b'Body00.bgyml', b'body' * 40)])
            sub = struct.unpack_from('<6I', data, dt + parameter)[2]
            self.assertEqual(files_of(sub), [(b'.listing', other)])
            # Not reachable through the name hash either.
            bucket = name_hash(parameter, b'.listing', fhs // 4)
            offset = struct.unpack_from('<I', data, fh + 4 * bucket)[0]
            while offset != NONE:
                p, _, _, _, nxt, length = struct.unpack_from('<IIQQII', data, ft + offset)
                self.assertFalse(p == parameter and data[ft + offset + 32:ft + offset + 32 + length] == b'.listing')
                offset = nxt

    def test_old_format_stamp_rebuilds(self):
        # A .native made before the .listing filter has a two-field stamp with the same
        # source identity; it must be rebuilt, not reused.
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'archive.img'
            raw = bytearray(32)
            index = len(raw)
            raw.extend(struct.pack('<QQIHBB', 1, 0, 0, 0, 0, 0))
            struct.pack_into('<8sQQQ', raw, 0, b'DMROMFS1', index, len(raw) - index, 1)
            source.write_bytes(raw)
            output = convert(source)
            stamp = Path(str(output) + '.source')
            st = source.stat()
            stamp.write_text(f'{st.st_size}\t{st.st_mtime_ns}\n')
            output.write_bytes(b'stale')
            convert(source)
            self.assertNotEqual(output.read_bytes(), b'stale')
            fields = stamp.read_text().split()
            self.assertEqual(fields[:2], [str(st.st_size), str(st.st_mtime_ns)])


if __name__ == '__main__':
    unittest.main()
