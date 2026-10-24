package com.sampmobile.xyvern.cef;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import java.io.IOException;
import java.io.File;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * SA:MP Mobile CEF - Java Manager for Android WebView Overlay.
 *
 * Mirrors the open.mp omp-cef PC browser API surface so web UIs written for
 * omp-cef keep working on mobile:
 *
 *   window.cef.on(name, callback)        - subscribe to server events
 *   window.cef.off(name, callback?)      - unsubscribe
 *   window.cef.emit(name, ...args)       - send event to client/server
 *   window.cef.set_focus(bool)          - focus / unfocus the browser
 *   window.cef.isChatInputOpen()        - true while an input is focused
 *   window.cef.exitGame()               - close the game
 *
 * Local resources are served through the "cef://" scheme (and the PC-style
 * "http://cef/" scheme) from the app's assets/cef/ folder, exactly like
 * omp-cef serves its local "cef" resource folder on PC.
 */
public class CefJavaManager {
    private static final String TAG = "SAMP_CEF_JAVA";
    private final Activity mActivity;
    private final FrameLayout mRootLayout;
    private WebView mWebView;
    private CefClientManager mClientManager;
    private boolean mIsShown = false;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private final AtomicBoolean mChatInputOpen = new AtomicBoolean(false);

    /** Master switch: when false the WebView is never created (Settings toggle). */
    private static volatile boolean sWebViewEnabled = true;
    public static void setWebViewEnabled(boolean enabled) { sWebViewEnabled = enabled; }
    public static boolean isWebViewEnabled() { return sWebViewEnabled; }

    /**
     * WebView overlay HUD: sentuhan TEMBUS ke game di bawahnya saat tidak ada
     * dialog HTML terbuka. JS mengaktifkan interaksi lewat cef.set_focus(true).
     */
    private static final class CefHudWebView extends WebView {
        private volatile boolean mInteractionEnabled = false;
        CefHudWebView(android.content.Context ctx) { super(ctx); }
        void setInteractionEnabled(boolean enabled) { mInteractionEnabled = enabled; }
        @Override public boolean onTouchEvent(MotionEvent event) {
            if (!mInteractionEnabled) return false; // tembus ke game di bawahnya
            return super.onTouchEvent(event);
        }
    }

    /** Device folder where server-synced CEF UI files are cached. Null if unavailable. */
    private File getUiDir() {
        File ext = mActivity.getExternalFilesDir(null);
        if (ext == null) return null;
        File dir = new File(ext, "SAMP/cef_ui");
        if (!dir.exists()) dir.mkdirs();
        return dir;
    }

    private static byte[] readAllBytes(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
        try { in.close(); } catch (IOException ignored) {}
        return out.toByteArray();
    }

