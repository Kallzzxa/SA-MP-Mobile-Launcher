package com.hopepride.samp.util;

import org.junit.Test;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.List;
import static org.junit.Assert.*;

public class ServerPlayerQueryTest {
    private final byte[] header = {'S','A','M','P',127,0,0,1,0x61,0x1e,'d'};

    @Test public void parsesDetailedPlayerDataAndEmptyList() throws Exception {
        ByteBuffer packet = ByteBuffer.allocate(128).order(ByteOrder.LITTLE_ENDIAN);
        packet.put(header).putShort((short) 1).put((byte) 7).put((byte) 4);
        packet.put("Pall".getBytes(StandardCharsets.UTF_8)).putInt(42).putInt(90);
        List<ServerPlayerQuery.Player> players = ServerPlayerQuery.parse(packet.array(), packet.position(), header);
        assertEquals(1, players.size());
        assertEquals(7, players.get(0).id);
        assertEquals("Pall", players.get(0).name);
        assertEquals(42, players.get(0).score);
        assertEquals(90, players.get(0).ping);
        packet.clear(); packet.put(header).putShort((short) 0);
        assertTrue(ServerPlayerQuery.parse(packet.array(), packet.position(), header).isEmpty());
    }

    @Test public void rejectsTruncatedAndWrongHeader() throws Exception {
        ByteBuffer packet = ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN);
        packet.put(header).putShort((short) 1).put((byte) 1).put((byte) 20);
        try { ServerPlayerQuery.parse(packet.array(), packet.position(), header); fail(); } catch (IOException expected) { }
        packet.clear(); packet.put(header).putShort((short) 0); packet.array()[10] = 'i';
        try { ServerPlayerQuery.parse(packet.array(), packet.position(), header); fail(); } catch (IOException expected) { }
    }
}
