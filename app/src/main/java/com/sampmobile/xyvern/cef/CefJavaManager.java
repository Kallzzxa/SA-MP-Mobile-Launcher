package com.sampmobile.xyvern.cef;

import android.app.Activity;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.view.MotionEvent;
/**
 * SA:MP Mobile CEF - Java Manager for Android WebView Overlay (Pro Edition)
 * Copyright © 2026 drgxbytezone & Community
 */
public class CefJavaManager {
    private static final String TAG = "SAMP_CEF_JAVA";
    private final Activity mActivity;
    private final FrameLayout mRootLayout;
    private WebView mWebView;
    private CefClientManager mClientManager;
    private boolean mIsShown = false;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

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
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
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

    public void setClientManager(CefClientManager clientManager) {
        this.mClientManager = clientManager;
        mMainHandler.post(() -> {
            if (mWebView != null && clientManager != null) {
                mWebView.addJavascriptInterface(
                        new CefWebInterface(clientManager), "AndroidCefBridge");
            }
        });
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
    public static class CefWebInterface {
        private final CefClientManager mClientManager;

        public CefWebInterface(CefClientManager clientManager) {
            this.mClientManager = clientManager;
        }

        @android.webkit.JavascriptInterface
        public void sendEvent(String eventName, String eventDataJson) {
            if (mClientManager != null) {
                mClientManager.sendClientEvent(eventName, eventDataJson);
            }
        }
    }
}