    /** omp-cef compatible JS bootstrap, injected before page scripts run. */
    private static final String BOOTSTRAP_JS =
        "(function(){\n" +
        "  if (window.cef && window.cef.__sampMobile) return;\n" +
        "  var handlers = {};\n" +
        "  var pending = {};\n" +
        "  function flush(name) {\n" +
        "    var q = pending[name];\n" +
        "    if (!q) return;\n" +
        "    delete pending[name];\n" +
        "    var list = handlers[name] || [];\n" +
        "    for (var i = 0; i < q.length; i++) {\n" +
        "      for (var j = 0; j < list.length; j++) { try { list[j].apply(null, q[i]); } catch (e) {} }\n" +
        "    }\n" +
        "  }\n" +
        "  var api = {\n" +
        "    __sampMobile: true,\n" +
        "    on: function(name, cb) {\n" +
        "      if (typeof name !== 'string' || typeof cb !== 'function') return;\n" +
        "      (handlers[name] = handlers[name] || []).push(cb);\n" +
        "      flush(name);\n" +
        "    },\n" +
        "    off: function(name, cb) {\n" +
        "      var list = handlers[name];\n" +
        "      if (!list) return;\n" +
        "      if (typeof cb !== 'function') { handlers[name] = []; return; }\n" +
        "      for (var i = list.length - 1; i >= 0; i--) { if (list[i] === cb) list.splice(i, 1); }\n" +
        "    },\n" +
        "    emit: function(name) {\n" +
        "      var args = Array.prototype.slice.call(arguments, 1);\n" +
        "      try { AndroidCefBridge.emitEvent(String(name), JSON.stringify(args)); } catch (e) {}\n" +
        "    },\n" +
        "    set_focus: function(focused) {\n" +
        "      try { AndroidCefBridge.setBrowserFocus(!!focused); } catch (e) {}\n" +
        "    },\n" +
        "    isChatInputOpen: function() {\n" +
        "      try { return !!AndroidCefBridge.isChatInputOpen(); } catch (e) { return false; }\n" +
        "    },\n" +
        "    exitGame: function() {\n" +
        "      try { AndroidCefBridge.exitGame(); } catch (e) {}\n" +
        "    }\n" +
        "  };\n" +
        "  if (!window.cef) { window.cef = api; }\n" +
        "  else { for (var k in api) { if (typeof window.cef[k] === 'undefined') window.cef[k] = api[k]; } }\n" +
        "  window.__sampcef = {\n" +
        "    dispatch: function(name, argsJson) {\n" +
        "      var args = [];\n" +
        "      try { args = JSON.parse(argsJson || '[]'); if (!Array.isArray(args)) args = [args]; }\n" +
        "      catch (e) { args = [argsJson]; }\n" +
        "      if (name && name.indexOf('__cefui_') === 0 && handleCefUi(name, args)) return;\n" +
        "      var list = handlers[name] || [];\n" +
        "      if (!list.length) { (pending[name] = pending[name] || []).push(args); return; }\n" +
        "      for (var i = 0; i < list.length; i++) { try { list[i].apply(null, args); } catch (e) {} }\n" +
        "    }\n" +
        "  };\n" +
        "  // Server-hosted CEF UI sync (scriptfiles/cef/ on the game server).\n" +
        "  // The server sends __cefui_* events; they are handled here and never\n" +
        "  // reach user handlers.\n" +
        "  var uiState = { file: '', received: 0, total: 0 };\n" +
        "  function ompProgress() {\n" +
        "    try {\n" +
        "      if (window.__ompcef) window.__ompcef.progress(uiState.file, uiState.received, uiState.total, uiState.received, uiState.total);\n" +
        "    } catch (e) {}\n" +
        "  }\n" +
        "  function handleCefUi(name, args) {\n" +
        "    try {\n" +
        "      if (name === '__cefui_version') {\n" +
        "        var serverVer = String(args[0]);\n" +
        "        var localVer = '';\n" +
        "        try { localVer = String(AndroidCefBridge.getUiVersion()); } catch (e) {}\n" +
        "        try { if (window.__ompcef) window.__ompcef.manifest(0, 0); } catch (e) {}\n" +
        "        if (localVer === serverVer && serverVer !== '') { api.emit('__cefui_uptodate'); }\n" +
        "        else { api.emit('__cefui_needfiles'); }\n" +
        "        return true;\n" +
        "      }\n" +
        "      if (name === '__cefui_begin') {\n" +
        "        uiState.file = String(args[0]);\n" +
        "        uiState.total = parseInt(args[1] || '0', 10) || 0;\n" +
        "        uiState.received = 0;\n" +
        "        AndroidCefBridge.uiFileBegin(uiState.file, uiState.total);\n" +
        "        ompProgress();\n" +
        "        return true;\n" +
        "      }\n" +
        "      if (name === '__cefui_chunk') {\n" +
        "        var b64 = String(args[0]);\n" +
        "        AndroidCefBridge.uiFileChunk(b64);\n" +
        "        uiState.received += Math.floor(b64.length * 3 / 4);\n" +
        "        ompProgress();\n" +
        "        return true;\n" +
        "      }\n" +
        "      if (name === '__cefui_end') {\n" +
        "        AndroidCefBridge.uiFileEnd(String(args[0]));\n" +
        "        return true;\n" +
        "      }\n" +
        "      if (name === '__cefui_done') {\n" +
        "        AndroidCefBridge.setUiVersion(String(args[0]));\n" +
        "        try { if (window.__ompcef) window.__ompcef.done(); } catch (e) {}\n" +
        "        api.emit('__cefui_ready');\n" +
        "        return true;\n" +
        "      }\n" +
        "    } catch (e) {}\n" +
        "    return false;\n" +
        "  }\n" +
        "  var inputFocused = false;\n" +
        "  function checkFocus(target) {\n" +
        "    var isInput = !!(target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable));\n" +
        "    if (isInput !== inputFocused) {\n" +
        "      inputFocused = isInput;\n" +
        "      try { AndroidCefBridge.setChatInputOpen(isInput); } catch (e) {}\n" +
        "      try { window.cef.emit('omp:cef:internal:inputFocusChanged', isInput); } catch (e) {}\n" +
        "    }\n" +
        "  }\n" +
        "  document.addEventListener('focusin', function(e) { checkFocus(e.target); }, true);\n" +
        "  document.addEventListener('focusout', function(e) {\n" +
        "    if (inputFocused) {\n" +
        "      inputFocused = false;\n" +
        "      try { AndroidCefBridge.setChatInputOpen(false); } catch (e) {}\n" +
        "      try { window.cef.emit('omp:cef:internal:inputFocusChanged', false); } catch (e) {}\n" +
        "    }\n" +
        "  }, true);\n" +
        "  document.addEventListener('mousedown', function(e) { checkFocus(e.target); }, true);\n" +
        "})();";

