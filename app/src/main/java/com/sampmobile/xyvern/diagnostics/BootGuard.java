package com.sampmobile.xyvern.diagnostics;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

/**
 * Remembers how the previous start-up ended and keeps a machine readable
 * marker in place while safe mode is active.
 *
 * A start-up is considered broken when the process is replaced before the
 * watchdog fires. Two broken start-ups in a row arm safe mode; the native layer
 * reads {@link #MARKER_NAME} and falls back to the untouched engine path instead
 * of installing its optional patches. Nothing of the player's data is deleted -
 * leaving safe mode only removes the marker.
 */
public final class BootGuard {
    public static final String MARKER_NAME = "safe_mode.on";

    private static final String PREFS = "hopepride_recovery";
    private static final String KEY_IN_PROGRESS = "boot_in_progress";
    private static final String KEY_FAILURES = "boot_failures";
    private static final String KEY_SAFE_MODE = "safe_mode";
    private static final long HEALTHY_UPTIME_MS = 25_000L;

    private BootGuard() {}

    public static void install(Context context) {
        final Context app = context.getApplicationContext();
        final SharedPreferences prefs = app.getSharedPreferences(PREFS, Context.MODE_PRIVATE);

        boolean interrupted = prefs.getBoolean(KEY_IN_PROGRESS, false);
        int failures = prefs.getInt(KEY_FAILURES, 0);
        boolean safeMode = prefs.getBoolean(KEY_SAFE_MODE, false);

        if (interrupted) {
            failures++;
            Diagnostics.event("BOOT_INTERRUPTED failures=" + failures);
            if (failures >= 2 && !safeMode) {
                safeMode = true;
                Diagnostics.event("SAFE_MODE_ARMED");
            }
        }

        prefs.edit()
                .putBoolean(KEY_IN_PROGRESS, true)
                .putInt(KEY_FAILURES, failures)
                .putBoolean(KEY_SAFE_MODE, safeMode)
                .apply();

        writeMarker(app, safeMode);

        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            prefs.edit()
                    .putBoolean(KEY_IN_PROGRESS, false)
                    .putInt(KEY_FAILURES, 0)
                    .apply();
            Diagnostics.event("BOOT_HEALTHY");
        }, HEALTHY_UPTIME_MS);
    }

    public static boolean isSafeMode(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                .getBoolean(KEY_SAFE_MODE, false);
    }

    public static void clearSafeMode(Context context) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                .putBoolean(KEY_SAFE_MODE, false)
                .putInt(KEY_FAILURES, 0)
                .apply();
        writeMarker(context, false);
        Diagnostics.event("SAFE_MODE_CLEARED");
    }

    private static void writeMarker(Context context, boolean enabled) {
        File internal = new File(context.getFilesDir(), MARKER_NAME);
        File external = context.getExternalFilesDir(null);
        File externalMarker = external == null ? null : new File(external, MARKER_NAME);

        if (!enabled) {
            if (internal.exists()) internal.delete();
            if (externalMarker != null && externalMarker.exists()) externalMarker.delete();
            return;
        }

        write(internal);
        if (externalMarker != null) write(externalMarker);
    }

    private static void write(File file) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write("safe-mode\n".getBytes(StandardCharsets.UTF_8));
        } catch (Exception error) {
            Log.w("HopeRecovery", "Could not write safe mode marker", error);
        }
    }
}
