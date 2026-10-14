#!/usr/bin/env python3
"""Recreate ignored build inputs from the owner's exact installer; no device I/O."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PKG_SHA = '3ebcfef395a6c5f1ef2162404228f1fbe571371f7389836f13c55c8a7636ce1d'
BINARY_SHA = '52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d'
PROBE_SHA = 'a878286264109defc1fbeff00f2bc33bc60f5468e20df647b207bac37ca0e6cc'
EXPANDED = ROOT / 'analysis/package-expanded'
RES = EXPANDED / 'Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources'


def run(*args):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT)


def check(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_va(binary, address, size):
    check(binary[:4] == b'\xcf\xfa\xed\xfe', 'Expected thin little-endian Mach-O 64')
    count = struct.unpack_from('<I', binary, 16)[0]
    pos = 32
    for _ in range(count):
        cmd, length = struct.unpack_from('<II', binary, pos)
        check(length >= 8 and pos + length <= len(binary), 'Invalid Mach-O command')
        if cmd == 0x19:
            va, _, offset, file_size = struct.unpack_from('<QQQQ', binary, pos + 24)
            if va <= address and address + size <= va + file_size:
                start = offset + address - va
                check(start + size <= len(binary), 'Table outside Mach-O')
                return binary[start:start + size]
        pos += length
    raise ValueError('Display table missing from expected Mach-O segment')


def waveform_header():
    thin = ROOT / 'analysis/thin/VirusControl.x86_64'
    thin.parent.mkdir(parents=True, exist_ok=True)
    run('/usr/bin/lipo', RES.parent / 'MacOS/Virus Control', '-thin', 'x86_64', '-output', thin)
    binary = thin.read_bytes()
    check(digest(binary) == BINARY_SHA, 'Unexpected editor binary; refusing fixed-address extraction')
    words = struct.unpack('<4096I', read_va(binary, 0x1e5ce0, 16384))
    lines = ['// Generated locally from the owner-supplied editor; not MIT data.',
             '#pragma once', '#include <cstdint>', 'namespace VirusRework::Graphs {',
             'inline constexpr std::uint32_t waveformBits[64][64] = {']
    for i in range(64):
        lines.append('{' + ','.join(f'0x{x:08x}u' for x in words[i*64:(i+1)*64]) + '},')
    lines.extend(['};', '}'])
    (ROOT / 'native/editor/graph_waveforms.h').write_text('\n'.join(lines) + '\n')


def browser_mapping():
    tree = ET.parse(RES / 'remotedevice.xml')
    bases = {'cc': 9, 'pp': 137, 'Bank C (6E) Single-3': 266, 'Bank D (6F) Single-4': 394}
    rows = []
    for group in tree.findall('./ParameterSets/ParameterSet'):
        for p in group:
            if group.get('name', '').startswith('Internal ') and p.get('name') != '123 All Notes Off':
                continue
            base = bases.get(p.get('defaultmessage'))
            if base is None:
                continue
            index = base + int(p.get('controllerindex'))
            if not (9 <= index < 265 or 266 <= index < 522):
                continue
            for ti in ([False, True] if index < 265 else [True]):
                rows.append((index, group.get('name') + '/' + p.get('name'),
                             int(p.get('minValue')), int(p.get('maxValue')), ti))
    rows.sort(key=lambda r: (r[4], r[1] == 'Filters/Select', r[0]))
    lines = ['// Generated locally from owner-supplied XML; not MIT data.',
             'struct Mapping { const char* name; unsigned index; int minimum, maximum; bool ti; };',
             'static constexpr Mapping mappings[]{']
    for index, name, lo, hi, ti in rows:
        lines.append(f' {{{json.dumps(name)},{index},{lo},{hi},{str(ti).lower()}}},')
    lines += ['};', 'static constexpr const char* category_names[]{']
    categories = tree.find("./Valuelists/Valuelist[@name='Val Name Categories']")
    check(categories is not None and len(categories) == 23, 'Missing category list')
    for entry in categories:
        lines.append(' ' + json.dumps((entry.text or '').strip()) + ',')
    lines += ['};']
    (ROOT / 'native/editor/browser_mapping.inc').write_text('\n'.join(lines) + '\n')


def test_patch():
    # Decode only the factory INIT bank required by the known RAM test fixture.
    # These are file operations; no updater, firmware transfer or hardware is used.
    from inspect_firmware import extract_images, decode_payload
    images = ROOT / 'analysis/firmware-images'
    extract_images(RES.parent.parent.parent / 'firmware.bin', images)
    binary = (images / 'vti.bin').read_bytes()
    check(binary[:4] == b'FORM' and int.from_bytes(binary[4:8], 'big') == len(binary)-8, 'Bad firmware container')
    pos = 8
    found = False
    while pos < len(binary):
        check(pos + 8 <= len(binary), 'Truncated bank header')
        tag, length = binary[pos:pos+4], int.from_bytes(binary[pos+4:pos+8], 'big')
        check(pos + 8 + length <= len(binary), 'Truncated bank')
        if tag == b'P065':
            data, _, _, _ = decode_payload(binary[pos+8:pos+8+length], 'P065')
            dest = images / 'decoded/vti/P065.bin'
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            found = True
        pos += 8 + length
    check(found, 'Factory INIT bank absent')
    run(sys.executable, ROOT / 'tools/make_full_emulator_patch.py')
    patch = bytearray((ROOT / 'analysis/full-emulator/init-wavetable-id1.syx').read_bytes())
    # Exact documented version-8 -> version-12 hardware readback normalization,
    # part zero and reduced volume. Verify the entire resulting fixture below.
    normalized = {8:0,9:12,11:0,12:0,100:45,112:1,113:127,137:64,
                  277:64,278:3,279:4,280:2,286:127,287:64,288:64,289:127,
                  290:127,291:64,308:64,328:64,338:127,339:127,340:64,
                  346:20,347:70,349:64,350:70,351:20,352:70,354:64,355:70,
                  515:48,516:127,517:64,519:1}
    for index, value in normalized.items():
        patch[index] = value
    patch[265] = sum(patch[5:265]) & 127
    patch[522] = sum(patch[5:522]) & 127
    check(digest(patch) == PROBE_SHA, 'Test fixture does not match verified hardware fixture')
    dest = ROOT / 'analysis/hardware/fixtures/stock-wavetable-probe.syx'
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(patch)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    args = parser.parse_args()
    check(digest(args.installer.read_bytes()) == PKG_SHA,
          'Installer SHA-256 does not match the supported 5.1.7.00 package')
    (ROOT / 'analysis').mkdir(exist_ok=True)
    if not EXPANDED.exists():
        run('/usr/sbin/pkgutil', '--expand-full', args.installer.resolve(), EXPANDED)
    check((RES / 'editor.xml').is_file(), 'Incomplete package expansion; use a fresh checkout')
    run(sys.executable, ROOT / 'tools/inventory_plugin_midi.py', '--xml-only')
    run(sys.executable, ROOT / 'tools/inventory_editor_ui.py')
    run(sys.executable, ROOT / 'native/vst3/generate_catalog.py')
    browser_mapping()
    waveform_header()
    test_patch()
    print('Prepared local resources. No vendor executable or device operation was run.')


if __name__ == '__main__':
    main()
