/*
 * Decompiled with JADX v1.2.0.
 */
package com.unity3d.player;

import android.app.Activity;

public class MultiWindowSupport {
    private static final String RESIZABLE_WINDOW = "unity.allow-resizable-window";
    private static boolean s_LastMultiWindowMode;

    public static boolean getAllowResizableWindow(Activity activity) {
        try {
            return isInMultiWindowMode(activity) && activity.getPackageManager().getApplicationInfo(activity.getPackageName(), 128).metaData.getBoolean(RESIZABLE_WINDOW);
        } catch (Exception unused) {
        }
        return false;
    }

    static boolean isInMultiWindowMode(Activity activity) {
        if (PlatformSupport.NOUGAT_SUPPORT) {
            return activity.isInMultiWindowMode();
        }
        return false;
    }

    public static boolean isMultiWindowModeChangedToTrue(Activity activity) {
        return !s_LastMultiWindowMode && isInMultiWindowMode(activity);
    }

    public static void saveMultiWindowMode(Activity activity) {
        s_LastMultiWindowMode = isInMultiWindowMode(activity);
    }
}
