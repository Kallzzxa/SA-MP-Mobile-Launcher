package com.sampmobile.xyvern.diagnostics;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/** Small per-process rolling log. No exception messages, URLs or user input. */
public final class DiagnosticStore {
    static final long MAX_BYTES = 128 * 1024;
    private final File current;
    private final File previous;

    public DiagnosticStore(File directory, String process) {
        if (!process.equals("launcher") && !process.equals("game")) throw new IllegalArgumentException();
        current = new File(directory, process + ".log");
        previous = new File(directory, process + ".previous.log");
    }

    public synchronized void append(String event, Throwable error) throws IOException {
        if (event == null || !event.matches("[A-Z0-9_]{1,80}")) throw new IllegalArgumentException();
        StringBuilder entry = new StringBuilder();
        SimpleDateFormat date = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US);
        date.setTimeZone(TimeZone.getTimeZone("UTC"));
        entry.append(date.format(new Date())).append(' ').append(event).append('\n');
        Throwable cause = error;
        for (int n = 0; cause != null && n < 4; n++, cause = cause.getCause()) {
            entry.append("  exception: ").append(safe(cause.getClass().getName())).append('\n');
            StackTraceElement[] frames = cause.getStackTrace();
            for (int i = 0; i < frames.length && i < 32; i++) {
                StackTraceElement f = frames[i];
                entry.append("  at ").append(safe(f.getClassName())).append('.')
                        .append(safe(f.getMethodName())).append(':').append(f.getLineNumber()).append('\n');
            }
        }
        byte[] bytes = entry.toString().getBytes(StandardCharsets.UTF_8);
        File dir = current.getParentFile();
        if (!dir.isDirectory() && !dir.mkdirs() && !dir.isDirectory()) throw new IOException("log directory unavailable");
        if (current.length() + bytes.length > MAX_BYTES) {
            if (previous.exists() && !previous.delete()) throw new IOException("rotation failed");
            if (current.exists() && !current.renameTo(previous)) throw new IOException("rotation failed");
        }
        try (FileOutputStream output = new FileOutputStream(current, true)) {
            output.write(bytes);
        }
    }

    private static String safe(String value) {
        if (value == null) return "unknown";
        value = value.replaceAll("[^A-Za-z0-9_.$<>-]", "_");
        return value.length() > 200 ? value.substring(0, 200) : value;
    }
}
