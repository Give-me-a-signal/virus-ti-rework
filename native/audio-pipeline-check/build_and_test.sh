#!/bin/sh
set -eu
check_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir=$(dirname -- "$check_dir")
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p "$check_dir/build"
run clang++ -arch arm64 -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$native_dir/transport-core/include" -I"$native_dir/usb-descriptors/include" \
    "$check_dir/main.cpp" "$native_dir/transport-core/src/transport_core.cpp" \
    "$native_dir/usb-descriptors/src/usb_descriptors.cpp" -o "$check_dir/build/audio-pipeline-check"
run "$check_dir/build/audio-pipeline-check"
