#!/usr/bin/env bash

set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
build="$root/build"
[[ "${1:-}" == "--clean" ]] && rm -rf "$build"

cmake_bin="$(command -v cmake || true)"
if [[ -z "$cmake_bin" ]]; then
    echo "cmake is not installed"
    exit 1
fi

"$cmake_bin" -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake"
"$cmake_bin" --build "$build"
echo "built: $root/build/bin/ets2_161_cabin_sound.dll"
