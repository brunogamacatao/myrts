#!/usr/bin/env bash
# Generates the API reference with Doxygen into build/docs/html (open index.html).
# Class/collaboration/include diagrams are added automatically when Graphviz (`dot`) is installed.
#   sudo apt install doxygen graphviz
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

if ! command -v doxygen >/dev/null; then
  echo "doxygen not found: sudo apt install doxygen graphviz" >&2
  exit 1
fi

have_dot=NO
command -v dot >/dev/null && have_dot=YES
[[ $have_dot == NO ]] && echo "note: Graphviz not found, building without diagrams" >&2

mkdir -p build/docs
{ cat docs/Doxyfile; echo "HAVE_DOT = $have_dot"; } | doxygen -
echo "Documentation: build/docs/html/index.html"
