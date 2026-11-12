# Shared helpers (sourced by the other scripts).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

require_vcpkg() {
  if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "vcpkg not found at VCPKG_ROOT=$VCPKG_ROOT. Run scripts/setup.sh first." >&2
    exit 1
  fi
}
