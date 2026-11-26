#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "================================================================"
  echo " POCO X6 Pro (duchamp) ROM Bake-In Installer"
  echo " Author: itachi duchamp"
  echo "================================================================"
  echo "Usage: $0 <path_to_unpacked_rom_directory>"
  echo "Example: $0 /home/user/rom_unpacked"
  exit 1
fi

ROM_ROOT="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Detect system partition
SYS_DIR=""
if [ -d "$ROM_ROOT/system/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system/system"
elif [ -d "$ROM_ROOT/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system"
else
  echo "Error: Cannot locate system partition in $ROM_ROOT"
  exit 1
fi

# Detect vendor partition
VEN_DIR=""
if [ -d "$ROM_ROOT/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/vendor"
elif [ -d "$ROM_ROOT/system/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/system/vendor"
else
  echo "Error: Cannot locate vendor partition in $ROM_ROOT"
  exit 1
fi

echo "================================================================"
echo " Baking POCO X6 Pro (duchamp) Fixes into ROM"
echo " Author: itachi duchamp"
echo "================================================================"
echo "System Root: $SYS_DIR"
echo "Vendor Root: $VEN_DIR"
echo ""

OUT="$SCRIPT_DIR/out"

# 1. Install AOD Daemon & Icon Helper
echo "-> [1/5] Installing itachi_aodd to $SYS_DIR/bin/"
mkdir -p "$SYS_DIR/bin" "$SYS_DIR/etc/init"
install -m 0755 "$OUT/system/bin/itachi_aodd" "$SYS_DIR/bin/itachi_aodd"
install -m 0644 "$SCRIPT_DIR/src/aod/itachi_aodd.rc" "$SYS_DIR/etc/init/itachi_aodd.rc"

# 2. Install FOD Shim & Sensor HAL
echo "-> [2/5] Installing libitachi_fp_duchamp.so and sensors.mt6897.so to $VEN_DIR/lib64/"
mkdir -p "$VEN_DIR/lib64/hw" "$VEN_DIR/etc/init"
install -m 0644 "$OUT/vendor/lib64/libitachi_fp_duchamp.so" "$VEN_DIR/lib64/libitachi_fp_duchamp.so"
install -m 0644 "$OUT/vendor/lib64/hw/sensors.mt6897.so" "$VEN_DIR/lib64/hw/sensors.mt6897.so"
install -m 0644 "$SCRIPT_DIR/src/fod/itachi_fp_duchamp.rc" "$VEN_DIR/etc/init/itachi_fp_duchamp.rc"

# 3. Patch mfp-daemon with patchelf (Permanent Hook)
echo "-> [3/5] Baking libitachi_fp_duchamp.so into mfp-daemon ELF header..."
if [ -f "$VEN_DIR/bin/hw/mfp-daemon" ]; then
  if which patchelf >/dev/null 2>&1; then
    patchelf --add-needed libitachi_fp_duchamp.so "$VEN_DIR/bin/hw/mfp-daemon" 2>/dev/null || true
    chmod 755 "$VEN_DIR/bin/hw/mfp-daemon"
    echo "   [OK] Successfully patched mfp-daemon ELF header"
  else
    echo "   [WARN] patchelf not found on host system!"
  fi
fi

# 4. Inject SELinux Rules
echo "-> [4/5] Injecting Strict SELinux CIL rules into vendor_sepolicy.cil..."
CIL_TARGET="$VEN_DIR/etc/selinux/vendor_sepolicy.cil"
if [ -f "$CIL_TARGET" ]; then
  cat "$SCRIPT_DIR/sepolicy/vendor_sepolicy.cil.append" >> "$CIL_TARGET"
  echo "   [OK] Appended rules to vendor_sepolicy.cil"
fi

# Remove precompiled sepolicy blobs to force dynamic recompile
for DIR in "$ROM_ROOT/odm/etc/selinux" "$VEN_DIR/etc/selinux"; do
  if [ -d "$DIR" ]; then
    find "$DIR" -maxdepth 1 -name "precompiled_sepolicy*" -exec rm -f {} + 2>/dev/null || true
  fi
done

# 5. Append Fast Unlock Properties
echo "-> [5/5] Appending performance and fast unlock properties to build.prop..."
cat << 'EOF' >> "$SYS_DIR/build.prop"

# POCO X6 Pro (duchamp) Fast Unlock & Hardware Optimizations - itachi duchamp
persist.vendor.sys.fp.fod.optimize=true
persist.vendor.sys.fp.fod.lhbmoffafterresult=1
persist.sys.fp.fod.screenoff=true
persist.vendor.fingerprint.fod.enable=true
ro.surface_flinger.set_touch_timer_ms=0
ro.hardware.fp.fod=true
EOF

echo "================================================================"
echo " [SUCCESS] All POCO X6 Pro fixes baked cleanly into ROM!"
echo " Author: itachi duchamp"
echo "================================================================"