    private static final Map<String, String> MIME_TYPES = new HashMap<>();
    static {
        MIME_TYPES.put("html", "text/html");
        MIME_TYPES.put("htm", "text/html");
        MIME_TYPES.put("js", "application/javascript");
        MIME_TYPES.put("mjs", "application/javascript");
        MIME_TYPES.put("css", "text/css");
        MIME_TYPES.put("json", "application/json");
        MIME_TYPES.put("png", "image/png");
        MIME_TYPES.put("jpg", "image/jpeg");
        MIME_TYPES.put("jpeg", "image/jpeg");
        MIME_TYPES.put("gif", "image/gif");
        MIME_TYPES.put("webp", "image/webp");
        MIME_TYPES.put("svg", "image/svg+xml");
        MIME_TYPES.put("ico", "image/x-icon");
        MIME_TYPES.put("woff", "font/woff");
        MIME_TYPES.put("woff2", "font/woff2");
        MIME_TYPES.put("ttf", "font/ttf");
        MIME_TYPES.put("otf", "font/otf");
        MIME_TYPES.put("mp3", "audio/mpeg");
        MIME_TYPES.put("ogg", "audio/ogg");
        MIME_TYPES.put("wav", "audio/wav");
        MIME_TYPES.put("mp4", "video/mp4");
        MIME_TYPES.put("webm", "video/webm");
        MIME_TYPES.put("txt", "text/plain");
        MIME_TYPES.put("xml", "application/xml");
    }

    public CefJavaManager(FrameLayout rootLayout, Activity activity) {
        this.mRootLayout = rootLayout;
        this.mActivity = activity;

        if (sWebViewEnabled) {
            mMainHandler.post(this::initWebView);
        } else {
            Log.i(TAG, "CEF WebView disabled in settings, skipping init");
        }
    }

