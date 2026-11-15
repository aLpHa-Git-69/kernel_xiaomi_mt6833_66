#!/usr/bin/env python3
"""Build a complete Camellia boot v2 image with the known-working MIUI ramdisk."""
import argparse
import gzip
import hashlib
import json
import pathlib
import stat
import struct
import zlib

from pack_ak3_import import validate_payload

REFERENCE_SHA256 = '9d95dc19ff7dd11c86ecf5fd179691661261a48af7dc19b283b35db1f934cc73'
STOCK_RAMDISK_SHA256 = '0d2cf12b36ad2f9fdbe4c05edfd4d37756006064ec7bd7f255647756619964e1'
PARTITION_SIZE = 64 * 1024 * 1024


def align(size, page):
    return (size + page - 1) // page * page


def split_boot(data):
    if len(data) < 1660 or data[:8] != b'ANDROID!':
        raise ValueError('not an Android boot image')
    page = struct.unpack_from('<I', data, 36)[0]
    version = struct.unpack_from('<I', data, 40)[0]
    if version != 2 or page != 2048:
        raise ValueError('expected Camellia boot header v2 and 2048-byte pages')
    sizes = [struct.unpack_from('<I', data, offset)[0] for offset in (8, 16, 24, 1632, 1648)]
    if sizes[2] or sizes[3]:
        raise ValueError('unexpected second stage or recovery DTBO')
    parts = []
    position = page
    for size in sizes:
        if position + size > len(data):
            raise ValueError('boot component exceeds image size')
        parts.append(data[position:position + size])
        position += align(size, page)
    return data[:page], parts


def boot_id(parts):
    digest = hashlib.sha1()
    for part in parts:
        digest.update(part)
        digest.update(struct.pack('<I', len(part)))
    return digest.digest()


def first_gzip_member(data):
    decoder = zlib.decompressobj(31)
    raw = decoder.decompress(data)
    raw += decoder.flush()
    if not decoder.eof:
        raise ValueError('truncated stock ramdisk')
    member = data[:len(data) - len(decoder.unused_data)]
    if hashlib.sha256(member).hexdigest() != STOCK_RAMDISK_SHA256:
        raise ValueError('stock MIUI ramdisk does not match the working release')
    if not raw.startswith(b'070701') or b'lib/modules/modules.load' in raw:
        raise ValueError('unexpected stock ramdisk contents')
    return member


def cpio_entry(name, content, mode, inode):
    encoded = name.encode() + b'\0'
    fields = (inode, mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1, 0,
              len(content), 0, 0, 0, 0, len(encoded), 0)
    header = b'070701' + b''.join(f'{field:08x}'.encode() for field in fields)
    result = header + encoded
    result += b'\0' * (-len(result) % 4)
    result += content
    result += b'\0' * (-len(content) % 4)
    return result


def modules_ramdisk(payload):
    result = bytearray()
    inode = 1
    for directory in ('lib', 'lib/firmware', 'lib/modules'):
        result.extend(cpio_entry(directory, b'', stat.S_IFDIR | 0o755, inode))
        inode += 1
    for path in sorted((payload / 'lib').rglob('*')):
        if path.is_file():
            name = path.relative_to(payload).as_posix()
            result.extend(cpio_entry(name, path.read_bytes(), stat.S_IFREG | 0o644, inode))
            inode += 1
    result.extend(cpio_entry('TRAILER!!!', b'', 0, inode))
    result.extend(b'\0' * (-len(result) % 512))
    return gzip.compress(result, compresslevel=9, mtime=0)


def create_boot(reference, payload, output, inventory):
    data = reference.read_bytes()
    if hashlib.sha256(data).hexdigest() != REFERENCE_SHA256:
        raise ValueError('reference boot image is not the verified r1.3 release')
    header, old_parts = split_boot(data)
    if header[576:596] != boot_id(old_parts):
        raise ValueError('reference boot header checksum mismatch')
    info = json.loads((payload / 'build-info.json').read_text())
    validate_payload(payload, info['variant'])
    required = set(inventory.read_text().splitlines())
    packaged = {path.name for path in (payload / 'lib/modules').glob('*.ko')}
    if packaged != required:
        raise ValueError('module inventory mismatch: missing=' + repr(sorted(required - packaged))
                         + ', unexpected=' + repr(sorted(packaged - required)))
    stock = first_gzip_member(old_parts[1])
    parts = [(payload / 'Image.gz').read_bytes(), stock + modules_ramdisk(payload),
             b'', b'', (payload / 'dtb').read_bytes()]
    updated = bytearray(header)
    for offset, part in zip((8, 16, 24, 1632, 1648), parts):
        struct.pack_into('<I', updated, offset, len(part))
    updated[576:608] = boot_id(parts) + b'\0' * 12
    command = (header[64:576].split(b'\0')[0] + header[608:1632].split(b'\0')[0]).decode()
    command = command.replace('log_buf_len=2M', 'log_buf_len=16M').encode()
    if len(command) >= 1536:
        raise ValueError('boot command line is too long')
    updated[64:576] = command[:512].ljust(512, b'\0')
    updated[608:1632] = command[512:].ljust(1024, b'\0')
    image = bytearray(updated)
    for part in parts:
        image.extend(part)
        image.extend(b'\0' * (-len(part) % 2048))
    if len(image) > PARTITION_SIZE:
        raise ValueError('boot image exceeds the 64 MiB boot partition')
    output.write_bytes(image)
    verify_header, verify_parts = split_boot(output.read_bytes())
    if verify_header[576:596] != boot_id(verify_parts) or verify_parts != parts:
        raise ValueError('boot image round-trip validation failed')
    if first_gzip_member(verify_parts[1]) != stock:
        raise ValueError('stock ramdisk was changed')
    digest = hashlib.sha256(image).hexdigest()
    output.with_suffix(output.suffix + '.sha256').write_text(digest + '  ' + output.name + '\n')
    report = dict(info, module_count=len(packaged), boot_size=len(image),
                  stock_ramdisk_sha256=STOCK_RAMDISK_SHA256, sha256=digest)
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=pathlib.Path)
    parser.add_argument('payload', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--inventory', type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[1] / 'ramdisk-modules.txt')
    args = parser.parse_args()
    create_boot(args.reference, args.payload, args.output, args.inventory)
