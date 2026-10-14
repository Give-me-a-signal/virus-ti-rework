#!/bin/bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${VTR_BUILD_DIR:-$here/build}"
sdk="$here/../../third_party/vst3sdk"
if [ ! -f "$sdk/public.sdk/source/vst/vstaudioeffect.h" ]; then
 echo "SDK missing. Run $here/bootstrap_sdk.sh first." >&2; exit 1
fi
# SDK's CMake checks xcodebuild even under Ninja. Apple's CLT is sufficient for
# these framework-only targets; record the actual CLT version when no Xcode exists.
args=()
if ! xcodebuild -version >/dev/null 2>&1; then
 clt_version="$(pkgutil --pkg-info com.apple.pkg.CLTools_Executables | awk '/^version:/{print $2}')"
 [ -n "$clt_version" ] || { echo "Apple command line tools unavailable" >&2; exit 1; }
 export XCODE_VERSION="$clt_version"
 args+=("-DXCODE_VERSION=$clt_version")
fi
"${VTR_PYTHON:-/usr/bin/python3}" "$here/generate_catalog.py"
cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DVTR_BUNDLE_EMULATOR=OFF "${args[@]}"
cmake --build "$build" --target VirusTIRework vst3_processor_tests vst3_editor_tests capture_fifo_tests local_control_tests meter_tests restoration_tests validator -j 6
ctest --test-dir "$build" --output-on-failure
bundle="$build/VST3/Release/Virus TI Rework Development.vst3"
"$build/bin/Release/validator" "$bundle" > "$build/validator.log" 2>&1
tail -4 "$build/validator.log"
codesign --force --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
file "$bundle/Contents/MacOS/Virus TI Rework Development"
shasum -a 256 "$bundle/Contents/MacOS/Virus TI Rework Development"
