package com.sampmobile.xyvern.loader;

import android.util.Log;

/**
 * CSALoaderBridge
 * 
 * JNI Bridge to communicate between the Java Launcher/Activity
 * and the native CSALoader SCM engine (libcsaloader.so).
 */
public final class CSALoaderBridge {
    private static final String TAG = "M-CSL";
    private static boolean sLoaded = false;

    static {
        try {
            System.loadLibrary("samp");
            sLoaded = true;
            Log.i(TAG, "CSALoader native interface initialized (via libsamp.so)");
        } catch (Throwable t) {
            Log.e(TAG, "Failed to load libsamp.so: " + t.getMessage());
        }
    }

    public static boolean isAvailable() {
        return sLoaded;
    }

    public static boolean init(String storagePath) {
        if (!sLoaded) return false;
        try {
            return nativeInit(storagePath);
        } catch (Throwable t) {
            Log.e(TAG, "Error initializing CSALoader: " + t.getMessage());
            return false;
        }
    }

    public static int scanScripts(String dirPath) {
        if (!sLoaded) return 0;
        try {
            return nativeScanScripts(dirPath);
        } catch (Throwable t) {
            Log.e(TAG, "Error scanning scripts: " + t.getMessage());
            return 0;
        }
    }

    public static void process(int tick) {
        if (!sLoaded) return;
        try {
            nativeProcess(tick);
        } catch (Throwable t) {
            Log.e(TAG, "Error processing scripts: " + t.getMessage());
        }
    }

    public static void triggerInteractive(int index) {
        if (!sLoaded) return;
        try {
            nativeTriggerInteractive(index);
        } catch (Throwable t) {
            Log.e(TAG, "Error triggering script: " + t.getMessage());
        }
    }

    public static int getScriptCount() {
        if (!sLoaded) return 0;
        try {
            return nativeGetScriptCount();
        } catch (Throwable t) {
            return 0;
        }
    }

    // Native declarations
    private static native boolean nativeInit(String storagePath);
    private static native int nativeScanScripts(String dirPath);
    private static native void nativeProcess(int tick);
    private static native void nativeTriggerInteractive(int index);
    private static native int nativeGetScriptCount();

    private CSALoaderBridge() {}
}
