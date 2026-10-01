package com.sampmobile.xyvern.game;

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
        byte[] toReturn = null;
        try {
            toReturn = str.getBytes("windows-874");
        } catch(UnsupportedEncodingException e) {}

        try {
            onInputEnd(toReturn);
        } catch (UnsatisfiedLinkError e5) {
            Log.e(TAG, e5.getMessage());
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
        super.onCreate(savedInstanceState);

        if(!SignatureChecker.isSignatureValid(this, getPackageName())) {
            Toast.makeText(this, "Use original launcher! No remake", Toast.LENGTH_LONG).show();
            return;
        }

        mKeyboard = new CustomKeyboard(this);
        mDialog = new DialogManager(this);
        mAttachEdit = new AttachEdit(this);
        mLoadingScreen = new LoadingScreen(this);
        instance = this;

        // 1. Nhận thông tin IP & Port từ Intent
        String ip = getIntent().getStringExtra("ip");
        int port = getIntent().getIntExtra("port", 7777);
        if (ip == null || ip.isEmpty()) ip = "127.0.0.1";

        try {
            initAssetManager(getAssets());
            
            // Đưa thanh trạng thái của LoadingScreen về 0% khi bắt đầu vào game
            runOnUiThread(() -> mLoadingScreen.setProgress(0));

            final String finalIp = ip;
            final int finalPort = port;

            // Jalankan thread loading agar Engine Native tidak dipanggil terlalu cepat (mencegah SIGSEGV)
            new Thread(new Runnable() {
                @Override
                public void run() {
                    for (int i = 0; i <= 100; i++) {
                        final int currentProgress = i;
                        runOnUiThread(() -> mLoadingScreen.setProgress(currentProgress));
                        try {
                            Thread.sleep(40); // Sekitar 4 detik total
                        } catch (InterruptedException ignored) {}
                    }

                    // Setelah loading selesai, siapkan file dan jalankan engine
                    runOnUiThread(() -> {
                        if (isFinishing() || isDestroyed()) return;
                        try {
                            updateSettingsIni(finalIp, finalPort);
                            
                            // Pastikan path storage bersih (tanpa double slash)
                            String storagePath = getExternalFilesDir(null).getAbsolutePath();
                            if (!storagePath.endsWith("/")) {
                                storagePath += "/";
                            }
                            
                            initializeSAMP(storagePath);
                        } catch (Exception e) {
                            Log.e(TAG, "Gagal inisialisasi game: " + e.getMessage());
                        }
                    });
                }
            }).start();

        } catch (UnsatisfiedLinkError e5) {
            Log.e(TAG, e5.getMessage());
        }

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

    private void updateSettingsIni(String ip, int port) {
        File iniFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        
        try {
            if (!iniFile.getParentFile().exists()) iniFile.getParentFile().mkdirs();

            SharedPreferenceCore spCore = new SharedPreferenceCore();
            Context ctx = getApplicationContext();
            
            android.content.SharedPreferences sp = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE);
            String nickname = sp.getString("nickname", "AndroidUser"); 
            if (nickname.length() > 24) nickname = nickname.substring(0, 24);
            
            String[] versions = {"0.3.7", "0.3.7-R1", "0.3.7-R3", "0.3.7-R4", "0.3.7-R5"};
            int vIdx = spCore.getInt(ctx, "VERSION", 0);
            String version = (vIdx >= 0 && vIdx < versions.length) ? versions[vIdx] : "0.3.7";

            // Gunakan format penulisan yang sangat bersih
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
        // Mematikan paksa proses game saat keluar untuk menghindari crash "destroyed mutex" 
        // yang sering terjadi di thread native saat activity dihancurkan.
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onHeightChanged(int orientation, int height) {}
}
