# Shared helpers (sourced by the other scripts).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

require_vcpkg() {
  require_tools
  if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "vcpkg not found at VCPKG_ROOT=$VCPKG_ROOT. Run scripts/setup.sh first." >&2
    exit 1
  fi
}

# Fails early (before a long vcpkg build) if system tools are missing.
require_tools() {
  local missing=() t
  for t in cmake ninja pkg-config git curl zip unzip tar autoconf automake libtoolize; do
    command -v "$t" >/dev/null || missing+=("$t")
  done
  if ((${#missing[@]})); then
    echo "Missing tools: ${missing[*]}" >&2
    echo "Run scripts/setup.sh, or on Debian/Ubuntu:" >&2
    echo "  sudo apt install cmake ninja-build pkg-config git curl zip unzip tar autoconf automake libtool" >&2
    exit 1
  fi
}
