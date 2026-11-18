package com.sampmobile.xyvern.game;

import android.widget.FrameLayout;

import com.sampmobile.xyvern.cef.CefJavaManager;
import com.sampmobile.xyvern.cef.CefMobileClient;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;

import com.google.firebase.crashlytics.FirebaseCrashlytics;
import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.game.ui.AttachEdit;
import com.sampmobile.xyvern.game.ui.CustomKeyboard;
import com.sampmobile.xyvern.game.ui.LoadingScreen;
import androidx.activity.OnBackPressedCallback;
import com.sampmobile.xyvern.game.ui.dialog.DialogManager;
import com.sampmobile.xyvern.launcher.util.SharedPreferenceCore;
import com.sampmobile.xyvern.launcher.util.SignatureChecker;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;

@Obfuscate
public class SAMP extends GTASA implements CustomKeyboard.InputListener, HeightProvider.HeightListener {
    private static final String TAG = "SAMP";
    private static SAMP instance;

    private CustomKeyboard mKeyboard;
    private DialogManager mDialog;
    private HeightProvider mHeightProvider;

    private AttachEdit mAttachEdit;
    private LoadingScreen mLoadingScreen;
    private CefJavaManager mCefJavaManager;
    private CefMobileClient mCefMobileClient;
    public static SAMP getInstance() {
        return instance;
    }

    private void showTab() {}
    private void hideTab() {}
    private void setTab(int id, String name, int score, int ping) {}
    private void clearTab() {}

    private void showLoadingScreen() {}

