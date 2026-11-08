package com.hopepride.samp.util;

import java.net.URI;

/** Strictly validate the HTTPS webhook destination without disclosing it in logs. */
public final class DiscordWebhook {
    private DiscordWebhook() {}

    public static boolean isValid(String value) {
        try {
            URI uri = new URI(value);
            String path = uri.getPath();
            return "https".equals(uri.getScheme()) && "discord.com".equals(uri.getHost())
                    && uri.getPort() == -1 && uri.getUserInfo() == null && uri.getRawQuery() == null
                    && uri.getFragment() == null && path != null
                    && path.matches("/api/webhooks/[0-9]+/[A-Za-z0-9._-]+");
        } catch (Exception ignored) {
            return false;
        }
    }
}
