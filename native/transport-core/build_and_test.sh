#!/bin/sh
set -eu
core_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${TRANSPORT_BUILD_DIR:-"$core_dir/build"}
compiler=${CXX:-clang++}
mkdir -p "$build_dir"
arch_flags=""
if [ "$(uname -s)" = Darwin ]; then
    arch_flags="-arch arm64"
fi
# CXX selects the compiler executable; intentionally does not eval shell text.
"$compiler" -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -g -O1 \
    $arch_flags -I"$core_dir/include" "$core_dir/src/transport_core.cpp" \
    "$core_dir/tests/transport_core_test.cpp" -o "$build_dir/transport_core_test"
"$build_dir/transport_core_test"
if "$compiler" -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -g -O1 \
    $arch_flags -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$core_dir/include" "$core_dir/src/transport_core.cpp" \
    "$core_dir/tests/transport_core_test.cpp" -o "$build_dir/transport_core_test_sanitized"; then
    "$build_dir/transport_core_test_sanitized"
else
    echo "Sanitizer build unavailable; unsanitized tests passed." >&2
fi
