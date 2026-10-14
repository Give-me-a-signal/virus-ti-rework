#!/bin/sh
set -eu
module_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p "$module_dir/build"
for variant in normal sanitized; do
    flags="-O2"
    if [ "$variant" = sanitized ]; then flags="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    run clang++ -arch arm64 -std=c++17 -Wall -Wextra -Wpedantic -Werror $flags \
        -I"$module_dir/include" "$module_dir/src/clock_sync.cpp" "$module_dir/tests/clock_sync_test.cpp" \
        -o "$module_dir/build/clock-sync-test-$variant"
    run "$module_dir/build/clock-sync-test-$variant"
done