    private void hideLoadingScreen() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mLoadingScreen.hide();
            }
        });
    }

    public void setPauseState(boolean pause) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (pause) {
                    mDialog.hideWithoutReset();
                    mAttachEdit.hideWithoutReset();
                } else {
                    if(mDialog.isShow) mDialog.showWithOldContent();
                    if(mAttachEdit.isShow) mAttachEdit.showWithoutReset();
                }
            }
        });
        if (mCefJavaManager != null) mCefJavaManager.setGamePaused(pause);
    }

    private native String nativeGetCefMobileServer();
    private native int nativeGetCefMobileGameState();
    private native int nativeGetCefMobilePlayerId();
    private native void nativeSetOmpCefManager(CefJavaManager manager);

    public String getCefMobileServer() {
        try {
            return nativeGetCefMobileServer();
        } catch (UnsatisfiedLinkError | Exception e) {
            return null;
        }
    }

    /** Game state native (5 = GAMESTATE_CONNECTED), atau -1 kalau tidak ada. */
    public int getCefMobileGameState() {
        try {
            return nativeGetCefMobileGameState();
        } catch (UnsatisfiedLinkError | Exception e) {
            return -1;
        }
    }

    /** Playerid lokal (0..999), atau -1 kalau belum join. */
    public int getCefMobilePlayerId() {
        try {
            return nativeGetCefMobilePlayerId();
        } catch (UnsatisfiedLinkError | Exception e) {
            return -1;
        }
    }

    public void exitGame(){
        FirebaseCrashlytics.getInstance().setCrashlyticsCollectionEnabled(false);
        finishAndRemoveTask();
        System.exit(0);
    }

    public void showDialog(int dialogId, int dialogTypeId, byte[] bArr, byte[] bArr2, byte[] bArr3, byte[] bArr4) {
        final String caption = new String(bArr, StandardCharsets.UTF_8);
        final String content = new String(bArr2, StandardCharsets.UTF_8);
        final String leftBtnText = new String(bArr3, StandardCharsets.UTF_8);
        final String rightBtnText = new String(bArr4, StandardCharsets.UTF_8);
        runOnUiThread(() -> { this.mDialog.show(dialogId, dialogTypeId, caption, content, leftBtnText, rightBtnText); });
    }

    private native void onInputEnd(byte[] str);
    @Override
    public void OnInputEnd(String str) {
        if (str != null) {
            String trimmed = str.trim();
            if (trimmed.equalsIgnoreCase("/q") || trimmed.equalsIgnoreCase("/quit")) {
                exitGame();
                return;
            }
        }

        byte[] toReturn = null;
        try {
            toReturn = str != null ? str.getBytes("windows-874") : new byte[0];
        } catch(UnsupportedEncodingException e) {}

        try {
            onInputEnd(toReturn);
        } catch (UnsatisfiedLinkError e5) {
        }
    }

    private void showKeyboard() {
        runOnUiThread(() -> {
            Log.d("AXL", "showKeyboard()");
            mKeyboard.ShowInputLayout();
        });
    }

    private void hideKeyboard() {
        runOnUiThread(() -> mKeyboard.HideInputLayout());
    }

    private void showEditObject() {
        runOnUiThread(() -> mAttachEdit.show());
    }

    private void hideEditObject() {
        runOnUiThread(() -> mAttachEdit.hide());
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        com.sampmobile.xyvern.launcher.util.LogcatRecorder.start(this);
        Log.i(TAG, "**** onCreate");
        super.onCreate(savedInstanceState);

        if(!SignatureChecker.isSignatureValid(this, getPackageName())) {
            Toast.makeText(this, "Use original launcher! No remake", Toast.LENGTH_LONG).show();
            return;
        }

        try {
            File extFiles = getExternalFilesDir(null);
            if (extFiles != null) {
                File modsDir = new File(extFiles, "mods");
                if (!modsDir.exists()) modsDir.mkdirs();
                File cleoDir = new File(extFiles, "cleo");
                if (!cleoDir.exists()) cleoDir.mkdirs();
                File cleoSaDir = new File(cleoDir, "sa");
                if (!cleoSaDir.exists()) cleoSaDir.mkdirs();
            }

            try {
                File rootCleo = new File(android.os.Environment.getExternalStorageDirectory(), "cleo");
                if (!rootCleo.exists()) rootCleo.mkdirs();
                File rootCleoSa = new File(rootCleo, "sa");
                if (!rootCleoSa.exists()) rootCleoSa.mkdirs();
            } catch (Exception ignored) {}

            boolean cleoEnabled = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE)
                    .getBoolean("cleo_enabled", true);
            if (cleoEnabled) {
                try {
                    System.loadLibrary("threadfix");
                    Log.i(TAG, "libthreadfix.so loaded successfully");
                } catch (Throwable e) {
                    Log.w(TAG, "Optional libthreadfix.so not loaded: " + e.getMessage());
                }

                try {
                    System.loadLibrary("SAcleo64");
                    Log.i(TAG, "CLEO Android 64-bit (libSAcleo64.so) loaded successfully");
                } catch (UnsatisfiedLinkError e64) {
                    try {
                        System.loadLibrary("cleo");
                        Log.i(TAG, "CLEO Android 32-bit (libcleo.so) loaded successfully");
                    } catch (Throwable e32) {
                        Log.e(TAG, "Failed to load CLEO library", e32);
                    }
                } catch (Throwable e) {
                    Log.e(TAG, "Failed to load libSAcleo64.so", e);
                }
            }

            boolean amlEnabled = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE)
                    .getBoolean("aml_enabled", true);
            if (amlEnabled) {
                try {
                    System.loadLibrary("AML");
                    Log.i(TAG, "AndroidModLoader loaded");
                } catch (UnsatisfiedLinkError e) {
                    Log.i(TAG, "libAML.so optional module not present");
                }
            }
        } catch (Exception error) {
            Log.e(TAG, "Error initializing mod loaders", error);
        }

        mKeyboard = new CustomKeyboard(this);
        mDialog = new DialogManager(this);
        mAttachEdit = new AttachEdit(this);
        mLoadingScreen = new LoadingScreen(this);
        instance = this;

        try {
            android.content.SharedPreferences cefPrefs = getSharedPreferences(
                    com.sampmobile.xyvern.launcher.util.SharedPreferenceCore.APP_PREFERENCES, MODE_PRIVATE);
            boolean cefEnabled = !cefPrefs.contains("CEF_ENABLED") || cefPrefs.getBoolean("CEF_ENABLED", true);
            if (!cefEnabled) {
                Log.i(TAG, "CefMobile disabled in settings");
            } else {
                FrameLayout rootLayout = findViewById(android.R.id.content);

                mCefJavaManager = new CefJavaManager(rootLayout, this);
                try { nativeSetOmpCefManager(mCefJavaManager); } catch (Throwable ignored) {}
                mCefMobileClient = new CefMobileClient(this, mCefJavaManager);
                mCefMobileClient.start();

                Log.i(TAG, "CefMobile initialized");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize CefMobile", e);
        }

        String ip = getIntent().getStringExtra("ip");
        int port = getIntent().getIntExtra("port", 7777);
        if (ip == null || ip.isEmpty()) ip = "127.0.0.1";

        try {
            initAssetManager(getAssets());
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "initAssetManager failed", e);
        }

        final String finalIp = ip;
        final int finalPort = port;
        GameDataManager.checkAndPrepare(this, new GameDataManager.ReadyCallback() {
            @Override public void onReady() {
                if (isFinishing() || isDestroyed()) return;
                startGameAfterDataReady(finalIp, finalPort);
            }
        });

        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                setEnabled(false);
                getOnBackPressedDispatcher().onBackPressed();
                onEventBackPressed();
                setEnabled(true);
            }
        });
    }

    private void startGameAfterDataReady(String ip, int port) {
        mLoadingScreen.setProgress(0);
        new Thread(() -> {
            for (int i = 0; i <= 100; i++) {
                final int progress = i;
                runOnUiThread(() -> mLoadingScreen.setProgress(progress));
                try { Thread.sleep(25); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); return; }
            }
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                try {
                    updateSettingsIni(ip, port);
                    File external = getExternalFilesDir(null);
                    if (external == null) {
                        Log.e(TAG, "External files directory unavailable");
                        Toast.makeText(this, "Storage unavailable", Toast.LENGTH_LONG).show();
                        finish();
                        return;
                    }
                    String storagePath = external.getAbsolutePath();
                    if (!storagePath.endsWith("/")) storagePath += "/";
                    initializeSAMP(storagePath);
                } catch (Exception e) {
                    Log.e(TAG, "Gagal inisialisasi game", e);
                    Toast.makeText(this, "Gagal memulai game: " + e.getMessage(), Toast.LENGTH_LONG).show();
                }
            });
        }).start();
    }

    private void updateSettingsIni(String ip, int port) {
        File iniFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        
        try {
            if (!iniFile.getParentFile().exists()) iniFile.getParentFile().mkdirs();

            SharedPreferenceCore spCore = new SharedPreferenceCore();
            Context ctx = getApplicationContext();
            
            android.content.SharedPreferences sp = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE);
            String nickname = sp.getString("nickname", "AndroidUser"); 
            if (nickname.length() > 24) nickname = nickname.substring(0, 24);
            
            String[] versions = {"0.3.DL-R1", "0.3.7", "0.3.7-R5"};
            int vIdx = spCore.getInt(ctx, "VERSION", 0);
            String version = (vIdx >= 0 && vIdx < versions.length) ? versions[vIdx] : "0.3.DL-R1";

            StringBuilder sb = new StringBuilder();
            sb.append("[client]\n");
            sb.append("name=").append(nickname).append("\n");
            sb.append("host=").append(ip).append("\n");
            sb.append("port=").append(port).append("\n");
            sb.append("password=\n");
            sb.append("version=").append(version).append("\n\n");
            
            sb.append("[gui]\n");
            sb.append("androidkeyboard=").append(spCore.getBoolean(ctx, "ANDROID_KEYBOARD") ? "true" : "false").append("\n");
            sb.append("VoiceChatEnable=").append(spCore.getBoolean(ctx, "VOICE_CHAT") ? "true" : "false").append("\n");
            sb.append("fps=").append(spCore.getBoolean(ctx, "FPS_DISPLAY") ? "true" : "false").append("\n");
            sb.append("ChatMaxMessages=").append(spCore.getInt(ctx, "MESSAGE_COUNT", 6)).append("\n");
            sb.append("FPSLimit=").append(spCore.getInt(ctx, "FPS_LIMIT", 60)).append("\n");

            java.io.FileOutputStream fos = new java.io.FileOutputStream(iniFile);
            fos.write(sb.toString().getBytes());
            fos.close();
            
            Log.d(TAG, "settings.ini saved for " + nickname + " to " + ip + ":" + port);
        } catch (Exception e) {
            Log.e(TAG, "Gagal menulis settings.ini: " + e.getMessage());
        }
    }

    private native void initializeSAMP(String storagePath);
    public native void initAssetManager(android.content.res.AssetManager assetManager);

    @Override
    public void onStart() {
        Log.i(TAG, "**** onStart");
        super.onStart();
    }

    @Override
    public void onRestart() {
        Log.i(TAG, "**** onRestart");
        super.onRestart();
    }

    @Override
    public void onResume() {
        Log.i(TAG, "**** onResume");
        super.onResume();
    }

    public native void onEventBackPressed();

    @SuppressLint("GestureBackNavigation")
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if(keyCode == KeyEvent.KEYCODE_BACK) {
            onEventBackPressed();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void onPause() {
        Log.i(TAG, "**** onPause");
        super.onPause();
    }

    @Override
    public void onStop() {
        Log.i(TAG, "**** onStop");
        super.onStop();
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "**** onDestroy");
        super.onDestroy();
        instance = null;
        if (mCefMobileClient != null) {
            try { mCefMobileClient.stop(); } catch (Exception ignored) {}
            mCefMobileClient = null;
        }
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onHeightChanged(int orientation, int height) {}
}
