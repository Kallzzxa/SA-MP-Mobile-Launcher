package com.sampmobile.xyvern.cef;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.CookieManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import org.json.JSONArray;
import org.json.JSONException;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

/**
 * Mengelola browser CefMobile (WebView) — UI web dari server SA-MP.
 *
 * Browser CefMobile (plugin server, port 8778) dan browser omp-cef
 * (plugin cef.so v1.3.0, transport UDP game+2) hidup berdampingan di sini.
 * Satu browser dipakai CefMobileClient (id 65000); browser omp-cef dibuat
 * lewat startOmpCef() dan dikelola OmpCefClient (id 1..9 untuk RP).
 *
 * - id 0..0xFFFE = satu browser, {@link #BROWSER_ALL} = semua browser.
 * - id yang lebih besar digambar di atas id yang lebih kecil.
 * - Semua state hanya disentuh dari main thread (setiap public method memakai mMainHandler.post).
 * - Event dari server yang tiba sebelum halaman selesai dimuat ditahan lalu dikirim saat onPageFinished.
 */
public class CefJavaManager {
    private static final String TAG = "SAMP_CEF_JAVA";

    /** target "semua browser". */
    public static final int BROWSER_ALL = 0xFFFF;
    /** batas jumlah browser per client (satu WebView memakan memori GPU). */
    public static final int MAX_BROWSERS = 32;
    private static final int MAX_PENDING_EVENTS = 64;

    /**
     * Objek JS "cef" untuk HTML: cef.emit("nama", "data") mengirim event/klik
     * ke server (OnCefMobileEvent di Pawn). data berupa object otomatis
     * di-JSON.stringify. cef.on("nama", fn) menerima event dari server
     * (CefMobile_SendEvent). Tidak menimpa "cef" buatan HTML sendiri.
     * _dispatch ditambahkan supaya event dari server juga sampai ke halaman
     * yang hanya mendengarkan lewat cef.on/_dispatch.
     */
    private static final String CEF_MOBILE_SHIM_JS =
            "(function(){var b=window.CefMobileBridge;if(!b||(window.cef&&window.cef.emit))return;"
          + "window.cef={emit:function(n,d){if(d===undefined||d===null)d='';"
          + "else if(typeof d!=='string')d=JSON.stringify(d);b.postEvent(String(n),d);},"
          + "on:function(n,f){window.addEventListener(n,function(e){f(e.detail);});},"
          + "_dispatch:function(e,d){window.dispatchEvent(new CustomEvent(e,{detail:d}));"
          + "if(typeof window.onCefEvent===\"function\"){window.onCefEvent(e,d);}},"
          + "id:b.getBrowserId()};})();";

    private static final String OMP_BOOTSTRAP_JS =
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
        "  else if (!window.cef.__sampMobile) { window.cef = api; }\n" +
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

    /** Dipanggil saat JS di browser CefMobile memanggil window.cef.emit(...). */
    public interface MobileEventListener {
        void onMobileEvent(int browserId, String event, String data);
    }

    /**
     * WebView yang bisa "tembus sentuhan". Kalau touch dimatikan, dispatchTouchEvent
     * return false sehingga event diteruskan ke view di bawahnya (UI game / SurfaceView
     * / Activity.onTouchEvent -> native).
     */
    private static class CefWebView extends WebView {
        private volatile boolean mTouchEnabled = true;

        CefWebView(Context context) {
            super(context);
        }

        void setTouchEnabled(boolean enabled) {
            mTouchEnabled = enabled;
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent ev) {
            if (!mTouchEnabled) return false;
            return super.dispatchTouchEvent(ev);
        }
    }

    private static final class Browser {
        final int id;
        final CefWebView view;
        boolean shown;
        boolean focused;
        boolean pageLoaded;
        boolean bridgeAttached;
        MobileEventListener mobileListener;
        boolean isOmp;
        boolean ompBridgeAttached;
        final ArrayList<String[]> pendingEvents = new ArrayList<>();

        Browser(int id, CefWebView view) {
            this.id = id;
            this.view = view;
        }
    }

    private interface BrowserOp {
        void run(Browser b);
    }

