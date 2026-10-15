#!/bin/bash
# Developer build entry point; end users should download the graphical beta.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || { echo 'Apple Silicon macOS is required.' >&2; exit 1; }
install=false
for arg in "$@"; do
 if [[ "$arg" == --install ]]; then install=true
 elif [[ "$arg" == *.pkg && -f "$arg" ]]; then echo 'An Access package is optional; import its appearance later from Setup.'
 else echo 'Usage: bash setup.sh [legacy-Access-package.pkg] [--install]' >&2; exit 2
 fi
done
bash "$root/tools/build_beta.sh"
if $install; then "$root/dist/Virus-TI-Rework-0.2.0-beta.1-macOS-arm64/Virus TI Rework Setup.app/Contents/MacOS/Virus TI Rework Setup" --install; fi
