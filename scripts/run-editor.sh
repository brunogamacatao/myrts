#!/usr/bin/env bash
# Usage: scripts/run-editor.sh [release|debug|debug-system] [game-folder]   (builds first)
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
preset="${1:-release}"
"$ROOT/scripts/build.sh" "$preset"
cd "build/$preset"   # the games and templates are copied next to the executables
exec ./myrts_engine "${2:-games/warcraft-like}"
