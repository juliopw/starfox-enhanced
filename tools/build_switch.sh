#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$project_root/build/switch"

: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro installation directory}"
if [[ ! -f "$DEVKITPRO/cmake/Switch.cmake" ]]; then
    echo "switch-cmake is not installed; install the devkitPro switch-dev group" >&2
    exit 1
fi

git -C "$project_root" submodule update --init \
    third_party/switch/libnx third_party/switch/SDL
make -C "$project_root/third_party/switch/libnx/nx" lib/libnx.a

cmake -S "$project_root" -B "$build_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$project_root/cmake/toolchains/NintendoSwitch.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    "$@"
cmake --build "$build_dir" --target starfox_switch_package --parallel
