package com.sampmobile.xyvern.launcher.util;

import android.content.Context;
import android.content.SharedPreferences;

public class SharedPreferenceCore {
    public static final String APP_PREFERENCES = "configlauncher";
    private static volatile SharedPreferences sPrefs;

    private static SharedPreferences getPrefs(Context context) {
        if (sPrefs == null) {
            synchronized (SharedPreferenceCore.class) {
                if (sPrefs == null) {
                    sPrefs = context.getApplicationContext().getSharedPreferences(APP_PREFERENCES, Context.MODE_PRIVATE);
                }
            }
        }
        return sPrefs;
    }

    public static void main(Context context) {
        getPrefs(context);
    }

    public void setString(Context context, String key, String value) {
        getPrefs(context).edit().putString(key, value).apply();
    }

    public void setInt(Context context, String key, int value) {
        getPrefs(context).edit().putInt(key, value).apply();
    }

    public void setBoolean(Context context, String key, boolean value) {
        getPrefs(context).edit().putBoolean(key, value).apply();
    }

    public void setLong(Context context, String key, long value) {
        getPrefs(context).edit().putLong(key, value).apply();
    }

    public void setFloat(Context context, String key, float value) {
        getPrefs(context).edit().putFloat(key, value).apply();
    }

    public String getString(Context context, String key) {
        return getPrefs(context).getString(key, "");
    }

    public int getInt(Context context, String key) {
        return getPrefs(context).getInt(key, 0);
    }

    public int getInt(Context context, String key, int defaultValue) {
        return getPrefs(context).getInt(key, defaultValue);
    }

    public boolean getBoolean(Context context, String key) {
        return getPrefs(context).getBoolean(key, false);
    }

    public long getLong(Context context, String key) {
        return getPrefs(context).getLong(key, 0);
    }

    public float getFloat(Context context, String key) {
        return getPrefs(context).getFloat(key, 0.0f);
    }
}
