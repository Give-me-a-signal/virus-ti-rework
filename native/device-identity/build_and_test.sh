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
for profile_variant in normal sanitized; do
 profile_flags=""
 if [ "$profile_variant" = sanitized ]; then profile_flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
 # The existing dense parser uses adjacent one-line statements; keep the
 # remaining warnings fatal without treating its indentation as a new failure.
 "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation $profile_flags -I"$identity_dir/../editor" \
  "$identity_dir/../vst3/device_profile_tests.cpp" "$identity_dir/../editor/browser_bank.cpp" \
  -o "$build_dir/device_profile_test_$profile_variant"
 "$build_dir/device_profile_test_$profile_variant"
done
