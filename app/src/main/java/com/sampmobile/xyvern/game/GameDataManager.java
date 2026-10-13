package com.sampmobile.xyvern.game;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Remote maintenance gate and game-data installer. */
public final class GameDataManager {
    private static final String APP_CONFIG = "https://raw.githubusercontent.com/Kallzzxa/hehehaha/refs/heads/main/app.json";
    private static final String PREFS = "samp_game_data";
    private static final String KEY_COMPLETE = "complete_";
    private static final int BLUE = Color.rgb(78, 126, 255);
    private static final int BG = Color.rgb(18, 22, 34);
    private static final int CARD = Color.rgb(29, 35, 52);
    private static final int WHITE = Color.rgb(245, 247, 255);
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final ExecutorService IO = Executors.newSingleThreadExecutor();

    public interface ReadyCallback { void onReady(); }
    private static class DataFile {
        String path, url; long size;
        DataFile(JSONObject o) throws Exception {
            path = o.getString("path"); url = o.getString("url");
            try { size = Long.parseLong(o.optString("size", "0")); } catch (Exception ignored) { size = 0; }
            File rel = new File(path);
            if (rel.isAbsolute() || path.contains("..") || path.trim().isEmpty()) throw new Exception("Invalid data path");
        }
    }

    private GameDataManager() {}

    public static void checkAndPrepare(Activity activity, ReadyCallback callback) {
        showLoading(activity, "Checking launcher status", "Connecting to configuration server...");
        IO.execute(() -> {
            try {
                JSONObject config = getJson(APP_CONFIG);
                boolean status = config.optBoolean("status", false)
                        && config.optBoolean("app_status", true)
                        && config.optBoolean("app_server_status", true);
                if (!status) {
                    MAIN.post(() -> showMaintenance(activity));
                    return;
                }
                String liteUrl = config.optString("lite_list_url", config.optString("lite_list", ""));
                String fullUrl = config.optString("full_list_url", config.optString("full_list", ""));
                String sampUrl = config.optString("samp_list_url", config.optString("samp_list", ""));
                String newUrl = config.optString("new_list_url", config.optString("new_list", ""));
                if (liteUrl.isEmpty() || fullUrl.isEmpty()) throw new Exception("Data manifest URL is missing in app.json");
                JSONArray liteBase = getJson(liteUrl).optJSONArray("files");
                JSONArray fullBase = getJson(fullUrl).optJSONArray("files");
                if (liteBase == null || fullBase == null || liteBase.length() == 0 || fullBase.length() == 0)
                    throw new Exception("Lite/Full manifest is empty or invalid");
                JSONArray samp = sampUrl.isEmpty() ? new JSONArray() : getJson(sampUrl).optJSONArray("files");
                JSONArray extra = newUrl.isEmpty() ? new JSONArray() : getJson(newUrl).optJSONArray("files");
                if (samp == null) samp = new JSONArray();
                if (extra == null) extra = new JSONArray();
                JSONArray lite = mergeFiles(liteBase, samp, extra);
                JSONArray full = mergeFiles(fullBase, samp, extra);
                String installed = activity.getSharedPreferences(PREFS, Activity.MODE_PRIVATE).getString("installed_type", "");
                boolean liteReady = isComplete(activity, "lite", lite);
                boolean fullReady = isComplete(activity, "full", full);
                if (("full".equals(installed) && fullReady) || ("lite".equals(installed) && liteReady) || fullReady || liteReady) {
                    MAIN.post(() -> { dismissLoading(); if (!activity.isFinishing()) callback.onReady(); });
                    return;
                }
                MAIN.post(() -> showChoice(activity, lite, full, callback));
            } catch (Exception e) {
                android.util.Log.e("GameDataManager", "Config check failed", e);
                MAIN.post(() -> showNetworkError(activity, e.getMessage(), callback));
            }
        });
    }

    // Merge package-specific files with common SAMP/New files, deduplicating by path.
    // The selected Lite/Full manifest wins when a path appears more than once.
    private static JSONArray mergeFiles(JSONArray base, JSONArray samp, JSONArray extra) throws Exception {
        java.util.LinkedHashMap<String, JSONObject> byPath = new java.util.LinkedHashMap<>();
        JSONArray[] sources = new JSONArray[]{base, samp, extra};
        for (JSONArray source : sources) {
            for (int i = 0; i < source.length(); i++) {
                JSONObject item = source.getJSONObject(i);
                String path = item.optString("path", "");
                if (!path.isEmpty() && !byPath.containsKey(path)) byPath.put(path, item);
            }
        }
        JSONArray merged = new JSONArray();
        for (JSONObject item : byPath.values()) merged.put(item);
        if (merged.length() == 0) throw new Exception("Combined data manifest is empty");
        return merged;
    }

