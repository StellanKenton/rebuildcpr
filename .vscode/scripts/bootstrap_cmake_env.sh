#!/bin/zsh

set -euo pipefail

workspace="$1"
build_dir="$2"
generator="$3"
toolchain_file="$4"
arm_toolchain_bin_dir="$5"
build_type="$6"
export_compile_commands="$7"
shift 7

"$workspace/.vscode/scripts/cmake_configure.sh" \
  "$workspace" \
  "$build_dir" \
  "$generator" \
  "$toolchain_file" \
  "$arm_toolchain_bin_dir" \
  "$build_type" \
  "$export_compile_commands" \
  "$@"

exec cmake --build "$build_dir" --parallel
