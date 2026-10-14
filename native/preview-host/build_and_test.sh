#!/bin/bash
set -euo pipefail
if pgrep -x VirusTIInterface >/dev/null; then
 echo "Quit Virus TI Interface before rebuilding its app or plug-in snapshot." >&2
 exit 1
fi
here="$(cd "$(dirname "$0")" && pwd)"
args=()
if ! xcodebuild -version >/dev/null 2>&1; then
 version="$(pkgutil --pkg-info com.apple.pkg.CLTools_Executables | awk '/^version:/{print $2}')"
 export XCODE_VERSION="$version";args+=("-DXCODE_VERSION=$version")
fi
cmake -S "$here" -B "$here/build" -G Ninja -DCMAKE_BUILD_TYPE=Release "${args[@]}"
cmake --build "$here/build" --target VirusTIInterface -j 6
ctest --test-dir "$here/build" --output-on-failure
codesign --force --sign - "$here/build/VirusTIInterface.app"
codesign --verify --strict "$here/build/VirusTIInterface.app"
