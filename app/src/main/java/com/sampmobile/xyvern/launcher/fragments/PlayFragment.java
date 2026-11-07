package com.sampmobile.xyvern.launcher.fragments;

import android.app.Dialog;
import android.content.Context;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.LruCache;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.Spinner;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;
import com.sampmobile.xyvern.game.SAMP;
import com.sampmobile.xyvern.game.StartupAssets;
import com.sampmobile.xyvern.launcher.MainActivity;
import com.sampmobile.xyvern.launcher.util.SAMPServerInfo;
import com.sampmobile.xyvern.launcher.util.SampQueryAPI;
import com.sampmobile.xyvern.launcher.util.ServerInput;
import com.sampmobile.xyvern.launcher.util.ServerSearch;
import com.sampmobile.xyvern.launcher.util.ServerSort;
import com.sampmobile.xyvern.launcher.util.ServerPlayerQuery;
import com.sampmobile.xyvern.launcher.util.PlayerListFilter;
import com.sampmobile.xyvern.launcher.util.ServerRulesQuery;
import com.sampmobile.xyvern.launcher.util.ServerPingQuery;
import com.sampmobile.xyvern.launcher.util.LauncherEvents;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

@Obfuscate
public class PlayFragment extends Fragment {

    public interface OnServerClickListener {
        void onServerClick(SAMPServerInfo server);
    }

    private static final String HOSTED_URL = "https://kethrnensm.github.io/web/hosted.json";
    private RecyclerView rvServers;
    private ServerAdapter adapter;
    private final ArrayList<SAMPServerInfo> hostedServers = new ArrayList<>();
    private final ArrayList<SAMPServerInfo> currentServers = new ArrayList<>();
    private TextView tabFavorites, tabHosted, emptyServers;
    private boolean hostedLoading = true;
    private boolean showingFavorites = true;
    private String searchQuery = "";
    private int serverSortPosition = 0;

    private Handler pingHandler;
    private Runnable pingRunnable;
    private Dialog playersDialog;
    private Dialog rulesDialog;

    // Thread pool to prevent thread starvation and unbounded thread creation
    private final ExecutorService queryExecutor = Executors.newFixedThreadPool(4);

