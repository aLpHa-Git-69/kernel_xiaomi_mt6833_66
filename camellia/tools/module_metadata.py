#!/usr/bin/env python3
"""Read kernel-module metadata directly from arm64 ELF sections."""
import argparse
import pathlib
import struct


def module_vermagic(data, label):
    if len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError(f'{label}: not a little-endian ELF64 module')
    if struct.unpack_from('<HH', data, 16) != (1, 183):
        raise ValueError(f'{label}: not a relocatable arm64 module')
    offset = struct.unpack_from('<Q', data, 40)[0]
    entry_size, count, names_index = struct.unpack_from('<HHH', data, 58)
    if (entry_size != 64 or not count or names_index >= count or offset < 64
            or offset + entry_size * count > len(data)):
        raise ValueError(f'{label}: invalid ELF section table')

    def header(index):
        return struct.unpack_from('<IIQQQQIIQQ', data, offset + index * entry_size)

    def contents(section):
        start, size = section[4:6]
        if start + size > len(data):
            raise ValueError(f'{label}: ELF section exceeds file size')
        return data[start:start + size]

    names_header = header(names_index)
    if names_header[1] != 3:
        raise ValueError(f'{label}: invalid ELF section-name table')
    names = contents(names_header)
    wanted = {b'.modinfo', b'.gnu.linkonce.this_module', b'__versions'}
    sections = {}
    for index in range(count):
        section = header(index)
        name_offset = section[0]
        end = names.find(b'\0', name_offset)
        if name_offset >= len(names) or end < 0:
            raise ValueError(f'{label}: invalid ELF section name')
        name = bytes(names[name_offset:end])
        if name in wanted:
            if name in sections or section[1] != 1:
                raise ValueError(f'{label}: invalid or duplicate module section')
            sections[name] = contents(section)
    if len(sections.get(b'.gnu.linkonce.this_module', b'')) < 64:
        raise ValueError(f'{label}: missing kernel module descriptor')
    magic = [entry[len(b'vermagic='):] for entry in sections.get(b'.modinfo', b'').split(b'\0')
             if entry.startswith(b'vermagic=')]
    if len(magic) != 1 or not magic[0]:
        raise ValueError(f'{label}: missing or ambiguous vermagic')
    value = magic[0].decode('ascii')
    if 'modversions' in value.split() and not sections.get(b'__versions'):
        raise ValueError(f'{label}: missing symbol-version section')
    return value


def check_directory(directory, release):
    modules = sorted(directory.glob('*.ko'))
    if not modules:
        raise ValueError(f'{directory}: no modules')
    expected = None
    for path in modules:
        magic = module_vermagic(path.read_bytes(), path.name)
        if not magic.startswith(release + ' '):
            raise ValueError(f'{path.name}: vermagic {magic!r} does not match {release!r}')
        if expected is not None and magic != expected:
            raise ValueError(f'{path.name}: module build flags differ from other modules')
        expected = magic
    print(f'ELF metadata: {len(modules)} modules verified for {release}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('release')
    parser.add_argument('directory', type=pathlib.Path)
    args = parser.parse_args()
    check_directory(args.directory, args.release)
