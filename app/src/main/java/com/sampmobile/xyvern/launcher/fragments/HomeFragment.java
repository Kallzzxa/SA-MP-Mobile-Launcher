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

        File baseDir = requireContext().getExternalFilesDir(null);
        if (baseDir == null) return;
        File zipFile = new File(baseDir, "files.zip");
        File partFile = new File(baseDir, "files.zip.part");

        executor.execute(() -> {
            try {
                // Cek apakah files.zip sudah pernah terunduh utuh sebelumnya
                long expectedTotal = 666478908L;
                if (!zipFile.exists() || zipFile.length() != expectedTotal) {
                    mainHandler.post(() -> tvDataDesc.setText("Downloading asset pack (~635 MB)..."));

                    long existingBytes = partFile.exists() ? partFile.length() : 0;
                    URL url = new URL(DATA_ZIP_URL);
                    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                    conn.setInstanceFollowRedirects(true);
                    conn.setConnectTimeout(15000);
                    conn.setReadTimeout(30000);

                    if (existingBytes > 0) {
                        conn.setRequestProperty("Range", "bytes=" + existingBytes + "-");
                    }

                    int responseCode = conn.getResponseCode();
                    if (responseCode == HttpURLConnection.HTTP_MOVED_TEMP || responseCode == HttpURLConnection.HTTP_MOVED_PERM || responseCode == 307 || responseCode == 308) {
                        String redirectUrl = conn.getHeaderField("Location");
                        if (redirectUrl != null) {
                            conn.disconnect();
                            conn = (HttpURLConnection) new URL(redirectUrl).openConnection();
                            conn.setConnectTimeout(15000);
                            conn.setReadTimeout(30000);
                            if (existingBytes > 0) {
                                conn.setRequestProperty("Range", "bytes=" + existingBytes + "-");
                            }
                            responseCode = conn.getResponseCode();
                        }
                    }

                    boolean isAppend = (responseCode == HttpURLConnection.HTTP_PARTIAL);
                    if (!isAppend && existingBytes > 0) {
                        existingBytes = 0; // Server tidak support range, mulai ulang dari 0
                    }

                    long totalBytes = conn.getContentLengthLong();
                    if (isAppend) {
                        totalBytes += existingBytes;
                    } else if (totalBytes <= 0) {
                        totalBytes = expectedTotal;
                    }

                    java.io.InputStream in = new java.io.BufferedInputStream(conn.getInputStream(), 65536);
                    java.io.FileOutputStream out = new java.io.FileOutputStream(partFile, isAppend);
                    byte[] buffer = new byte[65536];
                    long downloadedBytes = existingBytes;
                    long lastUpdate = System.currentTimeMillis();
                    int read;

                    while ((read = in.read(buffer)) != -1) {
                        out.write(buffer, 0, read);
                        downloadedBytes += read;

                        long now = System.currentTimeMillis();
                        if (now - lastUpdate > 150) {
                            lastUpdate = now;
                            final long cur = downloadedBytes;
                            final long tot = totalBytes;
                            final int percent = (int) ((cur * 100) / tot);
                            mainHandler.post(() -> {
                                pbDownload.setProgress(percent);
                                tvDownloadProgress.setText(percent + "% (" + (cur / 1048576) + "/" + (tot / 1048576) + " MB)");
                            });
                        }
                    }
                    out.flush();
                    out.close();
                    in.close();
                    conn.disconnect();

                    if (!partFile.renameTo(zipFile)) {
                        throw new Exception("Failed to complete asset pack download.");
                    }
                }

                // Step 2: Unzip files.zip directly into game data directory
                mainHandler.post(() -> {
                    pbDownload.setIndeterminate(false);
                    pbDownload.setMax(100);
                    pbDownload.setProgress(0);
                    tvDataDesc.setText("Extracting game assets...");
                    tvDownloadProgress.setText("Extracting...");
                });

                unzipFile(zipFile, baseDir);

                // Delete zip to save storage
                if (zipFile.exists()) {
                    zipFile.delete();
                }

                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    Toast.makeText(getContext(), "Game assets successfully installed!", Toast.LENGTH_LONG).show();
                    runVerification();
                });

            } catch (Exception e) {
                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    tvDataDesc.setText("Download error: " + e.getMessage() + "\nTap Download Assets to resume.");
                    Toast.makeText(getContext(), "Error: " + e.getMessage(), Toast.LENGTH_LONG).show();
                    runVerification();
                });
            }
        });
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
