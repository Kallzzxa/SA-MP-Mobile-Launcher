package com.sampmobile.xyvern.launcher.fragments;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.downloader.Error;
import com.downloader.OnDownloadListener;
import com.downloader.OnProgressListener;
import com.downloader.PRDownloader;
import com.downloader.PRDownloaderConfig;
import com.downloader.Progress;
import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;
import com.sampmobile.xyvern.game.GameDataDownloadService;
import com.sampmobile.xyvern.launcher.MainActivity;
import com.sampmobile.xyvern.launcher.data.FilesData;
import com.sampmobile.xyvern.launcher.util.GameDataVerifier;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

@Obfuscate
public class HomeFragment extends Fragment {

    // Direct download URL untuk data pack resmi GTA SA / SAMP
    private static final String DATA_ZIP_URL = "https://github.com/rezzylrn/samp-mobile-optimized/releases/download/data-v1/files.zip";

    private View dotStatus;
    private TextView tvDataTitle;
    private TextView tvDataDesc;
    private TextView tvManualPath;
    private ProgressBar pbDownload;
    private TextView tvDownloadProgress;
    private Button btnVerifyData;
    private Button btnDownloadData;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private boolean isDownloading = false;

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_home, container, false);

        if (getActivity() instanceof MainActivity) {
            ((MainActivity) getActivity()).hideKeyboard(getActivity());
        }

        dotStatus = view.findViewById(R.id.dot_status);
        tvDataTitle = view.findViewById(R.id.tv_data_title);
        tvDataDesc = view.findViewById(R.id.tv_data_desc);
        tvManualPath = view.findViewById(R.id.tv_manual_path);
        pbDownload = view.findViewById(R.id.pb_download);
        tvDownloadProgress = view.findViewById(R.id.tv_download_progress);
        btnVerifyData = view.findViewById(R.id.btn_verify_data);
        btnDownloadData = view.findViewById(R.id.btn_download_data);

        Context ctx = requireContext();
        File extDir = ctx.getExternalFilesDir(null);
        if (extDir != null) {
            tvManualPath.setText(extDir.getAbsolutePath());
        }

        try {
            PRDownloader.initialize(ctx.getApplicationContext(),
                    PRDownloaderConfig.newBuilder()
                            .setDatabaseEnabled(true)
                            .setReadTimeout(30000)
                            .setConnectTimeout(30000)
                            .build());
        } catch (Exception ignored) {
        }

        btnVerifyData.setOnClickListener(v -> runVerification());

        runVerification();

        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!isDownloading) {
            runVerification();
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        executor.shutdownNow();
    }

    private void runVerification() {
        if (getContext() == null) return;
        tvDataDesc.setText("Verifying game files...");
        btnVerifyData.setEnabled(false);

        executor.execute(() -> {
            GameDataVerifier.VerificationResult result = GameDataVerifier.verifyGameData(requireContext());
            mainHandler.post(() -> {
                btnVerifyData.setEnabled(true);
                if (result.isValid) {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_green);
                    tvDataTitle.setText("Game Data Ready");
                    tvDataDesc.setText("All core GTA SA & SAMP files are verified (" + (result.totalSizeBytes / 1048576) + " MB). Ready to play!");
                    btnVerifyData.setVisibility(View.GONE);
                    btnDownloadData.setVisibility(View.VISIBLE);
                    btnDownloadData.setEnabled(true);
                    btnDownloadData.setText("PLAY");
                    btnDownloadData.setOnClickListener(v -> {
                        if (getActivity() instanceof MainActivity) {
                            ((MainActivity) getActivity()).changeTab(1);
                        }
                    });
                } else {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_red);
                    tvDataTitle.setText("Game Data Incomplete");
                    tvDataDesc.setText("Verification failed (" + result.missingFiles.size() + " files missing).\nDownload the official asset package or copy files manually.");
                    btnVerifyData.setVisibility(View.VISIBLE);
                    btnVerifyData.setText("Verify");
                    btnDownloadData.setVisibility(View.VISIBLE);
                    btnDownloadData.setEnabled(true);
                    btnDownloadData.setText("Download Assets");
                    btnDownloadData.setOnClickListener(v -> {
                        if (isDownloading) {
                            Toast.makeText(getContext(), "Download in progress...", Toast.LENGTH_SHORT).show();
                        } else {
                            startOnlineDataDownload();
                        }
                    });
                }
            });
        });
    }

    private void startOnlineDataDownload() {
        if (getContext() == null) return;
        isDownloading = true;
        pbDownload.setVisibility(View.VISIBLE);
        tvDownloadProgress.setVisibility(View.VISIBLE);
        pbDownload.setIndeterminate(false);
        pbDownload.setMax(100);
        pbDownload.setProgress(0);
        tvDownloadProgress.setText("0%");
        tvDataDesc.setText("Connecting to asset server...");
        btnDownloadData.setEnabled(false);

        // Run in GameDataDownloadService so it survives the Activity and
        // resumes via Range + .part with retry; we only observe progress here.
        GameDataDownloadService.Listener l = s -> {
            if (!isAdded() || getContext() == null) return;
            mainHandler.post(() -> {
                if (!isAdded()) return;
                if (s.phase == GameDataDownloadService.PHASE_EXTRACT) {
                    pbDownload.setProgress(s.percent);
                    tvDownloadProgress.setText("Extracting " + s.percent + "% ("
                            + s.bytes + "/" + s.total + ")");
                } else if (s.phase == GameDataDownloadService.PHASE_DONE) {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    Toast.makeText(getContext(), "Game assets successfully installed!", Toast.LENGTH_LONG).show();
                    GameDataDownloadService.removeListener(this.listener);
                    runVerification();
                } else if (s.phase == GameDataDownloadService.PHASE_FAILED) {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    tvDataDesc.setText("Download error: " + s.error + "\nTap Download Assets to resume.");
                    GameDataDownloadService.removeListener(this.listener);
                } else if (s.phase == GameDataDownloadService.PHASE_DOWNLOAD) {
                    pbDownload.setProgress(s.percent);
                    tvDownloadProgress.setText(s.percent + "% (" + (s.bytes / 1048576)
                            + "/" + (s.total / 1048576) + " MB)");
                }
            });
        };
        this.listener = l;
        GameDataDownloadService.addListener(l);
        GameDataDownloadService.start(requireContext(),
                GameDataDownloadService.ACTION_ASSETS, DATA_ZIP_URL, "assets", 666478908L, "");
    }

    private volatile GameDataDownloadService.Listener listener;

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (listener != null) { GameDataDownloadService.removeListener(listener); listener = null; }
        executor.shutdownNow();
    }

    private void unzipFile(File zipFile, File targetDir) throws Exception {
        java.util.zip.ZipFile zf = new java.util.zip.ZipFile(zipFile);
        int totalEntries = zf.size();
        int processed = 0;
        byte[] buffer = new byte[65536];

        java.util.Enumeration<? extends java.util.zip.ZipEntry> entries = zf.entries();
        while (entries.hasMoreElements()) {
            java.util.zip.ZipEntry ze = entries.nextElement();
            processed++;

            String entryName = ze.getName();
            // Jika zip membungkus folder root 'files/' atau 'files\', strip prefix-nya
            if (entryName.startsWith("files/") || entryName.startsWith("files\\")) {
                entryName = entryName.substring(6);
            }
            if (entryName.isEmpty()) continue;

            File file = new File(targetDir, entryName);
            String canonicalDestPath = file.getCanonicalPath();
            if (!canonicalDestPath.startsWith(targetDir.getCanonicalPath())) {
                continue;
            }

            if (ze.isDirectory()) {
                file.mkdirs();
            } else {
                File parent = file.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }
                java.io.InputStream is = zf.getInputStream(ze);
                java.io.FileOutputStream fos = new java.io.FileOutputStream(file);
                int count;
                while ((count = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, count);
                }
                fos.flush();
                fos.close();
                is.close();
            }

            final int curProcessed = processed;
            final int tot = totalEntries;
            if (tot > 0 && curProcessed % 25 == 0) {
                final int percent = (curProcessed * 100) / tot;
                mainHandler.post(() -> {
                    pbDownload.setIndeterminate(false);
                    pbDownload.setProgress(percent);
                    tvDownloadProgress.setText("Extracting " + percent + "% (" + curProcessed + "/" + tot + ")");
                });
            }
        }
        zf.close();
    }
}
