#!/usr/bin/env python3
"""Read-only inventory of an already expanded macOS package; never executes payloads.

Usage: python3 tools/inventory_package.py analysis/package-expanded \
    --output analysis/package-inventory.json
Only Python's standard library is required. On macOS, file/otool supplement
the independently parsed Mach-O headers. Missing tools are reported as errors.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET

THIN = {b'\xce\xfa\xed\xfe': ('<', False), b'\xcf\xfa\xed\xfe': ('<', True),
        b'\xfe\xed\xfa\xce': ('>', False), b'\xfe\xed\xfa\xcf': ('>', True)}
FAT = {b'\xca\xfe\xba\xbe': ('>', False), b'\xbe\xba\xfe\xca': ('<', False),
       b'\xca\xfe\xba\xbf': ('>', True), b'\xbf\xba\xfe\xca': ('<', True)}
CPUS = {7: 'i386', 0x1000007: 'x86_64', 12: 'arm', 0x100000c: 'arm64',
        18: 'ppc', 0x1000012: 'ppc64'}


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def macho_headers(path):
    """Parse bounded header data directly, independently of file/otool output."""
    with path.open('rb') as stream:
        magic = stream.read(4)
        if magic not in THIN and magic not in FAT:
            return None
        offsets = [0]
        if magic in FAT:
            endian, wide = FAT[magic]
            count = struct.unpack(endian + 'I', stream.read(4))[0]
            if count > 128:
                return {'error': 'Unreasonable universal slice count', 'count': count}
            offsets = []
            fmt = endian + ('IIQQII' if wide else 'IIIII')
            for _ in range(count):
                values = struct.unpack(fmt, stream.read(struct.calcsize(fmt)))
                offsets.append(values[2])
        slices = []
        for offset in offsets:
            stream.seek(offset)
            slice_magic = stream.read(4)
            if slice_magic not in THIN:
                slices.append({'offset': offset, 'error': 'Slice is not Mach-O'})
                continue
            endian, wide = THIN[slice_magic]
            values = struct.unpack(endian + '6I', stream.read(24))
            cpu, subtype, filetype, ncmds, sizeofcmds, flags = values
            slices.append({'offset': offset, 'architecture': CPUS.get(cpu, hex(cpu)),
                           'cpu_type': cpu, 'cpu_subtype': subtype, 'bits': 64 if wide else 32,
                           'endianness': 'little' if endian == '<' else 'big',
                           'filetype': filetype, 'load_command_count': ncmds,
                           'load_commands_size': sizeofcmds, 'flags': flags})
        return {'universal': magic in FAT, 'slices': slices}


def inspect_command(program, arguments):
    executable = shutil.which(program)
    if executable is None:
        return {'error': f'{program} is unavailable'}
    try:
        result = subprocess.run([executable, *arguments], capture_output=True, text=True,
                                timeout=30, check=False)
        return {'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {'error': str(error)}


def json_default(value):
    if isinstance(value, bytes):
        return {'base64': base64.b64encode(value).decode('ascii')}
    return str(value)


def xml_node(node):
    return {'tag': node.tag, 'attributes': dict(node.attrib),
            'text': (node.text or '').strip(), 'children': [xml_node(n) for n in node]}


def inventory(root):
    result = {'schema_version': 1, 'source_root': str(root),
              'method': 'Static files only; no vendor code or installer scripts executed.',
              'files': [], 'mach_o': [], 'plists': [], 'installer_metadata': [],
              'scripts': [], 'errors': []}
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            result['files'].append({'path': relative, 'symlink_target': os.readlink(path)})
            continue
        if not path.is_file():
            continue
        record = {'path': relative, 'size': path.stat().st_size, 'sha256': sha256(path)}
        result['files'].append(record)
        try:
            headers = macho_headers(path)
            if headers is not None:
                native = dict(record, headers=headers)
                native['file'] = inspect_command('file', ['-b', str(path)])
                native['otool_dependencies'] = inspect_command('otool', ['-arch', 'all', '-L', str(path)])
                native['otool_load_commands'] = inspect_command('otool', ['-arch', 'all', '-l', str(path)])
                result['mach_o'].append(native)
            if path.suffix == '.plist':
                with path.open('rb') as stream:
                    result['plists'].append(dict(record, data=plistlib.load(stream)))
            if path.name in ('Distribution', 'PackageInfo'):
                result['installer_metadata'].append(dict(record, xml=xml_node(ET.parse(path).getroot())))
            if path.suffix == '.sh' or path.name in ('preinstall', 'postinstall', 'preflight', 'postflight'):
                contents = path.read_text(errors='replace')
                result['scripts'].append(dict(record, contents=contents,
                                              absolute_path_candidates=sorted(set(re.findall(r'/[^\s"\'`<>]+', contents)))))
        except (OSError, ValueError, struct.error, ET.ParseError, plistlib.InvalidFileException) as error:
            result['errors'].append({'path': relative, 'error': str(error)})
    counts = {}
    for native in result['mach_o']:
        for slice_ in native['headers'].get('slices', []):
            arch = slice_.get('architecture', 'unknown')
            counts[arch] = counts.get(arch, 0) + 1
    result['summary'] = {'files': len(result['files']), 'mach_o_files': len(result['mach_o']),
                         'architecture_slice_counts': counts, 'plists': len(result['plists']),
                         'scripts': len(result['scripts']), 'errors': len(result['errors'])}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    if not root.is_dir():
        parser.error('root must be an already extracted directory')
    output = args.output.resolve()
    if output == root or root in output.parents:
        parser.error('output must be outside the extracted tree')
    result = inventory(root)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, default=json_default) + '\n')
    print(json.dumps(result['summary'], indent=2))


if __name__ == '__main__':
    main()
