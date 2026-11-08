package com.hopepride.samp.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class DiscordWebhookTest {
    @Test public void acceptsOnlyDiscordWebhookEndpoints() {
        assertTrue(DiscordWebhook.isValid("https://discord.com/api/webhooks/123/example-token"));
        assertFalse(DiscordWebhook.isValid("http://discord.com/api/webhooks/123/example-token"));
        assertFalse(DiscordWebhook.isValid("https://discord.com.evil.test/api/webhooks/123/example-token"));
        assertFalse(DiscordWebhook.isValid("https://discord.com/api/webhooks/123/example-token?x=y"));
        assertFalse(DiscordWebhook.isValid("https://discord.com/api/webhooks/123/example-token/extra"));
        assertFalse(DiscordWebhook.isValid(null));
    }
}
