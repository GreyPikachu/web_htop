#!/usr/bin/env bash
set -Eeuo pipefail

readonly source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly build_dir="${WEB_HTOP_HARDENED_BUILD_DIR:-$source_dir/build-hardened}"

cmake \
    -S "$source_dir" \
    -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWEB_HTOP_BUILD_APPS=ON \
    -DWEB_HTOP_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=3 -fPIE" \
    -DCMAKE_EXE_LINKER_FLAGS="-pie -Wl,-z,relro,-z,now,-z,noexecstack"

cmake --build "$build_dir" --parallel "${WEB_HTOP_BUILD_JOBS:-2}"
ctest --test-dir "$build_dir" --output-on-failure --no-tests=error
