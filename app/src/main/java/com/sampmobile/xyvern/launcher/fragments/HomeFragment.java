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

    // Default manifest URL (bisa dikustom atau diarahkan ke mirror online manapun)
    private static final String DEFAULT_MANIFEST_URL = "https://samp-mobile.shop/client_config.json";

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
            tvManualPath.setText("Letakkan file GTA SA (data, texdb, models) di:\n" + extDir.getAbsolutePath());
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
        btnDownloadData.setOnClickListener(v -> {
            if (isDownloading) {
                Toast.makeText(getContext(), "Proses download sedang berjalan!", Toast.LENGTH_SHORT).show();
            } else {
                startOnlineDataDownload();
            }
        });

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
        tvDataDesc.setText("Sedang memverifikasi file game...");
        btnVerifyData.setEnabled(false);

        executor.execute(() -> {
            GameDataVerifier.VerificationResult result = GameDataVerifier.verifyGameData(requireContext());
            mainHandler.post(() -> {
                btnVerifyData.setEnabled(true);
                if (result.isValid) {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_green);
                    tvDataTitle.setText("Data Game Terverifikasi");
                    tvDataDesc.setText("Semua file dasar GTA SA & SAMP lengkap (" + (result.totalSizeBytes / 1048576) + " MB). Siap bermain!");
                    btnDownloadData.setText("Update Data");
                } else {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_red);
                    tvDataTitle.setText("Data Game Belum Lengkap");
                    tvDataDesc.setText("File tidak terverifikasi (" + result.missingFiles.size() + " komponen hilang).\nSilakan klik 'Download Data' atau copy manual data GTA SA.");
                    btnDownloadData.setText("Download Data");
                }
            });
        });
    }

    private void startOnlineDataDownload() {
        if (getContext() == null) return;
        isDownloading = true;
        pbDownload.setVisibility(View.VISIBLE);
        tvDownloadProgress.setVisibility(View.VISIBLE);
        pbDownload.setIndeterminate(true);
        tvDataDesc.setText("Mengambil daftar file game...");
        btnDownloadData.setEnabled(false);

        executor.execute(() -> {
            try {
                // Fetch config
                URL url = new URL(DEFAULT_MANIFEST_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(10000);
                conn.setReadTimeout(10000);

                if (conn.getResponseCode() != HttpURLConnection.HTTP_OK) {
                    throw new Exception("HTTP Response: " + conn.getResponseCode());
                }

                BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) sb.append(line);
                reader.close();

                JSONObject configJson = new JSONObject(sb.toString()).getJSONObject("client_config");
                String cacheFilesUrl = configJson.getString("url_cache_files");

                // Fetch files manifest
                URL filesUrl = new URL(cacheFilesUrl);
                HttpURLConnection filesConn = (HttpURLConnection) filesUrl.openConnection();
                filesConn.setConnectTimeout(10000);
                filesConn.setReadTimeout(10000);

                BufferedReader filesReader = new BufferedReader(new InputStreamReader(filesConn.getInputStream(), StandardCharsets.UTF_8));
                StringBuilder filesSb = new StringBuilder();
                while ((line = filesReader.readLine()) != null) filesSb.append(line);
                filesReader.close();

                JSONObject filesObj = new JSONObject(filesSb.toString());
                JSONArray filesArray = filesObj.getJSONArray("files");

                ArrayList<FilesData> downloadQueue = new ArrayList<>();
                long totalDownloadBytes = 0;
                File baseDir = requireContext().getExternalFilesDir(null);

                for (int i = 0; i < filesArray.length(); i++) {
                    JSONObject item = filesArray.getJSONObject(i);
                    String name = item.getString("name");
                    long size = item.getLong("size");
                    String path = item.getString("path");
                    String fileUrl = item.optString("url", "https://samp-mobile.shop/files/" + path);

                    File localFile = new File(baseDir, path);
                    if (!localFile.exists() || localFile.length() != size) {
                        downloadQueue.add(new FilesData(name, size, path, fileUrl));
                        totalDownloadBytes += size;
                    }
                }

                if (downloadQueue.isEmpty()) {
                    mainHandler.post(() -> {
                        isDownloading = false;
                        pbDownload.setVisibility(View.GONE);
                        tvDownloadProgress.setVisibility(View.GONE);
                        btnDownloadData.setEnabled(true);
                        Toast.makeText(getContext(), "Data sudah lengkap, tidak ada yang perlu diunduh!", Toast.LENGTH_SHORT).show();
                        runVerification();
                    });
                    return;
                }

                final long finalTotalBytes = totalDownloadBytes;
                final int totalFiles = downloadQueue.size();

                mainHandler.post(() -> {
                    pbDownload.setIndeterminate(false);
                    pbDownload.setMax(100);
                    pbDownload.setProgress(0);
                    tvDataDesc.setText("Mengunduh " + totalFiles + " file game...");
                });

                // Download loop
                long[] downloadedBytesAccumulator = new long[]{0};

                for (int i = 0; i < downloadQueue.size(); i++) {
                    FilesData fileItem = downloadQueue.get(i);
                    File target = new File(baseDir, fileItem.getPath());
                    if (target.getParentFile() != null && !target.getParentFile().exists()) {
                        target.getParentFile().mkdirs();
                    }

                    final int currentFileIdx = i + 1;
                    mainHandler.post(() -> {
                        tvDataDesc.setText("Mengunduh (" + currentFileIdx + "/" + totalFiles + "): " + fileItem.getName());
                    });

                    boolean[] fileDone = new boolean[]{false};
                    boolean[] fileFailed = new boolean[]{false};

                    String dirPath = target.getParent();
                    String fileName = target.getName();

                    PRDownloader.download(fileItem.getUrl(), dirPath, fileName)
                            .build()
                            .setOnProgressListener(new OnProgressListener() {
                                @Override
                                public void onProgress(Progress progress) {
                                    if (finalTotalBytes > 0) {
                                        long currentTotal = downloadedBytesAccumulator[0] + progress.currentBytes;
                                        int p = (int) ((currentTotal * 100) / finalTotalBytes);
                                        if (p > 100) p = 100;
                                        final int finalPercent = p;
                                        mainHandler.post(() -> {
                                            pbDownload.setProgress(finalPercent);
                                            tvDownloadProgress.setText(finalPercent + "%");
                                        });
                                    }
                                }
                            })
                            .start(new OnDownloadListener() {
                                @Override
                                public void onDownloadComplete() {
                                    downloadedBytesAccumulator[0] += fileItem.getSize();
                                    fileDone[0] = true;
                                }

                                @Override
                                public void onError(Error error) {
                                    fileFailed[0] = true;
                                    fileDone[0] = true;
                                }
                            });

                    while (!fileDone[0]) {
                        try {
                            Thread.sleep(50);
                        } catch (InterruptedException ignored) {}
                    }
                }

                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    Toast.makeText(getContext(), "Download data game selesai!", Toast.LENGTH_LONG).show();
                    runVerification();
                });

            } catch (Exception e) {
                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    tvDataDesc.setText("Gagal mengunduh data online (" + e.getMessage() + ").\nSilakan masukkan data GTA SA sendiri ke direktori yang tertera.");
                    Toast.makeText(getContext(), "Gagal koneksi server data. Silakan tambahkan file manual.", Toast.LENGTH_LONG).show();
                    runVerification();
                });
            }
        });
    }
}
