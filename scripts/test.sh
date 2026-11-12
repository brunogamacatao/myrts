#!/usr/bin/env bash
# Builds and runs the unit tests (no Allegro required).
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_vcpkg
cmake --preset tests-only
cmake --build --preset tests-only
ctest --preset tests-only
