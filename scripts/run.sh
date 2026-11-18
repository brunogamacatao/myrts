#!/usr/bin/env bash
# Usage: scripts/run.sh [release|debug|debug-system]   (builds first)
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
preset="${1:-release}"
"$ROOT/scripts/build.sh" "$preset"
cd "build/$preset"   # the game folder (games/warcraft-like) is copied next to the executable
exec ./myrts
