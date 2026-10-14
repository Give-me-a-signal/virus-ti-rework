#!/usr/bin/env python3
"""Capture all native editor pages in all three appearances without hardware."""
import argparse
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
    args = parser.parse_args()
    build = args.build.resolve()
    resources = build / 'VST3/Release/Virus TI Rework Development.vst3/Contents/Resources/OriginalEditor'
    output = build / 'gallery-renders'
    subprocess.run([str(build/'vst3_editor_tests'), str(resources), str(output)], check=True)
    gallery = ROOT / 'docs/screenshots'
    gallery.mkdir(exist_ok=True)
    pages = [0,1,2,3,4,5,6,7,11,9,8,12]
    files = {}
    for style in ['dark','polar','original']:
        for page in pages:
            name = f'{style}-page-{page:02d}.png'
            source = output/name
            if source.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
                raise ValueError(f'Invalid PNG: {source}')
            shutil.copy2(source, gallery/name)
            files[name] = hashlib.sha256(source.read_bytes()).hexdigest()
    manifest = {'captured_utc':datetime.now(timezone.utc).isoformat(),
                'method':'Real native Cocoa editor offscreen view capture; no mockup, live DAW or hardware',
                'fixtures':'Synthetic telemetry and local test patches; native footer controls omitted',
                'pages_per_appearance':12, 'appearances':['dark','polar','original'],
                'test_binary_sha256':hashlib.sha256((build/'vst3_editor_tests').read_bytes()).hexdigest(),
                'images':files}
    (gallery/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Captured {len(files)} native view images in {gallery}')


if __name__ == '__main__':
    main()
