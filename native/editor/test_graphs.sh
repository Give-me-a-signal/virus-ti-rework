#!/bin/sh
set -eu
cd "$(dirname "$0")"
destination="${TMPDIR:-/tmp}/virus-editor-graphs-test-$$"
trap 'rm -f "$destination"' EXIT
xcrun clang++ -arch arm64 -std=c++17 -fobjc-arc -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer graphs.mm graph_tests.mm -framework Cocoa -o "$destination"
ASAN_OPTIONS=detect_leaks=0 "$destination"
