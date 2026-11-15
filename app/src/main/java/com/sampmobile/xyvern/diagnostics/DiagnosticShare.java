package com.hopepride.samp.diagnostics;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.FileProvider;
import com.hopepride.samp.BuildConfig;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** User-initiated export only; raw native logs may contain private information. */
public final class DiagnosticShare {
    private static final ExecutorService IO = Executors.newSingleThreadExecutor();
    private static final long MAX_NATIVE_BYTES = 256 * 1024;
    private DiagnosticShare() {}

    public static void confirm(Activity activity) {
        new AlertDialog.Builder(activity).setTitle("Bagikan log diagnostik?")
            .setMessage("Paket berisi log launcher, log crash Java, dan bagian akhir log game/AML bila ada. " +
                    "Log game mentah dapat berisi nama, alamat server, jalur file atau informasi pribadi. " +
                    "Log juga dapat memuat token. Simpan dan periksa sebelum membagikan; jangan kirim ke channel publik. " +
                    "Log tidak dikirim otomatis.")
            .setNegativeButton("BATAL", null)
            .setPositiveButton("SIAPKAN", (dialog, which) -> prepare(activity))
            .show();
    }

    private static void prepare(Activity activity) {
        Toast.makeText(activity, "Menyiapkan log...", Toast.LENGTH_SHORT).show();
        Context context = activity.getApplicationContext();
        IO.execute(() -> {
            try {
                File zip = createReport(context);
                activity.runOnUiThread(() -> {
                    if (activity.isFinishing() || activity.isDestroyed()) return;
                    try {
                        Uri uri = FileProvider.getUriForFile(activity, activity.getPackageName() + ".provider", zip);
                        Intent intent = new Intent(Intent.ACTION_SEND).setType("application/zip")
                                .putExtra(Intent.EXTRA_STREAM, uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        intent.setClipData(ClipData.newRawUri("diagnostic report", uri));
                        activity.startActivity(Intent.createChooser(intent, "Bagikan log HopePride"));
                    } catch (RuntimeException e) {
                        Diagnostics.error("LOG_SHARE_FAILED", e);
                        Toast.makeText(activity, "Tidak dapat membuka menu berbagi log.", Toast.LENGTH_LONG).show();
                    }
                });
            } catch (Exception e) {
                Diagnostics.error("LOG_EXPORT_FAILED", e);
                activity.runOnUiThread(() -> {
                    if (!activity.isFinishing() && !activity.isDestroyed())
                        Toast.makeText(activity, "Gagal menyiapkan log.", Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    private static File createReport(Context context) throws IOException {
        File directory = new File(context.getCacheDir(), "diagnostic_reports");
        if (!directory.isDirectory() && !directory.mkdirs() && !directory.isDirectory()) throw new IOException();
        File[] oldReports = directory.listFiles();
        if (oldReports != null) for (File old : oldReports) {
            if (old.getName().startsWith("hopepride-log-") && old.getName().endsWith(".zip")
                    && System.currentTimeMillis() - old.lastModified() > 24 * 60 * 60 * 1000L) old.delete();
        }
        File zip = File.createTempFile("hopepride-log-", ".zip", directory);
        try {
          try (ZipOutputStream out = new ZipOutputStream(new FileOutputStream(zip))) {
            out.putNextEntry(new ZipEntry("INFO.txt"));
            String info = "HopePride " + BuildConfig.VERSION_NAME + "\nAndroid API " + Build.VERSION.SDK_INT + "\nABIs " + String.join(", ", Build.SUPPORTED_ABIS) +
                    "\nGAME_MISSING_ESSENTIAL indices: 0 texdb/gta3.img, 1 data/gta.dat, " +
                    "2 data/surface.dat, 3 data/fonts.dat, 4 data/handling.cfg, 5 AUDIO/CONFIG/BANKSLOT.DAT.\n" +
                    "Native logs are raw and may contain private information.\n" +
                    "Logs are not a complete record of all failures; native/system crashes may require logcat.\n";
            out.write(info.getBytes(StandardCharsets.UTF_8));
            out.closeEntry();
            File diag = new File(context.getFilesDir(), "diagnostics");
            for (String name : new String[] {"launcher.previous.log", "launcher.log", "game.previous.log", "game.log"})
                addTail(out, new File(diag, name), "diagnostics/" + name, DiagnosticStore.MAX_BYTES);
            File external = context.getExternalFilesDir(null);
            if (external != null) {
                for (String name : new String[] {"samp_log.txt", "crashlytics_log.txt", "aml_crashlog.txt",
                        "SAMP/aml_crashlog.txt", "AML/aml_crashlog.txt"})
                    addTail(out, new File(external, name), "native/" + name, MAX_NATIVE_BYTES);
            }
          }
          return zip;
        } catch (IOException | RuntimeException e) {
            zip.delete();
            throw e;
        }
    }

    private static void addTail(ZipOutputStream zip, File file, String name, long limit) throws IOException {
        if (!file.isFile() || !file.canRead()) return;
        try (RandomAccessFile input = new RandomAccessFile(file, "r")) {
            long length = input.length();
            input.seek(Math.max(0L, length - limit));
            zip.putNextEntry(new ZipEntry(name));
            byte[] buffer = new byte[8192];
            long remaining = Math.min(length, limit);
            while (remaining > 0) {
                int n = input.read(buffer, 0, (int) Math.min(buffer.length, remaining));
                if (n < 0) break;
                zip.write(buffer, 0, n);
                remaining -= n;
            }
            zip.closeEntry();
        }
    }
}
