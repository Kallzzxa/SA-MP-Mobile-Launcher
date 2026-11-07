package com.sampmobile.xyvern.diagnostics;

import android.app.Application;

public final class HopeApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        Diagnostics.install(this);
    }
}
