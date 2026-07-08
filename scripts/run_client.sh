#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${WEB_HTOP_BUILD_DIR:-"$project_root/build"}
exec "$build_dir/client/web_htop_client" "$@"
