#!/bin/sh
set -eu
cd "$(dirname "$0")"
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p build
run xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -Wall -Wextra -Wpedantic -Werror -g -O1 \
    -Iinclude src/usb_descriptors.cpp tests/usb_descriptors_test.cpp -o build/usb_descriptors_test
run build/usb_descriptors_test
run xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -Wall -Wextra -Wpedantic -Werror -g -O1 \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/usb_descriptors.cpp \
    tests/usb_descriptors_test.cpp -o build/usb_descriptors_test_sanitized
run build/usb_descriptors_test_sanitized
run xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -fobjc-arc -Wall -Wextra -Werror \
    -Iinclude src/usb_descriptors.cpp src/main.mm -framework Foundation -framework IOKit \
    -framework IOUSBHost -o build/virus-usb-descriptors
run build/virus-usb-descriptors --help
run python3 tests/cli_test.py