    private final Activity mActivity;
    private final FrameLayout mRootLayout;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    // hanya diakses dari main thread. TreeMap = urut id (id besar di atas)
    private final TreeMap<Integer, Browser> mBrowsers = new TreeMap<>();

    private CefWebView mNotifyWebView;
    private boolean mNotifyWebViewReady = false;

    // true saat menu pause / peta game terbuka: semua UI CEF disembunyikan sementara
    private boolean mGamePaused = false;

    // ---- omp-cef Android port (aurora-mp/omp-cef client, via WebView) ----
    /** VFS lookup for http://cef/<resource>/<internalPath>, supplied by OmpCefClient. */
    public interface OmpVfsProvider { byte[] getFile(String resource, String internalPath); }
    /** JS -> server event sink for omp browsers. */
    public interface OmpEventSink { void sendOmpClientEvent(int browserId, String eventName, String argsJson); }
    private volatile OmpVfsProvider mOmpVfsProvider;
    private volatile OmpEventSink mOmpEventSink;
    private volatile OmpCefClient mOmpClient;
    public void setOmpVfsProvider(OmpVfsProvider provider) { mOmpVfsProvider = provider; }
    public void setOmpEventSink(OmpEventSink sink) { mOmpEventSink = sink; }
    public java.io.File getExternalFilesDir() { return mActivity.getExternalFilesDir(null); }

    public CefJavaManager(FrameLayout rootLayout, Activity activity) {
        this.mRootLayout = rootLayout;
        this.mActivity = activity;

        // membuat WebView pertama juga "memanaskan" engine Chromium, jadi browser berikutnya lebih cepat
        mMainHandler.post(this::initNotifyWebView);
    }

    // ------------------------------------------------------------------ setup

    private void attachMobileBridgeIfNeeded(Browser b) {
        if (b.bridgeAttached || b.mobileListener == null) return;
        b.view.addJavascriptInterface(new CefMobileBridge(b.mobileListener, b.id), "CefMobileBridge");
        b.bridgeAttached = true;
    }

    private void attachOmpBridgeIfNeeded(Browser b) {
        if (b.ompBridgeAttached) return;
        b.view.addJavascriptInterface(new OmpBridge(this, b.id), "AndroidCefBridge");
        b.ompBridgeAttached = true;
    }

    private int dpToPx(int dp) {
        float density = mActivity.getResources().getDisplayMetrics().density;
        return Math.round(dp * density);
    }

    private void initNotifyWebView() {
        if (mNotifyWebView != null) return;

        mNotifyWebView = new CefWebView(mActivity);
        // overlay notifikasi hanya tampilan, tidak boleh menelan sentuhan
        mNotifyWebView.setTouchEnabled(false);
        mNotifyWebView.setFocusable(false);
        mNotifyWebView.setClickable(false);
        mNotifyWebView.setBackgroundColor(Color.TRANSPARENT);
        mNotifyWebView.setLayerType(View.LAYER_TYPE_SOFTWARE, null);

        WebSettings settings = mNotifyWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);

