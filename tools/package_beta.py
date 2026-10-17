#!/usr/bin/env python3
"""Package only native project code and permitted dependency notices."""
import argparse, hashlib, json, pathlib, re, shutil, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = '0.2.0-beta.1'

def run(*args):
    subprocess.run([str(a) for a in args], check=True)

def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=pathlib.Path)
    parser.add_argument('--snow-test', action='store_true', help='Label/package the unverified Snow playback preview')
    parser.add_argument('--full-test', action='store_true', help='Package the combined PR tester prerelease')
    args = parser.parse_args()
    build = args.build.resolve()
    flag = re.search(r'^VTR_TIMING_DAW_TEST_BUILD:[^=]+=(.*)$', (build/'CMakeCache.txt').read_text(), re.M)
    value = flag[1].strip().upper() if flag else 'OFF'
    if value not in ('', '0', 'OFF', 'NO', 'FALSE', 'N', 'IGNORE', 'NOTFOUND') and not value.endswith('-NOTFOUND'):
        raise SystemExit('Refusing to package an experimental timing DAW test build')
    if args.full_test and args.snow_test:
        raise SystemExit('Choose one test-build label')
    version = '0.2.0-test.1' if args.full_test else '0.2.0-snow-test.1' if args.snow_test else VERSION
    cache = (build/'CMakeCache.txt').read_text()
    is_snow = 'VTR_SNOW_PLAYBACK_TEST_BUILD:BOOL=ON' in cache
    is_full = 'VTR_FULL_TEST_BUILD:BOOL=ON' in cache
    if is_full != args.full_test:
        raise RuntimeError('Full test build/package flag mismatch')
    if is_snow != args.snow_test:
        raise RuntimeError('Snow build/package flag mismatch; refusing misleading release label')
    output = ROOT/'dist'/f'Virus-TI-Rework-{version}-macOS-arm64'
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    vst = output/'Virus TI Rework.vst3'
    au = output/'Virus TI Rework.component'
    setup = output/'Virus TI Rework Setup.app'
    for source, dest in [(build/'VST3/Release'/vst.name, vst), (build/'AU'/au.name, au),
                         (build/setup.name, setup)]:
        shutil.copytree(source, dest, symlinks=True)
    notices = vst/'Contents/Resources/Licenses'
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT/'LICENSE', notices/'Project-MIT.txt')
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md', notices/'THIRD_PARTY_NOTICES.md')
    for part in ('', 'base', 'cmake', 'pluginterfaces', 'public.sdk'):
        source = ROOT/'third_party/vst3sdk'/part/'LICENSE.txt'
        if source.exists():
            shutil.copy2(source, notices/f'VST3-{part or "sdk"}.txt')
    for source in (ROOT/'third_party/AudioUnitSDK').glob('*LICENSE*'):
        shutil.copy2(source, notices/f'AudioUnitSDK-{source.name}.txt')
    run('codesign', '--force', '--sign', '-', vst)
    nested = au/'Contents/Resources/plugin.vst3'
    shutil.rmtree(nested)
    shutil.copytree(vst, nested)
    run('codesign', '--force', '--sign', '-', au)
    payload = setup/'Contents/Resources/Payload'
    payload.mkdir(parents=True, exist_ok=True)
    for bundle in (vst, au):
        shutil.copytree(bundle, payload/bundle.name)
    (payload/'checksums.json').write_text(json.dumps({b.name: hashes(b) for b in (vst, au)}, indent=2)+'\n')
    run('codesign', '--force', '--sign', '-', setup)
    for bundle in (vst, au, setup):
        run('codesign', '--verify', '--deep', '--strict', bundle)
    shutil.copy2(ROOT/'docs/beta-validation.json', output/'release-validation.json')
    (output/'INSTALL.txt').write_text('''Virus TI Rework 0.2.0 beta 1 — Apple Silicon

Quit your DAW. Open Virus TI Rework Setup.app, choose AU and/or VST3,
and click Install / Update. Restart the DAW and rescan plug-ins.

Manual installation (Finder: Go > Go to Folder):
  Virus TI Rework.vst3 -> ~/Library/Audio/Plug-Ins/VST3/
  Virus TI Rework.component -> ~/Library/Audio/Plug-Ins/Components/
Before installing, move Virus TI Rework Development.vst3 out of the VST3
folder and keep it as a backup. Do not install both copies with the same ID.
The AU works on its own; installing VST3 separately is optional.

This is an ad-hoc signed, unnotarized beta. macOS may block downloaded
software. Inspect the release and use the per-app Open Anyway approval in
System Settings > Privacy & Security if available. Do not disable Gatekeeper
or SIP or run blanket quarantine-removal commands. Report installation
blocks that this documented path cannot resolve.

Dark and Polar work without Access software. Optional original artwork:
get your own Virus TI Software Suite 5.1.7.00.pkg from Access (sign-in may
be required): https://www.virus.info/download/585
Do not run the old package. In Setup choose Import Original Access
Appearance, or choose Import Original from the editor's appearance menu.
Select the package, wait for import, then choose Original Access in the editor.
Choose Modern UI to return to Dark/Polar. Import requires no developer tools;
the package is checksum-verified and extracted without running vendor code.
AU and VST3 share imported artwork outside the plug-in bundles.

Use a disposable DAW project. TI Desktop is the initial hardware target;
TI2/Snow and GarageBand remain unverified. Bounded packaged beta tests cover
Logic Pro, Bitwig, Ableton Live and LiveProfessor on M1 Pro/macOS 26.2.
Coverage differs by host; see the README's Tested setups section.
No firmware updates or stored-bank writes are supported. Keep the synth
connected and powered until RAM restoration completes after disconnect/exit.
Real-time recording only; the hardware cannot render offline.

Documentation, limitations and test reports:
https://github.com/Give-me-a-signal/virus-ti-rework
''')
    if args.snow_test:
        (output/'INSTALL.txt').write_text((ROOT/'docs/SNOW_TEST_BUILD.md').read_text())
        (output/'release-validation.json').write_text(json.dumps({
            'scope': 'Snow playback test 1, unverified hardware',
            'physical_snow_tested': False, 'emulator_is_not_usb_validation': True,
            'standard_build_keeps_snow_blocked': False,
            'requires_explicit_per_session_consent': False,
            'snow_connection_action': 'Connect TI',
            'requires_manual_snow_connection': True,
            'requires_recovery_session_record': True,
            'requires_current_format_stable_originals': True,
            'firmware_operations': False, 'stored_bank_writes': False,
            'instructions': 'INSTALL.txt',
        }, indent=2)+'\n')
    if args.full_test:
        shutil.copy2(ROOT/'docs/full-test-validation.json', output/'release-validation.json')
        shutil.copy2(ROOT/'docs/FULL_TEST_BUILD.md', output/'TESTING.md')
        (output/'INSTALL.txt').write_text('''Virus TI Rework 0.2.0-test.1 — build 2005 — Apple Silicon

Combined PR #6/#7/#9/#11 tester build, not a stable release.
Read TESTING.md: Logic live AU save/reopen is unresolved; physical Snow
support is unverified; marker timing remains disabled by default.

1. Quit all DAWs.
2. Open Virus TI Rework Setup.app, choose AU and/or VST3, click Install / Update.
3. Reopen your native Apple Silicon DAW and load Virus TI Rework.

Manual copy: .component -> ~/Library/Audio/Plug-Ins/Components/
             .vst3 -> ~/Library/Audio/Plug-Ins/VST3/
Use one copy of each format. The AU works independently. Setup backs up
previous versions. These have the same IDs as the beta; do not install both.

Ad-hoc signed, not notarized. Use per-app Open Anyway approval if macOS offers
it. Do not disable security protections or run blanket quarantine removal.
Report blocks that this documented approval cannot resolve.

Dark/Polar need no Access installer. Import Original Access Appearance is
optional and uses your own supported 5.1.7.00 package without executing it.
No firmware, factory sounds, emulator or hardware drivers are bundled.
Existing compatible CoreAudio/CoreMIDI drivers are required for hardware.

Use disposable projects. Allow the DAW microphone access if macOS requests it.
Use normal Connect TI for TI or Snow; Snow reconnects manually after reopen.
Leave the synth powered/connected until restoration finishes on disconnect/exit.
No firmware updates, USB resets or stored-bank writes are supported.

Reports: https://github.com/Give-me-a-signal/virus-ti-rework/issues
''')
    commit = subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git','status','--porcelain'], cwd=ROOT, text=True).strip())
    if args.full_test:
        if dirty:
            raise SystemExit('Refusing to distribute a full test build from a dirty checkout')
        validation = json.loads((output/'release-validation.json').read_text())
        validation.update(source_commit=commit, dirty=False)
        (output/'release-validation.json').write_text(json.dumps(validation, indent=2)+'\n')
    report = {'version': version, 'full_test_build': args.full_test, 'snow_playback_test': args.snow_test, 'physical_snow_verified': False, 'marker_timing_default': False, 'commit': commit, 'dirty': dirty, 'architecture': 'arm64',
              'developer_id_signed': False, 'notarized': False,
              'access_package_required': False, 'validation': 'See release-validation.json',
              'files': hashes(output)}
    (output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    forbidden = {'.png','.xml','.mid','.syx','.pkg','.bin','.dylib'}
    for p in output.rglob('*'):
        if p.is_symlink():
            raise RuntimeError(f'Release symlink: {p}')
        if p.is_file() and p.suffix.lower() in forbidden:
            raise RuntimeError(f'Forbidden release resource: {p}')
        if p.is_file() and p.read_bytes()[:4] == b'\xcf\xfa\xed\xfe':
            arch = subprocess.check_output(['lipo','-archs',str(p)],text=True).strip()
            if arch != 'arm64':
                raise RuntimeError(f'Unexpected release architecture: {p}: {arch}')
            links = subprocess.check_output(['otool','-L',str(p)],text=True).splitlines()[1:]
            for line in links:
                dependency=line.strip().split(' (',1)[0]
                if not dependency.startswith(('/usr/lib/','/System/Library/')):
                    raise RuntimeError(f'External runtime dependency: {p}: {dependency}')
        if p.is_file() and (str(ROOT).encode() in p.read_bytes()):
            raise RuntimeError(f'Private build path in {p}')
    archive = pathlib.Path(str(output)+'.zip')
    run('ditto','-c','-k','--sequesterRsrc','--keepParent',output,archive)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    (ROOT/'dist'/'SHA256SUMS.txt').write_text(f'{checksum}  {archive.name}\n')
    print(archive)

if __name__ == '__main__':
    main()