    // Cache query results (100 servers) to make scrolling instant and save network
    private final LruCache<String, String[]> queryCache = new LruCache<>(100);
    private volatile int queryGeneration = 0;

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_play, container, false);

        rvServers = view.findViewById(R.id.rv_servers);
        rvServers.setLayoutManager(new LinearLayoutManager(getContext()));
        rvServers.setHasFixedSize(true);

        tabFavorites = view.findViewById(R.id.tab_favorites);
        tabHosted = view.findViewById(R.id.tab_hosted);
        emptyServers = view.findViewById(R.id.empty_servers);

        EditText search = view.findViewById(R.id.search_servers);
        search.setText(searchQuery);
        search.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) {
                searchQuery = text.toString();
                updateList();
            }
            @Override public void afterTextChanged(Editable text) { }
        });

        Spinner serverSort = view.findViewById(R.id.server_sort);
        String[] serverSortLabels = {"Urutan asli", "Nama tersimpan A–Z", "Alamat A–Z"};
        ArrayAdapter<String> serverSortAdapter = new ArrayAdapter<>(requireContext(), R.layout.item_players_sort, serverSortLabels);
        serverSortAdapter.setDropDownViewResource(R.layout.item_players_sort);
        serverSort.setAdapter(serverSortAdapter);
        serverSort.setSelection(serverSortPosition);
        serverSort.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View selected, int position, long id) {
                if (serverSortPosition != position) {
                    serverSortPosition = position;
                    updateList();
                }
            }
            @Override public void onNothingSelected(AdapterView<?> parent) { }
        });

        tabFavorites.setOnClickListener(v -> switchTab(true));
        tabHosted.setOnClickListener(v -> switchTab(false));

        Button refreshServers = view.findViewById(R.id.refresh_servers);
        refreshServers.setOnClickListener(v -> {
            if (!showingFavorites && hostedLoading) {
                Toast.makeText(getContext(), "Daftar Hosted sedang dimuat.", Toast.LENGTH_SHORT).show();
                return;
            }
            synchronized (queryCache) {
                queryGeneration++;
                queryCache.evictAll();
            }
            if (!showingFavorites) loadData(view);
            updateList();
        });

        Button btnAdd = view.findViewById(R.id.btn_add_server);
        btnAdd.setOnClickListener(v -> showAddServerDialog());

        // Favorites are local and must be visible even when the hosted list is offline.
        switchTab(showingFavorites);
        loadData(view);

        return view;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        stopPingTask();
        if (playersDialog != null) playersDialog.dismiss();
        playersDialog = null;
        if (rulesDialog != null) rulesDialog.dismiss();
        rulesDialog = null;
        adapter = null;
        rvServers = null;
        emptyServers = null;
    }

    @Override
    public void onDestroy() {
        queryExecutor.shutdownNow();
        super.onDestroy();
    }

    private void switchTab(boolean favorites) {
        showingFavorites = favorites;
        tabFavorites.setBackgroundResource(favorites ? R.drawable.bg_hope_nav_selected : 0);
        tabHosted.setBackgroundResource(!favorites ? R.drawable.bg_hope_nav_selected : 0);
        updateList();
    }

    private void loadData(View view) {
        hostedLoading = true;
        queryExecutor.execute(() -> {
            try {
                URL url = new URL(HOSTED_URL);
                HttpURLConnection connection = (HttpURLConnection) url.openConnection();
                connection.setRequestMethod("GET");
                connection.setConnectTimeout(6000);
                connection.setReadTimeout(6000);

                if (connection.getResponseCode() == HttpURLConnection.HTTP_OK) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream(), StandardCharsets.UTF_8));
                    StringBuilder response = new StringBuilder();
                    String line;
                    while ((line = in.readLine()) != null) {
                        response.append(line);
                    }
                    in.close();

                    JSONObject jsonObject = new JSONObject(response.toString());
                    JSONArray jsonArray = jsonObject.getJSONArray("query");

                    ArrayList<SAMPServerInfo> fetched = new ArrayList<>();
                    for (int i = 0; i < jsonArray.length(); i++) {
                        JSONObject obj = jsonArray.getJSONObject(i);
                        SAMPServerInfo server = new SAMPServerInfo();
                        server.setServerName(obj.optString("name", "Unknown Server"));
                        server.setAddress(obj.optString("ip", ""));
                        server.setPort(obj.optInt("port", 7777));
                        server.setCurrentPlayerCount(obj.optInt("online", 0));
                        server.setMaxPlayerCount(obj.optInt("maxplayers", 0));
                        server.setType(SAMPServerInfo.Official.HOSTED);
                        fetched.add(server);
                    }

                    new Handler(Looper.getMainLooper()).post(() -> {
                        if (!isAdded() || getView() != view) return;
                        hostedServers.clear();
                        hostedServers.addAll(fetched);
                        hostedLoading = false;
                        updateList();
                    });
                }
                connection.disconnect();
            } catch (Exception e) {
                android.util.Log.w("PlayFragment", "Gagal memuat daftar Hosted", e);
            } finally {
                new Handler(Looper.getMainLooper()).post(() -> {
                    if (!isAdded() || getView() != view) return;
                    hostedLoading = false;
                    updateList();
                });
            }
        });
    }

    private void updateList() {
        currentServers.clear();
        if (showingFavorites) {
            currentServers.addAll(loadCustomServers());
        } else {
            currentServers.addAll(hostedServers);
        }

        if (!searchQuery.trim().isEmpty()) {
            java.util.Iterator<SAMPServerInfo> iterator = currentServers.iterator();
            while (iterator.hasNext()) {
                SAMPServerInfo server = iterator.next();
                String[] cached = queryCache.get(server.getAddress() + ":" + server.getPort());
                String cachedName = cached != null && cached.length > 3 ? cached[3] : null;
                if (!ServerSearch.matches(searchQuery, server.getServerName(), server.getAddress(), server.getPort(), cachedName)) {
                    iterator.remove();
                }
            }
        }

        // Sort only the displayed copy, never the persisted favorites or hosted source.
        if (serverSortPosition == 1) {
            java.util.Collections.sort(currentServers, (a, b) -> ServerSort.compareNames(a.getServerName(), b.getServerName()));
        } else if (serverSortPosition == 2) {
            java.util.Collections.sort(currentServers, (a, b) -> ServerSort.compareEndpoints(
                    a.getAddress(), a.getPort(), b.getAddress(), b.getPort()));
        }

        if (emptyServers != null) {
            emptyServers.setVisibility(currentServers.isEmpty() ? View.VISIBLE : View.GONE);
            emptyServers.setText(!searchQuery.trim().isEmpty() && (!hostedLoading || showingFavorites)
                    ? "Tidak ada server yang cocok.\nCoba nama tersimpan atau alamat lain."
                    : showingFavorites
                    ? "Belum ada server favorit.\nTambahkan server untuk mulai bermain."
                    : hostedLoading ? "Memuat daftar server..."
                    : "Daftar Hosted kosong atau tidak dapat dimuat.\nPeriksa koneksi internet dan coba lagi nanti.");
        }
        if (adapter == null) {
            adapter = new ServerAdapter(currentServers, this::showServerDetailDialog);
            rvServers.setAdapter(adapter);
        } else {
            adapter.notifyDataSetChanged();
        }
    }

    private void showServerDetailDialog(final SAMPServerInfo server) {
        if (getContext() == null) return;
        Dialog detailDialog = new Dialog(getContext());
        detailDialog.setContentView(R.layout.alertdialog_server);
        if (detailDialog.getWindow() != null) {
            detailDialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        TextView tvName = detailDialog.findViewById(R.id.server_hostname);
        TextView tvIp = detailDialog.findViewById(R.id.server_ip);
        TextView tvPort = detailDialog.findViewById(R.id.server_port);
        TextView tvOnline = detailDialog.findViewById(R.id.server_online);
        TextView tvMode = detailDialog.findViewById(R.id.server_mode);
        TextView tvLanguage = detailDialog.findViewById(R.id.server_language);
        EditText etUsername = detailDialog.findViewById(R.id.server_password);
        EditText etServerPassword = detailDialog.findViewById(R.id.server_join_password);
        Button btnConnect = detailDialog.findViewById(R.id.server_connect);
        TextView queryPing = detailDialog.findViewById(R.id.server_query_ping);
        TextView reportingNotice = detailDialog.findViewById(R.id.server_reporting_notice);
        if (LauncherEvents.isConfigured()) {
            boolean reportingEnabled = LauncherEvents.preferences(requireContext())
                    .getBoolean(LauncherEvents.ENABLED, LauncherEvents.DEFAULT_ENABLED);
            reportingNotice.setVisibility(View.VISIBLE);
            reportingNotice.setText(reportingEnabled
                    ? "Laporan masuk aktif: alamat dan port dikirim ke "
                        + (LauncherEvents.isDirectDiscord() ? "Discord" : "relay")
                        + "; layanan dapat melihat IP internet. Matikan di Pengaturan."
                    : "Laporan masuk dimatikan di Pengaturan.");
        }
        Button btnMeasurePing = detailDialog.findViewById(R.id.server_measure_ping);
        View ownerView = getView();
        btnMeasurePing.setOnClickListener(v -> {
            btnMeasurePing.setEnabled(false);
            queryPing.setText("Ping query: mengukur...");
            queryExecutor.execute(() -> {
                long measured = -1;
                try { measured = ServerPingQuery.measure(server.getAddress(), server.getPort()); }
                catch (Exception ignored) { }
                final long elapsed = measured;
                new Handler(Looper.getMainLooper()).post(() -> {
                    if (!isAdded() || getView() != ownerView || !detailDialog.isShowing()) return;
                    btnMeasurePing.setEnabled(true);
                    queryPing.setText(elapsed < 0 ? "Ping query: tidak tersedia"
                            : "Ping query: " + elapsed + " ms (bukan ping dalam game)");
                });
            });
        });
        Button btnPlayers = detailDialog.findViewById(R.id.server_players);
        btnPlayers.setOnClickListener(v -> showPlayerListDialog(server, detailDialog));
        Button btnRules = detailDialog.findViewById(R.id.server_rules);
        btnRules.setOnClickListener(v -> showServerRulesDialog(server, detailDialog));
        Button btnCopy = detailDialog.findViewById(R.id.server_copy_address);
        btnCopy.setOnClickListener(v -> {
            ClipboardManager clipboard = (ClipboardManager) requireContext().getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard == null) {
                Toast.makeText(getContext(), "Clipboard tidak tersedia.", Toast.LENGTH_SHORT).show();
                return;
            }
            clipboard.setPrimaryClip(ClipData.newPlainText("Alamat server", server.getAddress() + ":" + server.getPort()));
            Toast.makeText(getContext(), "Alamat server disalin.", Toast.LENGTH_SHORT).show();
        });
        Button btnAddFavorite = detailDialog.findViewById(R.id.server_add_favorite);
        if (server.getType() != SAMPServerInfo.Official.CUSTOM) {
            btnAddFavorite.setVisibility(View.VISIBLE);
            boolean alreadySaved = isCustomFavorite(server);
            btnAddFavorite.setEnabled(!alreadySaved);
            if (alreadySaved) btnAddFavorite.setText("Sudah di favorit");
            btnAddFavorite.setOnClickListener(v -> {
                if (!ServerInput.isValidHost(server.getAddress()) || !ServerInput.isValidPort(server.getPort())) {
                    Toast.makeText(getContext(), "Alamat server tidak valid.", Toast.LENGTH_SHORT).show();
                    return;
                }
                if (!isCustomFavorite(server)) {
                    SAMPServerInfo favorite = new SAMPServerInfo();
                    favorite.setAddress(server.getAddress());
                    favorite.setPort(server.getPort());
                    favorite.setServerName(server.getServerName());
                    favorite.setType(SAMPServerInfo.Official.CUSTOM);
                    ArrayList<SAMPServerInfo> favorites = loadCustomServers();
                    favorites.add(favorite);
                    saveCustomServers(favorites);
                    updateList();
                    Toast.makeText(getContext(), "Server disimpan ke favorit.", Toast.LENGTH_SHORT).show();
                }
                btnAddFavorite.setEnabled(false);
                btnAddFavorite.setText("Sudah di favorit");
            });
        }
        Button btnDelete = detailDialog.findViewById(R.id.save_favorites);
        ImageView btnClose = detailDialog.findViewById(R.id.server_close);

        tvName.setText(server.getServerName());
        tvIp.setText(server.getAddress());
        tvPort.setText("Port: " + server.getPort());
        tvOnline.setText(server.getCurrentPlayerCount() + " / " + server.getMaxPlayerCount());
        tvMode.setText("Mode: " + (server.getServerMode() != null ? server.getServerMode() : "Memuat..."));
        tvLanguage.setText("Bahasa: " + (server.getLanguage() != null ? server.getLanguage() : "Memuat..."));

        SharedPreferences sp = requireContext().getSharedPreferences("com.sampmobile.xyvern", Context.MODE_PRIVATE);
        String currentNick = sp.getString("nickname", "");
        etUsername.setText(currentNick);
        etUsername.setHint("Username");

        pingHandler = new Handler(Looper.getMainLooper());
        pingRunnable = new Runnable() {
            @Override
            public void run() {
                queryExecutor.execute(() -> {
                    try (SampQueryAPI query = new SampQueryAPI(server.getAddress(), server.getPort())) {
                        String[] info = query.mo7164b();
                        if (getActivity() != null) {
                            getActivity().runOnUiThread(() -> {
                                if (!isAdded() || !detailDialog.isShowing()) return;
                                if (info != null) {
                                    tvName.setText(info[3]);
                                    tvOnline.setText(info[1] + " / " + info[2]);
                                    tvMode.setText("Mode: " + info[4]);
                                    tvLanguage.setText("Bahasa: " + info[5]);
                                } else {
                                    tvOnline.setText("OFFLINE");
                                    tvMode.setText("Mode: -");
                                    tvLanguage.setText("Bahasa: -");
                                }
                            });
                        }
                    } catch (Exception ignored) {
                    }
                });
                if (pingHandler != null) {
                    pingHandler.postDelayed(this, 3000);
                }
            }
        };
        pingHandler.post(pingRunnable);

        detailDialog.setOnDismissListener(dialog -> {
            stopPingTask();
            if (playersDialog != null) playersDialog.dismiss();
            if (rulesDialog != null) rulesDialog.dismiss();
        });

        if (server.getType() == SAMPServerInfo.Official.CUSTOM) {
            btnDelete.setVisibility(View.VISIBLE);
            btnDelete.setText("Hapus favorit");
            btnDelete.setOnClickListener(v -> {
                stopPingTask();
                ArrayList<SAMPServerInfo> customServers = loadCustomServers();
                for (int i = 0; i < customServers.size(); i++) {
                    if (customServers.get(i).getAddress().equals(server.getAddress()) &&
                            customServers.get(i).getPort() == server.getPort()) {
                        customServers.remove(i);
                        break;
                    }
                }
                saveCustomServers(customServers);
                Toast.makeText(getContext(), "Server berhasil dihapus!", Toast.LENGTH_SHORT).show();
                detailDialog.dismiss();
                updateList();
            });
        } else {
            btnDelete.setVisibility(View.GONE);
        }

        btnClose.setOnClickListener(v -> {
            stopPingTask();
            detailDialog.dismiss();
        });

        btnConnect.setOnClickListener(v -> {
            String newNick = etUsername.getText().toString().trim();
            if (!ServerInput.isValidNickname(newNick)) {
                etUsername.setError("Nickname 3–24 karakter: huruf, angka, _ [ ] ( ) $ @ . =");
                return;
            }
            String password = etServerPassword.getText().toString();
            if (!ServerInput.isValidPassword(password)) {
                etServerPassword.setError("Password server maksimal 127 karakter ASCII tanpa spasi.");
                return;
            }
            if (!ServerInput.isValidHost(server.getAddress()) || !ServerInput.isValidPort(server.getPort())) {
                Toast.makeText(getContext(), "Alamat atau port server tidak valid.", Toast.LENGTH_LONG).show();
                return;
            }
            sp.edit().putString("nickname", newNick).apply();
            // Report a valid attempt even if data preparation or launch fails later.
            LauncherEvents.sendJoinAttempt(requireContext(), server.getAddress(), server.getPort());

            // Verifikasi data game sebelum meluncurkan client
            com.sampmobile.xyvern.launcher.util.GameDataVerifier.VerificationResult verify = 
                    com.sampmobile.xyvern.launcher.util.GameDataVerifier.verifyGameData(requireContext());
            if (!verify.isValid) {
                Toast.makeText(getContext(), 
                        "Data game belum lengkap! Silakan download data dari tab Home atau tambahkan file GTA SA manual.", 
                        Toast.LENGTH_LONG).show();
                stopPingTask();
                detailDialog.dismiss();
                if (getActivity() instanceof MainActivity) {
                    ((MainActivity) getActivity()).changeTab(0); // Pindah ke tab Home
                }
                return;
            }

            // The GTA engine reads these files before the game Activity can
            // initialize its asset manager. Preserve any existing user files.
            try {
                StartupAssets.prepare(requireContext().getExternalFilesDir(null),
                        name -> requireContext().getAssets().open(name));
            } catch (java.io.IOException | SecurityException e) {
                android.util.Log.e("PlayFragment", "Gagal menyiapkan font game", e);
                Toast.makeText(requireContext(), "Gagal menyiapkan font game: " + e.getMessage(), Toast.LENGTH_LONG).show();
                return;
            }

            stopPingTask();
            detailDialog.dismiss();
            Intent intent = new Intent(getActivity(), SAMP.class);
            intent.putExtra("ip", server.getAddress());
            intent.putExtra("port", server.getPort());
            intent.putExtra("nickname", newNick);
            intent.putExtra("server_password", password);
            startActivity(intent);
        });

        detailDialog.show();

        if (detailDialog.getWindow() != null && getContext() != null) {
            int screenWidth = getResources().getDisplayMetrics().widthPixels;
            int dialogWidth = (int) (screenWidth * 0.88f);
            detailDialog.getWindow().setLayout(dialogWidth, ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    private void showPlayerListDialog(SAMPServerInfo server, Dialog detailDialog) {
        if (!isAdded() || getView() == null || getContext() == null) return;
        View ownerView = getView();
        Dialog playersDialog = new Dialog(requireContext());
        this.playersDialog = playersDialog;
        playersDialog.setOnDismissListener(dialog -> {
            if (this.playersDialog == playersDialog) this.playersDialog = null;
        });
        playersDialog.setContentView(R.layout.alertdialog_players);
        if (playersDialog.getWindow() != null) {
            playersDialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }
        TextView title = playersDialog.findViewById(R.id.players_title);
        TextView status = playersDialog.findViewById(R.id.players_status);
        RecyclerView list = playersDialog.findViewById(R.id.players_list);
        list.setLayoutManager(new LinearLayoutManager(requireContext()));
        Button refresh = playersDialog.findViewById(R.id.players_refresh);
        Button close = playersDialog.findViewById(R.id.players_close);
        EditText search = playersDialog.findViewById(R.id.players_search);
        Spinner sort = playersDialog.findViewById(R.id.players_sort);
        String[] sortLabels = {"Urutan server", "Nama A–Z", "Skor tertinggi", "Ping terendah", "ID terkecil"};
        ArrayAdapter<String> sortAdapter = new ArrayAdapter<>(requireContext(), R.layout.item_players_sort, sortLabels);
        sortAdapter.setDropDownViewResource(R.layout.item_players_sort);
        sort.setAdapter(sortAdapter);
        title.setText("Daftar pemain");
        close.setOnClickListener(v -> playersDialog.dismiss());

        // Search and sorting operate only on the last response, without new network requests.
        class PlayerResults {
            java.util.List<ServerPlayerQuery.Player> all;
            boolean loading = true;
            void render() {
                if (loading) {
                    list.setAdapter(null);
                    status.setText("Memuat daftar pemain...");
                } else if (all == null) {
                    list.setAdapter(null);
                    status.setText("Daftar pemain tidak tersedia. Server mungkin offline atau menonaktifkan query pemain.");
                } else {
                    int selected = sort.getSelectedItemPosition();
                    PlayerListFilter.Order[] orders = PlayerListFilter.Order.values();
                    PlayerListFilter.Order order = selected >= 0 && selected < orders.length
                            ? orders[selected] : PlayerListFilter.Order.SERVER;
                    java.util.List<ServerPlayerQuery.Player> visible =
                            PlayerListFilter.apply(all, search.getText().toString(), order);
                    if (all.isEmpty()) status.setText("Tidak ada pemain dalam respons server.");
                    else if (visible.isEmpty()) status.setText("Tidak ada pemain yang cocok.");
                    else if (search.getText().toString().trim().isEmpty())
                        status.setText(all.size() + " pemain dalam respons server");
                    else status.setText(visible.size() + " dari " + all.size() + " pemain");
                    list.setAdapter(new PlayerListAdapter(visible));
                }
            }
        }
        PlayerResults results = new PlayerResults();
        search.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) { results.render(); }
            @Override public void afterTextChanged(Editable text) { }
        });
        sort.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) { results.render(); }
            @Override public void onNothingSelected(AdapterView<?> parent) { results.render(); }
        });
        Runnable loadPlayers = () -> {
            refresh.setEnabled(false);
            results.loading = true;
            results.render();
            queryExecutor.execute(() -> {
                java.util.List<ServerPlayerQuery.Player> players = null;
                try {
                    players = ServerPlayerQuery.query(server.getAddress(), server.getPort());
                } catch (Exception error) {
                    android.util.Log.w("PlayFragment", "Player query unavailable", error);
                }
                final java.util.List<ServerPlayerQuery.Player> result = players;
                new Handler(Looper.getMainLooper()).post(() -> {
                    if (!isAdded() || getView() != ownerView || !detailDialog.isShowing() || !playersDialog.isShowing()) return;
                    refresh.setEnabled(true);
                    results.all = result;
                    results.loading = false;
                    results.render();
                });
            });
        };
        refresh.setOnClickListener(v -> loadPlayers.run());
        playersDialog.show();
        if (playersDialog.getWindow() != null) {
            android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
            playersDialog.getWindow().setLayout((int) (metrics.widthPixels * 0.88f), (int) (metrics.heightPixels * 0.75f));
        }
        loadPlayers.run();
    }

    private void showServerRulesDialog(SAMPServerInfo server, Dialog detailDialog) {
        if (!isAdded() || getView() == null || getContext() == null) return;
        View ownerView = getView();
        Dialog dialog = new Dialog(requireContext());
        rulesDialog = dialog;
        dialog.setOnDismissListener(ignored -> { if (rulesDialog == dialog) rulesDialog = null; });
        dialog.setContentView(R.layout.alertdialog_rules);
        if (dialog.getWindow() != null) dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        TextView status = dialog.findViewById(R.id.rules_status);
        RecyclerView list = dialog.findViewById(R.id.rules_list);
        list.setLayoutManager(new LinearLayoutManager(requireContext()));
        Button refresh = dialog.findViewById(R.id.rules_refresh);
        Button close = dialog.findViewById(R.id.rules_close);
        close.setOnClickListener(v -> dialog.dismiss());
        Runnable load = () -> {
            refresh.setEnabled(false);
            list.setAdapter(null);
            status.setText("Memuat info server...");
            queryExecutor.execute(() -> {
                java.util.List<ServerRulesQuery.Rule> rules = null;
                try { rules = ServerRulesQuery.query(server.getAddress(), server.getPort()); }
                catch (Exception error) { android.util.Log.w("PlayFragment", "Rules query unavailable", error); }
                final java.util.List<ServerRulesQuery.Rule> result = rules;
                new Handler(Looper.getMainLooper()).post(() -> {
                    if (!isAdded() || getView() != ownerView || !detailDialog.isShowing() || !dialog.isShowing()) return;
                    refresh.setEnabled(true);
                    if (result == null) {
                        status.setText("Info tidak tersedia. Server mungkin offline atau menonaktifkan query info.");
                        return;
                    }
                    status.setText(result.isEmpty() ? "Server tidak mengirim info tambahan."
                            : result.size() + " info dari server");
                    list.setAdapter(new ServerRulesAdapter(result));
                });
            });
        };
        refresh.setOnClickListener(v -> load.run());
        dialog.show();
        if (dialog.getWindow() != null) {
            android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
            dialog.getWindow().setLayout((int) (metrics.widthPixels * 0.88f), (int) (metrics.heightPixels * 0.75f));
        }
        load.run();
    }

    private boolean isCustomFavorite(SAMPServerInfo server) {
        String address = server.getAddress();
        if (address == null) return false;
        for (SAMPServerInfo saved : loadCustomServers()) {
            if (saved.getPort() == server.getPort() && saved.getAddress() != null
                    && saved.getAddress().trim().equalsIgnoreCase(address.trim())) return true;
        }
        return false;
    }

    private void stopPingTask() {
        if (pingHandler != null && pingRunnable != null) {
            pingHandler.removeCallbacks(pingRunnable);
            pingRunnable = null;
        }
    }

    private void saveCustomServers(ArrayList<SAMPServerInfo> servers) {
        if (getContext() == null) return;
        SharedPreferences sp = getContext().getSharedPreferences("custom_servers_pref", Context.MODE_PRIVATE);
        JSONArray jsonArray = new JSONArray();
        for (SAMPServerInfo server : servers) {
            JSONObject obj = new JSONObject();
            try {
                obj.put("name", server.getServerName());
                obj.put("ip", server.getAddress());
                obj.put("port", server.getPort());
                jsonArray.put(obj);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        sp.edit().putString("custom_servers_json", jsonArray.toString()).apply();
    }

    private ArrayList<SAMPServerInfo> loadCustomServers() {
        ArrayList<SAMPServerInfo> servers = new ArrayList<>();
        if (getContext() == null) return servers;
        SharedPreferences sp = getContext().getSharedPreferences("custom_servers_pref", Context.MODE_PRIVATE);
        String json = sp.getString("custom_servers_json", "[]");
        try {
            JSONArray jsonArray = new JSONArray(json);
            for (int i = 0; i < jsonArray.length(); i++) {
                JSONObject obj = jsonArray.getJSONObject(i);
                SAMPServerInfo server = new SAMPServerInfo();
                server.setServerName(obj.optString("name", "Custom Server"));
                server.setAddress(obj.optString("ip", ""));
                server.setPort(obj.optInt("port", 7777));
                server.setType(SAMPServerInfo.Official.CUSTOM);
                server.setCurrentPlayerCount(0);
                server.setMaxPlayerCount(0);
                servers.add(server);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return servers;
    }

    private boolean serverAlreadyExists(String ip, int port) {
        String normalizedIp = ip.trim();
        for (SAMPServerInfo server : loadCustomServers()) {
            if (server.getPort() == port && server.getAddress().trim().equalsIgnoreCase(normalizedIp)) {
                return true;
            }
        }
        for (SAMPServerInfo server : hostedServers) {
            if (server.getPort() == port && server.getAddress().trim().equalsIgnoreCase(normalizedIp)) {
                return true;
            }
        }
        return false;
    }

    private void showAddServerDialog() {
        if (getContext() == null) return;
        Dialog addDialog = new Dialog(getContext());
        addDialog.setContentView(R.layout.alertdialog_addserver);
        if (addDialog.getWindow() != null) {
            addDialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        EditText etHost = addDialog.findViewById(R.id.server_host);
        Button btnAdd = addDialog.findViewById(R.id.server_add);
        Button btnCancel = addDialog.findViewById(R.id.server_close);

        btnAdd.setOnClickListener(v -> {
            String input = etHost.getText().toString().trim();
            if (input.isEmpty()) {
                Toast.makeText(getContext(), "Format harus IP:Port", Toast.LENGTH_SHORT).show();
                return;
            }

            String ip;
            int port = 7777;
            if (input.contains(":")) {
                String[] parts = input.split(":", -1);
                if (parts.length != 2 || parts[0].trim().isEmpty()) {
                    Toast.makeText(getContext(), "Format IP:Port tidak valid!", Toast.LENGTH_SHORT).show();
                    return;
                }
                ip = parts[0];
                try {
                    port = Integer.parseInt(parts[1]);
                } catch (NumberFormatException e) {
                    Toast.makeText(getContext(), "Port tidak valid!", Toast.LENGTH_SHORT).show();
                    return;
                }
            } else {
                ip = input;
            }

            ip = ip.trim();
            if (!ServerInput.isValidHost(ip)) {
                Toast.makeText(getContext(), "Alamat server tidak valid!", Toast.LENGTH_SHORT).show();
                return;
            }
            if (!ServerInput.isValidPort(port)) {
                Toast.makeText(getContext(), "Port harus di antara 1 dan 65535!", Toast.LENGTH_SHORT).show();
                return;
            }
            if (serverAlreadyExists(ip, port)) {
                Toast.makeText(getContext(), "Server tersebut sudah ada!", Toast.LENGTH_SHORT).show();
                return;
            }

            final String finalIp = ip;
            final int finalPort = port;

            SAMPServerInfo newServer = new SAMPServerInfo();
            newServer.setServerName(input);
            newServer.setAddress(ip);
            newServer.setPort(port);
            newServer.setType(SAMPServerInfo.Official.CUSTOM);

            queryExecutor.execute(() -> {
                try (SampQueryAPI query = new SampQueryAPI(finalIp, finalPort)) {
                    String[] info = query.mo7164b();
                    if (info != null && info[3] != null && !info[3].isEmpty()) {
                        newServer.setServerName(info[3]);
                        queryCache.put(finalIp + ":" + finalPort, info);
                    }
                } catch (Exception ignored) {
                }

                if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        if (!isAdded() || !addDialog.isShowing() || getView() == null) return;
                        if (serverAlreadyExists(finalIp, finalPort)) {
                            Toast.makeText(getContext(), "Server tersebut sudah ada!", Toast.LENGTH_SHORT).show();
                            addDialog.dismiss();
                            return;
                        }
                        ArrayList<SAMPServerInfo> customServers = loadCustomServers();
                        customServers.add(newServer);
                        saveCustomServers(customServers);
                        Toast.makeText(getContext(), "Server berhasil ditambahkan!", Toast.LENGTH_SHORT).show();
                        addDialog.dismiss();
                        updateList();
                    });
                }
            });
        });

        btnCancel.setOnClickListener(v -> addDialog.dismiss());
        addDialog.show();

        if (addDialog.getWindow() != null && getContext() != null) {
            int screenWidth = getResources().getDisplayMetrics().widthPixels;
            int dialogWidth = (int) (screenWidth * 0.88f);
            addDialog.getWindow().setLayout(dialogWidth, ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    private static class ServerRulesAdapter extends RecyclerView.Adapter<ServerRulesAdapter.RuleHolder> {
        private final java.util.List<ServerRulesQuery.Rule> rules;
        ServerRulesAdapter(java.util.List<ServerRulesQuery.Rule> rules) { this.rules = rules; }
        @NonNull @Override public RuleHolder onCreateViewHolder(@NonNull ViewGroup parent, int type) {
            return new RuleHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_server_player, parent, false));
        }
        @Override public void onBindViewHolder(@NonNull RuleHolder holder, int position) {
            ServerRulesQuery.Rule rule = rules.get(position);
            holder.name.setText(rule.name);
            holder.value.setText(rule.value);
        }
        @Override public int getItemCount() { return rules.size(); }
        static class RuleHolder extends RecyclerView.ViewHolder {
            final TextView name, value;
            RuleHolder(View view) {
                super(view);
                name = view.findViewById(R.id.player_name);
                value = view.findViewById(R.id.player_stats);
            }
        }
    }

    private static class PlayerListAdapter extends RecyclerView.Adapter<PlayerListAdapter.PlayerHolder> {
        private final java.util.List<ServerPlayerQuery.Player> players;

        PlayerListAdapter(java.util.List<ServerPlayerQuery.Player> players) { this.players = players; }

        @NonNull @Override public PlayerHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            return new PlayerHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_server_player, parent, false));
        }

        @Override public void onBindViewHolder(@NonNull PlayerHolder holder, int position) {
            ServerPlayerQuery.Player player = players.get(position);
            holder.name.setText(player.name);
            holder.stats.setText("ID " + player.id + "   •   Skor " + player.score + "   •   Ping " + player.ping + " ms");
        }

        @Override public int getItemCount() { return players.size(); }

        static class PlayerHolder extends RecyclerView.ViewHolder {
            final TextView name, stats;
            PlayerHolder(View view) {
                super(view);
                name = view.findViewById(R.id.player_name);
                stats = view.findViewById(R.id.player_stats);
            }
        }
    }

    private class ServerAdapter extends RecyclerView.Adapter<ServerAdapter.ViewHolder> {
        private final ArrayList<SAMPServerInfo> serverList;
        private final OnServerClickListener listener;

        public ServerAdapter(ArrayList<SAMPServerInfo> serverList, OnServerClickListener listener) {
            this.serverList = serverList;
            this.listener = listener;
        }

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View v = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_server, parent, false);
            return new ViewHolder(v);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            SAMPServerInfo server = serverList.get(position);
            String serverKey = server.getAddress() + ":" + server.getPort();
            holder.boundKey = serverKey;

            // Default display
            holder.tvName.setText(server.getServerName());
            holder.tvAddress.setText("Address: " + serverKey);
            holder.tvPlayers.setText("Players: " + server.getCurrentPlayerCount() + " / " + server.getMaxPlayerCount());
            holder.tvMode.setText("Gamemode: " + (server.getServerMode() != null ? server.getServerMode() : "Loading..."));
            holder.ivLock.setVisibility(server.getHasPassword() ? View.VISIBLE : View.GONE);

            // Check cache first
            String[] cached = queryCache.get(serverKey);
            if (cached != null) {
                applyServerInfo(holder, cached);
            } else {
                // Ignore results from before a manual refresh or a recycled row.
                final int generation = queryGeneration;
                queryExecutor.execute(() -> {
                    try (SampQueryAPI query = new SampQueryAPI(server.getAddress(), server.getPort())) {
                        String[] info = query.mo7164b();
                        synchronized (queryCache) {
                            if (info != null && generation == queryGeneration) queryCache.put(serverKey, info);
                        }
                        if (getActivity() != null) {
                            getActivity().runOnUiThread(() -> {
                                if (generation == queryGeneration && serverKey.equals(holder.boundKey)) {
                                    if (info != null) {
                                        applyServerInfo(holder, info);
                                    } else {
                                        if (holder.tvMode.getText().toString().contains("Loading")) {
                                            holder.tvMode.setText("Gamemode: Offline");
                                        }
                                    }
                                }
                            });
                        }
                    } catch (Exception ignored) {
                    }
                });
            }

            holder.itemView.setOnClickListener(v -> listener.onServerClick(server));
        }

        private void applyServerInfo(ViewHolder holder, String[] info) {
            holder.tvName.setText(info[3]);
            holder.tvPlayers.setText("Players: " + info[1] + " / " + info[2]);
            holder.tvMode.setText("Gamemode: " + info[4]);
            holder.ivLock.setVisibility("1".equals(info[0]) ? View.VISIBLE : View.GONE);
        }

        @Override
        public int getItemCount() {
            return serverList.size();
        }

        public class ViewHolder extends RecyclerView.ViewHolder {
            String boundKey;
            TextView tvName, tvAddress, tvPlayers, tvMode;
            ImageView ivLock;

            public ViewHolder(@NonNull View itemView) {
                super(itemView);
                tvName = itemView.findViewById(R.id.tv_server_name);
                tvAddress = itemView.findViewById(R.id.tv_server_address);
                tvPlayers = itemView.findViewById(R.id.tv_server_status);
                tvMode = itemView.findViewById(R.id.tv_server_mode);
                ivLock = itemView.findViewById(R.id.iv_lock);
            }
        }
    }
}
