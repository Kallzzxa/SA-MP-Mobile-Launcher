package com.hopepride.samp.launcher.crash;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import com.hopepride.samp.diagnostics.BootGuard;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Pusat crash: membaca crash_native.txt, samp_log.txt dan log diagnostik, lalu
 * menampilkan apa yang benar-benar tertulis di berkas — bukan kesimpulan karangan.
 */
public class CrashCenterActivity extends AppCompatActivity {

    private static final int PICK_FILE = 4712;
    private static final int BG = Color.parseColor("#0B1220");
    private static final int CARD = Color.parseColor("#132033");
    private static final int ACCENT = Color.parseColor("#33B9F7");
    private static final int TEXT = Color.parseColor("#E7F1FA");
    private static final int MUTED = Color.parseColor("#8FA6BC");

    private final List<File> files = new ArrayList<>();
    private LogAdapter adapter;
    private TextView safeModeState;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(buildUi());
        reload();
    }

    private View buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(16), dp(20), dp(16), dp(12));

        TextView title = new TextView(this);
        title.setText("Pusat Crash");
        title.setTextColor(TEXT);
        title.setTextSize(22);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);

        TextView hint = new TextView(this);
        hint.setText("Laporan dianalisis apa adanya: sinyal, alamat fault, simbol, dan pola yang dikenal. "
                + "Satu alamat crash bukan bukti akar masalah.");
        hint.setTextColor(MUTED);
        hint.setTextSize(12);
        hint.setPadding(0, dp(8), 0, dp(12));
        root.addView(hint);

        safeModeState = new TextView(this);
        safeModeState.setTextColor(Color.parseColor("#FFC97A"));
        safeModeState.setTextSize(12);
        root.addView(safeModeState);

        Button clearSafeMode = new Button(this);
        clearSafeMode.setText("Keluar dari Safe Mode");
        clearSafeMode.setAllCaps(false);
        clearSafeMode.setTextColor(Color.parseColor("#04121E"));
        clearSafeMode.setBackground(round(ACCENT, dp(12)));
        clearSafeMode.setOnClickListener(v -> {
            BootGuard.clearSafeMode(this);
            Toast.makeText(this, "Penanda safe mode dihapus", Toast.LENGTH_SHORT).show();
            updateSafeMode();
        });
        LinearLayout.LayoutParams clearParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        clearParams.topMargin = dp(8);
        root.addView(clearSafeMode, clearParams);

        ListView list = new ListView(this);
        list.setDivider(null);
        list.setDividerHeight(dp(8));
        list.setPadding(0, dp(12), 0, dp(8));
        list.setClipToPadding(false);
        adapter = new LogAdapter();
        list.setAdapter(adapter);
        root.addView(list, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        list.setOnItemClickListener((parent, view, position, id) -> open(files.get(position)));

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);

        Button pick = new Button(this);
        pick.setText("Buka berkas lain");
        pick.setAllCaps(false);
        pick.setTextColor(Color.parseColor("#04121E"));
        pick.setBackground(round(ACCENT, dp(12)));
        pick.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, PICK_FILE);
        });
        actions.addView(pick, weight());

        Button share = new Button(this);
        share.setText("Bagikan semua");
        share.setAllCaps(false);
        share.setTextColor(TEXT);
        share.setBackground(round(CARD, dp(12)));
        share.setOnClickListener(v -> shareAll());
        actions.addView(share, weight());

        root.addView(actions);
        return root;
    }

    private LinearLayout.LayoutParams weight() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        params.setMargins(dp(4), 0, dp(4), 0);
        return params;
    }

    private GradientDrawable round(int color, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(radius);
        return drawable;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void updateSafeMode() {
        boolean active = BootGuard.isSafeMode(this);
        safeModeState.setText(active
                ? "Safe mode AKTIF: patch opsional database tekstur tidak dipasang saat start berikutnya."
                : "Safe mode tidak aktif.");
    }

    private void reload() {
        files.clear();

        File external = getExternalFilesDir(null);
        addIfPresent(external == null ? null : new File(external, "crash_native.txt"));
        addIfPresent(external == null ? null : new File(external, "samp_log.txt"));

        File diagnostics = new File(getFilesDir(), "diagnostics");
        File[] logs = diagnostics.listFiles();
        if (logs != null) {
            for (File log : logs) addIfPresent(log);
        }

        adapter.notifyDataSetChanged();
        updateSafeMode();
    }

    private void addIfPresent(File file) {
        if (file != null && file.isFile() && file.length() > 0L) files.add(file);
    }

    private String read(File file) {
        try (InputStream in = new FileInputStream(file)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[16 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) out.write(buffer, 0, read);
            return out.toString("UTF-8");
        } catch (Exception error) {
            return "";
        }
    }

    private void open(File file) {
        String text = read(file);
        CrashReport report = CrashReport.parse(text);

        new AlertDialog.Builder(this)
                .setTitle(file.getName())
                .setMessage(report.summary())
                .setPositiveButton("Salin laporan", (dialog, which) -> {
                    copy(report.summary());
                    Toast.makeText(this, "Laporan disalin", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("Bagikan", (dialog, which) -> share(report.summary()))
                .setNeutralButton("Tutup", null)
                .show();
    }

    private void copy(String text) {
        ClipboardManager manager = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (manager != null) manager.setPrimaryClip(ClipData.newPlainText("HopePride", text));
    }

    private void share(String text) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_SUBJECT, "Laporan HopePride");
        intent.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(intent, "Bagikan laporan"));
    }

    private void shareAll() {
        StringBuilder builder = new StringBuilder();
        for (File file : files) {
            builder.append("=== ").append(file.getName()).append(" ===\n");
            CrashReport report = CrashReport.parse(read(file));
            builder.append(report.summary()).append('\n');
        }
        if (builder.length() == 0) {
            Toast.makeText(this, "Belum ada berkas log", Toast.LENGTH_SHORT).show();
            return;
        }
        share(builder.toString());
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_FILE || resultCode != Activity.RESULT_OK || data == null || data.getData() == null) return;

        try (InputStream in = getContentResolver().openInputStream(data.getData())) {
            if (in == null) return;
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[16 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) out.write(buffer, 0, read);

            CrashReport report = CrashReport.parse(out.toString("UTF-8"));
            new AlertDialog.Builder(this)
                    .setTitle("Berkas terpilih")
                    .setMessage(report.summary())
                    .setPositiveButton("Salin laporan", (dialog, which) -> copy(report.summary()))
                    .setNegativeButton("Tutup", null)
                    .show();
        } catch (Exception error) {
            Toast.makeText(this, "Gagal membaca berkas: " + error.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private final class LogAdapter extends BaseAdapter {
        @Override public int getCount() { return files.size(); }
        @Override public Object getItem(int position) { return files.get(position); }
        @Override public long getItemId(int position) { return position; }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            File file = files.get(position);

            LinearLayout card = new LinearLayout(CrashCenterActivity.this);
            card.setOrientation(LinearLayout.VERTICAL);
            card.setBackground(round(CARD, dp(14)));
            card.setPadding(dp(14), dp(12), dp(14), dp(12));

            TextView name = new TextView(CrashCenterActivity.this);
            name.setText(file.getName());
            name.setTextColor(TEXT);
            name.setTextSize(15);
            name.setTypeface(Typeface.DEFAULT_BOLD);
            card.addView(name);

            TextView meta = new TextView(CrashCenterActivity.this);
            String when = new SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.GERMANY).format(new Date(file.lastModified()));
            meta.setText(com.hopepride.samp.launcher.mods.ModRepository.readableSize(file.length()) + " • " + when);
            meta.setTextColor(MUTED);
            meta.setTextSize(12);
            meta.setPadding(0, dp(4), 0, 0);
            card.addView(meta);

            return card;
        }
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }
}
