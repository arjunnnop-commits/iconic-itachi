/*
 * Copyright (C) 2026 itachi duchamp
 * Native AOD Backlight & Display Coordination Daemon
 * Target: POCO X6 Pro (duchamp / n11a / MT6897)
 */

use std::{
    fs::{self, OpenOptions},
    io::{BufRead, BufReader, Write},
    path::Path,
    process::{Command, Stdio},
    thread,
    time::Duration,
};

// Verified backlight nodes for POCO X6 Pro Dimensity 8300
const BACKLIGHT_NODES: &[&str] = &[
    "/sys/class/leds/lcd-backlight/brightness",
    "/sys/class/mi_display/disp_feature/disp-DSI-0/brightness_clone",
    "/sys/devices/platform/soc/soc:mtk-leds/leds/lcd-backlight/brightness",
    "/sys/class/mi_display/disp_feature/disp-DSI-0/doze_brightness",
];

// Calibrated 14-bit (16383 max) levels
const AOD_BRIGHTNESS: u32 = 380;       // Smooth AOD clock
const FINGERPRINT_BRIGHTNESS: u32 = 1413; // Clear icon readability

fn log_info(msg: &str) {
    let _ = Command::new("/system/bin/log")
        .args(["-t", "ItachiAodD", msg])
        .status();
    println!("ItachiAodD: {msg}");
}

fn get_active_backlight_node() -> &'static str {
    for node in BACKLIGHT_NODES {
        if Path::new(node).exists() {
            return node;
        }
    }
    BACKLIGHT_NODES[0]
}

fn set_backlight(value: u32) {
    let node = get_active_backlight_node();
    if let Ok(mut f) = OpenOptions::new().write(true).open(node) {
        let _ = write!(f, "{value}");
    }
}

fn read_backlight() -> Option<u32> {
    let node = get_active_backlight_node();
    fs::read_to_string(node)
        .ok()?
        .trim()
        .parse::<u32>()
        .ok()
}

fn main() {
    log_info("Starting Itachi Duchamp AOD & SOFOD Daemon for POCO X6 Pro");

    // Wait until display sysfs node is initialized by kernel
    while !BACKLIGHT_NODES.iter().any(|p| Path::new(p).exists()) {
        thread::sleep(Duration::from_millis(500));
    }

    log_info(&format!("Active display node: {}", get_active_backlight_node()));

    loop {
        let mut child = match Command::new("/system/bin/logcat")
            .args([
                "-b", "events", "-b", "main", "-b", "system",
                "-v", "brief",
                "-s",
                "DisplayPowerController",
                "ColorAod",
                "OnScreenFingerprintIcon",
                "ItachiFpDuchamp",
                "ItachiSensorDuchamp",
            ])
            .stdout(Stdio::piped())
            .spawn()
        {
            Ok(c) => c,
            Err(e) => {
                log_info(&format!("Failed to spawn logcat: {e}"));
                thread::sleep(Duration::from_secs(2));
                continue;
            }
        };

        let stdout = match child.stdout.take() {
            Some(s) => s,
            None => {
                let _ = child.kill();
                thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        let reader = BufReader::new(stdout);

        for line_result in reader.lines() {
            let line = match line_result {
                Ok(l) => l,
                Err(_) => break,
            };

            // 1. DOZE ENTRY (AOD clock displayed)
            if line.contains("setScreenState changed:") && line.contains("->DOZE") {
                thread::sleep(Duration::from_millis(150));
                set_backlight(AOD_BRIGHTNESS);
                log_info("DOZE entry -> Set AOD backlight (380)");
            }

            // 2. AOD KEEPALIVE
            if line.contains("Final-state=DOZE_SUSPEND") || line.contains("performAodUpdate") {
                if read_backlight() == Some(0) {
                    set_backlight(AOD_BRIGHTNESS);
                }
            }

            // 3. FINGERPRINT ICON DISPLAYED
            if line.contains("OnScreenFingerprintIcon") {
                if line.contains("setVisibility VISIBLE") {
                    set_backlight(FINGERPRINT_BRIGHTNESS);
                    log_info("Fingerprint icon visible -> Boost icon backlight (1413)");
                } else if line.contains("setVisibility INVISIBLE") {
                    set_backlight(AOD_BRIGHTNESS);
                }
            }

            // 4. SCREEN WAKE TO ON (Unlocked or User Pressed Power Button)
            if line.contains("setScreenState changed:") && line.contains("->ON") {
                log_info("Screen interactive (ON) -> Release display override");
            }
        }

        thread::sleep(Duration::from_millis(500));
    }
}
