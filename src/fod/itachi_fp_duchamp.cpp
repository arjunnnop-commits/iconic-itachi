/*
 * Copyright (C) 2026 itachi duchamp
 * Ultra-Fast Hardware FOD & LHBM Compatibility Shim
 * Target: POCO X6 Pro (duchamp / n11a / MT6897)
 */

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <android/binder_parcel.h>
#include <android/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

// libbinder_ndk platform service manager declaration
extern "C" {
    AIBinder* AServiceManager_checkService(const char* instance);
}

#define LOG_TAG "ItachiFpDuchamp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static constexpr const char* TOUCH_FEATURE_SERVICE = 
    "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";

static constexpr const char* DISP_PARAM_NODE = 
    "/sys/class/mi_display/disp_feature/disp-DSI-0/disp_param";

static constexpr const char* GOODIX_FOD_NODE = 
    "/sys/devices/platform/goodix_ts.0/fod_enable";

static constexpr const char* CPU_BOOST_NODE = 
    "/sys/devices/system/cpu/cpufreq/policy4/scaling_min_freq";

static constexpr int FOD_GESTURE_KEY = 0x152; // 338

static std::atomic<bool> g_authenticated(false);
static std::atomic<bool> g_finger_down(false);

// AIDL Class definition for NDK binder
static AIBinder_Class* g_touchClass = nullptr;

static void* dummyClassCreate(void* args) { return args; }
static void dummyClassDestroy(void* /*userData*/) {}
static binder_status_t dummyClassTransact(AIBinder*, transaction_code_t, const AParcel*, AParcel*) {
    return STATUS_UNKNOWN_TRANSACTION;
}

static void ensureTouchClass(AIBinder* service) {
    if (!g_touchClass) {
        g_touchClass = AIBinder_Class_define(
            "vendor.xiaomi.hw.touchfeature.ITouchFeature",
            dummyClassCreate, dummyClassDestroy, dummyClassTransact);
    }
    if (g_touchClass && service) {
        AIBinder_associateClass(service, g_touchClass);
    }
}

// Fallback command execution for ITouchFeature
static void executeTouchFeatureServiceCall(int32_t mode, int32_t value) {
    pid_t pid = fork();
    if (pid == 0) {
        char mode_str[16];
        char val_str[16];
        snprintf(mode_str, sizeof(mode_str), "%d", mode);
        snprintf(val_str, sizeof(val_str), "%d", value);
        char* const args[] = {
            (char*)"/system/bin/service",
            (char*)"call",
            (char*)TOUCH_FEATURE_SERVICE,
            (char*)"9",
            (char*)"i32", (char*)"0",
            (char*)"i32", mode_str,
            (char*)"i32", val_str,
            nullptr
        };
        execv("/system/bin/service", args);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, WNOHANG);
    }
}

// Trigger Xiaomi TouchFeature (Mode 10 = FOD Spotlight Area)
static bool setXiaomiTouchFeature(int32_t mode, int32_t value) {
    AIBinder* service = AServiceManager_checkService(TOUCH_FEATURE_SERVICE);
    if (!service) {
        executeTouchFeatureServiceCall(mode, value);
        return false;
    }

    ensureTouchClass(service);

    AParcel* in = nullptr;
    AParcel* out = nullptr;
    binder_status_t status = AIBinder_prepareTransaction(service, &in);
    if (status == STATUS_OK && in != nullptr) {
        AParcel_writeInt32(in, 0);      // touchId = 0
        AParcel_writeInt32(in, mode);   // mode = 10 (FOD)
        AParcel_writeInt32(in, value);  // value: 1 = arm, 0 = disarm
        status = AIBinder_transact(service, 9, &in, &out, 0); // Tx 9: takes ownership of in
        if (out) AParcel_delete(out);
    } else {
        // Fallback to service call if NDK prepare fails
        executeTouchFeatureServiceCall(mode, value);
    }
    AIBinder_decStrong(service);
    return (status == STATUS_OK);
}

// Direct LHBM (Local High Brightness Mode) Spotlight Control
// On POCO X6 Pro (duchamp), feature 09 = local_hbm, feature 02 = hbm_fod
static void setLhbmSpotlight(bool enable) {
    int fd = open(DISP_PARAM_NODE, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        if (enable) {
            write(fd, "09 1\n", 5);
            write(fd, "02 1\n", 5);
        } else {
            write(fd, "09 0\n", 5);
            write(fd, "02 0\n", 5);
        }
        close(fd);
    }
}

// Direct Goodix FOD Sysfs Node Write
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

// Find Goodix touch input event device dynamically
static int findGoodixTouchDevice() {
    // Check known event5 first
    int fd = open("/dev/input/event5", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char name[128] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
            if (strstr(name, "goodix") != nullptr || strstr(name, "touch") != nullptr) {
                return fd;
            }
        }
        close(fd);
    }

    // Scan all /dev/input/event*
    for (int i = 0; i < 20; ++i) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            char name[128] = {0};
            if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
                if (strstr(name, "goodix") != nullptr) {
                    LOGI("Discovered Goodix touch device at %s (%s)", path, name);
                    return fd;
                }
            }
            close(fd);
        }
    }
    return -1;
}

// Low-latency Hardware Input Monitor Thread
extern "C" void* inputMonitorThread(void* /*arg*/) {
    LOGI("Starting hardware input event monitor on Goodix Touch for itachi duchamp");

    int fd = -1;
    while (fd < 0) {
        fd = findGoodixTouchDevice();
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
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
                            // Instant TOUCH DOWN: Turn ON spotlight & boost CPU
                            g_finger_down.store(true);
                            setLhbmSpotlight(true);
                            writeSysfsFodNode(true);
                            boostCpuOnTouch();
                            setXiaomiTouchFeature(10, 1);
                            LOGI("Hardware FOD Touch DOWN -> LHBM Spotlight ON (09 1, 02 1)");
                        } else if (evs[i].value == 0) {
                            // Touch UP: Instantly turn OFF spotlight
                            g_finger_down.store(false);
                            setLhbmSpotlight(false);
                            writeSysfsFodNode(false);
                            setXiaomiTouchFeature(10, 0);
                            LOGI("Hardware FOD Touch UP -> LHBM Spotlight OFF (09 0, 02 0)");
                        }
                    }
                }
            }
        } else if (ret < 0) {
            // Device reconnect handling
            close(fd);
            fd = -1;
            while (fd < 0) {
                fd = findGoodixTouchDevice();
                if (fd < 0) std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            pfd.fd = fd;
        }
    }
    if (fd >= 0) close(fd);
    return nullptr;
}

// Instant Turn-off callback called on authentication success/failure
extern "C" void onFingerprintAuthResult(bool success) {
    LOGI("Auth completed (success=%d) -> Instant LHBM Turn Off", success);
    g_authenticated.store(success);
    setLhbmSpotlight(false);
    writeSysfsFodNode(false);
    setXiaomiTouchFeature(10, 0);
}

// Library constructor running at mfp-daemon startup
__attribute__((constructor))
static void itachiFpDuchampInit() {
    LOGI("Initializing Itachi Duchamp Ultra-Fast FOD Shim for POCO X6 Pro");
    pthread_t thread;
    pthread_create(&thread, nullptr, inputMonitorThread, nullptr);
    pthread_detach(thread);
}
