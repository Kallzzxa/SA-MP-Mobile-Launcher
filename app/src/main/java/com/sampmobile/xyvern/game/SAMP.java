package com.sampmobile.xyvern.game;

import android.widget.FrameLayout;

import com.sampmobile.xyvern.cef.CefClientManager;
import com.sampmobile.xyvern.cef.CefJavaManager;
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
import com.sampmobile.xyvern.launcher.util.ServerInput;

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
    private CefClientManager mCefClientManager;
    private CefJavaManager mCefJavaManager;
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
                if (mLoadingScreen != null) mLoadingScreen.hide();
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
    }

    public void exitGame(){
        if (!com.google.firebase.FirebaseApp.getApps(this).isEmpty()) {
            FirebaseCrashlytics.getInstance().setCrashlyticsCollectionEnabled(false);
        }
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

    // A server may close an existing dialog without asking the player to reply.
    public void hideDialog() {
        runOnUiThread(() -> {
            if (mDialog != null && mDialog.isShow) mDialog.Hide();
        });
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

    // =========================================================================
    // KHU VỰC SỬA LOGIC ONCREATE - GHI SETTINGS.INI TRỰC TIẾP VÀ KHỞI CHẠY ENGINE
    // =========================================================================
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "**** onCreate");
        com.sampmobile.xyvern.diagnostics.Diagnostics.event("GAME_ACTIVITY_START");
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

            boolean cleoEnabled = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE)
                    .getBoolean("cleo_enabled", true);
            if (cleoEnabled) {
                try {
                    System.loadLibrary("cleo");
                    Log.i(TAG, "CLEO Android loaded");
                } catch (UnsatisfiedLinkError e) {
                    Log.i(TAG, "libcleo.so optional module not present");
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

        // ============================================================
        // SA:MP MOBILE CEF (can be toggled in launcher Settings)
        // ============================================================
        try {
            android.content.SharedPreferences cefPrefs = getSharedPreferences(
                    com.sampmobile.xyvern.launcher.util.SharedPreferenceCore.APP_PREFERENCES, MODE_PRIVATE);
            boolean cefEnabled = !cefPrefs.contains("CEF_ENABLED") || cefPrefs.getBoolean("CEF_ENABLED", true);
            com.sampmobile.xyvern.cef.CefJavaManager.setWebViewEnabled(cefEnabled);
            if (!cefEnabled) {
                Log.i(TAG, "SA:MP Mobile CEF disabled in settings");
            } else {
                FrameLayout rootLayout = findViewById(android.R.id.content);

                mCefClientManager = new CefClientManager(this);
                mCefJavaManager = new CefJavaManager(rootLayout, this);

                mCefClientManager.setJavaManager(mCefJavaManager);
                mCefJavaManager.setClientManager(mCefClientManager);

                Log.i(TAG, "SA:MP Mobile CEF initialized");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize SA:MP Mobile CEF", e);
        }

        // 1. Nhận thông tin IP & Port từ Intent
        String ip = getIntent().getStringExtra("ip");
        int port = getIntent().getIntExtra("port", 7777);
        String password = getIntent().getStringExtra("server_password");
        if (password == null) password = "";
        String nickname = getIntent().getStringExtra("nickname");
        if (nickname == null) nickname = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE)
                .getString("nickname", "AndroidUser");
        if (!ServerInput.isValidHost(ip) || !ServerInput.isValidPort(port)
                || !ServerInput.isValidPassword(password) || !ServerInput.isValidNickname(nickname)) {
            Toast.makeText(this, "Alamat, nickname, atau password server tidak valid.", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        try {
            initAssetManager(getAssets());
            com.sampmobile.xyvern.diagnostics.Diagnostics.event("GAME_ASSET_MANAGER_READY");
        } catch (UnsatisfiedLinkError e) {
            com.sampmobile.xyvern.diagnostics.Diagnostics.error("GAME_ASSET_MANAGER_FAILED", e);
            Log.e(TAG, "initAssetManager failed", e);
            Toast.makeText(this, "Library game tidak dapat dimulai. Periksa instalasi APK.", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // The launcher already checked maintenance and game data before opening this
        // non-exported activity. Configure the native client now, before Android's
        // surface callbacks can start the game, rather than after a network request
        // and an artificial loading delay.
        startGameAfterDataReady(ip, port, password, nickname);

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

    private void startGameAfterDataReady(String ip, int port, String password, String nickname) {
        try {
            com.sampmobile.xyvern.diagnostics.Diagnostics.event("GAME_SETTINGS_PREPARE");
            updateSettingsIni(ip, port, password, nickname);
            File external = getExternalFilesDir(null);
            if (external == null) throw new java.io.IOException("Penyimpanan game tidak tersedia.");
            String storagePath = external.getAbsolutePath();
            if (!storagePath.endsWith("/")) storagePath += "/";
            initializeSAMP(storagePath);
            com.sampmobile.xyvern.diagnostics.Diagnostics.event("GAME_NATIVE_INITIALIZATION_REQUESTED");
        } catch (Exception | UnsatisfiedLinkError e) {
            com.sampmobile.xyvern.diagnostics.Diagnostics.error("GAME_NATIVE_INITIALIZATION_FAILED", e);
            Log.e(TAG, "Gagal inisialisasi game", e);
            Toast.makeText(this, "Gagal memulai game: " + e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
        }
    }

    private void updateSettingsIni(String ip, int port, String password, String nickname) throws java.io.IOException {
        File root = getExternalFilesDir(null);
        if (root == null) throw new java.io.IOException("Penyimpanan game tidak tersedia.");
        File iniFile = new File(root, "SAMP/settings.ini");
        File parent = iniFile.getParentFile();
        if (parent == null || (!parent.isDirectory() && !parent.mkdirs()))
            throw new java.io.IOException("Gagal menyiapkan konfigurasi game.");

        SharedPreferenceCore spCore = new SharedPreferenceCore();
        Context ctx = getApplicationContext();
        
        String[] versions = {"0.3.DL-R1", "0.3.7", "0.3.7-R5"};
        int vIdx = spCore.getInt(ctx, "VERSION", 0);
        String version = (vIdx >= 0 && vIdx < versions.length) ? versions[vIdx] : "0.3.DL-R1";

        StringBuilder sb = new StringBuilder();
        sb.append("[client]\n");
        sb.append("name=").append(nickname).append("\n");
        sb.append("host=").append(ip).append("\n");
        sb.append("port=").append(port).append("\n");
        sb.append("password=").append(password).append("\n");
        sb.append("version=").append(version).append("\n\n");

        sb.append("[gui]\n");
        sb.append("androidkeyboard=").append(spCore.getBoolean(ctx, "ANDROID_KEYBOARD") ? "true" : "false").append("\n");
        sb.append("VoiceChatEnable=").append(spCore.getBoolean(ctx, "VOICE_CHAT") ? "true" : "false").append("\n");
        sb.append("fps=").append(spCore.getBoolean(ctx, "FPS_DISPLAY") ? "true" : "false").append("\n");
        sb.append("ChatMaxMessages=").append(spCore.getInt(ctx, "MESSAGE_COUNT", 6)).append("\n");
        sb.append("FPSLimit=").append(spCore.getInt(ctx, "FPS_LIMIT", 60)).append("\n");

        try (java.io.FileOutputStream fos = new java.io.FileOutputStream(iniFile)) {
            fos.write(sb.toString().getBytes(StandardCharsets.UTF_8));
            fos.flush();
        }
        Log.d(TAG, "Server settings saved for " + ip + ":" + port);
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
        com.sampmobile.xyvern.diagnostics.Diagnostics.event("GAME_ACTIVITY_DESTROY");
        super.onDestroy();
        instance = null;
        // Mematikan paksa proses game saat keluar untuk menghindari crash "destroyed mutex" 
        // yang sering terjadi di thread native saat activity dihancurkan.
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onHeightChanged(int orientation, int height) {}
}
