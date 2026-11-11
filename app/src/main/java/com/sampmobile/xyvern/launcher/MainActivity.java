package com.hopepride.samp.launcher;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.Manifest;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentPagerAdapter;

import com.joom.paranoid.Obfuscate;
import com.hopepride.samp.R;
import com.hopepride.samp.game.GameDataManager;
import com.hopepride.samp.launcher.config.Config;
import com.hopepride.samp.launcher.fragments.HomeFragment;
import com.hopepride.samp.launcher.fragments.PlayFragment;
import com.hopepride.samp.launcher.fragments.SettingsFragment;
import com.hopepride.samp.launcher.util.ConfigValidator;
import com.hopepride.samp.launcher.util.SharedPreferenceCore;
import com.hopepride.samp.launcher.util.ViewPagerWithoutSwipe;

import java.io.File;

@Obfuscate
public class MainActivity extends AppCompatActivity {

    private final String[] permissions = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO
    };

    public ViewPagerWithoutSwipe pa;
    private View navHome, navPlay, navSettings;
    private ImageView ivHome, ivPlay, ivSettings;
    private TextView tvHome, tvPlay, tvSettings;
    private boolean launcherInitialized = false;
    private MediaPlayer welcomePlayer;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        com.hopepride.samp.diagnostics.Diagnostics.event("LAUNCHER_ACTIVITY_START");
        // Keep the launcher Activity as the entry point, but do not initialize its tabs
        // until the maintenance check and game-data download gate have completed.
        setContentView(R.layout.activity_main);
        if (savedInstanceState == null) playWelcomeSound();
        Config.currentContext = this;
        checkFirstRun();

        // If the process died mid-download, relaunch the foreground service from
        // persisted state so the download continues without a manual re-tap.
        com.hopepride.samp.game.GameDataDownloadService.autoResume(this);

        GameDataManager.checkAndPrepare(this, () -> {
            if (isFinishing() || isDestroyed() || launcherInitialized) return;
            launcherInitialized = true;
            com.hopepride.samp.diagnostics.Diagnostics.event("LAUNCHER_DATA_GATE_READY");
            initializeLauncherUi();
        });
    }

    private void playWelcomeSound() {
        try {
            AudioAttributes attributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build();
            welcomePlayer = MediaPlayer.create(this, R.raw.hopepride_welcome, attributes,
                    AudioManager.AUDIO_SESSION_ID_GENERATE);
            if (welcomePlayer == null) {
                com.hopepride.samp.diagnostics.Diagnostics.event("WELCOME_PLAYER_UNAVAILABLE");
                return;
            }
            welcomePlayer.setOnCompletionListener(player -> releaseWelcomeSound());
            welcomePlayer.setOnErrorListener((player, what, extra) -> {
                com.hopepride.samp.diagnostics.Diagnostics.event("WELCOME_PLAYBACK_ERROR");
                Log.w("MainActivity", "Welcome sound failed: " + what + "/" + extra);
                releaseWelcomeSound();
                return true;
            });
            welcomePlayer.start();
            com.hopepride.samp.diagnostics.Diagnostics.event("WELCOME_PLAYBACK_STARTED");
        } catch (RuntimeException e) {
            com.hopepride.samp.diagnostics.Diagnostics.error("WELCOME_PLAYBACK_FAILED", e);
            Log.w("MainActivity", "Welcome sound unavailable", e);
            releaseWelcomeSound();
        }
    }

    private void releaseWelcomeSound() {
        MediaPlayer player = welcomePlayer;
        welcomePlayer = null;
        if (player != null) player.release();
    }

    @Override
    protected void onStop() {
        releaseWelcomeSound();
        super.onStop();
    }

    private void initializeLauncherUi() {
        ConfigValidator.validateConfigFiles(this);

        if (!isPermissionsGranted()) {
            ActivityCompat.requestPermissions(this, permissions, 1);
        }

        File file = new File(getExternalFilesDir(null) + "/download/update.apk");
        if (file.exists()) file.delete();

        clearModelCache();

        FragmentManager fm = getSupportFragmentManager();
        ViewPagerAdapter sa = new ViewPagerAdapter(fm);
        pa = findViewById(R.id.fragment_place);
        pa.setAdapter(sa);
        pa.setOffscreenPageLimit(3);

        initNav();
        updateNavUI(pa.getCurrentItem());
    }

    private void initNav() {
        navHome = findViewById(R.id.nav_home);
        navPlay = findViewById(R.id.nav_play);
        navSettings = findViewById(R.id.nav_settings);
        ivHome = findViewById(R.id.iv_home);
        ivPlay = findViewById(R.id.iv_play);
        ivSettings = findViewById(R.id.iv_settings);
        tvHome = findViewById(R.id.tv_home);
        tvPlay = findViewById(R.id.tv_play);
        tvSettings = findViewById(R.id.tv_settings);

        navHome.setOnClickListener(v -> changeTab(0));
        navPlay.setOnClickListener(v -> changeTab(1));
        navSettings.setOnClickListener(v -> changeTab(2));
    }

    private void updateNavUI(int position) {
        if (navHome == null || navPlay == null || navSettings == null) return;
        View[] tabs = {navHome, navPlay, navSettings};
        ImageView[] icons = {ivHome, ivPlay, ivSettings};
        TextView[] labels = {tvHome, tvPlay, tvSettings};
        for (int i = 0; i < tabs.length; i++) {
            boolean selected = i == position;
            tabs[i].setBackgroundResource(selected ? R.drawable.bg_hope_nav_selected : 0);
            tabs[i].setSelected(selected);
            int color = android.graphics.Color.parseColor(selected ? "#79D5FF" : "#8397AD");
            androidx.core.widget.ImageViewCompat.setImageTintList(icons[i],
                    android.content.res.ColorStateList.valueOf(color));
            labels[i].setTextColor(color);
        }
    }

    public void changeTab(int position) {
        if (pa != null) {
            getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
            pa.setCurrentItem(position, true);
            updateNavUI(position);
        }
    }

    public static void hideKeyboard(Activity activity) {
        if (activity != null) {
            InputMethodManager inputManager = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
            View currentFocusedView = activity.getCurrentFocus();
            if (inputManager != null && currentFocusedView != null) {
                inputManager.hideSoftInputFromWindow(currentFocusedView.getWindowToken(), InputMethodManager.HIDE_NOT_ALWAYS);
            }
        }
    }

    public class ViewPagerAdapter extends FragmentPagerAdapter {
        public ViewPagerAdapter(FragmentManager fragmentManager) { super(fragmentManager); }

        @NonNull
        @Override
        public Fragment getItem(int position) {
            if (position == 1) return new PlayFragment();
            if (position == 2) return new SettingsFragment();
            return new HomeFragment();
        }

        @Override
        public int getCount() { return 3; }
    }

    private void clearModelCache() {
        try {
            File file = new File(getExternalFilesDir(null).toString() + "/CINFO.BIN");
            if (file.exists()) file.delete();
            file = new File(getExternalFilesDir(null).toString() + "/models/MINFO.BIN");
            if (file.exists()) file.delete();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void checkFirstRun() {
        SharedPreferences prefs = getSharedPreferences("com.hopepride.samp", MODE_PRIVATE);
        if (prefs.getBoolean("firstrun", true)) {
            new SharedPreferenceCore().setInt(getApplicationContext(), "FPS_LIMIT", 60);
            new SharedPreferenceCore().setInt(getApplicationContext(), "MESSAGE_COUNT", 6);
            new SharedPreferenceCore().setBoolean(getApplicationContext(), "VOICE_CHAT", false);
            new SharedPreferenceCore().setBoolean(getApplicationContext(), "MODIFIED_DATA", false);
            new SharedPreferenceCore().setInt(getApplicationContext(), "VERSION", 0);
            prefs.edit().putBoolean("firstrun", false).apply();
        }
    }

    public boolean isPermissionsGranted() {
        for (String permission : permissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) return false;
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == 1 && !isPermissionsGranted()) {
            // The launcher remains gated by data readiness; permissions can be requested again if needed.
        }
    }
}
