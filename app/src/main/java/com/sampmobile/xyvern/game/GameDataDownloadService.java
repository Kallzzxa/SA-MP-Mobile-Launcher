package com.sampmobile.xyvern.game;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.net.SocketException;
import java.net.ConnectException;
import java.net.URL;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Foreground service that downloads + installs the game data package.
 *
 * - Survives the Activity being closed (foregroundServiceType=dataSync).
 * - Full resume: HTTP Range header + "<name>.zip.part"; partial files are
 *   never deleted on failure — the next attempt continues from the last byte.
 * - Automatic retry (max 5) with exponential backoff for transient errors
 *   (timeout / connection reset / HTTP 5xx). Permanent errors stop + report.
 * - Download state (url / version / expected size) is persisted in
 *   SharedPreferences, so if the process dies the download auto-resumes on
 *   the next app start ({@link #autoResume(Context)}).
 * - Progress is observable by any UI via the static {@link Listener} API.
 */
public class GameDataDownloadService extends Service {

    public static final String ACTION_GAME_DATA = "com.sampmobile.xyvern.action.DOWNLOAD_GAME_DATA";
    public static final String ACTION_ASSETS = "com.sampmobile.xyvern.action.DOWNLOAD_ASSETS";

    public static final String EXTRA_URL = "url";          // page or direct url
    public static final String EXTRA_VERSION = "version";
    public static final String EXTRA_SIZE = "size";
    public static final String EXTRA_SENTINEL = "sentinel";

    private static final String CHANNEL_ID = "game_data_download";
    private static final int NOTIF_ID = 1001;
    private static final int MAX_RETRIES = 5;
    private static final long RETRY_BACKOFF_MS = 3000;

    // ------------------------------------------------------------ state ----
    public static final int PHASE_IDLE = 0;
    public static final int PHASE_CONNECT = 1;
    public static final int PHASE_DOWNLOAD = 2;
    public static final int PHASE_EXTRACT = 3;
    public static final int PHASE_DONE = 4;
    public static final int PHASE_FAILED = 5;

    public static final class State {
        public volatile int phase = PHASE_IDLE;
        public volatile int percent = 0;
        public volatile long bytes = 0;
        public volatile long total = 0;
        public volatile String detail = "";
        public volatile String error = null;
    }

    public interface Listener { void onState(State s); }

    private static volatile State sState = new State();
    private static final java.util.concurrent.CopyOnWriteArrayList<Listener> sListeners =
            new java.util.concurrent.CopyOnWriteArrayList<>();

    public static State getState() { return sState; }
    public static void addListener(Listener l) { if (l != null) sListeners.add(l); }
    public static void removeListener(Listener l) { sListeners.remove(l); }

    private static void setState(int phase, int percent, long bytes, long total, String detail, String error) {
        State s = new State();
        s.phase = phase; s.percent = percent; s.bytes = bytes; s.total = total;
        s.detail = detail == null ? "" : detail; s.error = error;
        sState = s;
        for (Listener l : sListeners) {
            try { l.onState(sState); } catch (Exception ignored) {}
        }
    }

    // ------------------------------------------------------------ prefs ----
    private static final String PREFS_NAME = "samp_game_data";
    private static final String KEY_JOB = "pending_job";
    private static final String KEY_URL = "pending_url";
    private static final String KEY_VERSION = "pending_version";
    private static final String KEY_SIZE = "pending_size";
    private static final String KEY_SENTINEL = "pending_sentinel";

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    /** Persist download state so it can be auto-resumed after process death. */
    public static void savePending(Context c, String job, String pageUrl, String version,
                                   long expectedSize, String sentinel) {
        prefs(c).edit()
                .putString(KEY_JOB, job == null ? ACTION_GAME_DATA : job)
                .putString(KEY_URL, pageUrl == null ? "" : pageUrl)
                .putString(KEY_VERSION, version == null ? "" : version)
                .putLong("pending_size", expectedSize)
                .putString("pending_sentinel", sentinel == null ? "" : sentinel)
                .apply();
    }

    public static void clearPending(Context c) {
        prefs(c).edit().remove(KEY_JOB).remove(KEY_URL)
                .remove(KEY_VERSION).remove(KEY_SIZE).remove(KEY_SENTINEL).apply();
    }

    /** Returns true if a persisted, unfinished download exists. */
    public static boolean hasUnfinished(Context c) {
        SharedPreferences p = prefs(c);
        if (!p.contains(KEY_JOB)) return false;
        String installed = p.getString("data_version", "");
        String version = p.getString(KEY_VERSION, "");
        return !version.equals(installed);
    }

    /** Re-launch the service from persisted state (called on app open). */
    public static void autoResume(Context c) {
        if (!hasUnfinished(c)) return;
        State cur = sState;
        if (cur.phase == PHASE_CONNECT || cur.phase == PHASE_DOWNLOAD || cur.phase == PHASE_EXTRACT) {
            return; // already running
        }
        SharedPreferences p = prefs(c);
        Intent i = new Intent(c, GameDataDownloadService.class);
        i.setAction(p.getString(KEY_JOB, ACTION_GAME_DATA));
        i.putExtra(EXTRA_URL, p.getString(KEY_URL, ""));
        i.putExtra(EXTRA_VERSION, p.getString(KEY_VERSION, ""));
        i.putExtra(EXTRA_SIZE, p.getLong(KEY_SIZE, 0));
        i.putExtra(EXTRA_SENTINEL, p.getString(KEY_SENTINEL, ""));
        startForegroundCompat(c, i);
    }

    /** Kick off a download (safe from any context). */
    public static void start(Context c, String action, String pageUrl, String version,
                             long expectedSize, String sentinel) {
        savePending(c, action, pageUrl, version, expectedSize, sentinel);
        // Fresh job: drop the resolved direct url (MediaFire links are one-shot),
        // the .part file itself stays so any existing partial data still resumes.
        prefs(c).edit().remove("resolved_" + (version == null ? "" : version)).apply();
        Intent i = new Intent(c, GameDataDownloadService.class);
        i.setAction(action);
        i.putExtra(EXTRA_URL, pageUrl);
        i.putExtra(EXTRA_VERSION, version);
        i.putExtra(EXTRA_SIZE, expectedSize);
        i.putExtra(EXTRA_SENTINEL, sentinel);
        startForegroundCompat(c, i);
    }

    private static void startForegroundCompat(Context c, Intent i) {
        if (Build.VERSION.SDK_INT >= 26) {
            c.startForegroundService(i);
        } else {
            c.startService(i);
        }
    }

    // ------------------------------------------------------------ engine ---
    private Handler mWorker;
    private volatile boolean mRunning = false;
    private volatile boolean mCancelled = false;

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread t = new HandlerThread("GameDataDownload");
        t.start();
        mWorker = new Handler(t.getLooper());
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel ch = new NotificationChannel(CHANNEL_ID,
                    "Download Data Game", NotificationManager.IMPORTANCE_LOW);
            ch.setDescription("Progres unduhan data game");
            ch.setShowBadge(false);
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.createNotificationChannel(ch);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForegroundWithNotification(buildNotification("Menyiapkan unduhan...", 0));
        if (mRunning) return START_STICKY; // one job at a time
        final String action = intent != null ? intent.getAction() : ACTION_GAME_DATA;
        final String url = intent != null ? intent.getStringExtra(EXTRA_URL) : null;
        final String version = intent != null ? intent.getStringExtra(EXTRA_VERSION) : "";
        final long size = intent != null ? intent.getLongExtra(EXTRA_SIZE, 0) : 0;
        final String sentinel = intent != null ? intent.getStringExtra(EXTRA_SENTINEL) : "";
        mCancelled = false;
        mRunning = true;
        mWorker.post(() -> runJob(action, url, version, size, sentinel));
        return START_REDELIVER_INTENT; // restart after process death with same extras
    }

    @Override
    public void onDestroy() {
        mCancelled = true;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    // ------------------------------------------------------------ job ------
    private void runJob(String action, String pageUrl, String version, long expectedSize, String sentinel) {
        try {
            Context c = this;
            File base = getExternalFilesDir(null);
            if (base == null) throw new Exception("Penyimpanan tidak tersedia.");
            File cache = getExternalCacheDir();
            if (cache == null) cache = getCacheDir();

            boolean isGameData = !ACTION_ASSETS.equals(action);
            String versionTag = (version == null || version.isEmpty()) ? "1" : version;

            // 1. Resolve page -> direct url (MediaFire), persisted for resume.
            String resolvedKey = "resolved_" + versionTag;
            String direct = prefs(this).getString(resolvedKey, "");
            if (direct == null || direct.isEmpty()) {
                setState(PHASE_CONNECT, 0, 0, expectedSize, "Mencari link download...", null);
                direct = resolveDownloadUrl(pageUrl);
                prefs(this).edit().putString(resolvedKey, direct).apply();
            }
            if (direct == null || direct.isEmpty()) throw new Exception("Link download tidak valid.");

            File part = new File(cache, "game_data_" + versionTag + ".zip.part");
            File finalZip = new File(cache, "game_data_" + versionTag + ".zip");

            // 2. + 3. Download with full resume + retry-with-backoff.
            downloadLoop(direct, part, expectedSize);

            // 5. Verify size BEFORE extract.
            if (expectedSize > 0 && part.length() != expectedSize)
                throw new Exception("Ukuran file tidak sesuai (" + part.length()
                        + " / " + expectedSize + "). Download corrupt.");

            setState(PHASE_EXTRACT, 100, part.length(), part.length(), "Menyiapkan ekstrak...", null);
            updateNotification(buildNotification("Mengekstrak...", 100));
            if (part.renameTo(finalZip)) {
                if (isGameData) {
                    // Delegate extract + sentinel check + .data_ready marker.
                    GameDataManager.finalizeGameDataInstall(c, finalZip, versionTag, sentinel);
                    try { if (finalZip.exists()) finalZip.delete(); } catch (Exception ignored) {}
                    clearPending(c);
                    setState(PHASE_DONE, 100, 100, 100, "Data game siap dimainkan", null);
                } else {
                    // HomeFragment assets job: unzip into game dir, no marker change.
                    File zip = new File(base, "files.zip");
                    if (zip.exists()) zip.delete();
                    if (!finalZip.renameTo(zip)) throw new Exception("Gagal menyiapkan file hasil unduhan.");
                    unzipTo(c, zip, base);
                    if (zip.exists()) zip.delete();
                    clearPending(c);
                    setState(PHASE_DONE, 100, 100, 100, "Game assets terpasang", null);
                }
            } else {
                throw new Exception("Gagal menyiapkan file hasil unduhan.");
            }
        } catch (Exception e) {
            String msg = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
            setState(PHASE_FAILED, sState.percent, sState.bytes, sState.total, "Gagal", msg);
        } finally {
            mRunning = false;
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
    }

    // ------------------------------------------------------------ download -
    /** Full resume loop: Range + .part + retry with backoff. Never deletes the partial file. */
    private void downloadLoop(String directUrl, File part, long expectedSize) throws Exception {
        int attempt = 0;
        while (true) {
            try {
                downloadOnce(directUrl, part, expectedSize);
                return;
            } catch (Exception e) {
                if (mCancelled) throw new Exception("Download dibatalkan.");
                if (isTransient(e) && ++attempt <= MAX_RETRIES) {
                    long backoffMs = RETRY_BACKOFF_MS * (1L << Math.min(attempt - 1, 4));
                    setState(sState.phase, sState.percent, sState.bytes, sState.total,
                            "Terputus — retry " + attempt + "/" + MAX_RETRIES, null);
                    try { Thread.sleep(backoffMs); } catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                    }
                    continue;
                }
                throw e;
            }
        }
    }

    /** True when the error looks transient and is worth retrying with backoff. */
    private static boolean isTransient(Exception e) {
        if (e instanceof SocketTimeoutException) return true;
        if (e instanceof ConnectException) return true;
        if (e instanceof SocketException) return true; // reset / broken pipe
        if (e instanceof java.io.IOException) {
            String m = String.valueOf(e.getMessage()).toLowerCase(Locale.US);
            return m.contains("timeout") || m.contains("reset") || m.contains("broken pipe");
        }
        return false;
    }

    /** One attempt: continue from the current .part length via Range, else start over. */
    private void downloadOnce(String directUrl, File part, long expectedSize) throws Exception {
        long existing = part.exists() ? part.length() : 0L;
        HttpURLConnection conn = connect(directUrl, existing);
        int code = conn.getResponseCode();
        if (code < 200 || code >= 300)
            throw new Exception("Server menolak download (HTTP " + code + ").");

        boolean append = (code == HttpURLConnection.HTTP_PARTIAL);
        if (!append && existing > 0) existing = 0; // server ignored Range: restart from 0

        long total = conn.getContentLengthLong();
        if (append) total += existing;
        if (total <= 0) total = Math.max(existing, expectedSize);

        setState(PHASE_DOWNLOAD, (int) (total > 0 ? Math.min(100, existing * 100 / total) : 0),
                existing, total, "Mengunduh...", null);

        long downloaded = existing;
        long lastUi = 0;
        long startNs = System.nanoTime();
        InputStream in = new BufferedInputStream(conn.getInputStream(), 128 * 1024);
        OutputStream out = new BufferedOutputStream(new FileOutputStream(part, append));
        try {
            byte[] buf = new byte[128 * 1024];
            int n;
            while ((n = in.read(buf)) != -1) {
                if (mCancelled) throw new Exception("Download dibatalkan.");
                out.write(buf, 0, n);
                downloaded += n;
                long now = System.currentTimeMillis();
                if (now - lastUi > 300) {
                    lastUi = now;
                    long elapsed = Math.max(1, (System.nanoTime() - startNs) / 1_000_000_000L);
                    long speed = (downloaded - existing) / elapsed;
                    int pct = total > 0 ? (int) Math.min(100, (downloaded * 100) / total) : 0;
                    setState(PHASE_DOWNLOAD, pct, downloaded, total,
                            fmtBytes(downloaded) + (total > 0 ? " / " + fmtBytes(total) : "")
                                    + "  •  " + fmtBytes(speed) + "/dtk", null);
                    updateNotification(buildNotification(
                            "Mengunduh " + pct + "% (" + fmtBytes(downloaded)
                                    + (total > 0 ? " / " + fmtBytes(total) : "") + ")", pct));
                }
            }
            out.flush();
            ((FileOutputStream) out).getFD().sync();
            setState(PHASE_DOWNLOAD, 100, downloaded, downloaded, "Download selesai", null);
        } finally {
            try { in.close(); } catch (Exception ignored) {}
            try { out.close(); } catch (Exception ignored) {}
            conn.disconnect();
        }
    }

    private HttpURLConnection connect(String url, long rangeStart) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(20000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", "M-SAMP-Launcher");
        c.setInstanceFollowRedirects(true);
        if (rangeStart > 0) c.setRequestProperty("Range", "bytes=" + rangeStart + "-");
        return c;
    }

    /** Resolve a MediaFire file-page URL to a direct download URL. */
    private static String resolveDownloadUrl(String url) throws Exception {
        if (url != null && url.contains("mediafire.com/file/")) {
            String html = GameDataManager.httpGet(url);
            Matcher m = Pattern.compile("https://download\\d*\\.mediafire\\.com/[^\\s\"'<>]+")
                    .matcher(html);
            if (m.find()) return m.group(0);
            throw new Exception("Tidak bisa menemukan link download langsung dari MediaFire.");
        }
        return url;
    }

    // ------------------------------------------------------------ extract --
    /** Shared unzip (assets job): strips the "files/" wrapper like HomeFragment did. */
    private void unzipTo(Context c, File zipFile, File targetDir) throws Exception {
        java.util.zip.ZipFile zf = new java.util.zip.ZipFile(zipFile);
        int totalEntries = zf.size();
        int processed = 0;
        byte[] buffer = new byte[65536];

        java.util.Enumeration<? extends java.util.zip.ZipEntry> entries = zf.entries();
        while (entries.hasMoreElements()) {
            java.util.zip.ZipEntry ze = entries.nextElement();
            processed++;

            String entryName = ze.getName();
            if (entryName.startsWith("files/") || entryName.startsWith("files\\")) {
                entryName = entryName.substring(6);
            }
            if (entryName.isEmpty()) continue;

            File file = new File(targetDir, entryName);
            if (!file.getCanonicalPath().startsWith(targetDir.getCanonicalPath())) continue;

            if (ze.isDirectory()) {
                file.mkdirs();
            } else {
                File parent = file.getParentFile();
                if (parent != null && !parent.exists()) parent.mkdirs();
                java.io.InputStream is = zf.getInputStream(ze);
                java.io.FileOutputStream fos = new java.io.FileOutputStream(file);
                int count;
                while ((count = is.read(buffer)) != -1) fos.write(buffer, 0, count);
                fos.flush();
                fos.close();
                is.close();
            }

            if (totalEntries > 0 && processed % 25 == 0) {
                int pct = (processed * 100) / totalEntries;
                setState(PHASE_EXTRACT, pct, processed, totalEntries,
                        "Mengekstrak " + processed + "/" + totalEntries, null);
            }
        }
        zf.close();
    }

    // ------------------------------------------------------------ notif ----
    private void startForegroundWithNotification(Notification n) {
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } else {
            startForeground(NOTIF_ID, n);
        }
    }

    private void updateNotification(Notification n) {
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        if (nm != null) nm.notify(NOTIF_ID, n);
    }

    private Notification buildNotification(String text, int percent) {
        PendingIntent pi = openMainIntent();
        Notification.Builder b = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);
        return b
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentTitle("M-SAMP: Unduh Data Game")
                .setContentText(text)
                .setProgress(100, percent, percent <= 0)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .setContentIntent(pi)
                .build();
    }

    private PendingIntent openMainIntent() {
        Intent i = new Intent();
        i.setClassName(getPackageName(), "com.sampmobile.xyvern.launcher.MainActivity");
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= 23) flags |= PendingIntent.FLAG_IMMUTABLE;
        return PendingIntent.getActivity(this, 0, i, flags);
    }

    // ------------------------------------------------------------ helpers --
    private static String fmtBytes(long n) {
        if (n < 1024) return n + " B";
        String[] u = {"B", "KB", "MB", "GB"};
        int i = 0; double v = n;
        while (v >= 1024 && i < u.length - 1) { v /= 1024; i++; }
        return String.format(Locale.US, i == 0 ? "%.0f %s" : "%.2f %s", v, u[i]);
    }
}
