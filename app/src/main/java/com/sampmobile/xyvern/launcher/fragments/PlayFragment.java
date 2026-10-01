package com.sampmobile.xyvern.launcher.fragments;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;
import com.sampmobile.xyvern.game.SAMP;
import com.sampmobile.xyvern.launcher.MainActivity;
import com.sampmobile.xyvern.launcher.util.SAMPServerInfo;
import com.sampmobile.xyvern.launcher.util.SampQueryAPI;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

@Obfuscate
public class PlayFragment extends Fragment {

    public interface OnServerClickListener {
        void onServerClick(SAMPServerInfo server);
    }

    private static final String HOSTED_URL = "https://kethrnensm.github.io/web/hosted.json";
    private RecyclerView rvServers;
    private ServerAdapter adapter;
    private ArrayList<SAMPServerInfo> hostedServers = new ArrayList<>();
    private ArrayList<SAMPServerInfo> currentServers = new ArrayList<>();
    private TextView tabFavorites, tabHosted;
    private boolean showingFavorites = true;

    private Handler pingHandler;
    private Runnable pingRunnable;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_play, container, false);

        rvServers = view.findViewById(R.id.rv_servers);
        rvServers.setLayoutManager(new LinearLayoutManager(getContext()));

        tabFavorites = view.findViewById(R.id.tab_favorites);
        tabHosted = view.findViewById(R.id.tab_hosted);

        tabFavorites.setOnClickListener(v -> switchTab(true));
        tabHosted.setOnClickListener(v -> switchTab(false));

        Button btnAdd = view.findViewById(R.id.btn_add_server);
        btnAdd.setOnClickListener(v -> showAddServerDialog());

        loadData();

        return view;
    }

    private void switchTab(boolean favorites) {
        showingFavorites = favorites;
        tabFavorites.setBackgroundResource(favorites ? R.drawable.bg_nav_active : 0);
        tabHosted.setBackgroundResource(!favorites ? R.drawable.bg_nav_active : 0);
        updateList();
    }

    private void loadData() {
        new Thread(() -> {
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

                    hostedServers.clear();
                    for (int i = 0; i < jsonArray.length(); i++) {
                        JSONObject obj = jsonArray.getJSONObject(i);
                        SAMPServerInfo server = new SAMPServerInfo();
                        server.setServerName(obj.getString("name"));
                        server.setAddress(obj.getString("ip"));
                        server.setPort(obj.getInt("port"));
                        server.setCurrentPlayerCount(obj.getInt("online"));
                        server.setMaxPlayerCount(obj.getInt("maxplayers"));
                        server.setType(SAMPServerInfo.Official.HOSTED);
                        hostedServers.add(server);
                    }
                }
                connection.disconnect();
            } catch (Exception e) {
                e.printStackTrace();
            }

            if (getActivity() != null) {
                getActivity().runOnUiThread(this::updateList);
            }
        }).start();
    }

    private void updateList() {
        currentServers.clear();
        if (showingFavorites) {
            currentServers.addAll(loadCustomServers());
        } else {
            currentServers.addAll(hostedServers);
        }

        if (adapter == null) {
            adapter = new ServerAdapter(currentServers, this::showServerDetailDialog);
            rvServers.setAdapter(adapter);
        } else {
            adapter.notifyDataSetChanged();
        }
    }

    private void showServerDetailDialog(final SAMPServerInfo server) {
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
        Button btnConnect = detailDialog.findViewById(R.id.server_connect);
        Button btnDelete = detailDialog.findViewById(R.id.save_favorites);
        ImageView btnClose = detailDialog.findViewById(R.id.server_close);

        tvName.setText(server.getServerName());
        tvIp.setText(server.getAddress());
        tvPort.setText(String.valueOf(server.getPort()));
        tvOnline.setText(server.getCurrentPlayerCount() + " / " + server.getMaxPlayerCount());
        tvMode.setText(server.getServerMode() != null ? server.getServerMode() : "Loading...");
        tvLanguage.setText(server.getLanguage() != null ? server.getLanguage() : "Loading...");

        SharedPreferences sp = getContext().getSharedPreferences("com.sampmobile.xyvern", Context.MODE_PRIVATE);
        String currentNick = sp.getString("nickname", "");
        etUsername.setText(currentNick);
        etUsername.setHint("Username");

        pingHandler = new Handler(Looper.getMainLooper());
        pingRunnable = new Runnable() {
            @Override
            public void run() {
                new Thread(() -> {
                    SampQueryAPI query = new SampQueryAPI(server.getAddress(), server.getPort());
                    String[] info = query.mo7164b();
                    if (query.f7277a != null) {
                        query.f7277a.close();
                    }

                    final String[] finalInfo = info;

                    if (getActivity() != null) {
                        getActivity().runOnUiThread(() -> {
                            if (finalInfo != null) {
                                tvName.setText(finalInfo[3]);
                                tvOnline.setText(finalInfo[1] + " / " + finalInfo[2]);
                                tvMode.setText(finalInfo[4]);
                                tvLanguage.setText(finalInfo[5]);
                            } else {
                                tvOnline.setText("OFFLINE");
                                tvMode.setText("-");
                                tvLanguage.setText("-");
                            }
                        });
                    }
                }).start();
                if (pingHandler != null) {
                    pingHandler.postDelayed(this, 3000);
                }
            }
        };
        pingHandler.post(pingRunnable);

        detailDialog.setOnDismissListener(dialog -> stopPingTask());

        if (server.getType() == SAMPServerInfo.Official.CUSTOM) {
            btnDelete.setVisibility(View.VISIBLE);
            btnDelete.setText("Delete");
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
                Toast.makeText(getContext(), "Đã xóa server!", Toast.LENGTH_SHORT).show();
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
            if (newNick.isEmpty()) {
                Toast.makeText(getContext(), "Vui lòng nhập Username", Toast.LENGTH_SHORT).show();
                return;
            }
            sp.edit().putString("nickname", newNick).apply();

            stopPingTask();
            detailDialog.dismiss();
            Intent intent = new Intent(getActivity(), SAMP.class);
            intent.putExtra("ip", server.getAddress());
            intent.putExtra("port", server.getPort());
            startActivity(intent);
        });

        detailDialog.show();

        if (detailDialog.getWindow() != null && getContext() != null) {
            int screenWidth = getResources().getDisplayMetrics().widthPixels;
            int dialogWidth = (int) (screenWidth * 0.88f);
            detailDialog.getWindow().setLayout(dialogWidth, ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    private void stopPingTask() {
        if (pingHandler != null && pingRunnable != null) {
            pingHandler.removeCallbacks(pingRunnable);
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
                server.setServerName(obj.getString("name"));
                server.setAddress(obj.getString("ip"));
                server.setPort(obj.getInt("port"));
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

    private void showAddServerDialog() {
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
                Toast.makeText(getContext(), "Vui lòng nhập IP:Port", Toast.LENGTH_SHORT).show();
                return;
            }

            String ip;
            int port = 7777;
            if (input.contains(":")) {
                String[] parts = input.split(":");
                ip = parts[0];
                try {
                    port = Integer.parseInt(parts[1]);
                } catch (NumberFormatException e) {
                    Toast.makeText(getContext(), "Port không hợp lệ", Toast.LENGTH_SHORT).show();
                    return;
                }
            } else {
                ip = input;
            }

            final String finalIp = ip;
            final int finalPort = port;

            SAMPServerInfo newServer = new SAMPServerInfo();
            newServer.setServerName(input);
            newServer.setAddress(ip);
            newServer.setPort(port);
            newServer.setType(SAMPServerInfo.Official.CUSTOM);

            new Thread(() -> {
                SampQueryAPI query = new SampQueryAPI(finalIp, finalPort);
                String[] info = query.mo7164b();
                if (query.f7277a != null) query.f7277a.close();

                if (info != null && info[3] != null && !info[3].isEmpty()) {
                    newServer.setServerName(info[3]);
                }

                if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        ArrayList<SAMPServerInfo> customServers = loadCustomServers();
                        customServers.add(newServer);
                        saveCustomServers(customServers);
                        Toast.makeText(getContext(), "Đã thêm server!", Toast.LENGTH_SHORT).show();
                        addDialog.dismiss();
                        updateList();
                    });
                }
            }).start();
        });

        btnCancel.setOnClickListener(v -> addDialog.dismiss());

        addDialog.show();

        if (addDialog.getWindow() != null && getContext() != null) {
            int screenWidth = getResources().getDisplayMetrics().widthPixels;
            int dialogWidth = (int) (screenWidth * 0.88f);
            addDialog.getWindow().setLayout(dialogWidth, ViewGroup.LayoutParams.WRAP_CONTENT);
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
            
            // Set data awal
            holder.tvName.setText(server.getServerName() + " |");
            holder.tvAddress.setText("Address: " + server.getAddress() + ":" + server.getPort());
            holder.tvPlayers.setText("Players: " + server.getCurrentPlayerCount() + " / " + server.getMaxPlayerCount());
            holder.tvMode.setText("Gamemode: " + (server.getServerMode() != null ? server.getServerMode() : "Loading..."));
            holder.ivLock.setVisibility(server.getHasPassword() ? View.VISIBLE : View.GONE);

            // Jalankan query background untuk update info aslinya (Hostname, Player, Mode)
            new Thread(() -> {
                SampQueryAPI query = new SampQueryAPI(server.getAddress(), server.getPort());
                String[] info = query.mo7164b(); // Ambil info lengkap
                if (query.f7277a != null) query.f7277a.close();

                if (info != null && getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        holder.tvName.setText(info[3] + " |"); // Hostname asli
                        holder.tvPlayers.setText("Players: " + info[1] + " / " + info[2]);
                        holder.tvMode.setText("Gamemode: " + info[4]);
                        
                        // Update status gembok (info[0] adalah flag password: "1" jika ada)
                        boolean hasPass = info[0].equals("1");
                        holder.ivLock.setVisibility(hasPass ? View.VISIBLE : View.GONE);
                    });
                } else if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        if (holder.tvMode.getText().toString().equals("Gamemode: Loading...")) {
                            holder.tvMode.setText("Gamemode: Offline");
                        }
                    });
                }
            }).start();

            holder.itemView.setOnClickListener(v -> listener.onServerClick(server));
        }

        @Override
        public int getItemCount() {
            return serverList.size();
        }

        public class ViewHolder extends RecyclerView.ViewHolder {
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