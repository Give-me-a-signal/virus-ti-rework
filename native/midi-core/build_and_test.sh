#!/bin/sh
set -eu
core_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${MIDI_BUILD_DIR:-"$core_dir/build"}
compiler=${CXX:-clang++}
mkdir -p "$build_dir"
arch_flags=""
python_bin=${PYTHON:-python3}
if [ "$(uname -s)" = Darwin ]; then
    arch_flags="-arch arm64"
    python_bin=${PYTHON:-/usr/bin/python3}
fi
"$python_bin" "$core_dir/scripts/verify_xml_templates.py"
for test_name in midi_core_test usb_midi1_test; do
    "$compiler" -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -O1 \
        $arch_flags -I"$core_dir/include" "$core_dir/tests/$test_name.cpp" \
        -o "$build_dir/$test_name"
    "$build_dir/$test_name"
    "$compiler" -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -O1 \
        $arch_flags -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I"$core_dir/include" "$core_dir/tests/$test_name.cpp" \
        -o "$build_dir/${test_name}_sanitized"
    "$build_dir/${test_name}_sanitized"
done
