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
                    btnDownloadData.setText("Download Ulang Data");
                } else {
                    dotStatus.setBackgroundResource(R.drawable.bg_dot_red);
                    tvDataTitle.setText("Data Game Belum Lengkap");
                    tvDataDesc.setText("File tidak terverifikasi (" + result.missingFiles.size() + " komponen hilang).\nKlik 'Download Data' untuk mengunduh data pack resmi (files.zip).");
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
        pbDownload.setIndeterminate(false);
        pbDownload.setMax(100);
        pbDownload.setProgress(0);
        tvDownloadProgress.setText("0%");
        tvDataDesc.setText("Menghubungkan ke server data...");
        btnDownloadData.setEnabled(false);

        File baseDir = requireContext().getExternalFilesDir(null);
        if (baseDir == null) return;
        File zipFile = new File(baseDir, "files.zip");

        executor.execute(() -> {
            try {
                // Step 1: Download files.zip langsung dari hosting cepat GitHub Release
                mainHandler.post(() -> tvDataDesc.setText("Mengunduh data pack (files.zip ~635 MB)..."));

                URL url = new URL(DATA_ZIP_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setInstanceFollowRedirects(true);
                conn.setConnectTimeout(15000);
                conn.setReadTimeout(30000);

                int responseCode = conn.getResponseCode();
                // Handle manual redirect if needed (GitHub releases redirect to AWS S3/Azure)
                if (responseCode == HttpURLConnection.HTTP_MOVED_TEMP || responseCode == HttpURLConnection.HTTP_MOVED_PERM || responseCode == 307 || responseCode == 308) {
                    String redirectUrl = conn.getHeaderField("Location");
                    if (redirectUrl != null) {
                        conn.disconnect();
                        conn = (HttpURLConnection) new URL(redirectUrl).openConnection();
                        conn.setConnectTimeout(15000);
                        conn.setReadTimeout(30000);
                    }
                }

                long totalBytes = conn.getContentLengthLong();
                if (totalBytes <= 0) totalBytes = 666478908L; // fallback length

                java.io.InputStream in = new java.io.BufferedInputStream(conn.getInputStream(), 65536);
                java.io.FileOutputStream out = new java.io.FileOutputStream(zipFile);
                byte[] buffer = new byte[65536];
                long downloadedBytes = 0;
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

                // Step 2: Unzip files.zip langsung ke root folder data game
                mainHandler.post(() -> {
                    pbDownload.setIndeterminate(true);
                    tvDataDesc.setText("Mengekstrak data game ke direktori lokal...");
                    tvDownloadProgress.setText("Mengekstrak...");
                });

                unzipFile(zipFile, baseDir);

                // Hapus zip untuk menghemat storage
                if (zipFile.exists()) {
                    zipFile.delete();
                }

                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    Toast.makeText(getContext(), "Data game berhasil diunduh dan dipasang!", Toast.LENGTH_LONG).show();
                    runVerification();
                });

            } catch (Exception e) {
                if (zipFile.exists()) {
                    zipFile.delete();
                }
                mainHandler.post(() -> {
                    isDownloading = false;
                    pbDownload.setVisibility(View.GONE);
                    tvDownloadProgress.setVisibility(View.GONE);
                    btnDownloadData.setEnabled(true);
                    tvDataDesc.setText("Gagal mengunduh: " + e.getMessage() + "\nSilakan cek koneksi atau letakkan file GTA SA manual.");
                    Toast.makeText(getContext(), "Gagal download data: " + e.getMessage(), Toast.LENGTH_LONG).show();
                    runVerification();
                });
            }
        });
    }

    private void unzipFile(File zipFile, File targetDir) throws Exception {
        java.util.zip.ZipInputStream zis = new java.util.zip.ZipInputStream(
                new java.io.BufferedInputStream(new java.io.FileInputStream(zipFile), 65536));
        java.util.zip.ZipEntry ze;
        byte[] buffer = new byte[65536];

        while ((ze = zis.getNextEntry()) != null) {
            String entryName = ze.getName();
            // Cegah zip slip vulnerability
            File file = new File(targetDir, entryName);
            String canonicalDestPath = file.getCanonicalPath();
            if (!canonicalDestPath.startsWith(targetDir.getCanonicalPath())) {
                zis.closeEntry();
                continue;
            }

            if (ze.isDirectory()) {
                file.mkdirs();
            } else {
                File parent = file.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }
                java.io.FileOutputStream fos = new java.io.FileOutputStream(file);
                int count;
                while ((count = zis.read(buffer)) != -1) {
                    fos.write(buffer, 0, count);
                }
                fos.flush();
                fos.close();
            }
            zis.closeEntry();
        }
        zis.close();
    }
}
