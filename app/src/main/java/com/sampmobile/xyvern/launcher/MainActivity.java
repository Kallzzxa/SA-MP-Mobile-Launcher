package com.sampmobile.xyvern.launcher;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentPagerAdapter;

import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;
import com.sampmobile.xyvern.launcher.config.Config;
import com.sampmobile.xyvern.launcher.fragments.HomeFragment;
import com.sampmobile.xyvern.launcher.fragments.SettingsFragment;
import com.sampmobile.xyvern.launcher.util.ConfigValidator;
import com.sampmobile.xyvern.launcher.util.SharedPreferenceCore;
import com.sampmobile.xyvern.launcher.util.ViewPagerWithoutSwipe;

import java.io.File;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;
import android.Manifest;
import android.content.SharedPreferences;

import android.widget.ImageView;
import android.widget.TextView;
import com.sampmobile.xyvern.launcher.fragments.PlayFragment;

@Obfuscate
public class MainActivity extends AppCompatActivity {

    private final String[] permissions = {"android.permission.READ_EXTERNAL_STORAGE", "android.permission.WRITE_EXTERNAL_STORAGE", "android.permission.RECORD_AUDIO"};

    public ViewPagerWithoutSwipe pa;
    private View navHome, navPlay, navSettings;
    private ImageView ivHome, ivPlay, ivSettings;
    private TextView tvHome, tvPlay, tvSettings;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Config.currentContext = this;
        ConfigValidator.validateConfigFiles(this);

        checkFirstRun();

        if (!isPermissionsGranted()) {
            ActivityCompat.requestPermissions(this, permissions, 1);
        }

        File file = new File(getExternalFilesDir(null) + "/download/update.apk");
        if (file.exists()) {
            file.delete();
        }

        clearModelCache();

        FragmentManager fm = getSupportFragmentManager();
        ViewPagerAdapter sa = new ViewPagerAdapter(fm);
        
        pa = findViewById(R.id.fragment_place);
        pa.setAdapter(sa);
        pa.setOffscreenPageLimit(3);

        initNav();
        updateNavUI(0);
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
        // Reset all
        navHome.setBackground(null);
        navPlay.setBackground(null);
        navSettings.setBackground(null);

        tvHome.setVisibility(View.GONE);
        tvPlay.setVisibility(View.GONE);
        tvSettings.setVisibility(View.GONE);

        // Set active
        if (position == 0) {
            navHome.setBackgroundResource(R.drawable.bg_nav_active);
            tvHome.setVisibility(View.VISIBLE);
        } else if (position == 1) {
            navPlay.setBackgroundResource(R.drawable.bg_nav_active);
            tvPlay.setVisibility(View.VISIBLE);
        } else if (position == 2) {
            navSettings.setBackgroundResource(R.drawable.bg_nav_active);
            tvSettings.setVisibility(View.VISIBLE);
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
        public ViewPagerAdapter(FragmentManager fragmentManager) {
            super(fragmentManager);
        }

        @NonNull
        @Override
        public Fragment getItem(int position) {
            if (position == 1) return new PlayFragment();
            if (position == 2) return new SettingsFragment();
            return new HomeFragment();
        }

        @Override
        public int getCount() {
            return 3;
        }
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
        SharedPreferences prefs = getSharedPreferences("com.sampmobile.xyvern", MODE_PRIVATE);
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
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == 1) {
            if (!isPermissionsGranted()) {
                // Optionally handle permission denial
            }
        }
    }
}
