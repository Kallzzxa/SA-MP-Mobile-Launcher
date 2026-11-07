package com.sampmobile.xyvern.launcher.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.sampmobile.xyvern.BuildConfig;

import org.json.JSONObject;
import org.json.JSONArray;

import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/** Best-effort events; users can disable reporting. Direct mode exposes the webhook in the APK. */
public final class LauncherEvents {
    public static final String PREFS = "launcher_event_reporting";
    public static final String ENABLED = "join_attempt_enabled";
    public static final boolean DEFAULT_ENABLED = true;
    private static final ThreadPoolExecutor EXECUTOR = new ThreadPoolExecutor(1, 1, 10, TimeUnit.SECONDS,
            new ArrayBlockingQueue<>(8), new ThreadPoolExecutor.DiscardPolicy());

    private LauncherEvents() {}

    public static boolean isDirectDiscord() {
        return DiscordWebhook.isValid(BuildConfig.DISCORD_WEBHOOK_URL);
    }

    public static boolean isConfigured() {
        if (isDirectDiscord()) return true;
        try {
            URL url = new URL(BuildConfig.EVENT_RELAY_URL);
            String host = url.getHost().toLowerCase(java.util.Locale.ROOT);
            return "https".equalsIgnoreCase(url.getProtocol()) && !host.isEmpty()
                    && url.getUserInfo() == null && url.getRef() == null && url.getQuery() == null
                    && !host.equals("discord.com") && !host.endsWith(".discord.com")
                    && !host.equals("discordapp.com") && !host.endsWith(".discordapp.com");
        } catch (Exception ignored) {
            return false;
        }
    }

    public static SharedPreferences preferences(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    public static void sendJoinAttempt(Context context, String host, int port) {
        if (!isConfigured()) {
            Log.w("LauncherEvents", "Reporting is not configured in this APK");
            return;
        }
        if (!preferences(context).getBoolean(ENABLED, DEFAULT_ENABLED)) {
            Log.i("LauncherEvents", "Reporting is disabled in settings");
            return;
        }
        if (!ServerInput.isValidHost(host) || !ServerInput.isValidPort(port)) return;
        Context appContext = context.getApplicationContext();
        EXECUTOR.execute(() -> {
            if (preferences(appContext).getBoolean(ENABLED, DEFAULT_ENABLED)) post(host, port);
        });
    }

    private static void post(String host, int port) {
        HttpURLConnection connection = null;
        try {
            boolean direct = isDirectDiscord();
            JSONObject body;
            String destination;
            if (direct) {
                body = new JSONObject()
                        .put("content", "HopePride: percobaan masuk ke " + host + ":" + port + " (belum tentu berhasil).")
                        .put("allowed_mentions", new JSONObject().put("parse", new JSONArray()));
                destination = BuildConfig.DISCORD_WEBHOOK_URL;
            } else {
                body = new JSONObject().put("event", "join_attempt").put("host", host).put("port", port);
                destination = BuildConfig.EVENT_RELAY_URL;
            }
            byte[] payload = body.toString().getBytes(StandardCharsets.UTF_8);
            connection = (HttpURLConnection) new URL(destination).openConnection();
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(8000);
            connection.setReadTimeout(8000);
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            connection.setDoOutput(true);
            connection.setFixedLengthStreamingMode(payload.length);
            try (java.io.OutputStream output = connection.getOutputStream()) { output.write(payload); }
            int status = connection.getResponseCode();
            if (status >= 200 && status < 300) {
                Log.i("LauncherEvents", "Event endpoint accepted the report");
            } else {
                // Status only: never log the webhook URL, token, response, or player data.
                Log.w("LauncherEvents", "Event endpoint rejected the report (HTTP " + status + ")");
            }
        } catch (java.net.SocketTimeoutException ignored) {
            Log.w("LauncherEvents", "Event request timed out");
        } catch (Exception ignored) {
            // Reporting must never block or prevent a player from launching the game.
            // Do not log exception text; it may contain the webhook URL.
            Log.w("LauncherEvents", "Event could not be sent (network or TLS error)");
        } finally {
            if (connection != null) connection.disconnect();
        }
    }
}
