package com.hopepride.samp.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class ServerInputTest {
    @Test public void validatesSampNicknames() {
        assertTrue(ServerInput.isValidNickname("Pall_Fann"));
        assertTrue(ServerInput.isValidNickname("[HP]Pall"));
        assertFalse(ServerInput.isValidNickname("ab"));
        assertFalse(ServerInput.isValidNickname("player name"));
        assertFalse(ServerInput.isValidNickname("player\nname"));
        assertFalse(ServerInput.isValidNickname("1234567890123456789012345"));
        assertFalse(ServerInput.isValidNickname(null));
    }

    @Test public void validatesAddressAndPort() {
        assertTrue(ServerInput.isValidHost("play.example.com"));
        assertTrue(ServerInput.isValidHost("127.0.0.1"));
        assertFalse(ServerInput.isValidHost("example.com\nport=1"));
        assertFalse(ServerInput.isValidHost(""));
        assertFalse(ServerInput.isValidHost(null));
        assertTrue(ServerInput.isValidPort(7777));
        assertFalse(ServerInput.isValidPort(0));
        assertFalse(ServerInput.isValidPort(65536));
    }

    @Test public void validatesServerPasswordForNativeIni() {
        assertTrue(ServerInput.isValidPassword(""));
        assertTrue(ServerInput.isValidPassword("secret-123!"));
        assertFalse(ServerInput.isValidPassword("line\nbreak"));
        assertFalse(ServerInput.isValidPassword("two words"));
        assertFalse(ServerInput.isValidPassword(null));
    }
}
