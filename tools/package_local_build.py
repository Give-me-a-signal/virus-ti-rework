#!/usr/bin/env python3
"""Zip a validated local bundle, retaining resource provenance and checksums."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    bundle = args.bundle.resolve()
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    out = ROOT / 'dist'
    out.mkdir(exist_ok=True)
    archive = out / 'Virus-TI-Rework-0.1.0-arm64-LOCAL-ONLY.zip'
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(bundle), str(archive)], check=True)
    manifest = {'version':'0.1.0', 'architecture':'arm64', 'hardware_build':True,
                'notarized':False, 'signing':'local ad-hoc',
                'distribution':'LOCAL ONLY: includes owner-supplied Access resources, not an MIT-only release',
                'archive':archive.name, 'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
                'files':{str(p.relative_to(bundle)):hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in sorted(bundle.rglob('*')) if p.is_file()}}
    (out / 'local-build-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(f'Local archive: {archive}')


if __name__ == '__main__':
    main()
