#!/bin/sh
set -eu
module_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir=$(dirname -- "$module_dir")
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p "$module_dir/build"
for variant in normal sanitized; do
    flags="-O2"
    if [ "$variant" = sanitized ]; then flags="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    # flags intentionally expands to compiler arguments.
    run clang++ -arch arm64 -std=c++17 -Wall -Wextra -Wpedantic -Werror $flags \
        -I"$module_dir/include" -I"$native_dir/transport-core/include" \
        -I"$native_dir/usb-descriptors/include" \
        "$module_dir/src/audio_engine.cpp" "$module_dir/tests/audio_engine_test.cpp" \
        "$native_dir/transport-core/src/transport_core.cpp" \
        "$native_dir/usb-descriptors/src/usb_descriptors.cpp" \
        -o "$module_dir/build/audio-engine-test-$variant"
    run "$module_dir/build/audio-engine-test-$variant"
done
# Compiles and links the real macOS adapter; only test Objective-C stand-ins run.
run clang++ -arch arm64 -mmacosx-version-min=12.0 -std=c++17 -fobjc-arc \
    -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$module_dir/include" -I"$native_dir/transport-core/include" -I"$native_dir/usb-descriptors/include" \
    "$module_dir/src/audio_engine.cpp" "$module_dir/src/iousbhost_backend.mm" \
    "$module_dir/tests/iousbhost_backend_test.mm" "$native_dir/transport-core/src/transport_core.cpp" \
    "$native_dir/usb-descriptors/src/usb_descriptors.cpp" -framework IOUSBHost -framework IOKit -framework Foundation \
    -o "$module_dir/build/iousbhost-backend-test"
run "$module_dir/build/iousbhost-backend-test"
