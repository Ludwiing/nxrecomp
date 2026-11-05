#!/usr/bin/env python3
"""Present an extracted DMROMFS1 archive as a standard Nintendo RomFS image.

Uses a copy-on-write clone when supported. Original archives are never modified.
Layout and name hashing follow libnx romfs_header/romfs_dir/romfs_file.
"""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import struct
import tempfile

NONE = 0xffffffff


def align(n, size=4):
    return (n + size - 1) & -size


def name_hash(parent, name, count):
    h = parent ^ 123456789
    for byte in name:
        h = (((h >> 5) | (h << 27)) ^ byte) & NONE
    return h % count


def convert(source):
    source = Path(source)
    output = Path(str(source) + '.native')
    stamp = Path(str(output) + '.source')
    with source.open('rb') as original:
        stat = os.fstat(original.fileno())
        identity = f'{stat.st_size}\t{stat.st_mtime_ns}\n'
        head = original.read(32)
        if head[:8] != b'DMROMFS1':
            return source
        if output.exists() and stamp.exists() and stamp.read_text() == identity:
            return output
        _, index, length, count = struct.unpack('<8sQQQ', head)
        if not (1 <= count <= 2_000_000 and count * 24 <= length <= 256 * 1024 * 1024
                and 32 <= index <= stat.st_size and length == stat.st_size - index):
            raise ValueError('Invalid DMROMFS1 index bounds')
        original.seek(index)
        table = original.read(length)
        entries = []
        for k in range(count):
            a, b, name_off, name_len, kind, reserved = struct.unpack_from('<QQIHBB', table, k * 24)
            start = count * 24 + name_off
            if kind not in (0, 1) or reserved or start + name_len > len(table):
                raise ValueError('Invalid DMROMFS1 entry')
            name = table[start:start + name_len]
            if b'/' in name or b'\0' in name or name in (b'.', b'..') or (k and not name):
                raise ValueError('Invalid archive name')
            if kind and (a < 32 or a > index or b > index - a):
                raise ValueError('Archive file lies outside data region')
            entries.append(dict(a=a, b=b, name=name, kind=kind, parent=None, sibling=NONE))
        if entries[0]['kind'] or entries[0]['name']:
            raise ValueError('Invalid archive root')
        entries[0]['parent'] = 0
        for k, entry in enumerate(entries):
            if entry['kind']:
                continue
            a, b = entry['a'], entry['b']
            if a > count or b > count - a or (b and a <= k):
                raise ValueError('Invalid directory children')
            for child in entries[a:a + b]:
                if child['parent'] is not None:
                    raise ValueError('Directory graph has duplicate children')
                child['parent'] = k
            for kind in (0, 1):
                children = [c for c in entries[a:a + b] if c['kind'] == kind]
                for left, right in zip(children, children[1:]):
                    left['next_entry'] = right
        if any(e['parent'] is None for e in entries):
            raise ValueError('Unreachable archive entry')
        directories = [e for e in entries if not e['kind']]
        files = [e for e in entries if e['kind']]
        for group, base_size in ((directories, 24), (files, 32)):
            offset = 0
            for e in group:
                e['offset'] = offset
                offset += base_size + align(len(e['name']))
        dir_table = bytearray(sum(24 + align(len(e['name'])) for e in directories))
        file_table = bytearray(sum(32 + align(len(e['name'])) for e in files))
        dir_hash = [NONE] * max(3, len(directories))
        file_hash = [NONE] * max(3, len(files))
        # Keep file bytes in place in the CoW clone. Relocate only the files
        # before 0x200, where the standard RomFS header/data boundary belongs.
        dh_off = max(0x200, align(index, 16))
        dt_off = dh_off + 4 * len(dir_hash)
        fh_off = dt_off + len(dir_table)
        ft_off = fh_off + 4 * len(file_hash)
        end = align(ft_off + len(file_table), 16)
        relocated = []
        for e in files:
            e['physical'] = e['a']
            if e['a'] < 0x200:
                e['physical'] = end
                relocated.append(e)
                end = align(end + e['b'], 16)
        for group, hashes, table_out in ((directories, dir_hash, dir_table), (files, file_hash, file_table)):
            for e in group:
                parent = entries[e['parent']]['offset']
                sibling = e.get('next_entry', {}).get('offset', NONE)
                bucket = name_hash(parent, e['name'], len(hashes))
                previous = hashes[bucket]
                hashes[bucket] = e['offset']
                if e['kind']:
                    packed = struct.pack('<IIQQII', parent, sibling, e['physical'] - 0x200,
                                         e['b'], previous, len(e['name']))
                else:
                    children = entries[e['a']:e['a'] + e['b']]
                    first_dir = next((c['offset'] for c in children if not c['kind']), NONE)
                    first_file = next((c['offset'] for c in children if c['kind']), NONE)
                    packed = struct.pack('<6I', parent, sibling, first_dir, first_file, previous, len(e['name']))
                table_out[e['offset']:e['offset'] + len(packed)] = packed
                start = e['offset'] + len(packed)
                table_out[start:start + len(e['name'])] = e['name']
        fd, temporary = tempfile.mkstemp(prefix='.romfs-native-', dir=source.parent)
        try:
            with os.fdopen(fd, 'w+b') as target:
                try:
                    fcntl.ioctl(target.fileno(), 0x40049409, original.fileno()) # FICLONE
                    mode = 'CoW clone'
                except OSError:
                    original.seek(0)
                    shutil.copyfileobj(original, target, 4 * 1024 * 1024)
                    mode = 'streamed copy'
                for e in relocated:
                    original.seek(e['a']); target.seek(e['physical'])
                    remaining = e['b']
                    while remaining:
                        chunk = original.read(min(remaining, 4 * 1024 * 1024))
                        if not chunk:
                            raise ValueError('Truncated archive data')
                        target.write(chunk); remaining -= len(chunk)
                for offset, data in ((dh_off, struct.pack(f'<{len(dir_hash)}I', *dir_hash)),
                                     (dt_off, dir_table), (fh_off, struct.pack(f'<{len(file_hash)}I', *file_hash)),
                                     (ft_off, file_table)):
                    target.seek(offset); target.write(data)
                target.seek(0)
                target.write(struct.pack('<10Q', 0x50, dh_off, 4 * len(dir_hash), dt_off, len(dir_table),
                                         fh_off, 4 * len(file_hash), ft_off, len(file_table), 0x200))
                target.truncate(end)
                target.flush(); os.fsync(target.fileno())
            if os.stat(source).st_mtime_ns != stat.st_mtime_ns or os.stat(source).st_size != stat.st_size:
                raise ValueError('Source changed during conversion')
            os.replace(temporary, output)
            stamp.write_text(identity)
            print(f'{output}: {len(directories)} directories, {len(files)} files; {mode}', flush=True)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    args = parser.parse_args()
    with open(args.source + '.native.lock', 'a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        print(convert(args.source))
