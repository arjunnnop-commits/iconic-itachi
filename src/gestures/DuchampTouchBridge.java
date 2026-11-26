/*
 * Copyright (C) 2026 itachi duchamp
 * Double Tap to Wake (DT2W) Bridge for POCO X6 Pro (duchamp)
 */

package com.android.server.wm;

import android.content.Context;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ServiceManager;
import android.provider.Settings;
import android.util.Log;

public final class DuchampTouchBridge {
    private static final String TAG = "DuchampTouchBridge";
    private static final String TOUCH_FEATURE = "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";
    private static final int TOUCH_DOUBLE_TAP = 14;

    private DuchampTouchBridge() {}

    public static void setDoubleTapMode(Context context, boolean enabled) {
        try {
            IBinder binder = ServiceManager.getService(TOUCH_FEATURE);
            if (binder == null) {
                Log.e(TAG, "Xiaomi ITouchFeature unavailable");
                return;
            }

            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            try {
                data.writeInterfaceToken("vendor.xiaomi.hw.touchfeature.ITouchFeature");
                data.writeInt(0);                  // touchId = 0
                data.writeInt(TOUCH_DOUBLE_TAP);   // mode = 14
                data.writeInt(enabled ? 1 : 0);    // value = 1 or 0
                binder.transact(9, data, reply, 0); // Tx code 9
                reply.readException();
                Log.i(TAG, "DT2W mode successfully set to: " + enabled);
            } finally {
                data.recycle();
                reply.recycle();
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to apply DT2W mode", e);
        }
    }
}
