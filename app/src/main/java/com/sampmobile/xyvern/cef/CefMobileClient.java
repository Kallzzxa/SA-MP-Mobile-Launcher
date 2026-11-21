package com.sampmobile.xyvern.cef;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.net.Uri;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Locale;
import java.util.zip.CRC32;

import com.sampmobile.xyvern.game.SAMP;
import com.sampmobile.xyvern.R;

/**
 * CefMobile client (M2): UI web dari server SA-MP.
 *
 * Alur (lihat cefmobile/PROTOCOL.md — kontrak wire M1):
 *  1. Saat sudah connect ke server (gameState == CONNECTED dan playerid
 *     valid), ambil GET /cef-mobile/manifest dari http://&lt;server&gt;:8778.
 *  2. Bandingkan dengan cache lokal di &lt;gameDir&gt;/SAMP/cef-mobile/ (+ file
 *     .version): download HANYA file yang crc32-nya beda atau belum ada,
 *     lewat GET /cef-mobile/files/&lt;relpath&gt;. Kalau versi sama dan semua
 *     CRC cocok, tidak ada download sama sekali (relog = zero downloads).
 *  3. Panggil GET /cef-mobile/hello/&lt;playerid&gt; sekali kalau sudah siap.
 *  4. Poll GET /cef-mobile/cmd/&lt;playerid&gt; tiap ~500 ms dan jalankan
 *     perintahnya lewat CefJavaManager (browser id 65000).
 *  5. JS di halaman memanggil window.cef.emit(event, data) -> POST
 *     /cef-mobile/event.
 *  6. Saat disconnect / activity destroy: berhenti poll + destroy browser.
 *
 * Kalau server tidak menjawab (tidak ada plugin CefMobile), client diam
 * dan game jalan seperti biasa: CEF UI hanya tambahan, bukan syarat join.
 * Semua network di background thread; semua WebView di UI thread; semua
 * langkah dibungkus try/catch supaya kegagalan CefMobile tidak pernah
 * crash game.
 */
public class CefMobileClient implements CefJavaManager.MobileEventListener {
    private static final String TAG = "CefMobile";

    /** Browser id khusus CefMobile — tidak boleh bentrok dengan jalur native (0..0xFFFE). */
    public static final int MOBILE_BROWSER_ID = 65000;

    private static final int HTTP_PORT = 8778;
    private static final int CONNECT_TIMEOUT_MS = 3000;
    private static final int READ_TIMEOUT_MS = 3000;
    private static final int POLL_TIMEOUT_MS = 2000;
    private static final int DOWNLOAD_CONNECT_TIMEOUT_MS = 10000;
    private static final int DOWNLOAD_READ_TIMEOUT_MS = 60000;
    private static final long POLL_INTERVAL_MS = 500;
    private static final long RETRY_INTERVAL_MS = 3000;
    private static final int GAMESTATE_CONNECTED = 5;

    private static final String USER_AGENT = "CefMobile";

    private final Activity mActivity;
    private final CefJavaManager mJavaManager;
    private final SAMP mSamp;
    private final File mCacheDir;
    private final File mVersionFile;

    private volatile boolean mRunning;
    private Thread mThread;

    // state sesi aktif (hanya disentuh dari monitor thread, kecuali stop())
    private volatile boolean mSessionActive;
    private String mSessionServer; // "host:port"
    private int mSessionPlayerId = -1;
    private long mNextManifestAttempt;
    private long mNextPoll;

    // progress dialog "Downloading Assets" (hanya disentuh dari UI thread)
    private AlertDialog mProgressDialog;
    private TextView mProgressTotal;
    private ProgressBar mProgressOverall;
    private LinearLayout mProgressList;
    private ArrayList<DownloadRow> mProgressRows;
    private long mProgressTotalBytes;

    /** Satu baris file di dialog download. */
    private static final class DownloadRow {
        TextView status;
        ProgressBar bar;
        long size;
        long done;
        int lastPct = -1;
    }

