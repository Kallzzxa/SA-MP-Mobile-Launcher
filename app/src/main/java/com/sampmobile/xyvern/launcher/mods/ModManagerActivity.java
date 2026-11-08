package com.sampmobile.xyvern.launcher.mods;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Pengelola mod: impor dari penyimpanan, aktif/nonaktif, hapus, cari, filter kategori,
 * dan riwayat. Hanya bekerja pada file — pemuatan ke dalam game belum didukung engine,
 * dan itu ditulis apa adanya di layar, bukan disembunyikan.
 */
public class ModManagerActivity extends AppCompatActivity {

    private static final int PICK_FILES = 4711;
    private static final int BG = Color.parseColor("#0B1220");
    private static final int CARD = Color.parseColor("#132033");
    private static final int ACCENT = Color.parseColor("#33B9F7");
    private static final int TEXT = Color.parseColor("#E7F1FA");
    private static final int MUTED = Color.parseColor("#8FA6BC");

    private final List<ModEntry> all = new ArrayList<>();
    private final List<ModEntry> shown = new ArrayList<>();

    private ModRepository repository;
    private ModAdapter adapter;
    private TextView counter;
    private EditText search;
    private Spinner filter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        repository = new ModRepository(this);
        setContentView(buildUi());
        reload();
    }

    private View buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(16), dp(20), dp(16), dp(12));

        TextView title = new TextView(this);
        title.setText("Pengelola Mod");
        title.setTextColor(TEXT);
        title.setTextSize(22);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);

        TextView notice = new TextView(this);
        notice.setText("Status: Sebagian. File di sini disiapkan dan divalidasi, tetapi engine belum "
                + "mendukung pemuatan model/tekstur kustom, jadi mod tidak diterapkan ke dalam game.");
        notice.setTextColor(Color.parseColor("#FFC97A"));
        notice.setTextSize(12);
        notice.setPadding(0, dp(8), 0, dp(12));
        root.addView(notice);

        counter = new TextView(this);
        counter.setTextColor(MUTED);
        counter.setTextSize(12);
        root.addView(counter);

        search = new EditText(this);
        search.setHint("Cari nama mod");
        search.setHintTextColor(MUTED);
        search.setTextColor(TEXT);
        search.setSingleLine(true);
        search.setBackground(round(CARD, dp(12)));
        search.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams searchParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        searchParams.topMargin = dp(12);
        root.addView(search, searchParams);
        search.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) { applyFilter(); }
            @Override public void afterTextChanged(Editable s) { }
        });

        filter = new Spinner(this);
        ArrayAdapter<String> categories = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item,
                new String[]{"Semua kategori", "Model dan peta", "Suara", "Skrip", "Gambar antarmuka", "Lainnya"});
        filter.setAdapter(categories);
        LinearLayout.LayoutParams filterParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        filterParams.topMargin = dp(8);
        root.addView(filter, filterParams);
        filter.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) { applyFilter(); }
            @Override public void onNothingSelected(AdapterView<?> parent) { }
        });

        ListView list = new ListView(this);
        list.setDivider(null);
        list.setDividerHeight(dp(8));
        list.setPadding(0, dp(12), 0, dp(8));
        list.setClipToPadding(false);
        adapter = new ModAdapter();
        list.setAdapter(adapter);
        LinearLayout.LayoutParams listParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f);
        root.addView(list, listParams);
        list.setOnItemClickListener((parent, view, position, id) -> showActions(shown.get(position)));
        list.setOnItemLongClickListener((parent, view, position, id) -> {
            showActions(shown.get(position));
            return true;
        });

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);

        Button importButton = button("Impor mod");
        importButton.setOnClickListener(v -> pickFiles());
        actions.addView(importButton, weight());

        Button historyButton = button("Riwayat");
        historyButton.setOnClickListener(v -> showHistory());
        actions.addView(historyButton, weight());

        root.addView(actions);
        return root;
    }

    private LinearLayout.LayoutParams weight() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        params.setMargins(dp(4), 0, dp(4), 0);
        return params;
    }

    private Button button(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextColor(Color.parseColor("#04121E"));
        button.setBackground(round(ACCENT, dp(12)));
        return button;
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

    private void reload() {
        all.clear();
        all.addAll(repository.scan());
        all.sort((left, right) -> left.name().compareToIgnoreCase(right.name()));
        applyFilter();
    }

    private void applyFilter() {
        String query = search.getText().toString().trim().toLowerCase(Locale.ROOT);
        String category = filter.getSelectedItemPosition() <= 0 ? null : (String) filter.getSelectedItem();

        shown.clear();
        for (ModEntry entry : all) {
            if (category != null && !category.equals(entry.category)) continue;
            if (!query.isEmpty() && !entry.name().toLowerCase(Locale.ROOT).contains(query)) continue;
            shown.add(entry);
        }

        long total = 0L;
        int problems = 0;
        for (ModEntry entry : all) {
            total += entry.size;
            if (!entry.isValid()) problems++;
        }

        counter.setText(all.size() + " mod • " + ModRepository.readableSize(total)
                + (problems > 0 ? " • " + problems + " bermasalah" : "")
                + " • folder: " + ModRepository.ACTIVE_DIR + "/");
        adapter.notifyDataSetChanged();
    }

    private void pickFiles() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(intent, PICK_FILES);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_FILES || resultCode != Activity.RESULT_OK || data == null) return;

        List<Uri> uris = new ArrayList<>();
        if (data.getClipData() != null) {
            for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                uris.add(data.getClipData().getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }

        List<String> report = new ArrayList<>();
        for (Uri uri : uris) {
            try {
                ModRepository.ImportResult result = repository.importFrom(uri, displayName(uri));
                report.add((result.accepted ? "Berhasil • " : "Ditolak • ") + result.detail);
            } catch (Exception error) {
                report.add("Gagal • " + displayName(uri) + ": " + error.getMessage());
            }
        }

        reload();
        if (!report.isEmpty()) {
            new AlertDialog.Builder(this)
                    .setTitle("Hasil impor")
                    .setMessage(String.join("\n\n", report))
                    .setPositiveButton("Tutup", null)
                    .show();
        }
    }

    private String displayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    String name = cursor.getString(index);
                    if (name != null && !name.isEmpty()) return name;
                }
            }
        } catch (Exception ignored) {
            // fall through to the path based name
        }

        String path = uri.getLastPathSegment();
        return path == null ? "mod" : path;
    }

    private void showActions(ModEntry entry) {
        new AlertDialog.Builder(this)
                .setTitle(entry.name())
                .setMessage(entry.category + " • " + entry.readableSize() + " • " + entry.statusText()
                        + (entry.isValid() ? "" : "\n\nKeterangan: " + entry.problem))
                .setPositiveButton(entry.enabled ? "Nonaktifkan" : "Aktifkan", (dialog, which) -> {
                    boolean moved = repository.setEnabled(entry, !entry.enabled);
                    Toast.makeText(this, moved ? "Status diubah" : "Gagal memindahkan file",
                            Toast.LENGTH_SHORT).show();
                    reload();
                })
                .setNegativeButton("Hapus", (dialog, which) -> confirmDelete(entry))
                .setNeutralButton("Tutup", null)
                .show();
    }

    private void confirmDelete(ModEntry entry) {
        new AlertDialog.Builder(this)
                .setTitle("Hapus mod?")
                .setMessage(entry.name() + " (" + entry.readableSize() + ") akan dihapus dari penyimpanan launcher. "
                        + "Tindakan ini tidak dapat dibatalkan.")
                .setPositiveButton("Hapus", (dialog, which) -> {
                    boolean deleted = repository.delete(entry);
                    Toast.makeText(this, deleted ? "Mod dihapus" : "Gagal menghapus",
                            Toast.LENGTH_SHORT).show();
                    reload();
                })
                .setNegativeButton("Batal", null)
                .show();
    }

    private void showHistory() {
        List<String> lines = repository.history();
        new AlertDialog.Builder(this)
                .setTitle("Riwayat pemasangan")
                .setMessage(lines.isEmpty() ? "Belum ada aktivitas." : String.join("\n", lines))
                .setPositiveButton("Tutup", null)
                .show();
    }

    private final class ModAdapter extends BaseAdapter {
        @Override public int getCount() { return shown.size(); }
        @Override public Object getItem(int position) { return shown.get(position); }
        @Override public long getItemId(int position) { return position; }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ModEntry entry = shown.get(position);

            LinearLayout card = new LinearLayout(ModManagerActivity.this);
            card.setOrientation(LinearLayout.VERTICAL);
            card.setBackground(round(CARD, dp(14)));
            card.setPadding(dp(14), dp(12), dp(14), dp(12));

            TextView name = new TextView(ModManagerActivity.this);
            name.setText(entry.name());
            name.setTextColor(TEXT);
            name.setTextSize(15);
            name.setTypeface(Typeface.DEFAULT_BOLD);
            card.addView(name);

            TextView meta = new TextView(ModManagerActivity.this);
            meta.setText(entry.category + " • " + entry.readableSize() + " • " + entry.statusText());
            meta.setTextColor(entry.isValid() ? MUTED : Color.parseColor("#FF9A9A"));
            meta.setTextSize(12);
            meta.setPadding(0, dp(4), 0, 0);
            card.addView(meta);

            return card;
        }
    }
}
