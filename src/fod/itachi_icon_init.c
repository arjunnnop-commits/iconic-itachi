/*
 * Copyright (C) 2026 itachi duchamp
 * SystemUI Custom Fingerprint Icon Initializer
 * Target: POCO X6 Pro (duchamp)
 */

#include <android/log.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_TAG "ItachiIconInit"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int main(void) {
    FILE *fp = fopen("/data/system/packages.list", "re");
    if (!fp) return 1;

    char line[512];
    unsigned long appid = 0;
    while (fgets(line, sizeof(line), fp)) {
        char pkg[256];
        unsigned long uid;
        if (sscanf(line, "%255s %lu", pkg, &uid) == 2 && strcmp(pkg, "com.android.systemui") == 0) {
            appid = uid;
            break;
        }
    }
    fclose(fp);

    if (!appid) return 1;

    // Ensure /data/oplus/os exists with 0771 permissions
    mkdir("/data/oplus", 0771);
    mkdir("/data/oplus/os", 0771);
    chmod("/data/oplus/os", 0771);

    LOGI("SystemUI UID %lu registered for custom fingerprint icons", appid);
    return 0;
}
