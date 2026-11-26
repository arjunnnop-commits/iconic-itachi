#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/30.0.16248370}}"

CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
if [ ! -f "$CC" ]; then
    CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang"
fi

mkdir -p "$ROOT/build"

"$CC" -shared -fPIC -O2 -Wall -Wextra \
  -Wl,-soname,sensors.mt6897.so \
  "$ROOT/sensors_itachi_duchamp.c" \
  -o "$ROOT/build/sensors.mt6897.so" \
  -llog -ldl -pthread

echo "Built sensors.mt6897.so"
