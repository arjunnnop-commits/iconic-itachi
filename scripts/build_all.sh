#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/30.0.16248370}}"

echo "================================================================"
echo " Building POCO X6 Pro (duchamp) Hardware Compatibility Suite"
echo " Author: itachi duchamp"
echo "================================================================"

mkdir -p "$ROOT/out/system/bin" "$ROOT/out/vendor/lib64/hw" "$ROOT/out/vendor/etc/init" "$ROOT/out/system/etc/init"

# 1. Build FOD Shim
echo "-> [1/3] Building itachi_fp_duchamp.so..."
cmake -S "$ROOT/src/fod" -B "$ROOT/src/fod/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35 -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/src/fod/build" -j"$(nproc)"
cp "$ROOT/src/fod/build/libitachi_fp_duchamp.so" "$ROOT/out/vendor/lib64/"
cp "$ROOT/src/fod/build/itachi_icon_init" "$ROOT/out/system/bin/"

# 2. Build Sensor HAL
echo "-> [2/3] Building sensors.mt6897.so..."
bash "$ROOT/src/sensor/build.sh"
cp "$ROOT/src/sensor/build/sensors.mt6897.so" "$ROOT/out/vendor/lib64/hw/"

# 3. Build AOD Daemon
echo "-> [3/3] Building itachi_aodd..."
LINKER="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$LINKER"
export CC_aarch64_linux_android="$LINKER"
cd "$ROOT/src/aod"
cargo build --release --target aarch64-linux-android
cd "$ROOT"
cp "$ROOT/src/aod/target/aarch64-linux-android/release/itachi_aodd" "$ROOT/out/system/bin/"

# Copy inits
cp "$ROOT/src/fod/itachi_fp_duchamp.rc" "$ROOT/out/vendor/etc/init/"
cp "$ROOT/src/aod/itachi_aodd.rc" "$ROOT/out/system/etc/init/"

echo "================================================================"
echo " [SUCCESS] All binaries compiled successfully in out/"
echo "================================================================"
ls -lR "$ROOT/out"