    private static volatile AlertDialog loadingDialog;
    private static void showLoading(Activity a, String title, String message) {
        if (a.isFinishing()) return;
        LinearLayout root = panel(a); root.setPadding(dp(a,24),dp(a,22),dp(a,24),dp(a,22));
        TextView h = text(a,title,21,true); TextView m = text(a,message,14,false); m.setTextColor(Color.LTGRAY);
        ProgressBar p = new ProgressBar(a); p.setIndeterminate(true);
        root.addView(h); addGap(a,root,8); root.addView(m); addGap(a,root,18); root.addView(p);
        AlertDialog d = new AlertDialog.Builder(a).setView(root).setCancelable(false).create();
        d.setOnShowListener(v -> styleWindow(a,d)); loadingDialog=d; d.show(); styleWindow(a,d);
    }
    private static void dismissLoading() { try { if (loadingDialog != null && loadingDialog.isShowing()) loadingDialog.dismiss(); } catch(Exception ignored) {} loadingDialog=null; }

    private static void showMaintenance(Activity a) {
        if (a.isFinishing()) return; dismissLoading();
        LinearLayout root = panel(a); root.setPadding(dp(a,24),dp(a,24),dp(a,24),dp(a,22));
        TextView icon=text(a,"⚙",32,true); icon.setTextColor(BLUE); icon.setGravity(Gravity.CENTER);
        TextView title=text(a,"M-SAMP Maintenance",22,true); title.setGravity(Gravity.CENTER);
        TextView msg=text(a,"The Samp client is maintenance for update",14,false); msg.setTextColor(Color.LTGRAY); msg.setGravity(Gravity.CENTER);
        Button exit=button(a,"EXIT"); exit.setOnClickListener(v -> { a.finishAffinity(); android.os.Process.killProcess(android.os.Process.myPid()); });
        root.addView(icon); addGap(a,root,8); root.addView(title); addGap(a,root,10); root.addView(msg); addGap(a,root,22); root.addView(exit);
        AlertDialog d=new AlertDialog.Builder(a).setView(root).setCancelable(false).create(); d.show(); styleWindow(a,d);
    }

    private static void showChoice(Activity a, JSONArray lite, JSONArray full, ReadyCallback cb) {
        if (a.isFinishing()) return; dismissLoading();
        LinearLayout root=panel(a); root.setPadding(dp(a,22),dp(a,22),dp(a,22),dp(a,20));
        TextView title=text(a,"Download Game Data",22,true); TextView sub=text(a,"Choose data package before entering the game.",14,false); sub.setTextColor(Color.LTGRAY);
        root.addView(title); addGap(a,root,6); root.addView(sub); addGap(a,root,18);
        Button liteBtn=button(a,"LITE  •  900 MB"); Button fullBtn=button(a,"FULL  •  2 GB");
        root.addView(liteBtn); addGap(a,root,10); root.addView(fullBtn);
        AlertDialog d=new AlertDialog.Builder(a).setView(root).setCancelable(false).create(); d.show(); styleWindow(a,d);
        liteBtn.setOnClickListener(v -> { d.dismiss(); download(a,"lite",lite,cb); });
        fullBtn.setOnClickListener(v -> { d.dismiss(); download(a,"full",full,cb); });
    }

    private static void showNetworkError(Activity a, String detail, ReadyCallback cb) {
        if (a.isFinishing()) return; dismissLoading();
        new AlertDialog.Builder(a).setTitle("Unable to load game data")
                .setMessage("Could not read app.json or the data list. Check your internet and try again.\n\n" + (detail == null ? "" : detail))
                .setCancelable(false).setPositiveButton("RETRY", (d,w) -> checkAndPrepare(a,cb))
                .setNegativeButton("EXIT", (d,w) -> a.finishAffinity()).show();
    }

