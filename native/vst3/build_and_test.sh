#!/bin/bash
# Compatibility entry point for the resource-independent public beta.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
exec bash "$root/tools/build_beta.sh"
