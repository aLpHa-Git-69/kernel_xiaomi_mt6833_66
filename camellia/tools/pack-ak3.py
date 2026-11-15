#!/usr/bin/env python3
"""Replace both r1.3 payloads with freshly built, validated kernel payloads."""
import argparse
import gzip
import hashlib
import json
import pathlib
import re
import struct
import zipfile

TEMPLATE_SHA256 = 'd13e7230aa579d05a7d3ed3f066be02868d6d9d941bc664f03b658d01bd17b0e'


def validate_payload(path, variant):
    info = json.loads((path / 'build-info.json').read_text())
    if info['variant'] != variant:
        raise ValueError('wrong payload variant: ' + str(path))
    image = gzip.decompress((path / 'Image.gz').read_bytes())
    if len(image) < 64 or image[56:60] != b'ARM\x64':
        raise ValueError('not an arm64 kernel Image: ' + str(path))
    dtb = (path / 'dtb').read_bytes()
    if len(dtb) < 40 or struct.unpack_from('>II', dtb) != (0xd00dfeed, len(dtb)):
        raise ValueError('invalid merged device tree: ' + str(path))
    modules = path / 'lib/modules'
    order = (modules / 'modules.load').read_text().splitlines()
    if not order or len(order) != len(set(order)):
        raise ValueError('empty or duplicate module load list')
    for name in order:
        if pathlib.PurePosixPath(name).name != name or not name.endswith('.ko'):
            raise ValueError('invalid module name: ' + name)
        data = (modules / name).read_bytes()
        if data[:5] != b'\x7fELF\x02' or struct.unpack_from('<H', data, 18)[0] != 183:
            raise ValueError('not an arm64 module: ' + name)
    if set(order) != {p.name for p in modules.glob('*.ko')}:
        raise ValueError('module load list does not cover all packaged modules')
    for provider in ('camellia_aw87559.ko', 'camellia_fs1815.ko'):
        if order.index(provider) >= order.index('mt6833-mt6359p.ko'):
            raise ValueError('speaker provider is ordered after sound card')
    return info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('template', type=pathlib.Path)
    parser.add_argument('base', type=pathlib.Path)
    parser.add_argument('sukisu', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    if hashlib.sha256(args.template.read_bytes()).hexdigest() != TEMPLATE_SHA256:
        raise ValueError('AnyKernel3 template does not match the verified r1.3 release')
    base = validate_payload(args.base, 'base')
    sukisu = validate_payload(args.sukisu, 'sukisu')
    if base['source_commit'] != sukisu['source_commit']:
        raise ValueError('base and SukiSU were built from different source commits')
    label = 'Camellia 6.6 speaker test ' + base['source_commit'][:8]
    with zipfile.ZipFile(args.template) as template, zipfile.ZipFile(
        args.output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as output:
        for entry in template.infolist():
            if entry.filename.startswith(('base/', 'sukisu/')):
                continue
            data = template.read(entry)
            if entry.filename == 'anykernel.sh':
                text = data.decode()
                text, count = re.subn(r'^kernel\.string=.*$', 'kernel.string=' + label,
                                     text, flags=re.MULTILINE)
                if count != 1:
                    raise ValueError('unexpected AnyKernel3 properties')
                text = text.replace('patch_cmdline log_buf_len log_buf_len=2M;',
                                    'patch_cmdline log_buf_len log_buf_len=16M;')
                data = text.encode()
            output.writestr(entry, data)
        for variant, path in (('base', args.base), ('sukisu', args.sukisu)):
            for source in sorted(path.rglob('*')):
                if source.is_file():
                    output.write(source, variant + '/' + source.relative_to(path).as_posix())
        output.writestr('speaker-test.txt', label + '\n\n'
                        'Volume Up: SukiSU-Ultra. Volume Down: base.\n'
                        'After boot, test media, ringtone, call speakerphone and headphones.\n'
                        'Capture dmesg and logcat while the speaker is playing.\n'
                        'AW87559 should report chip ID 0x5a and scene music: 0.\n')
    with zipfile.ZipFile(args.output) as output:
        if output.testzip() is not None:
            raise ValueError('corrupt output archive')
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix(args.output.suffix + '.sha256').write_text(
        digest + '  ' + args.output.name + '\n'
    )
    print(digest + '  ' + args.output.name)


if __name__ == '__main__':
    main()
