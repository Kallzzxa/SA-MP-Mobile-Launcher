package com.sampmobile.xyvern.diagnostics;

import android.app.Application;
import android.content.Context;
import android.os.Process;
import android.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.nio.charset.StandardCharsets;

/** Installed separately in the launcher and game processes; never uploads logs. */
public final class Diagnostics {
    private static DiagnosticStore store;
    private Diagnostics() {}

    public static void install(Application application) {
        String process = processName().endsWith(":game") ? "game" : "launcher";
        store = new DiagnosticStore(new File(application.getFilesDir(), "diagnostics"), process);
        event("PROCESS_START");
        Thread.UncaughtExceptionHandler prior = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            try { error("JAVA_UNCAUGHT_EXCEPTION", error); } catch (Throwable ignored) { }
            if (prior != null) prior.uncaughtException(thread, error);
            else { Process.killProcess(Process.myPid()); System.exit(10); }
        });
    }

    private static String processName() {
        try (FileInputStream in = new FileInputStream("/proc/self/cmdline")) {
            byte[] bytes = new byte[256];
            int n = in.read(bytes);
            if (n <= 0) return "";
            int end = 0;
            while (end < n && bytes[end] != 0) end++;
            return new String(bytes, 0, end, StandardCharsets.UTF_8);
        } catch (Exception ignored) { return ""; }
    }

    public static void event(String event) { record(event, null); }
    public static void error(String event, Throwable error) { record(event, error); }
    private static void record(String event, Throwable error) {
        try { if (store != null) store.append(event, error); }
        catch (Throwable ignored) { Log.w("HopeDiagnostics", "Could not write diagnostic log"); }
    }
}
