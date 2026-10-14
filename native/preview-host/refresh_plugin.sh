#!/bin/bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
if pgrep -x VirusTIInterface >/dev/null; then
 echo 'Quit Virus TI Interface before replacing its plug-in snapshot.' >&2
 exit 1
fi
app="$here/build/VirusTIInterface.app"
source_bundle="$here/../vst3/build/VST3/Release/Virus TI Rework Development.vst3"
snapshot="$app/Contents/Resources/Virus TI Rework Development.vst3"
[ -d "$app" ] && [ -d "$source_bundle" ] || { echo 'Build the host and plug-in first.' >&2; exit 1; }
/usr/bin/ditto "$source_bundle" "$snapshot"
/usr/bin/codesign --verify --strict "$snapshot"
"$app/Contents/MacOS/VirusTIInterface" --self-test
/usr/bin/codesign --force --sign - "$app"
/usr/bin/codesign --verify --strict "$app"
/usr/bin/shasum -a 256 "$snapshot/Contents/MacOS/Virus TI Rework Development"
