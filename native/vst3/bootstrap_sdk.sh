#!/bin/bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
sdk="$here/../../third_party/vst3sdk"
revision=3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
if [ ! -d "$sdk" ]; then
 git clone --no-checkout https://github.com/steinbergmedia/vst3sdk.git "$sdk"
fi
if [ -n "$(git -C "$sdk" status --porcelain)" ]; then
 echo "Refusing to change SDK with local modifications" >&2; exit 1
fi
git -C "$sdk" checkout --detach "$revision"
git -C "$sdk" submodule update --init base cmake pluginterfaces public.sdk
git -C "$sdk" rev-parse HEAD
git -C "$sdk" submodule status
