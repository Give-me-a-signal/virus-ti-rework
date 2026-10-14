#!/bin/sh
set -eu
cd "$(dirname "$0")"
# RTK is preferred when installed; the developer host may not have it.
run() {
    if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi
}
run mkdir -p build
run xcrun clang -arch arm64 -mmacosx-version-min=12.0 -fobjc-arc -Wall -Wextra -Werror \
    -framework Foundation -framework IOKit -framework CoreMIDI -framework CoreAudio \
    main.m -o build/virus-probe
printf '%s\n' "Built native/virus-probe/build/virus-probe (arm64)"