    private static void download(Activity a, String type, JSONArray arr, ReadyCallback cb) {
        LinearLayout root=panel(a); root.setPadding(dp(a,22),dp(a,22),dp(a,22),dp(a,20));
        TextView title=text(a,"Downloading " + type.toUpperCase(),21,true);
        TextView status=text(a,"Preparing files...",14,false); status.setTextColor(Color.LTGRAY);
        ProgressBar progress=new ProgressBar(a,null,android.R.attr.progressBarStyleHorizontal); progress.setMax(100); progress.setProgress(0);
        root.addView(title); addGap(a,root,10); root.addView(status); addGap(a,root,12); root.addView(progress);
        AlertDialog d=new AlertDialog.Builder(a).setView(root).setCancelable(false).create(); d.show(); styleWindow(a,d);
        IO.execute(() -> {
            try {
                List<DataFile> files=new ArrayList<>(); long total=0, done=0;
                for(int i=0;i<arr.length();i++){ DataFile f=new DataFile(arr.getJSONObject(i)); files.add(f); total+=Math.max(0,f.size); }
                final long totalBytes=total;
                for(int i=0;i<files.size();i++) {
                    DataFile f=files.get(i); File target=new File(a.getExternalFilesDir(null),f.path);
                    if (target.isFile() && (f.size<=0 || target.length()==f.size)) { done += f.size>0?f.size:target.length(); continue; }
                    File parent=target.getParentFile(); if(parent!=null && !parent.exists() && !parent.mkdirs()) throw new Exception("Cannot create folder: "+parent);
                    File tmp=new File(target.getAbsolutePath()+".part");
                    downloadFile(f.url,tmp);
                    if(f.size>0 && tmp.length()!=f.size) { tmp.delete(); throw new Exception("File size mismatch: "+f.path); }
                    if(target.exists() && !target.delete()) throw new Exception("Cannot replace file: "+f.path);
                    if(!tmp.renameTo(target)) throw new Exception("Cannot save file: "+f.path);
                    done += f.size>0?f.size:target.length();
                    final int percent=totalBytes>0?(int)Math.min(100,(done*100)/totalBytes):(i+1)*100/files.size();
                    final String msg="Downloading "+(i+1)+" / "+files.size()+" files";
                    MAIN.post(() -> { if(d.isShowing()){ progress.setProgress(percent); status.setText(msg+" • "+percent+"%"); } });
                }
                if(!isComplete(a,type,arr)) throw new Exception("Some data files are missing after download");
                a.getSharedPreferences(PREFS,Activity.MODE_PRIVATE).edit().putString("installed_type",type).putBoolean(KEY_COMPLETE+type,true).apply();
                MAIN.post(() -> { if(d.isShowing()) d.dismiss(); Toast.makeText(a,"Game data ready",Toast.LENGTH_SHORT).show(); if(!a.isFinishing()) cb.onReady(); });
            } catch(Exception e) {
                android.util.Log.e("GameDataManager","Download failed",e);
                MAIN.post(() -> { if(d.isShowing()) d.dismiss(); if(!a.isFinishing()) new AlertDialog.Builder(a).setTitle("Download failed").setMessage(e.getMessage()==null?"Please try again.":e.getMessage()).setCancelable(false).setPositiveButton("RETRY",(x,w)->download(a,type,arr,cb)).setNegativeButton("EXIT",(x,w)->a.finishAffinity()).show(); });
            }
        });
    }

    private static boolean isComplete(Activity a,String type,JSONArray arr) throws Exception {
        File root=a.getExternalFilesDir(null); if(root==null)return false;
        for(int i=0;i<arr.length();i++) { DataFile f=new DataFile(arr.getJSONObject(i)); File target=new File(root,f.path); if(!target.isFile() || (f.size>0 && target.length()!=f.size)) return false; }
        return arr.length()>0;
    }
    private static JSONObject getJson(String address) throws Exception {
        HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection(); c.setConnectTimeout(15000); c.setReadTimeout(30000); c.setRequestProperty("User-Agent","M-SAMP-Launcher"); c.setInstanceFollowRedirects(true);
        try(InputStream in=new BufferedInputStream(c.getInputStream()); BufferedReader r=new BufferedReader(new InputStreamReader(in,"UTF-8"))) { StringBuilder b=new StringBuilder(); String line; while((line=r.readLine())!=null)b.append(line); return new JSONObject(b.toString()); }
        finally { c.disconnect(); }
    }
    private static void downloadFile(String address,File out) throws Exception {
        HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection(); c.setConnectTimeout(20000); c.setReadTimeout(60000); c.setRequestProperty("User-Agent","M-SAMP-Launcher"); c.setInstanceFollowRedirects(true);
        try(InputStream in=new BufferedInputStream(c.getInputStream()); FileOutputStream fos=new FileOutputStream(out)) { byte[] buf=new byte[64*1024]; int n; while((n=in.read(buf))!=-1)fos.write(buf,0,n); fos.flush(); }
        finally { c.disconnect(); }
    }
    private static LinearLayout panel(Activity a) { LinearLayout l=new LinearLayout(a); l.setOrientation(LinearLayout.VERTICAL); l.setGravity(Gravity.CENTER_HORIZONTAL); l.setBackground(shape(CARD,22)); return l; }
    private static TextView text(Activity a,String s,int size,boolean bold) { TextView t=new TextView(a); t.setText(s); t.setTextColor(WHITE); t.setTextSize(size); if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD); return t; }
    private static Button button(Activity a,String s) { Button b=new Button(a); b.setText(s); b.setTextColor(Color.WHITE); b.setTextSize(14); b.setAllCaps(false); b.setTypeface(Typeface.DEFAULT,Typeface.BOLD); b.setBackground(shape(BLUE,14)); b.setPadding(dp(a,12),dp(a,8),dp(a,12),dp(a,8)); b.setMinHeight(dp(a,48)); return b; }
    private static GradientDrawable shape(int color,int radius) { GradientDrawable d=new GradientDrawable(); d.setColor(color); d.setCornerRadius(radius); return d; }
    private static void addGap(Activity a,LinearLayout l,int h) { View v=new View(a); l.addView(v,new LinearLayout.LayoutParams(1,dp(a,h))); }
    private static int dp(Activity a,int n) { return (int)(n*a.getResources().getDisplayMetrics().density+0.5f); }
    private static void styleWindow(Activity a,AlertDialog d) { if(d.getWindow()!=null){ d.getWindow().setBackgroundDrawableResource(android.R.color.transparent); d.getWindow().setLayout(Math.min(a.getResources().getDisplayMetrics().widthPixels-dp(a,40),dp(a,440)),-2); } }
}
