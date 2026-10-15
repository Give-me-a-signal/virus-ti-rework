#!/bin/sh
set -eu
identity_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir="$identity_dir/build"
mkdir -p "$build_dir"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$identity_dir/include" \
 "$identity_dir/tests/device_identity_test.cpp" -o "$build_dir/device_identity_test"
"$build_dir/device_identity_test"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -fno-omit-frame-pointer -I"$identity_dir/include" \
 "$identity_dir/tests/device_identity_test.cpp" -o "$build_dir/device_identity_test_sanitized"
"$build_dir/device_identity_test_sanitized"
