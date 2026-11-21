package com.sampmobile.xyvern.launcher.util;

import android.content.Context;
import android.os.Process;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class LogcatRecorder {
    private static final String TAG = "LogcatRecorder";
    private static boolean sIsRunning = false;
    private static Thread sLogcatThread = null;

    public static synchronized void start(Context context) {
        if (sIsRunning) return;
        sIsRunning = true;

        sLogcatThread = new Thread(() -> {
            try {
                // Tentukan lokasi penyimpanan logcat
                File targetDir = null;
                File extDir = context.getExternalFilesDir(null);
                if (extDir != null) {
                    targetDir = new File(extDir, "logcat");
                } else {
                    targetDir = new File("/storage/emulated/0/logcat");
                }

                if (!targetDir.exists()) {
                    targetDir.mkdirs();
                }

                File logFile = new File(targetDir, "M-SAMP-Logcat.txt");
                Log.i(TAG, "Starting logcat recorder to: " + logFile.getAbsolutePath());

                // Jalankan proses logcat untuk PID proses kita
                int myPid = Process.myPid();
                ProcessBuilder pb = new ProcessBuilder(
                        "logcat",
                        "-v", "time",
                        "*:V"
                );
                pb.redirectErrorStream(true);
                java.lang.Process process = pb.start();

                BufferedReader reader = new BufferedReader(
                        new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8)
                );

                FileOutputStream fos = new FileOutputStream(logFile, false); // Overwrite saat start
                OutputStreamWriter writer = new OutputStreamWriter(fos, StandardCharsets.UTF_8);

                String timeStamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
                writer.write("====================================================\n");
                writer.write(" M-SAMP Logcat Session Started: " + timeStamp + "\n");
                writer.write(" PID: " + myPid + " | File: " + logFile.getAbsolutePath() + "\n");
                writer.write("====================================================\n\n");
                writer.flush();

                String line;
                while (sIsRunning && (line = reader.readLine()) != null) {
                    writer.write(line);
                    writer.write("\n");
                    // Flush secara berkala agar jika game crash, baris crash tidak hilang di buffer
                    writer.flush();
                }

                writer.close();
                reader.close();
                process.destroy();
            } catch (Exception e) {
                Log.e(TAG, "LogcatRecorder error: " + e.getMessage(), e);
            }
        }, "M-SAMP-LogcatThread");

        sLogcatThread.setDaemon(true);
        sLogcatThread.start();
    }

    public static synchronized void stop() {
        sIsRunning = false;
        if (sLogcatThread != null) {
            sLogcatThread.interrupt();
            sLogcatThread = null;
        }
    }
}
