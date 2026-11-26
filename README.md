# POCO X6 Pro (duchamp) Hardware Compatibility Suite

[![Device](https://img.shields.io/badge/Device-POCO%20X6%20Pro%20(duchamp)-blue.svg)]()
[![Platform](https://img.shields.io/badge/SoC-Dimensity%208300--Ultra%20(MT6897)-orange.svg)]()
[![Author](https://img.shields.io/badge/Author-itachi%20duchamp-red.svg)]()
[![License](https://img.shields.io/badge/License-Apache%202.0-brightgreen.svg)](LICENSE)

Native hardware compatibility layer for **POCO X6 Pro (duchamp / n11a / MediaTek Dimensity 8300-Ultra)** running ColorOS / OxygenOS ports.

Author: **itachi duchamp**

---

## ⚡ Included Features & Optimizations

1. **Ultra-Fast FOD (Zero Polling Latency)**:
   - Direct `/dev/input/event5` (`goodix_ts`) hardware interrupt listener.
   - Triggers CPU governor boost to max frequency upon physical touch (<1ms).
   - Unlocks in ~0.1s with zero lockscreen dimming lag.
2. **Instant LHBM Spotlight Turn-Off**:
   - Immediately disarms Xiaomi `ITouchFeature` Mode 10 upon biometric match.
   - Eliminates persistent green/white optical glare after finger release.
3. **Double Tap to Wake (DT2W)**:
   - Bridges ColorOS setting toggle (`oplus_customize_gesture_double_touch`) to Xiaomi TouchFeature Mode 14 (`TOUCH_DOUBLE_TAP`).
4. **Proximity Sensor Fix**:
   - Routes Elliptic Labs Ultrasound sensor events (`0x03000000`) for reliable call screen-off.
5. **AOD & SOFOD Backlight Coordination (`itachi_aodd`)**:
   - Calibrated 14-bit backlight writing (Level 380 in DOZE, Level 1413 in fingerprint hint).
   - Zero screen-off flicker.
6. **Rise to Wake (`sensors.mt6897.so`)**:
   - Bridges Xiaomi pickup sensor (`0x19`, type `33171036`) to ColorOS tilt detector (`22`).

---

## 📁 Repository Structure

```text
├── src/
│   ├── fod/              # itachi_fp_duchamp.so (Fast FOD & LHBM Turn-Off)
│   ├── sensor/           # sensors.mt6897.so (MediaTek Sensor HAL Wrapper)
│   ├── aod/              # itachi_aodd (Rust Backlight & AOD Service)
│   ├── gestures/         # DuchampTouchBridge.java & patch_dt2w.py (DT2W)
│   └── proximity/        # patch_proximity.py (Ultrasound Call Sensor Fix)
├── overlay/              # DuchampDozeOverlay.apk (Framework Doze RRO)
├── sepolicy/             # Strict Enforcing SELinux CIL & file_contexts
├── scripts/              # build_all.sh compilation script
├── bake_into_rom.sh      # One-click ROM Bake-In installer
└── .github/workflows/    # Automated GitHub Actions Cloud Build
```

---

## 🚀 How to Build

### Option 1: Automated Cloud Build via GitHub Actions (Recommended)
1. Push this repository to your GitHub:
   ```bash
   git init
   git add .
   git commit -m "Initial commit by itachi duchamp"
   git branch -M main
   git remote add origin https://github.com/<YOUR_USERNAME>/<REPO_NAME>.git
   git push -u origin main
   ```
2. Go to the **Actions** tab on your GitHub repository.
3. Download the compiled `itachi-duchamp-poco-x6-pro-suite.zip` ready to bake!

### Option 2: Local Build (Linux / WSL)
Requirements: Android NDK r27c+, Rust with `aarch64-linux-android` target, CMake, Ninja.

```bash
chmod +x scripts/build_all.sh
./scripts/build_all.sh
```

---

## 📲 How to Bake into ROM (Rootless / Built-in)

Unpack your target ColorOS ROM and run:
```bash
chmod +x bake_into_rom.sh
./bake_into_rom.sh /path/to/unpacked_rom
```

This injects all binaries into `/system/bin`, `/vendor/lib64`, and `/vendor/lib64/hw`, patches the `mfp-daemon` ELF header with `patchelf`, appends strict SELinux rules, and adds fast unlock properties into `build.prop`.

---

## 📜 License
Copyright (C) 2026 itachi duchamp. Licensed under the Apache License, Version 2.0.
