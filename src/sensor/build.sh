#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/30.0.16248370}}"

BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$BIN/clang"
TARGET="aarch64-linux-android34"

mkdir -p "$ROOT/build"

$CC --target=$TARGET -shared -fPIC -O2 -Wall -Wextra \
  -D__ANDROID_API__=34 \
  -Wl,-soname,sensors.mt6897.so \
  -Wl,-z,max-page-size=16384 \
  "$ROOT/sensors_itachi_duchamp.c" \
  -o "$ROOT/build/sensors.mt6897.so" \
  -llog -ldl

echo "Built sensors.mt6897.so successfully"
