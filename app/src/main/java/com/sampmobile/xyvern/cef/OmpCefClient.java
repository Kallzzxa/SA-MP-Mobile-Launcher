package com.sampmobile.xyvern.cef;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.sampmobile.xyvern.cef.omp.OmpApp;
import com.sampmobile.xyvern.cef.omp.OmpClient;
import com.sampmobile.xyvern.cef.omp.OmpPacket;
import com.sampmobile.xyvern.cef.omp.OmpResourceManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Glue between the omp-cef transport (com.sampmobile.xyvern.cef.omp) and
 * the Android WebView UI (CefJavaManager). Owned by CefJavaManager: created
 * in startOmpCef(), destroyed in stopOmpCef().
 *
 * Flow: OmpClient handshake -> manifest -> ServerConfig (master key) ->
 * OmpResourceManager download -> DownloadComplete -> OmpApp flushes queued
 * browser creates -> CefJavaManager creates WebViews -> server/user events
 * flow both ways through OmpApp / OmpClient.
 *
 * All UI calls are posted to the main thread (WebView must run there);
 * transport callbacks arrive on OmpClient threads.
 */
public final class OmpCefClient implements OmpClient.Listener, OmpApp.Ui,
        CefJavaManager.OmpEventSink, CefJavaManager.OmpVfsProvider {
    private static final String TAG = "SAMP_OMP_CEF";

    private final CefJavaManager javaManager;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private OmpClient client;
    private OmpResourceManager resources;
    private OmpApp app;
    private volatile boolean stopped;

    public OmpCefClient(CefJavaManager javaManager) {
        this.javaManager = javaManager;
    }

    public void start(String host, int gamePort, int playerId) throws Exception {
        stopped = false;
        File ext = javaManager.getExternalFilesDir();
        File cacheDir = new File(ext, "SAMP/cef_omp/" + host + "_" + gamePort);
        if (!cacheDir.exists()) cacheDir.mkdirs();
        resources = new OmpResourceManager(cacheDir, new OmpResourceManager.ProgressListener() {
            @Override public void onDownloadStart(List<String> files) {
                Log.i(TAG, "omp download start: " + files);
            }
            @Override public void onFileProgress(String file, long downloaded, long total) {
                // Progress is also visible in the PC client's download dialog;
                // on Android the loading.html page shows it via window.__ompcef
                // if the gamemode wires it up. Keep this cheap: no log per chunk.
            }
            @Override public void onDownloadComplete() {
                Log.i(TAG, "omp download complete");
            }
        });
        app = new OmpApp(this);
        javaManager.setOmpVfsProvider(this);
        client = new OmpClient(this);
        client.connect(host, gamePort, playerId);
    }

    public void stop() {
        stopped = true;
        if (client != null) {
            try { client.disconnect(); } catch (Exception ignored) {}
            client = null;
        }
        javaManager.setOmpVfsProvider(null);
    }

    // ---- OmpClient.Listener ----

    @Override public void onConnected(String manifestJson) {
        Log.i(TAG, "omp connected, manifest=" + manifestJson);
        resources.onManifest(manifestJson);
    }

    @Override public void onFailed(String reason) {
        Log.w(TAG, "omp failed: " + reason + " (falling back to legacy MobileCEF if present)");
    }

    @Override public void onDisconnected() {
        Log.i(TAG, "omp disconnected");
        mainHandler.post(() -> {
            if (app != null) app.reset();
        });
    }

    @Override public void onServerConfig(byte[] masterResourceKey, boolean resourcesLoaderUi) {
        resources.setMasterKey(masterResourceKey);
        List<String[]> req = resources.filesToRequest();
        if (req.isEmpty()) {
            // Everything was already cached: skip straight to ready.
            sendDownloadCompleteAndFlush();
        } else {
            client.sendRequestFiles(req);
        }
    }

    @Override public void onFileData(OmpPacket.Packet fileData) {
        try {
            boolean done = resources.onFileData(fileData);
            if (done) sendDownloadCompleteAndFlush();
        } catch (Exception e) {
            Log.e(TAG, "omp onFileData failed: " + e.getMessage());
        }
    }

    @Override public void onEmitEvent(OmpPacket.Packet emit) {
        OmpPacket.Packet reply = app.handleEmit(emit);
        if (reply != null && client != null) {
            client.sendClientEmitEvent(reply.browserId, reply.eventName, reply.args);
        }
    }

    private void sendDownloadCompleteAndFlush() {
        if (client != null) client.sendDownloadComplete();
        Runnable flush = app.onResourcesComplete();
        // Queued creates must run on the main thread (they create WebViews).
        mainHandler.post(flush);
        app.markDownloadCompleteSent();
    }

    // ---- CefJavaManager.OmpVfsProvider ----

    @Override public byte[] getFile(String resource, String internalPath) {
        if (resources == null) return null;
        return resources.getFileContent(resource, internalPath);
    }

    // ---- CefJavaManager.OmpEventSink (JS -> server) ----

    @Override public void sendOmpClientEvent(int browserId, String eventName, String argsJson) {
        if (client == null || stopped) return;
        List<OmpPacket.Argument> args = parseArgsJson(argsJson);
        client.sendClientEmitEvent(browserId, eventName, args);
    }

    static List<OmpPacket.Argument> parseArgsJson(String argsJson) {
        List<OmpPacket.Argument> out = new ArrayList<>();
        if (argsJson == null || argsJson.isEmpty()) return out;
        try {
            JSONArray arr = new JSONArray(argsJson);
            for (int i = 0; i < arr.length(); i++) {
                Object v = arr.opt(i);
                if (v == null || v == JSONObject.NULL) {
                    // omp-cef has no null argument type; send an empty string.
                    out.add(OmpPacket.Argument.str(""));
                } else if (v instanceof Boolean) {
                    out.add(OmpPacket.Argument.bool((Boolean) v));
                } else if (v instanceof Number) {
                    double d = ((Number) v).doubleValue();
                    if (d == Math.floor(d) && Math.abs(d) < 2_000_000_000d) {
                        out.add(OmpPacket.Argument.integer((int) d));
                    } else {
                        out.add(OmpPacket.Argument.flt((float) d));
                    }
                } else {
                    out.add(OmpPacket.Argument.str(String.valueOf(v)));
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "parseArgsJson failed: " + e.getMessage());
        }
        return out;
    }

    // ---- OmpApp.Ui (server -> WebView) ----

    @Override public void createBrowser(int browserId, String url, boolean focused, boolean controlsChat, float width, float height) {
        javaManager.ompCreateBrowser(browserId, url, focused, controlsChat, width, height);
    }

    @Override public void destroyBrowser(int browserId) {
        javaManager.ompDestroyBrowser(browserId);
    }

    @Override public void setBrowserVisible(int browserId, boolean visible) {
        javaManager.ompSetBrowserVisible(browserId, visible);
    }

    @Override public void setBrowserLayer(int browserId, int layer) {
        javaManager.ompSetBrowserLayer(browserId, layer);
    }

    @Override public void setBrowserFocus(int browserId, boolean focused) {
        javaManager.ompSetBrowserFocus(browserId, focused);
    }

    @Override public void loadBrowserUrl(int browserId, String url) {
        javaManager.ompLoadBrowserUrl(browserId, url);
    }

    @Override public void reloadBrowser(int browserId, boolean ignoreCache) {
        javaManager.ompReloadBrowser(browserId, ignoreCache);
    }

    @Override public void emitToBrowser(int browserId, String name, List<OmpPacket.Argument> args) {
        javaManager.ompDispatchToBrowser(browserId, name, argsToJson(args));
    }

    @Override public void exitGame() {
        mainHandler.post(() -> {
            try {
                // Reuse the legacy exit path: finish the activity process.
                // CefJavaManager's bridge does this via the activity; here
                // we approximate with the same call the WebView bridge uses
                // by asking the manager to hide and letting the game handle
                // the rest is not possible, so kill the process like the
                // legacy bridge does.
                android.os.Process.killProcess(android.os.Process.myPid());
            } catch (Exception ignored) {}
        });
    }

    @Override public void setChatInputOpen(boolean open) {
        // WebView input focus is already tracked by CefJavaManager's
        // bootstrap (setChatInputOpen on the legacy bridge). omp chat input
        // state from the server is informational on Android.
    }

    static String argsToJson(List<OmpPacket.Argument> args) {
        StringBuilder b = new StringBuilder("[");
        for (int i = 0; i < args.size(); i++) {
            if (i > 0) b.append(',');
            OmpPacket.Argument a = args.get(i);
            switch (a.type) {
                case OmpPacket.ARG_STRING: {
                    b.append('"');
                    String s = a.stringValue == null ? "" : a.stringValue;
                    for (int j = 0; j < s.length(); j++) {
                        char c = s.charAt(j);
                        if (c == '"') b.append("\\\"");
                        else if (c == '\\') b.append("\\\\");
                        else if (c == '\n') b.append("\\n");
                        else if (c == '\r') b.append("\\r");
                        else b.append(c);
                    }
                    b.append('"');
                    break;
                }
                case OmpPacket.ARG_INTEGER: b.append(a.intValue); break;
                case OmpPacket.ARG_FLOAT: b.append(a.floatValue); break;
                case OmpPacket.ARG_BOOL: b.append(a.boolValue ? "true" : "false"); break;
                default: b.append("null"); break;
            }
        }
        b.append(']');
        return b.toString();
    }
}