    public CefMobileClient(Activity activity, CefJavaManager javaManager) {
        this.mActivity = activity;
        this.mJavaManager = javaManager;
        this.mSamp = (activity instanceof SAMP) ? (SAMP) activity : null;

        File root = activity.getExternalFilesDir(null);
        // Sama seperti GameDataManager: data game tinggal di <ext>/SAMP/
        this.mCacheDir = new File(root, "SAMP/cef-mobile");
        this.mVersionFile = new File(mCacheDir, ".version");
    }

    // ------------------------------------------------------------------ lifecycle

    public synchronized void start() {
        if (mRunning) return;
        mRunning = true;
        mThread = new Thread(this::runLoop, "CefMobile");
        mThread.setDaemon(true);
        mThread.start();
        Log.i(TAG, "started, cache=" + mCacheDir.getAbsolutePath());
    }

    public synchronized void stop() {
        mRunning = false;
        if (mThread != null) mThread.interrupt();
        teardownSession();
        dismissProgressUi();
    }

    private void runLoop() {
        while (mRunning) {
            try {
                tick();
            } catch (Exception e) {
                Log.e(TAG, "tick failed", e);
            }
            try {
                Thread.sleep(POLL_INTERVAL_MS);
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    // ------------------------------------------------------------------ monitor tick

    private void tick() {
        String server = safeServer();
        int state = safeGameState();
        int playerId = safePlayerId();

        boolean connected = server != null && !server.isEmpty()
                && state == GAMESTATE_CONNECTED
                && playerId >= 0 && playerId <= 999;
        if (!connected) {
            if (mSessionActive) {
                Log.i(TAG, "disconnected (state=" + state + "), stopping session");
                teardownSession();
            }
            return;
        }

        if (mSessionActive
                && (!server.equals(mSessionServer) || playerId != mSessionPlayerId)) {
            Log.i(TAG, "server/player changed, restarting session");
            teardownSession();
        }

        if (!mSessionActive) {
            long now = System.currentTimeMillis();
            if (now < mNextManifestAttempt) return;
            mNextManifestAttempt = now + RETRY_INTERVAL_MS;
            tryStartSession(server, playerId);
            return;
        }

        long now = System.currentTimeMillis();
        if (now < mNextPoll) return;
        mNextPoll = now + POLL_INTERVAL_MS;
        pollCommands(server, playerId);
    }

    private String safeServer() {
        try {
            return mSamp == null ? null : mSamp.getCefMobileServer();
        } catch (Exception e) {
            return null;
        }
    }

    private int safeGameState() {
        try {
            return mSamp == null ? -1 : mSamp.getCefMobileGameState();
        } catch (Exception e) {
            return -1;
        }
    }

    private int safePlayerId() {
        try {
            return mSamp == null ? -1 : mSamp.getCefMobilePlayerId();
        } catch (Exception e) {
            return -1;
        }
    }

    // nativeGetCefMobileServer() returns "host:gameport" (e.g.
    // "192.168.1.4:7777"). The CefMobile HTTP server lives on HTTP_PORT,
    // so strip the game port before building URLs — otherwise every URL
    // becomes "http://host:7777:8778/..." and new URL() throws
    // MalformedURLException, so the client never reaches the server.
    private static String hostOf(String server) {
        if (server == null) return null;
        int c = server.lastIndexOf(':');
        return c >= 0 ? server.substring(0, c) : server;
    }

    private void teardownSession() {
        mSessionActive = false;
        mSessionServer = null;
        mSessionPlayerId = -1;
        dismissProgressUi();
        try {
            if (mJavaManager != null) mJavaManager.destroyBrowser(MOBILE_BROWSER_ID);
        } catch (Exception e) {
            Log.w(TAG, "teardown destroyBrowser failed", e);
        }
    }

    // ------------------------------------------------------------------ session start (manifest + sync)

    private static final class ManifestFile {
        String name;
        long size;
        long crc32;
    }

    private boolean tryStartSession(String server, int playerId) {
        String base = "http://" + hostOf(server) + ":" + HTTP_PORT;
        JSONObject manifest;
        try {
            manifest = httpGetJson(base + "/cef-mobile/manifest", READ_TIMEOUT_MS);
        } catch (Exception e) {
            Log.i(TAG, "no CefMobile on " + server + ": " + e.getMessage());
            return false;
        }
        if (manifest == null) {
            Log.i(TAG, "no CefMobile on " + server + " (manifest unreachable)");
            return false;
        }

        int version = manifest.optInt("version", 1);
        JSONArray filesJson = manifest.optJSONArray("files");
        if (filesJson == null) {
            Log.w(TAG, "malformed manifest from " + server + " (no files array)");
            return false;
        }

        ArrayList<ManifestFile> files = new ArrayList<>();
        for (int i = 0; i < filesJson.length(); i++) {
            JSONObject f = filesJson.optJSONObject(i);
            if (f == null) continue;
            String name = f.optString("name", "");
            if (name.isEmpty()) continue;
            ManifestFile mf = new ManifestFile();
            mf.name = name;
            mf.size = f.optLong("size", -1);
            try {
                mf.crc32 = Long.parseLong(f.optString("crc32", "-1"));
            } catch (NumberFormatException e) {
                mf.crc32 = -1;
            }
            files.add(mf);
        }

        ArrayList<ManifestFile> toDownload = new ArrayList<>();
        long totalBytes = 0;
        for (ManifestFile mf : files) {
            File cached = new File(mCacheDir, mf.name);
            if (!cached.isFile() || mf.crc32 < 0 || crc32Of(cached) != mf.crc32) {
                toDownload.add(mf);
                if (mf.size > 0) totalBytes += mf.size;
            }
        }

        if (!toDownload.isEmpty()) {
            Log.i(TAG, "downloading " + toDownload.size() + "/" + files.size()
                    + " files (" + totalBytes + " bytes) from " + server);
            showProgressUi(toDownload, totalBytes);
            try {
                for (int i = 0; i < toDownload.size(); i++) {
                    ManifestFile mf = toDownload.get(i);
                    if (!mRunning) return false;
                    downloadFile(base, mf, i);
                    markFileDone(i);
                }
            } catch (Exception e) {
                Log.w(TAG, "sync failed: " + e.getMessage());
                dismissProgressUi();
                return false;
            } finally {
                dismissProgressUi();
            }
        }

        writeCachedVersion(version);

        // juga me-refresh last-seen di server).
        try {
            httpGetJson(base + "/cef-mobile/hello/" + playerId, READ_TIMEOUT_MS);
        } catch (Exception ignored) {
        }

        mSessionServer = server;
        mSessionPlayerId = playerId;
        mSessionActive = true;
        mNextPoll = 0;
        Log.i(TAG, "session started on " + server + " playerid=" + playerId
                + " version=" + version);
        return true;
    }

    // ------------------------------------------------------------------ cmd polling

    private void pollCommands(String server, int playerId) {
        JSONObject resp;
        try {
            resp = httpGetJson("http://" + hostOf(server) + ":" + HTTP_PORT
                    + "/cef-mobile/cmd/" + playerId, POLL_TIMEOUT_MS);
        } catch (Exception e) {
            return; // server HTTP mati sesaat: coba lagi di tick berikutnya
        }
        if (resp == null) return;
        JSONArray cmds = resp.optJSONArray("cmds");
        if (cmds == null) return;
        for (int i = 0; i < cmds.length(); i++) {
            JSONObject c = cmds.optJSONObject(i);
            if (c == null) continue;
            try {
                executeCommand(c);
            } catch (Exception e) {
                Log.w(TAG, "cmd failed: " + c, e);
            }
        }
    }

    private void executeCommand(JSONObject c) {
        String type = c.optString("type", "");
        switch (type) {
            case "create": {
                String url = c.optString("url", "");
                if (url.isEmpty()) break;
                String resolved = resolveUrl(url);
                Log.i(TAG, "cmd create url=" + url + " -> " + resolved);
                mJavaManager.openMobileBrowser(MOBILE_BROWSER_ID, resolved, this);
                break;
            }
            case "show":
                mJavaManager.showBrowser(MOBILE_BROWSER_ID);
                break;
            case "hide":
                mJavaManager.hideBrowser(MOBILE_BROWSER_ID);
                break;
            case "destroy":
                mJavaManager.destroyBrowser(MOBILE_BROWSER_ID);
                break;
            case "focus":
                mJavaManager.setBrowserFocus(MOBILE_BROWSER_ID, c.optBoolean("focus", true));
                break;
            case "event": {
                String event = c.optString("event", "");
                String data = c.optString("data", "");
                if (!event.isEmpty()) {
                    // dispatchServerEvent: window.onCefEvent + window.cef._dispatch
                    mJavaManager.dispatchServerEvent(MOBILE_BROWSER_ID, event, data);
                }
                break;
            }
            default:
                Log.i(TAG, "unknown cmd type ignored: " + type);
                break;
        }
    }

    /** http(s):// atau file:// dipakai apa adanya; sisanya file cache lokal. */
    private String resolveUrl(String url) {
        if (url.startsWith("http://") || url.startsWith("https://")
                || url.startsWith("file://")) {
            return url;
        }
        return Uri.fromFile(new File(mCacheDir, url)).toString();
    }

    // ------------------------------------------------------------------ JS -> server

    @Override
    public void onMobileEvent(int browserId, String event, String data) {
        final String server = mSessionServer;
        final int playerId = mSessionPlayerId;
        if (server == null || playerId < 0) {
            Log.w(TAG, "onMobileEvent dropped (no session): " + event);
            return;
        }
        final String ev = event == null ? "" : event;
        final String dt = data == null ? "" : data;
        Thread t = new Thread(() -> postEvent(server, playerId, ev, dt), "CefMobileEvent");
        t.setDaemon(true);
        t.start();
    }

    private void postEvent(String server, int playerId, String event, String data) {
        HttpURLConnection c = null;
        try {
            JSONObject body = new JSONObject();
            body.put("playerid", playerId);
            body.put("event", event);
            body.put("data", data);
            byte[] payload = body.toString().getBytes("UTF-8");

            c = (HttpURLConnection) new URL("http://" + hostOf(server) + ":" + HTTP_PORT
                    + "/cef-mobile/event").openConnection();
            c.setConnectTimeout(CONNECT_TIMEOUT_MS);
            c.setReadTimeout(READ_TIMEOUT_MS);
            c.setRequestMethod("POST");
            c.setRequestProperty("Content-Type", "application/json");
            c.setRequestProperty("User-Agent", USER_AGENT);
            c.setDoOutput(true);
            c.setFixedLengthStreamingMode(payload.length);
            try (OutputStream out = c.getOutputStream()) {
                out.write(payload);
            }
            int code = c.getResponseCode();
            if (code != 200) {
                Log.w(TAG, "POST /event HTTP " + code + " for event " + event);
            }
        } catch (Exception e) {
            Log.w(TAG, "POST /event failed: " + e.getMessage());
        } finally {
            if (c != null) c.disconnect();
        }
    }

    // ------------------------------------------------------------------ HTTP helpers

    private JSONObject httpGetJson(String urlStr, int readTimeoutMs) throws Exception {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(urlStr).openConnection();
            c.setConnectTimeout(CONNECT_TIMEOUT_MS);
            c.setReadTimeout(readTimeoutMs);
            c.setRequestProperty("User-Agent", USER_AGENT);
            c.setInstanceFollowRedirects(true);
            int code = c.getResponseCode();
            if (code != 200) return null;
            try (InputStream in = new BufferedInputStream(c.getInputStream())) {
                String body = readAllUtf8(in);
                return new JSONObject(body);
            }
        } finally {
            if (c != null) c.disconnect();
        }
    }

    private void downloadFile(String base, ManifestFile mf, int index) throws Exception {
        String encoded = encodeRelPath(mf.name);
        HttpURLConnection c = null;
        File dest = new File(mCacheDir, mf.name);
        File tmp = new File(mCacheDir, mf.name + ".tmp");
        try {
            File parent = dest.getParentFile();
            if (parent != null) parent.mkdirs();
            c = (HttpURLConnection) new URL(base + "/cef-mobile/files/" + encoded).openConnection();
            c.setConnectTimeout(DOWNLOAD_CONNECT_TIMEOUT_MS);
            c.setReadTimeout(DOWNLOAD_READ_TIMEOUT_MS);
            c.setRequestProperty("User-Agent", USER_AGENT);
            c.setInstanceFollowRedirects(true);
            int code = c.getResponseCode();
            if (code != 200) throw new Exception("HTTP " + code + " for " + mf.name);
            try (InputStream in = new BufferedInputStream(c.getInputStream());
                 OutputStream out = new BufferedOutputStream(new FileOutputStream(tmp))) {
                byte[] buf = new byte[64 * 1024];
                int n;
                long bytesRead = 0;
                int lastPct = -1;
                long lastReport = 0;
                while ((n = in.read(buf)) != -1) {
                    out.write(buf, 0, n);
                    bytesRead += n;
                    // Lapor progress hanya saat persen berubah (atau tiap 256 KB
                    if (mf.size > 0) {
                        int pct = (int) Math.min(100, (bytesRead * 100) / mf.size);
                        if (pct != lastPct) {
                            lastPct = pct;
                            postFileProgress(index, bytesRead);
                        }
                    } else if (bytesRead - lastReport >= 256 * 1024) {
                        lastReport = bytesRead;
                        postFileProgress(index, bytesRead);
                    }
                }
            }
            if (mf.size >= 0 && tmp.length() != mf.size) {
                throw new Exception("size mismatch for " + mf.name
                        + " (got " + tmp.length() + ", want " + mf.size + ")");
            }
            if (mf.crc32 >= 0 && crc32Of(tmp) != mf.crc32) {
                throw new Exception("crc mismatch for " + mf.name);
            }
            if (dest.exists() && !dest.delete()) {
                throw new Exception("cannot replace " + mf.name);
            }
            if (!tmp.renameTo(dest)) {
                throw new Exception("cannot store " + mf.name);
            }
        } finally {
            if (c != null) c.disconnect();
            try {
                if (tmp.exists() && !tmp.equals(dest)) tmp.delete();
            } catch (Exception ignored) {
            }
        }
    }

    /** Encode tiap segmen path; '/' pemisah dipertahankan. */
    private static String encodeRelPath(String rel) throws Exception {
        String[] parts = rel.split("/");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < parts.length; i++) {
            if (i > 0) sb.append('/');
            sb.append(URLEncoder.encode(parts[i], "UTF-8").replace("+", "%20"));
        }
        return sb.toString();
    }

    private static String readAllUtf8(InputStream in) throws Exception {
        StringBuilder sb = new StringBuilder();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) != -1) {
            sb.append(new String(buf, 0, n, "UTF-8"));
        }
        return sb.toString();
    }

    // ------------------------------------------------------------------ cache + CRC32

    /**
     * CRC-32/IEEE (java.util.zip.CRC32). Test vector: CRC32("123456789")
     * == 0xCBF43926.
     */
    private static long crc32Of(File f) {
        CRC32 crc = new CRC32();
        try (InputStream in = new BufferedInputStream(new FileInputStream(f))) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) != -1) {
                crc.update(buf, 0, n);
            }
            return crc.getValue();
        } catch (Exception e) {
            return -1;
        }
    }

    private int readCachedVersion() {
        try (InputStream in = new FileInputStream(mVersionFile)) {
            String s = readAllUtf8(in).trim();
            return Integer.parseInt(s);
        } catch (Exception e) {
            return -1;
        }
    }

    private void writeCachedVersion(int version) {
        try {
            mCacheDir.mkdirs();
            try (OutputStream out = new FileOutputStream(mVersionFile)) {
                out.write(Integer.toString(version).getBytes("UTF-8"));
            }
        } catch (Exception e) {
            Log.w(TAG, "write .version failed", e);
        }
    }

    // ------------------------------------------------------------------ progress UI "Downloading Assets"

    // Dialog "Downloading Assets" (cef_mobile_download.xml): satu baris per
    // gagal dibuat, download tetap jalan tanpa UI (log saja).
    private void showProgressUi(final ArrayList<ManifestFile> toDownload, final long totalBytes) {
        mActivity.runOnUiThread(() -> {
            try {
                if (mActivity.isFinishing() || mActivity.isDestroyed()) return;
                if (mProgressDialog != null) return;
                View root = LayoutInflater.from(mActivity)
                        .inflate(R.layout.cef_mobile_download, null);
                mProgressTotal = root.findViewById(R.id.tv_total_percent);
                mProgressOverall = root.findViewById(R.id.pb_overall);
                mProgressList = root.findViewById(R.id.ll_asset_items);
                mProgressRows = new ArrayList<>();
                mProgressTotalBytes = totalBytes;
                for (ManifestFile mf : toDownload) {
                    View row = LayoutInflater.from(mActivity)
                            .inflate(R.layout.cef_mobile_download_item, mProgressList, false);
                    TextView name = row.findViewById(R.id.tv_file_name);
                    TextView status = row.findViewById(R.id.tv_file_status);
                    ProgressBar bar = row.findViewById(R.id.pb_file);
                    name.setText(mf.name);
                    status.setText("Queued");
                    status.setTextColor(Color.parseColor("#71717A"));
                    bar.setProgress(0);
                    mProgressList.addView(row);
                    DownloadRow dr = new DownloadRow();
                    dr.status = status;
                    dr.bar = bar;
                    dr.size = mf.size;
                    mProgressRows.add(dr);
                }
                updateOverallUi();
                mProgressDialog = new AlertDialog.Builder(mActivity)
                        .setView(root)
                        .setCancelable(false)
                        .create();
                mProgressDialog.show();
            } catch (Exception e) {
                Log.w(TAG, "showProgressUi failed", e);
            }
        });
    }

    private void updateOverallUi() {
        if (mProgressTotal == null || mProgressOverall == null || mProgressRows == null) return;
        long done = 0;
        int doneFiles = 0;
        for (DownloadRow r : mProgressRows) {
            if (r.size > 0) done += Math.min(Math.max(0, r.done), r.size);
            if (r.lastPct >= 100) doneFiles++;
        }
        int pct = mProgressTotalBytes > 0
                ? (int) Math.min(100, (done * 100) / mProgressTotalBytes) : 0;
        mProgressTotal.setText(pct + "% • " + doneFiles + "/" + mProgressRows.size() + " Files");
        mProgressOverall.setProgress(pct);
    }

    private void postFileProgress(final int index, final long bytesRead) {
        mActivity.runOnUiThread(() -> {
            if (mProgressRows == null || index < 0 || index >= mProgressRows.size()) return;
            DownloadRow dr = mProgressRows.get(index);
            dr.done = bytesRead;
            if (dr.size > 0) {
                int pct = (int) Math.min(100, (bytesRead * 100) / dr.size);
                if (pct == dr.lastPct) return;
                dr.lastPct = pct;
                dr.bar.setProgress(pct);
                dr.bar.setProgressTintList(ColorStateList.valueOf(Color.parseColor("#DC2626")));
                dr.status.setText(pct + "% (" + formatSize(bytesRead) + ")");
                dr.status.setTextColor(Color.parseColor("#DC2626"));
            } else {
                dr.status.setText(formatSize(bytesRead));
                dr.status.setTextColor(Color.parseColor("#DC2626"));
            }
            updateOverallUi();
        });
    }

    private void markFileDone(final int index) {
        mActivity.runOnUiThread(() -> {
            if (mProgressRows == null || index < 0 || index >= mProgressRows.size()) return;
            DownloadRow dr = mProgressRows.get(index);
            dr.done = Math.max(0, dr.size);
            dr.lastPct = 100;
            dr.bar.setProgress(100);
            dr.bar.setProgressTintList(ColorStateList.valueOf(Color.parseColor("#10B981")));
            dr.status.setText("100% (" + formatSize(dr.size) + ")");
            dr.status.setTextColor(Color.parseColor("#10B981"));
            updateOverallUi();
        });
    }

    private static String formatSize(long bytes) {
        if (bytes < 0) return "?";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format(Locale.US, "%.0f KB", bytes / 1024.0);
        return String.format(Locale.US, "%.1f MB", bytes / (1024.0 * 1024.0));
    }

    private void dismissProgressUi() {
        mActivity.runOnUiThread(() -> {
            try {
                if (mProgressDialog != null) {
                    if (mProgressDialog.isShowing()) mProgressDialog.dismiss();
                    mProgressDialog = null;
                }
            } catch (Exception ignored) {
            }
            mProgressTotal = null;
            mProgressOverall = null;
            mProgressList = null;
            mProgressRows = null;
        });
    }
}
