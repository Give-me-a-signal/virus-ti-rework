#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
run() { if command -v rtk >/dev/null 2>&1; then rtk proxy "$@"; else "$@"; fi; }
run mkdir -p native/editor/build
bank_dir='analysis/package-expanded/Additional_items.pkg/Payload/Library/Application Support/Access Music/Virus TI/Patches'
resource_dir='analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources'
run xcrun clang++ -std=c++17 -arch arm64 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer native/editor/browser_bank.cpp native/editor/browser_bank_tests.cpp -o native/editor/build/browser_bank_tests
run native/editor/build/browser_bank_tests "$bank_dir"
run xcrun clang++ -std=c++17 -arch arm64 -fobjc-arc -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer native/editor/browser_bank.cpp native/editor/browser.mm native/editor/browser_tests.mm -framework Cocoa -o native/editor/build/browser_tests
run native/editor/build/browser_tests "$resource_dir" "$bank_dir"
