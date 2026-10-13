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
        "      var list = handlers[name] || [];\n" +
        "      if (!list.length) { (pending[name] = pending[name] || []).push(args); return; }\n" +
        "      for (var i = 0; i < list.length; i++) { try { list[i].apply(null, args); } catch (e) {} }\n" +
        "    }\n" +
        "  };\n" +
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

        mMainHandler.post(this::initWebView);
    }

    private void initWebView() {
        if (mWebView != null) return;

        // Keep normal WebView touch dispatch enabled so HTML controls can be clicked.
        mWebView = new WebView(mActivity);
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
     * Serve the omp-cef style local schemes from assets/cef/:
     *   cef://index.html        -> assets/cef/index.html
     *   http://cef/index.html   -> assets/cef/index.html   (PC omp-cef scheme)
     */
    private WebResourceResponse serveCefScheme(Uri uri) {
        if (uri == null) return null;
        String scheme = uri.getScheme();
        String host = uri.getHost();
        boolean isCefScheme = "cef".equalsIgnoreCase(scheme)
                || ("http".equalsIgnoreCase(scheme) && "cef".equalsIgnoreCase(host));
        if (!isCefScheme) return null;

        String path = uri.getPath();
        if (path == null || path.isEmpty() || path.equals("/")) path = "/index.html";
        if (path.startsWith("/")) path = path.substring(1);
        // block directory traversal
        if (path.contains("..")) return null;

        String assetPath = "cef/" + path;
        try {
            AssetManager am = mActivity.getAssets();
            InputStream in = am.open(assetPath);
            String ext = "";
            int dot = assetPath.lastIndexOf('.');
            if (dot >= 0) ext = assetPath.substring(dot + 1).toLowerCase();
            String mime = MIME_TYPES.get(ext);
            if (mime == null) mime = "application/octet-stream";
            // text types get a charset, binary types must pass null encoding
            String encoding = (mime.startsWith("text/") || mime.equals("application/javascript")
                    || mime.equals("application/json") || mime.equals("image/svg+xml")
                    || mime.equals("application/xml")) ? "UTF-8" : null;
            Log.d(TAG, "serving cef:// -> assets/" + assetPath);
            return new WebResourceResponse(mime, encoding, in);
        } catch (IOException e) {
            Log.w(TAG, "cef asset missing: " + assetPath);
            return null;
        }
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
            if (mWebView != null) {
                mWebView.loadUrl(url);
            }
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
    }
}
