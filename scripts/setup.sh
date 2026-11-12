#!/usr/bin/env bash
# One-time setup: system packages (Debian/Ubuntu) and vcpkg.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if command -v apt-get >/dev/null; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build pkg-config git curl zip unzip tar \
    autoconf automake libtool libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
    libxinerama-dev libgl-dev libglu1-mesa-dev libasound2-dev
else
  echo "Non-apt system: install the packages listed in README.md manually." >&2
fi

if [[ ! -d "$VCPKG_ROOT" ]]; then
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi
[[ -x "$VCPKG_ROOT/vcpkg" ]] || "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

echo "Done. Add to your shell profile:  export VCPKG_ROOT=$VCPKG_ROOT"
