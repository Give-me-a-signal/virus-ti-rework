#!/bin/sh
set -eu
cd "$(dirname "$0")"
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p build
for mode in native sanitized; do
    flags=""
    if [ "$mode" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    run xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -Wall -Wextra -Wpedantic -Werror -g -O1 $flags \
        -Iinclude -I../audio-engine/include -I../usb-descriptors/include -I../transport-core/include \
        src/usb_session.cpp ../usb-descriptors/src/usb_descriptors.cpp ../transport-core/src/transport_core.cpp \
        tests/usb_session_test.cpp -o "build/usb_session_test_$mode"
    run "build/usb_session_test_$mode"
done
run xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -fobjc-arc -Wall -Wextra -Werror -g -O1 \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/iousbhost_controls.mm \
    tests/iousbhost_controls_test.mm -framework Foundation -framework IOKit -framework IOUSBHost \
    -o build/iousbhost_controls_test_sanitized
run build/iousbhost_controls_test_sanitized
