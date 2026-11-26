/*
 * Copyright (C) 2026 itachi duchamp
 * Native AOD Backlight & FOD Coordination Daemon
 * Target: POCO X6 Pro (duchamp / n11a / MT6897)
 */

use std::{
    fs::{self, File, OpenOptions},
    io::{BufRead, BufReader, Read, Write},
    path::Path,
    process::{Command, Stdio},
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

// Hardware sysfs nodes for POCO X6 Pro Dimensity 8300
const BACKLIGHT_NODES: &[&str] = &[
    "/sys/class/leds/lcd-backlight/brightness",
    "/sys/class/mi_display/disp_feature/disp-DSI-0/brightness_clone",
    "/sys/devices/platform/soc/soc:mtk-leds/leds/lcd-backlight/brightness",
    "/sys/class/mi_display/disp_feature/disp-DSI-0/doze_brightness",
];

const DISP_PARAM_NODE: &str = "/sys/class/mi_display/disp_feature/disp-DSI-0/disp_param";
const GOODIX_FOD_NODE: &str = "/sys/devices/platform/goodix_ts.0/fod_enable";

// Calibrated 14-bit (16383 max) levels
const AOD_BRIGHTNESS: u32 = 380;          // Smooth AOD clock
const FINGERPRINT_BRIGHTNESS: u32 = 1413; // Clear icon readability

const FOD_GESTURE_KEY: u16 = 0x152; // 338
const EV_KEY: u16 = 0x01;

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

// Write to Xiaomi display feature disp_param node
fn set_lhbm_spotlight(enable: bool) {
    if let Ok(mut f) = OpenOptions::new().write(true).open(DISP_PARAM_NODE) {
        if enable {
            let _ = f.write_all(b"09 1\n");
            let _ = f.write_all(b"02 1\n");
        } else {
            let _ = f.write_all(b"09 0\n");
            let _ = f.write_all(b"02 0\n");
        }
    }
}

fn set_fod_enable(enable: bool) {
    if let Ok(mut f) = OpenOptions::new().write(true).open(GOODIX_FOD_NODE) {
        let val = if enable { b"1" } else { b"0" };
        let _ = f.write_all(val);
    }
}

fn set_xiaomi_touch_mode(enable: bool) {
    let val = if enable { "1" } else { "0" };
    let _ = Command::new("/system/bin/service")
        .args([
            "call",
            "vendor.xiaomi.hw.touchfeature.ITouchFeature/default",
            "9",
            "i32", "0",
            "i32", "10",
            "i32", val,
        ])
        .status();
}

// Find Goodix touch event file
fn find_goodix_input_device() -> Option<String> {
    // Check event5 first
    if Path::new("/dev/input/event5").exists() {
        return Some("/dev/input/event5".to_string());
    }

    for i in 0..20 {
        let path = format!("/dev/input/event{i}");
        if Path::new(&path).exists() {
            return Some(path);
        }
    }
    None
}

// Low-latency hardware touch monitoring thread running as root
fn start_hardware_touch_monitor(active_flag: Arc<AtomicBool>) {
    thread::spawn(move || {
        log_info("Starting hardware FOD touch thread in itachi_aodd (root)");

        loop {
            let dev_path = match find_goodix_input_device() {
                Some(p) => p,
                None => {
                    thread::sleep(Duration::from_millis(500));
                    continue;
                }
            };

            let mut file = match File::open(&dev_path) {
                Ok(f) => f,
                Err(_) => {
                    thread::sleep(Duration::from_millis(500));
                    continue;
                }
            };

            // Linux input_event struct: 16 or 24 bytes depending on arch
            // timeval (16 bytes on 64-bit), type (u16), code (u16), value (i32) -> total 24 bytes
            let mut buf = [0u8; 24];

            while file.read_exact(&mut buf).is_ok() {
                let ev_type = u16::from_ne_bytes([buf[16], buf[17]]);
                let ev_code = u16::from_ne_bytes([buf[18], buf[19]]);
                let ev_value = i32::from_ne_bytes([buf[20], buf[21], buf[22], buf[23]]);

                if ev_type == EV_KEY && ev_code == FOD_GESTURE_KEY {
                    if ev_value == 1 {
                        // Touch DOWN -> Turn ON LHBM optical spotlight
                        active_flag.store(true, Ordering::SeqCst);
                        set_lhbm_spotlight(true);
                        set_fod_enable(true);
                        set_xiaomi_touch_mode(true);
                        log_info("Root FOD Touch DOWN -> Spotlight ON (09 1, 02 1)");
                    } else if ev_value == 0 {
                        // Touch UP -> Turn OFF LHBM optical spotlight immediately
                        active_flag.store(false, Ordering::SeqCst);
                        set_lhbm_spotlight(false);
                        set_fod_enable(false);
                        set_xiaomi_touch_mode(false);
                        log_info("Root FOD Touch UP -> Spotlight OFF (09 0, 02 0)");
                    }
                }
            }

            thread::sleep(Duration::from_millis(200));
        }
    });
}

fn main() {
    log_info("Starting Itachi Duchamp AOD & SOFOD Daemon for POCO X6 Pro");

    // Wait until display sysfs node is initialized by kernel
    while !BACKLIGHT_NODES.iter().any(|p| Path::new(p).exists()) {
        thread::sleep(Duration::from_millis(500));
    }

    log_info(&format!("Active display node: {}", get_active_backlight_node()));

    let touch_active = Arc::new(AtomicBool::new(false));
    start_hardware_touch_monitor(touch_active.clone());

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
                "Biometrics/EnrollClient",
                "FingerprintService",
                "mfp-daemon",
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

            // 5. FINGER DOWN EVENT FROM MFP-DAEMON OR DRIVER
            if line.contains("SYS_NODE_EVENT_FINGER_DOWN")
                || line.contains("detected finger down")
            {
                set_lhbm_spotlight(true);
                set_fod_enable(true);
            }

            // 6. FINGER UP OR AUTH FINISHED -> KILL SPOTLIGHT WITH ZERO DELAY
            if line.contains("SYS_NODE_EVENT_FINGER_UP")
                || line.contains("onAcquired(7)")
                || line.contains("onAuthenticated")
                || line.contains("onError")
            {
                set_lhbm_spotlight(false);
            }
        }

        thread::sleep(Duration::from_millis(500));
    }
}
