#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${VTR_BUILD_DIR:-$root/native/vst3/build-beta}"
snow=OFF
full=OFF
version=0.2.0-beta.1
if [[ "${VTR_SNOW_TEST_BUILD:-0}" == 1 ]]; then snow=ON; version=0.2.0-snow-test.1; fi
if [[ "${VTR_FULL_TEST_BUILD:-0}" == 1 ]]; then full=ON; version=0.2.0-test.1; fi
bash "$root/native/vst3/bootstrap_sdk.sh"
ausdk="$root/third_party/AudioUnitSDK"
revision=bd98b31feff57a15989fcfab4cd86dc63382b1ac
if [[ ! -d "$ausdk/.git" ]]; then git clone https://github.com/apple/AudioUnitSDK.git "$ausdk"; fi
[[ -z "$(git -C "$ausdk" status --porcelain)" ]] || { echo 'AudioUnitSDK has local changes'; exit 1; }
git -C "$ausdk" checkout --detach "$revision"
if ! xcodebuild -version >/dev/null 2>&1; then
 export XCODE_VERSION="$(pkgutil --pkg-info com.apple.pkg.CLTools_Executables | awk '/^version:/{print $2}')"
fi
# The SDK skips xcodebuild when the environment value exists but compares the
# CMake variable. Supply both for a clean Command Line Tools-only checkout.
cmake -S "$root/native/vst3" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DPython3_EXECUTABLE="${VTR_PYTHON:-/usr/bin/python3}" -DXCODE_VERSION="${XCODE_VERSION:-}" -DVTR_SNOW_PLAYBACK_TEST_BUILD="$snow" -DVTR_FULL_TEST_BUILD="$full" -DVTR_TIMING_DAW_TEST_BUILD=OFF -DVTR_BUNDLE_EMULATOR=OFF
cmake --build "$build" -j 6
ctest --test-dir "$build" --output-on-failure
"$build/bin/Release/validator" "$build/VST3/Release/Virus TI Rework.vst3" > "$build/validator.log" 2>&1
tail -4 "$build/validator.log"
if [[ "$full" == ON ]]; then
 "${VTR_PYTHON:-/usr/bin/python3}" "$root/tools/package_beta.py" "$build" --full-test
elif [[ "$snow" == ON ]]; then
 "${VTR_PYTHON:-/usr/bin/python3}" "$root/tools/package_beta.py" "$build" --snow-test
else
 "${VTR_PYTHON:-/usr/bin/python3}" "$root/tools/package_beta.py" "$build"
fi
"$build/setup_install_tests" "$root/dist/Virus-TI-Rework-$version-macOS-arm64/Virus TI Rework Setup.app/Contents/Resources/Payload"