    private void initWebView() {
        if (mWebView != null) return;

        // Pakai CefHudWebView: sentuhan tembus ke game kecuali dialog HTML sedang fokus.
        mWebView = new CefHudWebView(mActivity);
        mWebView.setFocusable(true);
        mWebView.setFocusableInTouchMode(true);
        mWebView.setClickable(true);
        mWebView.setLongClickable(true);
        mWebView.setBackgroundColor(Color.TRANSPARENT);
        mWebView.setLayerType(View.LAYER_TYPE_HARDWARE, null);

        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setAllowFileAccessFromFileURLs(true);
        settings.setAllowUniversalAccessFromFileURLs(true);
        settings.setMediaPlaybackRequiresUserGesture(false);

        // Enable Cookies
        CookieManager.getInstance().setAcceptCookie(true);
        CookieManager.getInstance().setAcceptThirdPartyCookies(mWebView, true);

        // Web Chrome Client for JS Console Logging
        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
                Log.d(TAG, "[CEF JS Console] " + consoleMessage.message() +
                        " -- From line " + consoleMessage.lineNumber() + " of " + consoleMessage.sourceId());
                return true;
            }
        });

        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, android.graphics.Bitmap favicon) {
                super.onPageStarted(view, url, favicon);
                injectBootstrap(view);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                injectBootstrap(view);
                Log.d(TAG, "WebView page loaded successfully: " + url);
                if (mClientManager != null) {
                    mClientManager.onBrowserInit(true, -1);
                }
            }

            @Override
            public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
                super.onReceivedError(view, errorCode, description, failingUrl);
                Log.e(TAG, "WebView error (" + errorCode + "): " + description);
                if (mClientManager != null) {
                    mClientManager.onBrowserInit(false, errorCode);
                }
            }

            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                WebResourceResponse local = serveCefScheme(request.getUrl());
                return local != null ? local : super.shouldInterceptRequest(view, request);
            }

            @Override
            @SuppressWarnings("deprecation")
            public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
                WebResourceResponse local = serveCefScheme(Uri.parse(url));
                return local != null ? local : super.shouldInterceptRequest(view, url);
            }
        });

        mWebView.setVisibility(View.GONE);
        mRootLayout.addView(mWebView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        ));

        // Register the bridge even if the manager was set before WebView initialization.
        if (mClientManager != null) {
            mWebView.addJavascriptInterface(
                    new CefWebInterface(mClientManager), "AndroidCefBridge");
        }
    }

    private void injectBootstrap(WebView view) {
        try {
            view.evaluateJavascript(BOOTSTRAP_JS, null);
        } catch (Exception e) {
            Log.w(TAG, "bootstrap inject failed: " + e.getMessage());
        }
    }

    /**
     * Serve the omp-cef style local schemes:
     *   cef://index.html        -> device SAMP/cef_ui/index.html (server-synced),
     *                              fallback assets/cef/index.html
     *   http://cef/index.html   -> same (PC omp-cef scheme)
     */
    private WebResourceResponse serveCefScheme(Uri uri) {
        CefContent c = resolveCefContent(uri);
        if (c == null) return null;
        InputStream in = new java.io.ByteArrayInputStream(c.data);
        return new WebResourceResponse(c.mime, encodingOf(c.mime), in);
    }

    /** Resolved cef:// content: raw bytes + mime. Null if not a cef URL or not found. */
    private static class CefContent {
        final byte[] data;
        final String mime;
        CefContent(byte[] data, String mime) { this.data = data; this.mime = mime; }
    }

    private CefContent resolveCefContent(Uri uri) {
        if (uri == null) return null;
        String scheme = uri.getScheme();
        String host = uri.getHost();
        boolean isCefScheme = "cef".equalsIgnoreCase(scheme)
                || ("http".equalsIgnoreCase(scheme) && "cef".equalsIgnoreCase(host));
        if (!isCefScheme) return null;

        String path;
        if ("cef".equalsIgnoreCase(scheme)) {
            // cef://__internal/loading.html -> __internal/loading.html
            // Android Uri parses the first segment as host, so prepend it back.
            String p = uri.getPath();
            if (host != null && !host.isEmpty()) {
                path = host + (p != null ? p : "");
            } else {
                path = p;
            }
        } else {
            // http://cef/index.html -> /index.html
            path = uri.getPath();
        }
        if (path == null || path.isEmpty() || path.equals("/")) path = "/index.html";
        if (path.startsWith("/")) path = path.substring(1);
        // block directory traversal
        if (path.contains("..")) return null;

        // 1) server-synced UI on the device wins
        try {
            File uiDir = getUiDir();
            if (uiDir != null) {
                File dev = new File(uiDir, path);
                if (dev.isFile() && dev.canRead()) {
                    Log.d(TAG, "serving cef:// -> " + dev.getAbsolutePath());
                    return new CefContent(readAllBytes(new java.io.FileInputStream(dev)), mimeOf(path));
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "cef device file error: " + path);
        }

        // 2) fallback to bundled assets/cef/
        String assetPath = "cef/" + path;
        try {
            AssetManager am = mActivity.getAssets();
            InputStream in = am.open(assetPath);
            try {
                Log.d(TAG, "serving cef:// -> assets/" + assetPath);
                return new CefContent(readAllBytes(in), mimeOf(path));
            } finally {
                try { in.close(); } catch (IOException ignored) {}
            }
        } catch (IOException e) {
            Log.w(TAG, "cef asset missing: " + assetPath);
            return null;
        }
    }

    private static String mimeOf(String path) {
        String ext = "";
        int dot = path.lastIndexOf('.');
        if (dot >= 0) ext = path.substring(dot + 1).toLowerCase();
        String mime = MIME_TYPES.get(ext);
        return mime != null ? mime : "application/octet-stream";
    }

    private static String encodingOf(String mime) {
        // text types get a charset, binary types must pass null encoding
        return (mime.startsWith("text/") || mime.equals("application/javascript")
                || mime.equals("application/json") || mime.equals("image/svg+xml")
                || mime.equals("application/xml")) ? "UTF-8" : null;
    }

    public void setClientManager(CefClientManager clientManager) {
        this.mClientManager = clientManager;
        mMainHandler.post(() -> {
            if (mWebView != null && clientManager != null) {
                mWebView.addJavascriptInterface(
                        new CefWebInterface(clientManager), "AndroidCefBridge");
            }
        });
    }

    /** Dispatch a server -> browser CEF event into window.cef handlers. */
    public void dispatchServerEvent(String eventName, String argsJson) {
        if (eventName == null) return;
        final String js = "window.__sampcef && window.__sampcef.dispatch("
                + jsString(eventName) + "," + jsString(argsJson == null ? "[]" : argsJson) + ");";
        mMainHandler.post(() -> {
            if (mWebView != null) {
                try {
                    mWebView.evaluateJavascript(js, null);
                } catch (Exception e) {
                    Log.w(TAG, "dispatchServerEvent failed: " + e.getMessage());
                }
            }
        });
    }

    private static String jsString(String s) {
        StringBuilder b = new StringBuilder(s.length() + 2);
        b.append('\'');
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '\\': b.append("\\\\"); break;
                case '\'': b.append("\\'"); break;
                case '\n': b.append("\\n"); break;
                case '\r': b.append("\\r"); break;
                case '\u2028': b.append("\\u2028"); break;
                case '\u2029': b.append("\\u2029"); break;
                default: b.append(c);
            }
        }
        b.append('\'');
        return b.toString();
    }

    public void loadUrl(String url) {
        mMainHandler.post(() -> {
            if (mWebView == null) return;
            // Load cef:// pages by injecting content directly: WebView's
            // shouldInterceptRequest is unreliable for the main frame with
            // custom schemes on some devices/Android versions ("webpage not
            // available"). Sub-resources still go through the interceptor.
            try {
                CefContent c = resolveCefContent(Uri.parse(url));
                if (c != null && c.mime.startsWith("text/html")) {
                    String html = new String(c.data, java.nio.charset.StandardCharsets.UTF_8);
                    mWebView.loadDataWithBaseURL(url, html, "text/html", "UTF-8", null);
                    return;
                }
            } catch (Exception e) {
                Log.w(TAG, "cef direct load failed, falling back to loadUrl: " + e.getMessage());
            }
            mWebView.loadUrl(url);
        });
    }

    public void showBrowserView() {
        mMainHandler.post(() -> {
            if (mWebView != null) {
                mWebView.setAlpha(0f);
                mWebView.setVisibility(View.VISIBLE);
                mWebView.setFocusable(true);
                mWebView.setFocusableInTouchMode(true);
                mWebView.animate().alpha(1f).setDuration(200).start();
                mIsShown = true;
            }
        });
    }

    public void hideBrowserView() {
        mMainHandler.post(() -> {
            if (mWebView != null) {
                mWebView.animate().alpha(0f).setDuration(150).withEndAction(() -> {
                    mWebView.setVisibility(View.GONE);
                    mIsShown = false;
                }).start();
            }
        });
    }

    public boolean isShow() {
        return mIsShown;
    }

    public WebView getWebView() {
        return mWebView;
    }

    /**
     * JavaScript Bridge Interface attached to WebView (window.AndroidCefBridge)
     */
    public class CefWebInterface {
        private final CefClientManager mClientManager;

        public CefWebInterface(CefClientManager clientManager) {
            this.mClientManager = clientManager;
        }

        /** Legacy single-event bridge. */
        @android.webkit.JavascriptInterface
        public void sendEvent(String eventName, String eventDataJson) {
            emitEvent(eventName, eventDataJson);
        }

        /** omp-cef style: window.cef.emit(name, ...args) with args as JSON array. */
        @android.webkit.JavascriptInterface
        public void emitEvent(String eventName, String argsJson) {
            if (mClientManager != null) {
                mClientManager.sendClientEvent(eventName, argsJson == null ? "[]" : argsJson);
            }
        }

        @android.webkit.JavascriptInterface
        public void setChatInputOpen(boolean open) {
            mChatInputOpen.set(open);
        }

        @android.webkit.JavascriptInterface
        public boolean isChatInputOpen() {
            return mChatInputOpen.get();
        }

        @android.webkit.JavascriptInterface
        public void setBrowserFocus(boolean focused) {
            mMainHandler.post(() -> {
                if (mWebView == null) return;
                if (focused) {
                    mWebView.requestFocus();
                    mWebView.requestFocusFromTouch();
                } else {
                    mWebView.clearFocus();
                }
                if (mWebView instanceof CefHudWebView) ((CefHudWebView) mWebView).setInteractionEnabled(focused);
            });
        }

        @android.webkit.JavascriptInterface
        public void exitGame() {
            mMainHandler.post(() -> {
                try {
                    mActivity.finishAffinity();
                } catch (Exception ignored) {}
                android.os.Process.killProcess(android.os.Process.myPid());
            });
        }

        // ---------------------------------------------------------------
        // Server-hosted CEF UI sync (scriptfiles/cef/ on the game server).
        // Files are cached in <game>/SAMP/cef_ui/ and served via cef://.
        // ---------------------------------------------------------------
        private java.io.OutputStream mUiOut;
        private String mUiFileName = "";

        private File uiSafeFile(String name) {
            if (name == null) return null;
            name = name.replace('\\', '/');
            while (name.startsWith("/")) name = name.substring(1);
            if (name.isEmpty() || name.contains("..")) return null;
            try {
                File dir = getUiDir();
                File f = new File(dir, name);
                String canonDir = dir.getCanonicalPath() + java.io.File.separator;
                String canonFile = f.getCanonicalPath();
                if (!canonFile.startsWith(canonDir)) return null;
                return f;
            } catch (Exception e) {
                return null;
            }
        }

        @android.webkit.JavascriptInterface
        public String getUiVersion() {
            try {
                File ver = new File(getUiDir(), ".version");
                if (!ver.isFile()) return "";
                byte[] buf = new byte[(int) Math.min(ver.length(), 64)];
                try (java.io.FileInputStream fis = new java.io.FileInputStream(ver)) {
                    int n = fis.read(buf);
                    if (n > 0) return new String(buf, 0, n, "UTF-8").trim();
                }
            } catch (Exception e) {
                Log.w(TAG, "getUiVersion failed: " + e.getMessage());
            }
            return "";
        }

        @android.webkit.JavascriptInterface
        public boolean setUiVersion(String version) {
            try {
                File ver = new File(getUiDir(), ".version");
                try (java.io.FileOutputStream fos = new java.io.FileOutputStream(ver)) {
                    fos.write((version == null ? "" : version).getBytes("UTF-8"));
                }
                return true;
            } catch (Exception e) {
                Log.w(TAG, "setUiVersion failed: " + e.getMessage());
                return false;
            }
        }

        @android.webkit.JavascriptInterface
        public boolean uiFileBegin(String name, long totalBytes) {
            try {
                if (mUiOut != null) { try { mUiOut.close(); } catch (Exception ignored) {} mUiOut = null; }
                File f = uiSafeFile(name);
                if (f == null) return false;
                File parent = f.getParentFile();
                if (parent != null && !parent.exists()) parent.mkdirs();
                mUiOut = new java.io.BufferedOutputStream(new java.io.FileOutputStream(f));
                mUiFileName = name;
                return true;
            } catch (Exception e) {
                Log.w(TAG, "uiFileBegin failed: " + e.getMessage());
                return false;
            }
        }

        @android.webkit.JavascriptInterface
        public boolean uiFileChunk(String base64) {
            try {
                if (mUiOut == null || base64 == null) return false;
                byte[] data = android.util.Base64.decode(base64, android.util.Base64.DEFAULT);
                mUiOut.write(data);
                return true;
            } catch (Exception e) {
                Log.w(TAG, "uiFileChunk failed: " + e.getMessage());
                return false;
            }
        }

        @android.webkit.JavascriptInterface
        public boolean uiFileEnd(String name) {
            try {
                if (mUiOut != null) { mUiOut.flush(); mUiOut.close(); mUiOut = null; }
                mUiFileName = "";
                return true;
            } catch (Exception e) {
                Log.w(TAG, "uiFileEnd failed: " + e.getMessage());
                return false;
            }
        }
    }
}
