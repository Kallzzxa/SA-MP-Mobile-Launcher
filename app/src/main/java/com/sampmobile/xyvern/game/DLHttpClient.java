package com.sampmobile.xyvern.game;

import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * HTTPS file downloader for SA-MP 0.3.DL custom models.
 *
 * Called from native DLManager via JNI for URLs the native raw-socket HTTP
 * client cannot handle (https://). Uses Android's HttpURLConnection, which
 * speaks TLS with the system trust store and full certificate validation.
 *
 * Security: certificate validation is NEVER disabled. Redirects are followed
 * by HttpURLConnection's default policy (same-protocol only for 301/302).
 */
public class DLHttpClient {

    /**
     * Downloads urlStr to destPath.
     *
     * @param urlStr   absolute http(s) URL
     * @param destPath local file path to write (overwritten)
     * @param maxBytes hard cap on bytes written; aborts with -100 if exceeded
     * @return 0 on success, negative error code on failure:
     *         -1 generic I/O error, -100 size cap exceeded,
     *         -(http status) for non-2xx responses (e.g. -404)
     */
    public static int downloadFile(String urlStr, String destPath, long maxBytes) {
        HttpURLConnection conn = null;
        InputStream in = null;
        FileOutputStream out = null;
        try {
            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(8000);
            conn.setReadTimeout(15000);
            conn.setRequestProperty("User-Agent", "SAMP/0.3");
            conn.setRequestProperty("Accept", "*/*");
            conn.setInstanceFollowRedirects(true);
            conn.connect();

            int code = conn.getResponseCode();
            if (code < 200 || code >= 300) {
                return -code;
            }

            in = conn.getInputStream();
            out = new FileOutputStream(destPath);
            byte[] buf = new byte[16384];
            long total = 0;
            int n;
            while ((n = in.read(buf)) != -1) {
                total += n;
                if (total > maxBytes) {
                    return -100;
                }
                out.write(buf, 0, n);
            }
            out.flush();
            return 0;
        } catch (Exception e) {
            return -1;
        } finally {
            try { if (in != null) in.close(); } catch (Exception ignored) {}
            try { if (out != null) out.close(); } catch (Exception ignored) {}
            if (conn != null) conn.disconnect();
        }
    }
}
