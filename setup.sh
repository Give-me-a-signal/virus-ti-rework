#!/bin/bash
# Build a private, local plug-in from source and owner-supplied Access resources.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
if [[ $# -lt 1 || $# -gt 2 || ( $# -eq 2 && "$2" != "--install" ) ]]; then
 echo "Usage: bash setup.sh '/path/to/Virus TI Software Suite 5.1.7.00.pkg' [--install]" >&2
 exit 2
fi
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || { echo 'Native Apple silicon macOS is required.' >&2; exit 1; }
for tool in cmake ninja git; do command -v "$tool" >/dev/null || { echo "Missing prerequisite: $tool" >&2; exit 1; }; done
xcrun --find clang++ >/dev/null
python="${VTR_PYTHON:-/usr/bin/python3}"
"$python" -c 'import xml.etree.ElementTree as E; E.fromstring("<check/>")'
"$python" "$root/tools/prepare_local_resources.py" "$1"
bash "$root/native/vst3/bootstrap_sdk.sh"
export VTR_BUILD_DIR="$root/native/vst3/build-package"
bash "$root/native/vst3/build_and_test.sh"
bundle="$VTR_BUILD_DIR/VST3/Release/Virus TI Rework Development.vst3"
mkdir -p "$bundle/Contents/Resources/Licenses"
cp "$root/LICENSE" "$bundle/Contents/Resources/Licenses/Project-MIT.txt"
cp "$root/THIRD_PARTY_NOTICES.md" "$bundle/Contents/Resources/Licenses/THIRD_PARTY_NOTICES.md"
for component in . base cmake pluginterfaces public.sdk; do
 licence="$root/third_party/vst3sdk/$component/LICENSE.txt"
 if [[ -f "$licence" ]]; then cp "$licence" "$bundle/Contents/Resources/Licenses/VST3-${component//./sdk}.txt"; fi
done
codesign --force --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
"$python" "$root/tools/package_local_build.py" "$bundle"
if [[ "${2:-}" == --install ]]; then bash "$root/tools/install_local.sh" "$bundle"; fi
echo "Local build ready: $bundle"
echo 'This bundle contains your imported Access resources; do not upload it as an MIT-only binary release.'
