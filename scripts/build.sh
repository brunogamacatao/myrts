#!/usr/bin/env bash
# Usage: scripts/build.sh [release|debug|tests-only|debug-system]   (default: release)
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
preset="${1:-release}"
[[ "$preset" == *-system ]] || require_vcpkg
cmake --preset "$preset"
cmake --build --preset "$preset"