        mNotifyWebView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                mNotifyWebViewReady = true;
                Log.i(TAG, "Notify overlay WebView ready");
            }
        });

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dpToPx(300), dpToPx(500)
        );
        params.gravity = android.view.Gravity.TOP | android.view.Gravity.START;
        mRootLayout.addView(mNotifyWebView, params);

        mNotifyWebView.loadUrl("file:///android_asset/html/notify.html");
    }

    // ------------------------------------------------------------------ browser lifecycle (main thread)

    private Browser createBrowser(final int id, final MobileEventListener mobileListener) {
        if (mBrowsers.size() >= MAX_BROWSERS) {
            Log.e(TAG, "createBrowser(" + id + ") ditolak: sudah mencapai MAX_BROWSERS=" + MAX_BROWSERS);
            return null;
        }

        CefWebView wv = new CefWebView(mActivity);
        wv.setTouchEnabled(false);
        wv.setBackgroundColor(Color.TRANSPARENT);
        wv.setLayerType(View.LAYER_TYPE_HARDWARE, null);

        WebSettings settings = wv.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setAllowFileAccessFromFileURLs(true);
        settings.setAllowUniversalAccessFromFileURLs(true);
        settings.setMediaPlaybackRequiresUserGesture(false);

        CookieManager.getInstance().setAcceptCookie(true);
        CookieManager.getInstance().setAcceptThirdPartyCookies(wv, true);

        final Browser b = new Browser(id, wv);
        b.mobileListener = mobileListener;

        wv.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
                Log.d(TAG, "[" + id + "] console says " + consoleMessage.message() +
                        " -- From line " + consoleMessage.lineNumber() + " of " + consoleMessage.sourceId());
                return true;
            }
        });

        wv.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageCommitVisible(WebView view, String url) {
                super.onPageCommitVisible(view, url);
                view.evaluateJavascript(b.isOmp ? OMP_BOOTSTRAP_JS : CEF_MOBILE_SHIM_JS, null);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                if (mBrowsers.get(id) != b) return; // browser sudah di-destroy
                view.evaluateJavascript(b.isOmp ? OMP_BOOTSTRAP_JS : CEF_MOBILE_SHIM_JS, null);
                Log.d(TAG, "[" + id + "] Page loaded: " + url);

                b.pageLoaded = true;
                flushPendingEvents(b);
            }

            @Override
            @SuppressWarnings("deprecation")
            public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
                super.onReceivedError(view, errorCode, description, failingUrl);
                if (mBrowsers.get(id) != b) return;
                Log.e(TAG, "[" + id + "] WebView error (" + errorCode + "): " + description + " url=" + failingUrl);
            }

            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                WebResourceResponse local = serveOmpVfs(request.getUrl());
                return local != null ? local : super.shouldInterceptRequest(view, request);
            }

            @Override
            @SuppressWarnings("deprecation")
            public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
                WebResourceResponse local = serveOmpVfs(Uri.parse(url));
                return local != null ? local : super.shouldInterceptRequest(view, url);
            }
        });

        wv.setVisibility(View.GONE);
        mRootLayout.addView(wv, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        ));

        mBrowsers.put(id, b);
        attachMobileBridgeIfNeeded(b);
        reorderViews();

        Log.i(TAG, "Browser " + id + " dibuat (total=" + mBrowsers.size() + ")");
        return b;
    }

    // id kecil di bawah, id besar di atas; overlay notifikasi selalu paling atas
    private void reorderViews() {
        for (Browser b : mBrowsers.values()) b.view.bringToFront();
        if (mNotifyWebView != null) mNotifyWebView.bringToFront();
    }

    private void destroyInternal(Browser b) {
        Log.i(TAG, "Destroy browser " + b.id);
        mBrowsers.remove(b.id);
        b.pendingEvents.clear();

        b.view.animate().cancel();
        b.view.stopLoading();
        b.view.setWebViewClient(new WebViewClient());
        b.view.setWebChromeClient(null);
        b.view.removeJavascriptInterface("CefMobileBridge");
        b.view.removeJavascriptInterface("AndroidCefBridge");

        ViewGroup parent = (ViewGroup) b.view.getParent();
        if (parent != null) parent.removeView(b.view);
        b.view.destroy();
    }

    private void startLoad(Browser b, String url) {
        b.pageLoaded = false;
        b.pendingEvents.clear(); // event lama ditujukan ke halaman yang akan diganti
        Log.d(TAG, "[" + b.id + "] Loading URL: " + url);
        b.view.loadUrl(url);
    }

    private void showInternal(Browser b) {
        if (mGamePaused) {
            // menu pause terbuka: catat bahwa browser ini harus tampil, wujudkan saat game dilanjutkan
            b.shown = true;
            return;
        }
        if (b.shown && b.view.getVisibility() == View.VISIBLE) return;

        b.shown = true;
        b.view.animate().cancel();
        b.view.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        b.view.setAlpha(0f);
        b.view.setVisibility(View.VISIBLE);
        b.view.animate().alpha(1f).setDuration(200).start();
        Log.i(TAG, "[" + b.id + "] Showing WebView");
    }

    private void hideInternal(final Browser b) {
        if (!b.shown) return;

        b.shown = false;
        b.view.animate().cancel();
        b.view.animate().alpha(0f).setDuration(150).withEndAction(() -> {
            if (!b.shown) { // belum di-show lagi selama animasi
                b.view.setVisibility(View.GONE);
                b.view.setLayerType(View.LAYER_TYPE_NONE, null);
                mRootLayout.invalidate();
            }
        }).start();
        Log.i(TAG, "[" + b.id + "] Hiding WebView");
    }

    private void forTargets(int id, String what, BrowserOp op) {
        if (id == BROWSER_ALL) {
            for (Browser b : new ArrayList<>(mBrowsers.values())) op.run(b);
            return;
        }
        Browser b = mBrowsers.get(id);
        if (b == null) {
            Log.w(TAG, what + ": browser " + id + " tidak ada (belum di-Init atau sudah di-Destroy)");
            return;
        }
        op.run(b);
    }

    // ------------------------------------------------------------------ public API (thread-safe)

    public void destroyBrowser(final int id) {
        mMainHandler.post(() -> forTargets(id, "destroyBrowser", this::destroyInternal));
    }

    /**
     * Buat (kalau belum ada) browser CefMobile lalu muat url dan tampilkan.
     * Dipanggil lagi = muat ulang url. Browser ini memakai CefMobileBridge +
     * CEF_MOBILE_SHIM_JS. Dipanggil dari CefMobileClient.
     */
    public void openMobileBrowser(final int id, final String url, final MobileEventListener listener) {
        mMainHandler.post(() -> {
            if (id < 0 || id >= BROWSER_ALL) {
                Log.e(TAG, "openMobileBrowser: browser id " + id + " tidak valid");
                return;
            }
            if (url == null || url.isEmpty()) {
                Log.e(TAG, "openMobileBrowser: url is null/empty");
                return;
            }
            if (listener == null) {
                Log.e(TAG, "openMobileBrowser: listener is null");
                return;
            }

            Browser b = mBrowsers.get(id);
            if (b == null) b = createBrowser(id, listener);
            if (b == null) return;

            b.mobileListener = listener;
            attachMobileBridgeIfNeeded(b);
            b.focused = true;
            b.view.setTouchEnabled(true);
            startLoad(b, url);
            showInternal(b);
        });
    }

    public void showBrowser(final int id) {
        mMainHandler.post(() -> forTargets(id, "showBrowser", this::showInternal));
    }

    public void hideBrowser(final int id) {
        mMainHandler.post(() -> forTargets(id, "hideBrowser", this::hideInternal));
    }

    /** Ganti url browser yang sudah ada. */
    public void loadUrl(final int id, final String url) {
        if (url == null || url.isEmpty()) {
            Log.e(TAG, "loadUrl failed: url is null/empty");
            return;
        }
        mMainHandler.post(() -> forTargets(id, "loadUrl", b -> startLoad(b, url)));
    }

    /** fokus = HTML menerima sentuhan; tidak fokus = sentuhan tembus ke game. */
    public void setBrowserFocus(final int id, final boolean isFocused) {
        mMainHandler.post(() -> forTargets(id, "setBrowserFocus", b -> {
            b.focused = isFocused;
            b.view.setTouchEnabled(isFocused);
            if (isFocused) {
                b.view.requestFocus();
            } else {
                b.view.clearFocus();
            }
            Log.d(TAG, "[" + b.id + "] Browser focus set to: " + isFocused);
        }));
    }

    /**
     * Dipanggil saat menu pause / peta game dibuka (paused = true) atau ditutup.
     * Semua UI CEF disembunyikan selama pause, lalu dikembalikan sesuai status show/hide dari server.
     */
    public void setGamePaused(final boolean paused) {
        mMainHandler.post(() -> {
            if (mGamePaused == paused) return;
            mGamePaused = paused;
            Log.i(TAG, "Game paused=" + paused + " -> UI CEF " + (paused ? "disembunyikan" : "dikembalikan"));

            for (Browser b : mBrowsers.values()) applyPauseVisibility(b);

            if (mNotifyWebView != null) {
                mNotifyWebView.setVisibility(paused ? View.INVISIBLE : View.VISIBLE);
            }
        });
    }

    private void applyPauseVisibility(Browser b) {
        if (mGamePaused) {
            if (b.view.getVisibility() == View.VISIBLE) b.view.setVisibility(View.INVISIBLE);
        } else if (b.shown) {
            b.view.animate().cancel();
            b.view.setAlpha(1f);
            b.view.setVisibility(View.VISIBLE);
        }
    }

    /** true kalau ada minimal satu browser yang sedang tampil. Hanya untuk main thread. */
    public boolean isShow() {
        for (Browser b : mBrowsers.values()) {
            if (b.shown) return true;
        }
        return false;
    }


    // ---------------------------------------------------------------
    // omp-cef port: browser management lewat Browser map yang sama.
    // Browser omp-cef dibuat lewat startOmpCef() (dipanggil dari native
    // saat game connect); browser CefMobile (id 65000) tidak tersentuh.
    // World2D / WorldObject3D tidak pernah dibuat di sini: OmpApp sudah
    // menolaknya dengan BrowserCreateResult ERROR_GENERIC.
    // ---------------------------------------------------------------

    /** Start the omp-cef client for the current game connection. Called from native code. */
    public void startOmpCef(String host, int gamePort, int playerId) {
        mMainHandler.post(() -> {
            if (mOmpClient != null) {
                try { mOmpClient.stop(); } catch (Exception ignored) {}
                mOmpClient = null;
            }
            try {
                OmpCefClient client = new OmpCefClient(this);
                mOmpClient = client;
                setOmpEventSink(client);
                client.start(host, gamePort, playerId);
            } catch (Exception e) {
                Log.e(TAG, "startOmpCef failed: " + e.getMessage());
            }
        });
    }

    /** Stop the omp-cef client (game disconnect / restart). Called from native code. */
    public void stopOmpCef() {
        mMainHandler.post(() -> {
            if (mOmpClient != null) {
                try { mOmpClient.stop(); } catch (Exception ignored) {}
                mOmpClient = null;
            }
            setOmpEventSink(null);
            for (Browser b : new ArrayList<>(mBrowsers.values())) {
                if (b.isOmp) destroyInternal(b);
            }
        });
    }

    private Browser createOmpBrowser(final int id) {
        Browser b = mBrowsers.get(id);
        if (b != null) return b;
        b = createBrowser(id, null);
        if (b == null) return null;
        b.isOmp = true;
        attachOmpBridgeIfNeeded(b);
        return b;
    }

    /**
     * Sisipkan OMP_BOOTSTRAP_JS ke dalam HTML supaya window.cef (yang asli,
     * bicara ke AndroidCefBridge) sudah ada SEBELUM script halaman jalan.
     * Sebelumnya bootstrap baru disuntik di onPageFinished, jadi script
     * halaman (mis. js/cef-bridge.js) duluan melihat window.cef belum ada,
     * membuat stub yang cuma console.log, dan stub itu bertahan karena
     * merge bootstrap lama hanya mengisi key yang undefined (on/emit stub
     * tidak pernah tertimpa). Akibatnya RP.emit/RP.on tidak pernah sampai
     * ke server — tombol DAFTAR/MASUK tidak berfungsi sama sekali.
     * Dipakai di ompLoadInto (loadDataWithBaseURL) dan serveOmpVfs
     * (navigasi antar halaman), jadi semua halaman omp dapat window.cef
     * yang asli sejak parse. onPageFinished tetap menyuntik sebagai
     * fallback; guard __sampMobile di bootstrap mencegah instalasi ganda.
     */
    private static String injectOmpBootstrap(String html) {
        if (html == null) return null;
        String script = "<script>" + OMP_BOOTSTRAP_JS + "</script>";
        String lower = html.toLowerCase(java.util.Locale.ROOT);
        int head = lower.indexOf("<head");
        if (head >= 0) {
            int end = html.indexOf('>', head);
            if (end >= 0) return html.substring(0, end + 1) + script + html.substring(end + 1);
        }
        int doctype = lower.indexOf("<!doctype");
        if (doctype >= 0) {
            int end = html.indexOf('>', doctype);
            if (end >= 0) return html.substring(0, end + 1) + script + html.substring(end + 1);
        }
        return script + html;
    }

    private void ompLoadInto(Browser b, String url) {
        try {
            byte[] data = null;
            String mime = null;
            Uri uri = Uri.parse(url);
            if ("http".equalsIgnoreCase(uri.getScheme()) && "cef".equalsIgnoreCase(uri.getHost())) {
                String rawPath = uri.getPath();
                if (rawPath != null && rawPath.length() > 1) {
                    String rel = rawPath.substring(1);
                    int slash = rel.indexOf('/');
                    if (slash > 0 && !rel.substring(slash + 1).contains("..")) {
                        OmpVfsProvider provider = mOmpVfsProvider;
                        if (provider != null) {
                            data = provider.getFile(rel.substring(0, slash), rel.substring(slash + 1));
                            mime = mimeOf(rel.substring(slash + 1));
                        }
                    }
                }
            }
            if (data != null && mime != null && mime.startsWith("text/html")) {
                String html = new String(data, java.nio.charset.StandardCharsets.UTF_8);
                html = injectOmpBootstrap(html);
                b.pageLoaded = false;
                b.pendingEvents.clear();
                Log.d(TAG, "[" + b.id + "] Loading omp HTML from VFS: " + url);
                b.view.loadDataWithBaseURL(url, html, "text/html", "UTF-8", null);
                return;
            }
        } catch (Exception e) {
            Log.w(TAG, "omp direct load failed, falling back: " + e.getMessage());
        }
        startLoad(b, url);
    }

    public void ompCreateBrowser(final int browserId, final String url, final boolean focused,
                                 final boolean controlsChat, final float width, final float height) {
        mMainHandler.post(() -> {
            Browser b = createOmpBrowser(browserId);
            if (b == null) return;
            showInternal(b);
            ompLoadInto(b, url);
            if (focused) ompSetBrowserFocus(browserId, true);
        });
    }

    public void ompDestroyBrowser(final int browserId) {
        mMainHandler.post(() -> forTargets(browserId, "ompDestroyBrowser", this::destroyInternal));
    }

    public void ompSetBrowserVisible(final int browserId, final boolean visible) {
        mMainHandler.post(() -> forTargets(browserId, "ompSetBrowserVisible", b -> {
            if (visible) showInternal(b); else hideInternal(b);
        }));
    }

    public void ompSetBrowserLayer(final int browserId, final int layer) {
        mMainHandler.post(() -> forTargets(browserId, "ompSetBrowserLayer", b -> {
            try { b.view.setElevation(layer * 10f); } catch (Exception ignored) {}
        }));
    }

    public void ompSetBrowserFocus(final int browserId, final boolean focused) {
        mMainHandler.post(() -> {
            for (Browser b : mBrowsers.values()) {
                if (!b.isOmp) continue;
                boolean target = b.id == browserId;
                b.focused = focused && target;
                b.view.setTouchEnabled(focused && target);
                if (focused && target) b.view.requestFocus();
                else if (target) b.view.clearFocus();
            }
        });
    }

    public void ompLoadBrowserUrl(final int browserId, final String url) {
        if (url == null || url.isEmpty()) return;
        mMainHandler.post(() -> forTargets(browserId, "ompLoadBrowserUrl", b -> ompLoadInto(b, url)));
    }

    public void ompReloadBrowser(final int browserId, final boolean ignoreCache) {
        mMainHandler.post(() -> forTargets(browserId, "ompReloadBrowser", b -> {
            if (ignoreCache) { try { b.view.clearCache(true); } catch (Exception ignored) {} }
            b.view.reload();
        }));
    }

    public void ompDispatchToBrowser(final int browserId, final String eventName, final String argsJson) {
        if (eventName == null) return;
        final String js = "window.__sampcef && window.__sampcef.dispatch("
                + jsString(eventName) + "," + jsString(argsJson == null ? "[]" : argsJson) + ");";
        mMainHandler.post(() -> forTargets(browserId, "ompDispatchToBrowser", b -> {
            try { b.view.evaluateJavascript(js, null); } catch (Exception e) {
                Log.w(TAG, "ompDispatchToBrowser failed: " + e.getMessage());
            }
        }));
    }

    private static String jsString(String s) {
        return "\"" + escapeJsString(s == null ? "" : s) + "\"";
    }

    /** Layani http://cef/<resource>/<path> dari VFS omp (isi .pak yang di-download). */
    private WebResourceResponse serveOmpVfs(Uri uri) {
        OmpVfsProvider provider = mOmpVfsProvider;
        if (provider == null || uri == null) return null;
        if (!"http".equalsIgnoreCase(uri.getScheme()) || !"cef".equalsIgnoreCase(uri.getHost())) return null;
        String rawPath = uri.getPath();
        if (rawPath == null || rawPath.length() <= 1) return null;
        String rel = rawPath.substring(1);
        int slash = rel.indexOf('/');
        if (slash <= 0) return null;
        String resource = rel.substring(0, slash);
        String internal = rel.substring(slash + 1);
        if (internal.contains("..")) return null;
        byte[] data = provider.getFile(resource, internal);
        if (data == null) return null;
        Log.d(TAG, "serving http://cef/ from omp VFS: " + resource + "/" + internal);
        String mime = mimeOf(internal);
        if (mime.startsWith("text/html")) {
            String html = injectOmpBootstrap(new String(data, java.nio.charset.StandardCharsets.UTF_8));
            data = html.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        }
        return new WebResourceResponse(mime, "UTF-8", new ByteArrayInputStream(data));
    }

    private static String mimeOf(String path) {
        String ext = "";
        int dot = path.lastIndexOf('.');
        if (dot >= 0 && dot + 1 < path.length()) ext = path.substring(dot + 1).toLowerCase();
        switch (ext) {
            case "html": case "htm": return "text/html";
            case "js": case "mjs": return "application/javascript";
            case "css": return "text/css";
            case "json": return "application/json";
            case "png": return "image/png";
            case "jpg": case "jpeg": return "image/jpeg";
            case "gif": return "image/gif";
            case "svg": return "image/svg+xml";
            case "webp": return "image/webp";
            case "ico": return "image/x-icon";
            case "woff": return "font/woff";
            case "woff2": return "font/woff2";
            case "ttf": return "font/ttf";
            case "mp3": return "audio/mpeg";
            case "ogg": return "audio/ogg";
            case "mp4": return "video/mp4";
            default: return "application/octet-stream";
        }
    }

    // ------------------------------------------------------------------ server -> html

    public void dispatchServerEvent(final int id, final String eventName, final String eventData) {
        mMainHandler.post(() -> {
            if ("cef_ui_notification".equals(eventName)) {
                dispatchToNotifyWebView(eventName, eventData);
                return;
            }

            forTargets(id, "dispatchServerEvent(" + eventName + ")", b -> {
                if (!b.pageLoaded) {
                    // halaman belum siap: tahan, kirim saat onPageFinished
                    if (b.pendingEvents.size() >= MAX_PENDING_EVENTS) b.pendingEvents.remove(0);
                    b.pendingEvents.add(new String[]{eventName, eventData});
                    Log.d(TAG, "[" + b.id + "] event '" + eventName + "' ditahan sampai halaman selesai dimuat");
                    return;
                }
                deliverEvent(b, eventName, eventData);
            });
        });
    }

    private void flushPendingEvents(Browser b) {
        if (b.pendingEvents.isEmpty()) return;
        ArrayList<String[]> copy = new ArrayList<>(b.pendingEvents);
        b.pendingEvents.clear();
        for (String[] e : copy) deliverEvent(b, e[0], e[1]);
    }

    private void deliverEvent(Browser b, String eventName, String eventData) {
        if ("_cef_eval".equals(eventName)) {
            try {
                JSONArray arr = new JSONArray(eventData);
                String code = arr.getString(0);
                b.view.evaluateJavascript(code, null);
            } catch (JSONException e) {
                Log.e(TAG, "[" + b.id + "] Failed to parse _cef_eval payload: " + eventData, e);
            }
            return;
        }

        Log.d(TAG, "[" + b.id + "] dispatchServerEvent: " + eventName + " data=" + eventData);
        b.view.evaluateJavascript(buildEventJs(eventName, eventData), null);
    }

    /**
     * Kirim event ke halaman HTML dalam dua gaya sekaligus:
     *  1) window.addEventListener("namaEvent", e => e.detail)
     *  2) function onCefEvent(event, data) { ... } (kalau didefinisikan di HTML)
     */
    private String buildEventJs(String eventName, String eventData) {
        String n = escapeJsString(eventName);
        String d = escapeJsString(eventData);
        return "(function(){var n=\"" + n + "\",d=\"" + d + "\";"
                + "window.dispatchEvent(new CustomEvent(n,{detail:d}));"
                + "if(typeof window.onCefEvent===\"function\"){window.onCefEvent(n,d);}"
                + "})();";
    }

    private void dispatchToNotifyWebView(final String eventName, final String eventData) {
        if (mNotifyWebView == null || !mNotifyWebViewReady) {
            Log.w(TAG, "dispatchToNotifyWebView: notify overlay is not ready, event was dropped.");
            return;
        }
        mNotifyWebView.evaluateJavascript(buildEventJs(eventName, eventData), null);
    }

    // Escape agar payload aman di dalam string literal JS
    private static String escapeJsString(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\u2028", "\\u2028")
                .replace("\u2029", "\\u2029");
    }

    // ------------------------------------------------------------------ html -> server

    /**
     * Objek JS "CefMobileBridge" untuk browser CefMobile. Dipanggil dari
     * CEF_MOBILE_SHIM_JS saat halaman memanggil window.cef.emit(nama, data);
     * event diteruskan ke MobileEventListener (CefMobileClient), yang
     * me-POST-nya ke /cef-mobile/event di server. Tidak ada jalur ke native.
     */
    public static class CefMobileBridge {
        private final MobileEventListener mListener;
        private final int mBrowserId;

        public CefMobileBridge(MobileEventListener listener, int browserId) {
            this.mListener = listener;
            this.mBrowserId = browserId;
        }

        @JavascriptInterface
        public void postEvent(String eventName, String eventData) {
            if (mListener != null) {
                mListener.onMobileEvent(mBrowserId,
                        eventName == null ? "" : eventName,
                        eventData == null ? "" : eventData);
            }
        }

        /** id browser ini (CefMobile memakai id 65000). */
        @JavascriptInterface
        public int getBrowserId() {
            return mBrowserId;
        }
    }

    /**
     * Objek JS "AndroidCefBridge" untuk browser omp-cef. Dipakai OMP_BOOTSTRAP_JS
     * (window.cef.emit/on + window.__sampcef.dispatch). Event JS diteruskan
     * ke OmpEventSink (OmpCefClient) lewat channel UDP/KCP omp-cef.
     */
    public static class OmpBridge {
        private final CefJavaManager mManager;
        private final int mBrowserId;
        private volatile boolean mChatInputOpen;

        public OmpBridge(CefJavaManager manager, int browserId) {
            this.mManager = manager;
            this.mBrowserId = browserId;
        }

        @JavascriptInterface
        public void emitEvent(String eventName, String argsJson) {
            OmpEventSink sink = mManager.mOmpEventSink;
            if (sink != null) {
                sink.sendOmpClientEvent(mBrowserId,
                        eventName == null ? "" : eventName,
                        argsJson == null ? "[]" : argsJson);
            }
        }

        @JavascriptInterface
        public void setBrowserFocus(boolean focused) {
            mManager.ompSetBrowserFocus(mBrowserId, focused);
        }

        @JavascriptInterface
        public boolean isChatInputOpen() {
            return mChatInputOpen;
        }

        @JavascriptInterface
        public void setChatInputOpen(boolean open) {
            mChatInputOpen = open;
        }

        @JavascriptInterface
        public void exitGame() {
            android.os.Process.killProcess(android.os.Process.myPid());
        }

        // Sinkronisasi UI server-hosted (__cefui_*): tidak dipakai gamemode
        // ini (server mengirim .pak, bukan file satu-satu) — stub saja.
        @JavascriptInterface
        public String getUiVersion() { return ""; }
        @JavascriptInterface
        public void uiFileBegin(String file, int total) {}
        @JavascriptInterface
        public void uiFileChunk(String b64) {}
        @JavascriptInterface
        public void uiFileEnd(String file) {}
        @JavascriptInterface
        public void setUiVersion(String version) {}
    }
}