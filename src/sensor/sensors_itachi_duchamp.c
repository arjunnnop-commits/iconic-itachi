/*
 * Copyright (C) 2026 itachi duchamp
 * MediaTek MT6897 Sensor Wrapper HAL
 * Target: POCO X6 Pro (duchamp / n11a)
 */

#include <dlfcn.h>
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#define LOG_TAG "ItachiSensorDuchamp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define REAL_HAL_PATH "/vendor/lib64/hw/sensors.mediatek.V2.0.so"

#define HW_MODULE_TAG 0x48574d54u
#define SENSORS_HARDWARE_MODULE_ID "sensors"
#define HAL_MODULE_INFO_SYM HMI
#define HAL_MODULE_INFO_SYM_AS_STR "HMI"

#define SENSOR_FLAG_ON_CHANGE_MODE 0x00000002u
#define SENSOR_FLAG_WAKE_UP        0x00000001u

// Verified hardware handles from live POCO X6 Pro dumpsys
#define XIAOMI_PICKUP_TYPE 33171036
#define XIAOMI_PICKUP_HANDLE 0x19

#define XIAOMI_ALS_RAW_TYPE 33171111
#define XIAOMI_ALS_RAW_HANDLE 0x6c

#define SENSOR_TYPE_LIGHT 5
#define OPLUS_TILT_TYPE 22
#define OPLUS_SYNTH_TILT_TYPE 65611
#define OPLUS_SYNTH_HANDLE 0x6f11

typedef struct hw_module_t hw_module_t;
typedef struct hw_device_t hw_device_t;
typedef struct sensors_module_t sensors_module_t;
typedef struct hw_module_methods_t hw_module_methods_t;

struct hw_module_methods_t {
    int (*open)(const struct hw_module_t *, const char *, struct hw_device_t **);
};

struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
    uint16_t hal_api_version;
    const char *id;
    const char *name;
    const char *author;
    hw_module_methods_t *methods;
    void *dso;
    uint64_t reserved[25];
};

struct hw_device_t {
    uint32_t tag;
    uint32_t version;
    hw_module_t *module;
    uint64_t reserved[12];
    int (*close)(hw_device_t *);
};

typedef struct sensor_t {
    const char *name;
    const char *vendor;
    int32_t version;
    int32_t handle;
    int32_t type;
    float maxRange;
    float resolution;
    float power;
    int32_t minDelay;
    uint32_t fifoReservedEventCount;
    uint32_t fifoMaxEventCount;
    const char *stringType;
    const char *requiredPermission;
    int64_t maxDelay;
    uint32_t flags;
    void *reserved[2];
} sensor_t;

typedef struct sensors_module_t {
    struct hw_module_t common;
    int (*get_sensors_list)(struct sensors_module_t *module, struct sensor_t const **list);
    int (*set_operation_mode)(unsigned int mode);
} sensors_module_t;

static sensors_module_t* g_real_module = NULL;
static sensor_t* g_custom_sensor_list = NULL;
static int g_custom_sensor_count = 0;
static int g_backlight_fd = -1;

// Read real-time 14-bit OLED backlight level
__attribute__((unused))
static float get_display_backlight(void) {
    if (g_backlight_fd < 0) {
        g_backlight_fd = open("/sys/class/leds/lcd-backlight/brightness", O_RDONLY | O_CLOEXEC);
        if (g_backlight_fd < 0) {
            g_backlight_fd = open("/sys/class/mi_display/disp_feature/disp-DSI-0/brightness_clone", O_RDONLY | O_CLOEXEC);
        }
    }
    if (g_backlight_fd >= 0) {
        char buf[16] = {0};
        pread(g_backlight_fd, buf, sizeof(buf) - 1, 0);
        return (float)atoi(buf);
    }
    return 0.0f;
}

static int itachi_sensor_get_list(struct sensors_module_t* module,
                                  struct sensor_t const** list) {
    (void)module;
    if (!g_real_module) {
        void* handle = dlopen(REAL_HAL_PATH, RTLD_NOW);
        if (!handle) {
            LOGE("Failed to load real HAL: %s", dlerror());
            return 0;
        }
        g_real_module = (sensors_module_t*)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    }

    struct sensor_t const* real_list = NULL;
    int count = g_real_module->get_sensors_list(g_real_module, &real_list);

    if (!g_custom_sensor_list && count > 0) {
        g_custom_sensor_list = (sensor_t*)malloc(sizeof(sensor_t) * (count + 1));
        memcpy(g_custom_sensor_list, real_list, sizeof(sensor_t) * count);

        // Add ColorOS synthetic tilt sensor (bridges Xiaomi pickup to ColorOS Rise to Wake)
        sensor_t* tilt = &g_custom_sensor_list[count];
        tilt->name = "Itachi Duchamp Tilt Detector";
        tilt->vendor = "itachi duchamp";
        tilt->version = 1;
        tilt->handle = OPLUS_SYNTH_HANDLE;
        tilt->type = OPLUS_SYNTH_TILT_TYPE;
        tilt->maxRange = 1.0f;
        tilt->resolution = 1.0f;
        tilt->power = 0.01f;
        tilt->minDelay = 0;
        tilt->fifoReservedEventCount = 0;
        tilt->fifoMaxEventCount = 0;
        tilt->stringType = "oplus.sensor.tilt_detector";
        tilt->requiredPermission = "";
        tilt->maxDelay = 0;
        tilt->flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP;

        g_custom_sensor_count = count + 1;
    }

    *list = g_custom_sensor_list;
    return g_custom_sensor_count;
}

static int itachi_sensor_open(const struct hw_module_t* module, const char* id,
                              struct hw_device_t** device) {
    (void)module;
    if (!g_real_module) {
        void* handle = dlopen(REAL_HAL_PATH, RTLD_NOW);
        if (!handle) return -1;
        g_real_module = (sensors_module_t*)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    }
    return g_real_module->common.methods->open((struct hw_module_t*)g_real_module, id, device);
}

static struct hw_module_methods_t itachi_sensors_module_methods = {
    .open = itachi_sensor_open
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HW_MODULE_TAG,
        .module_api_version = 1,
        .hal_api_version = 0,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "POCO X6 Pro (duchamp) Sensor Wrapper HAL",
        .author = "itachi duchamp",
        .methods = &itachi_sensors_module_methods,
        .dso = NULL,
        .reserved = {0}
    },
    .get_sensors_list = itachi_sensor_get_list,
    .set_operation_mode = NULL
};
