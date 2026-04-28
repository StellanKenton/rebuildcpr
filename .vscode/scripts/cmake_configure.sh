#!/bin/zsh

set -euo pipefail

workspace="$(cd "$(dirname "$0")/../.." && pwd)"
python_user_bin="$HOME/Library/Python/3.9/bin"
tool_bin="$workspace/.tools/arm-gnu-toolchain/bin"
export PATH="$python_user_bin:/usr/local/bin:/opt/homebrew/bin:$tool_bin:$PATH"

source_dir="$1"
build_dir="$2"
generator="$3"
toolchain_file="$4"
arm_toolchain_bin_dir="$5"
build_type="$6"
export_compile_commands="$7"
shift 7

cache_file="$build_dir/CMakeCache.txt"
expected_compiler="$arm_toolchain_bin_dir/arm-none-eabi-gcc"

require_tool() {
  local tool="$1"

  if ! command -v "$tool" >/dev/null 2>&1; then
    print -ru2 -- "[cmake-configure] Required tool not found: $tool"
    print -ru2 -- "[cmake-configure] PATH=$PATH"
    exit 127
  fi
}

cache_value() {
  local key="$1"

  if [[ ! -f "$cache_file" ]]; then
    return 0
  fi

  sed -n "s|^${key}:[^=]*=||p" "$cache_file" | head -n 1
}

reset_build_dir() {
  print -r -- "[cmake-configure] Removing stale CMake cache from $build_dir"
  rm -f "$build_dir/CMakeCache.txt"
  rm -rf "$build_dir/CMakeFiles" "$build_dir/.cmake"
  rm -f "$build_dir/build.ninja" "$build_dir/cmake_install.cmake" "$build_dir/compile_commands.json"
}

mkdir -p "$build_dir"

require_tool cmake
require_tool ninja

if [[ ! -x "$expected_compiler" ]]; then
  print -ru2 -- "[cmake-configure] Arm compiler not found or not executable: $expected_compiler"
  exit 127
fi

if [[ -f "$cache_file" ]]; then
  cached_toolchain_file="$(cache_value CMAKE_TOOLCHAIN_FILE)"
  cached_arm_toolchain_bin_dir="$(cache_value ARM_GNU_TOOLCHAIN_BIN_DIR)"
  cached_c_compiler="$(cache_value CMAKE_C_COMPILER)"
  cached_asm_compiler="$(cache_value CMAKE_ASM_COMPILER)"

  if [[ "$cached_toolchain_file" != "$toolchain_file" ]] \
    || [[ "$cached_arm_toolchain_bin_dir" != "$arm_toolchain_bin_dir" ]] \
    || [[ "$cached_c_compiler" != "$expected_compiler" ]] \
    || [[ "$cached_asm_compiler" != "$expected_compiler" ]]; then
    reset_build_dir
  fi
fi

exec cmake \
  -S "$source_dir" \
  -B "$build_dir" \
  -G "$generator" \
  -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
  -DCMAKE_TOOLCHAIN_FILE=$toolchain_file \
  -DARM_GNU_TOOLCHAIN_BIN_DIR=$arm_toolchain_bin_dir \
  -DCMAKE_BUILD_TYPE=$build_type \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=$export_compile_commands \
  "$@"
