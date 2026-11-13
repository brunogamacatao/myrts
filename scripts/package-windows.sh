#!/usr/bin/env bash
# Cross-compiles the game for Windows 10/11 64-bit (MinGW-w64 + static vcpkg) and zips it.
# Usage: scripts/package-windows.sh        -> dist/myrts-windows-x64.zip
# Needs (Debian/Ubuntu): sudo apt install g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix zip
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_vcpkg
for t in x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-gcc-posix x86_64-w64-mingw32-windres; do
  command -v "$t" >/dev/null || { echo "Missing $t (apt install g++-mingw-w64-x86-64-posix)" >&2; exit 1; }
done

BUILD="$ROOT/build/windows"
NAME=myrts-windows-x64
DIST="$ROOT/dist"
mkdir -p "$BUILD/tools"

# vcpkg's tool-copy step wants PowerShell to gather DLLs; the build is fully static, so a no-op stub is enough.
printf '#!/bin/sh\nexit 0\n' > "$BUILD/tools/pwsh"
chmod +x "$BUILD/tools/pwsh"
export PATH="$BUILD/tools:$PATH"

# Toolchain wrapper: our MinGW triplet (cmake/windows) on top of vcpkg's toolchain.
cat > "$BUILD/toolchain.cmake" <<EOT
set(VCPKG_OVERLAY_TRIPLETS "$ROOT/cmake/windows")
set(VCPKG_TARGET_TRIPLET x64-mingw-static-release)
set(VCPKG_HOST_TRIPLET x64-linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ROOT/cmake/windows/mingw-posix.cmake")
include("$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
EOT

cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMYRTS_BUILD_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE="$BUILD/toolchain.cmake" -DCMAKE_EXE_LINKER_FLAGS="-static -mwindows"
cmake --build "$BUILD"

rm -rf "$DIST/$NAME" "$DIST/$NAME.zip"
mkdir -p "$DIST/$NAME"
cp "$BUILD/myrts.exe" "$DIST/$NAME/"
cp -r assets/. "$DIST/$NAME/"
cp NOTICE.md "$DIST/$NAME/"
cat > "$DIST/$NAME/LEIAME.txt" <<'EOT'
myrts - Windows 10/11 (64 bits)

Como jogar: extraia TODO o conteudo do zip para uma pasta e de dois cliques em myrts.exe.
Nao precisa instalar nada (sem DLLs ou runtimes extras). Mantenha o myrts.exe junto das
pastas data, maps, music, sounds, sprites etc.: o jogo as carrega da pasta do executavel.

Controles: veja README.md do projeto. Esc cancela o comando / limpa a selecao / sai.
Creditos dos assets: credits.txt e NOTICE.md.
EOT
(cd "$DIST" && zip -qr "$NAME.zip" "$NAME")
echo "Done: $DIST/$NAME.zip ($(du -h "$DIST/$NAME.zip" | cut -f1))"
