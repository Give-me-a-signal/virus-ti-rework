#!/usr/bin/env python3
"""Capture all native editor pages in all three appearances without hardware."""
import argparse
import os
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--original', action='store_true', help='Also render an already imported Original Access appearance')
    args = parser.parse_args()
    build = args.build.resolve()
    resources = build / 'no-access-resources'
    output = build / 'gallery-renders'
    env = dict(os.environ)
    if args.original:
        env['VTR_TEST_ORIGINAL_APPEARANCE'] = '1'
    subprocess.run([str(build/'vst3_editor_tests'), str(resources), str(output)], check=True, env=env)
    gallery = ROOT / 'docs/screenshots'
    gallery.mkdir(exist_ok=True)
    pages = [0,1,2,3,4,5,6,7,11,9,8,12]
    files = {}
    styles = ['dark','polar'] + (['original'] if args.original else [])
    for style in styles:
        for page in pages:
            name = f'{style}-page-{page:02d}.png'
            source = output/f'{style}-{page:02d}.png'
            if source.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
                raise ValueError(f'Invalid PNG: {source}')
            shutil.copy2(source, gallery/name)
            files[name] = hashlib.sha256(source.read_bytes()).hexdigest()
    manifest = {'captured_utc':datetime.now(timezone.utc).isoformat(),
                'method':'Real native Cocoa editor offscreen view capture; no mockup, live DAW or hardware',
                'fixtures':'Disconnected native editor; project-authored layouts and optional locally imported original appearance',
                'pages_per_appearance':12, 'appearances':styles,
                'test_binary_sha256':hashlib.sha256((build/'vst3_editor_tests').read_bytes()).hexdigest(),
                'images':files}
    (gallery/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Captured {len(files)} native view images in {gallery}')


if __name__ == '__main__':
    main()
