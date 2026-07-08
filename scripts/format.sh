#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_root"
command -v clang-format >/dev/null || { echo 'clang-format is required' >&2; exit 1; }
mode=(-i)
if [[ ${1:-} == --check ]]; then mode=(--dry-run --Werror); fi
find common server client tests tools/scheduler -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.c' \) -print0 |
    xargs -0 clang-format "${mode[@]}"
