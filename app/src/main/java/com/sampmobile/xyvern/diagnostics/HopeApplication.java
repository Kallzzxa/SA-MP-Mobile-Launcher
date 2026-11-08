package com.hopepride.samp.diagnostics;

import android.app.Application;

public final class HopeApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        Diagnostics.install(this);
        BootGuard.install(this);
    }
}
