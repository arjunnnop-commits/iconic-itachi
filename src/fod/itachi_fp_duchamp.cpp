/*
 * Copyright (C) 2026 itachi duchamp
 * Ultra-Fast Hardware FOD & LHBM Compatibility Shim
 * Target: POCO X6 Pro (duchamp / n11a / MT6897)
 */

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <android/binder_parcel.h>
#include <android/binder_manager.h>
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <atomic>
#include <thread>
#include <chrono>

#define LOG_TAG "ItachiFpDuchamp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static constexpr const char* TOUCH_FEATURE_SERVICE = 
    "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";

static constexpr const char* GOODIX_FOD_NODE = 
    "/sys/devices/platform/goodix_ts.0/fod_enable";

static constexpr const char* CPU_BOOST_NODE = 
    "/sys/devices/system/cpu/cpufreq/policy4/scaling_min_freq";

static constexpr int FOD_GESTURE_KEY = 0x152;

static std::atomic<bool> g_authenticated(false);
static std::atomic<bool> g_finger_down(false);

// Trigger Xiaomi TouchFeature (Mode 10 = FOD Spotlight Area)
static bool setXiaomiTouchFeature(int32_t mode, int32_t value) {
    AIBinder* service = AServiceManager_checkService(TOUCH_FEATURE_SERVICE);
    if (!service) {
        LOGE("ITouchFeature service not found");
        return false;
    }

    AParcel* in = nullptr;
    AParcel* out = nullptr;
    binder_status_t status = AIBinder_prepareTransaction(service, &in);
    if (status == STATUS_OK && in != nullptr) {
        AParcel_writeInt32(in, 0);      // touchId = 0
        AParcel_writeInt32(in, mode);   // mode = 10 (FOD)
        AParcel_writeInt32(in, value);  // value: 1 = arm, 0 = disarm
        status = AIBinder_transact(service, 9, &in, &out, 0); // Tx 9
        if (out) AParcel_delete(out);
    }
    AIBinder_decStrong(service);
    return (status == STATUS_OK);
}

// Fallback Direct Sysfs Node Write
static void writeSysfsFodNode(bool enable) {
    int fd = open(GOODIX_FOD_NODE, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        char val = enable ? '1' : '0';
        write(fd, &val, 1);
        close(fd);
    }
}

// CPU Governor Instant Boost on Touch Event (<1ms latency)
static void boostCpuOnTouch() {
    int fd = open(CPU_BOOST_NODE, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        write(fd, "2400000", 7);
        close(fd);
    }
}

// Low-latency Hardware Input Monitor Thread
static void* inputMonitorThread(void*) {
    LOGI("Starting hardware input event monitor on Goodix Touch");
    int fd = open("/dev/input/event5", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        for (int i = 0; i < 10; ++i) {
            char path[32];
            snprintf(path, sizeof(path), "/dev/input/event%d", i);
            fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd >= 0) break;
        }
    }

    if (fd < 0) {
        LOGE("Failed to open input device");
        return nullptr;
    }

    struct pollfd pfd = { fd, POLLIN, 0 };
    struct input_event evs[16];

    while (true) {
        int ret = poll(&pfd, 1, 1000);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytes = read(fd, evs, sizeof(evs));
            if (bytes > 0) {
                int count = bytes / sizeof(struct input_event);
                for (int i = 0; i < count; ++i) {
                    if (evs[i].type == EV_KEY && evs[i].code == FOD_GESTURE_KEY) {
                        if (evs[i].value == 1) {
                            // Instant TOUCH DOWN
                            g_finger_down.store(true);
                            boostCpuOnTouch();
                            setXiaomiTouchFeature(10, 1);
                            writeSysfsFodNode(true);
                            LOGI("Hardware FOD Touch DOWN -> Boost & LHBM Arm");
                        } else if (evs[i].value == 0) {
                            // Touch UP
                            g_finger_down.store(false);
                            if (g_authenticated.load()) {
                                setXiaomiTouchFeature(10, 0);
                                writeSysfsFodNode(false);
                            }
                        }
                    }
                }
            }
        }
    }
    close(fd);
    return nullptr;
}

// Instant Turn-off callback called on authentication success/failure
extern "C" void onFingerprintAuthResult(bool success) {
    LOGI("Auth completed (success=%d) -> Instant LHBM Turn Off", success);
    g_authenticated.store(success);
    setXiaomiTouchFeature(10, 0);
    writeSysfsFodNode(false);
}

// Library constructor running at mfp-daemon startup
__attribute__((constructor))
static void itachiFpDuchampInit() {
    LOGI("Initializing Itachi Duchamp Ultra-Fast FOD Shim for POCO X6 Pro");
    pthread_t thread;
    pthread_create(&thread, nullptr, inputMonitorThread, nullptr);
    pthread_detach(thread);
}
