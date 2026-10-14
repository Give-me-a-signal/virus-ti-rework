#!/usr/bin/env python3
"""Check tracked files for accidental local payloads before pushing source."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
files = subprocess.check_output(['git','ls-files','-z'], cwd=root).decode().split('\0')
errors = []
for name in filter(None, files):
    p = Path(name)
    if p.parts[0] in ('analysis','third_party','dist') or any(s.startswith('build') or s == 'target' for s in p.parts[:-1]):
        errors.append(f'Local tree tracked: {name}')
    if p.suffix.lower() in ('.pkg','.dmg','.syx','.mid','.wav','.vtrstate','.bwproject','.dylib','.so','.a','.o'):
        errors.append(f'Payload tracked: {name}')
    data = (root/p).read_bytes()
    if data[:4] in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\x7fELF'):
        errors.append(f'Executable tracked: {name}')
    if p.suffix == '.png' and p.parent != Path('docs/screenshots'):
        errors.append(f'Unexpected artwork: {name}')
    if re.search(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)', data):
        errors.append(f'Possible credential: {name}')
for name in ('native/vst3/catalog.h','native/editor/browser_mapping.inc','native/editor/graph_waveforms.h'):
    if name in files:
        errors.append(f'Generated vendor-derived data tracked: {name}')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'PASS source boundary: {len(list(filter(None, files)))} tracked files; no local payload or recognized secret pattern')
