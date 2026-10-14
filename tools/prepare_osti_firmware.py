#!/usr/bin/env python3
"""Replace only the vti.bin member of a copy of the original installer bundle."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/full-emulator'
sha=lambda data:hashlib.sha256(data).hexdigest()


def main():
    manifest=json.loads((ROOT/'analysis/firmware-images/extraction-manifest.json').read_text())
    original=Path(manifest['source']).read_bytes()
    assert sha(original)=='8e3daea8552f9ecb1b7205bb5f81e7391dcf406e86af81fc7124dba00fb5cd6a'
    entry=next(e for e in manifest['images'] if e['name']=='vti.bin')
    replacement=(ROOT/'analysis/wavetable-trace/wav-import/prism-formant/vti.Prism-Formant.experimental.bin').read_bytes()
    assert sha(replacement)=='e219474ac54c00a1be9c982f5649929991e1d3932b490d4584720636467d2cbc'
    offset,length=entry['outer_offset'],entry['size']
    assert len(replacement)==length and sha(original[offset:offset+length])==entry['sha256']
    patched=original[:offset]+replacement+original[offset+length:]
    assert len(patched)==len(original)
    assert patched[:offset]==original[:offset] and patched[offset+length:]==original[offset+length:]
    changes=sum(a!=b for a,b in zip(original,patched))
    assert changes==386
    OUT.mkdir(exist_ok=True)
    dest=OUT/'firmware.Prism-Formant.experimental.bin'
    if dest.exists():assert dest.read_bytes()==patched, 'Different output already exists'
    else:dest.write_bytes(patched)
    record={'original_bundle':manifest['source'],'original_sha256':sha(original),
            'modified_bundle_sha256':sha(patched),'vti_offset':offset,'vti_length':length,
            'changed_bytes':changes,'other_members_unchanged':True}
    (OUT/'container-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record))


if __name__=='__main__':main()
