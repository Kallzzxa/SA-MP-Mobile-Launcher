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

        view.findViewById(R.id.btn_browse_servers).setOnClickListener(v -> {
            if (getActivity() instanceof MainActivity) {
                ((MainActivity) getActivity()).changeTab(1);
            }
        });
        btnVerifyData.setOnClickListener(v -> runVerification());

        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        int phase = GameDataDownloadService.getState().phase;
        if (phase == GameDataDownloadService.PHASE_CONNECT
                || phase == GameDataDownloadService.PHASE_DOWNLOAD
                || phase == GameDataDownloadService.PHASE_EXTRACT) {
            showDownloadProgress();
            observeDownload(true);
        } else if (!isDownloading) {
            runVerification();
        }
    }

    private void runVerification() {
        if (getContext() == null) return;
        tvDataDesc.setText("Memeriksa file inti game...");
        btnVerifyData.setEnabled(false);

        final Context appContext = requireContext().getApplicationContext();
        final View currentView = getView();
        executor.execute(() -> {
            GameDataVerifier.VerificationResult result = GameDataVerifier.verifyGameData(appContext);
            mainHandler.post(() -> {
                if (currentView == null || getView() != currentView || !isAdded() || isDownloading) return;
                btnVerifyData.setEnabled(true);
                if (result.isValid) {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_green);
                    tvDataTitle.setText("File inti ditemukan");
                    tvDataDesc.setText("File inti ditemukan (" + (result.totalSizeBytes / 1048576) + " MB). Pilih server untuk mulai bermain.");
                    btnVerifyData.setVisibility(View.GONE);
                    btnDownloadData.setVisibility(View.VISIBLE);
                    btnDownloadData.setEnabled(true);
                    btnDownloadData.setText("Pilih server");
                    btnDownloadData.setOnClickListener(v -> {
                        if (getActivity() instanceof MainActivity) {
                            ((MainActivity) getActivity()).changeTab(1);
                        }
                    });
                } else {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_red);
                    tvDataTitle.setText("Data belum lengkap");
                    tvDataDesc.setText("File yang belum ditemukan: " + android.text.TextUtils.join(", ", result.missingFiles) + ".\nUnduh atau salin data secara manual.");
                    btnVerifyData.setVisibility(View.VISIBLE);
                    btnVerifyData.setText("Periksa");
                    btnDownloadData.setVisibility(View.VISIBLE);
                    btnDownloadData.setEnabled(true);
                    btnDownloadData.setText("Unduh data");
                    btnDownloadData.setOnClickListener(v -> {
                        if (isDownloading) {
                            Toast.makeText(getContext(), "Unduhan sedang berlangsung...", Toast.LENGTH_SHORT).show();
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
        showDownloadProgress();
        observeDownload(false);
        GameDataDownloadService.start(requireContext(),
                GameDataDownloadService.ACTION_ASSETS, DATA_ZIP_URL, "assets", 0L, "");
    }

    private void showDownloadProgress() {
        isDownloading = true;
        pbDownload.setVisibility(View.VISIBLE);
        tvDownloadProgress.setVisibility(View.VISIBLE);
        pbDownload.setIndeterminate(false);
        pbDownload.setMax(100);
        pbDownload.setProgress(0);
        tvDownloadProgress.setText("0%");
        tvDataDesc.setText("Menghubungkan ke server unduhan...");
        btnDownloadData.setEnabled(false);
        btnVerifyData.setEnabled(false);
    }

    private void observeDownload(boolean replayState) {
        if (listener != null) return;
        final View currentView = getView();
        GameDataDownloadService.Listener l = s -> {
            if (!isAdded() || getContext() == null) return;
            mainHandler.post(() -> {
                if (!isAdded() || currentView == null || getView() != currentView || !isDownloading) return;
                if (s.phase == GameDataDownloadService.PHASE_EXTRACT) {
                    pbDownload.setProgress(s.percent);
                    tvDownloadProgress.setText("Mengekstrak " + s.percent + "% ("
                            + s.bytes + "/" + s.total + ")");
                } else if (s.phase == GameDataDownloadService.PHASE_DONE) {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    Toast.makeText(getContext(), s.detail == null || s.detail.isEmpty()
                            ? "Download selesai. Periksa data game." : s.detail, Toast.LENGTH_LONG).show();
                    GameDataDownloadService.removeListener(this.listener);
                    this.listener = null;
                    runVerification();
                } else if (s.phase == GameDataDownloadService.PHASE_FAILED) {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    btnVerifyData.setEnabled(true);
                    tvDataDesc.setText("Unduhan gagal: " + s.error + "\nKetuk Unduh data untuk mencoba lagi.");
                    GameDataDownloadService.removeListener(this.listener);
                    this.listener = null;
                } else if (s.phase == GameDataDownloadService.PHASE_DOWNLOAD) {
                    pbDownload.setProgress(s.percent);
                    tvDownloadProgress.setText(s.percent + "% (" + (s.bytes / 1048576)
                            + "/" + (s.total / 1048576) + " MB)");
                }
            });
        };
        this.listener = l;
        GameDataDownloadService.addListener(l);
        // Catch a state transition that happened just before registration.
        GameDataDownloadService.State state = GameDataDownloadService.getState();
        if (replayState && state.phase != GameDataDownloadService.PHASE_IDLE) l.onState(state);
    }

    private volatile GameDataDownloadService.Listener listener;

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (listener != null) { GameDataDownloadService.removeListener(listener); listener = null; }
        isDownloading = false;
    }

    @Override
    public void onDestroy() {
        executor.shutdownNow();
        super.onDestroy();
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
                    tvDownloadProgress.setText("Mengekstrak " + percent + "% (" + curProcessed + "/" + tot + ")");
                });
            }
        }
        zf.close();
    }
}
