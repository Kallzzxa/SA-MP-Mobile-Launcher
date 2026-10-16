package com.sampmobile.xyvern.cef;

import android.app.Activity;
import android.util.Log;

public class CefClientManager {
    private static final String TAG = "SAMP_CEF_CLIENT";
    private final Activity mActivity;
    private CefJavaManager mJavaManager;

    public CefClientManager(Activity activity) {
        this.mActivity = activity;
    }

    public void setJavaManager(CefJavaManager javaManager) {
        this.mJavaManager = javaManager;

        try {
            nativeSetJavaManager(javaManager);
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "nativeSetJavaManager failed: " + e.getMessage());
        }
    }

    public CefJavaManager getJavaManager() {
        return mJavaManager;
    }

    public void onBrowserInit(boolean isSuccess, int errorCode) {
        try {
            nativeOnBrowserInit(isSuccess, errorCode);
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "nativeOnBrowserInit call failed: " + e.getMessage());
        }
    }

    public void sendClientEvent(String eventName, String eventDataJson) {
        try {
            nativeSendClientEvent(eventName, eventDataJson);
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "nativeSendClientEvent call failed: " + e.getMessage());
        }
    }
    private native void nativeSetJavaManager(CefJavaManager javaManager);
    private native void nativeOnBrowserInit(boolean isSuccess, int errorCode);
    private native void nativeSendClientEvent(String eventName, String eventDataJson);
}