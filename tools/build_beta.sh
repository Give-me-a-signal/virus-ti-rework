#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${VTR_BUILD_DIR:-$root/native/vst3/build-beta}"
bash "$root/native/vst3/bootstrap_sdk.sh"
ausdk="$root/third_party/AudioUnitSDK"
revision=bd98b31feff57a15989fcfab4cd86dc63382b1ac
if [[ ! -d "$ausdk/.git" ]]; then git clone https://github.com/apple/AudioUnitSDK.git "$ausdk"; fi
[[ -z "$(git -C "$ausdk" status --porcelain)" ]] || { echo 'AudioUnitSDK has local changes'; exit 1; }
git -C "$ausdk" checkout --detach "$revision"
if ! xcodebuild -version >/dev/null 2>&1; then
 export XCODE_VERSION="$(pkgutil --pkg-info com.apple.pkg.CLTools_Executables | awk '/^version:/{print $2}')"
fi
cmake -S "$root/native/vst3" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DPython3_EXECUTABLE="${VTR_PYTHON:-/usr/bin/python3}"
cmake --build "$build" -j 6
ctest --test-dir "$build" --output-on-failure
"$build/bin/Release/validator" "$build/VST3/Release/Virus TI Rework.vst3" > "$build/validator.log" 2>&1
tail -4 "$build/validator.log"
"${VTR_PYTHON:-/usr/bin/python3}" "$root/tools/package_beta.py" "$build"
"$build/setup_install_tests" "$root/dist/Virus-TI-Rework-0.2.0-beta.1-macOS-arm64/Virus TI Rework Setup.app/Contents/Resources/Payload"
