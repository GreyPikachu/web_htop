#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output_dir=${1:-"$project_root/build-bpf"}
for command in clang bpftool pkg-config c++; do
    command -v "$command" >/dev/null || { echo "Missing dependency: $command" >&2; exit 1; }
done
pkg-config --exists libbpf || { echo 'Install libbpf development headers.' >&2; exit 1; }
[[ -r /sys/kernel/btf/vmlinux ]] || { echo 'Kernel BTF is unavailable.' >&2; exit 1; }
case $(uname -m) in
    x86_64) architecture=x86 ;;
    aarch64) architecture=arm64 ;;
    *) echo 'Supported BPF build architectures: x86_64, aarch64.' >&2; exit 1 ;;
esac
mkdir -p -- "$output_dir"
bpftool btf dump file /sys/kernel/btf/vmlinux format c > "$output_dir/vmlinux.h"
read -r -a bpf_cflags <<< "$(pkg-config --cflags libbpf)"
read -r -a bpf_libs <<< "$(pkg-config --libs libbpf)"
clang -g -O2 -target bpf -D"__TARGET_ARCH_$architecture" -I "$output_dir" "${bpf_cflags[@]}" \
    -c "$project_root/tools/scheduler/runqlat.bpf.c" -o "$output_dir/runqlat.bpf.o"
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic "${bpf_cflags[@]}" \
    "$project_root/tools/scheduler/main.cpp" -o "$output_dir/web_htop_sched" "${bpf_libs[@]}" -pthread
printf 'Built profiler: %s/web_htop_sched\n' "$output_dir"
