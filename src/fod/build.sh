#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/30.0.16248370}}"

BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$BIN/clang"
CXX="$BIN/clang++"
TARGET="aarch64-linux-android34"

echo "Using NDK: $NDK"
mkdir -p "$ROOT/build"

# 1. Compile libitachi_fp_duchamp.so
echo "Building libitachi_fp_duchamp.so..."
$CXX --target=$TARGET -shared -fPIC -O2 -std=c++20 \
  -D__ANDROID_API__=34 \
  -Wl,-soname,libitachi_fp_duchamp.so \
  -Wl,-z,max-page-size=16384 \
  "$ROOT/itachi_fp_duchamp.cpp" \
  -o "$ROOT/build/libitachi_fp_duchamp.so" \
  -lbinder_ndk -llog -ldl

# 2. Compile itachi_icon_init executable
echo "Building itachi_icon_init..."
$CC --target=$TARGET -fPIE -pie -O2 \
  -D__ANDROID_API__=34 \
  -Wl,-z,max-page-size=16384 \
  "$ROOT/itachi_icon_init.c" \
  -o "$ROOT/build/itachi_icon_init" \
  -llog

echo "Built FOD Shim & Icon Init successfully"
