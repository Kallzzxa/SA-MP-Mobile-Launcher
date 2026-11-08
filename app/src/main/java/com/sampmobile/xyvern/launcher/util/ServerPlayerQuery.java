package com.hopepride.samp.launcher.util;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Optional SA-MP detailed player query ('d'); many servers disable or limit it. */
public final class ServerPlayerQuery {
    private ServerPlayerQuery() {}

    public static final class Player {
        public final int id, score, ping;
        public final String name;
        Player(int id, String name, int score, int ping) {
            this.id = id; this.name = name; this.score = score; this.ping = ping;
        }
    }

    public static List<Player> query(String host, int port) throws IOException {
        if (host == null || host.isEmpty() || port < 1 || port > 65535) throw new IOException("Alamat query tidak didukung");
        InetAddress address = null;
        for (InetAddress candidate : InetAddress.getAllByName(host)) {
            if (candidate instanceof Inet4Address) { address = candidate; break; }
        }
        if (address == null) throw new IOException("Query pemain memerlukan IPv4");
        byte[] ip = address.getAddress();
        byte[] header = new byte[]{'S', 'A', 'M', 'P', ip[0], ip[1], ip[2], ip[3],
                (byte) port, (byte) (port >> 8), 'd'};
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.connect(address, port); // Accept datagrams only from the requested server.
            socket.setSoTimeout(2500);
            socket.send(new DatagramPacket(header, header.length));
            byte[] data = new byte[65507];
            DatagramPacket response = new DatagramPacket(data, data.length);
            socket.receive(response);
            return parse(data, response.getLength(), header);
        }
    }

    static List<Player> parse(byte[] data, int length, byte[] expectedHeader) throws IOException {
        if (data == null || expectedHeader == null || expectedHeader.length != 11 || length < 13 || length > data.length)
            throw new IOException("Respons daftar pemain tidak lengkap");
        for (int i = 0; i < 11; i++) {
            if (data[i] != expectedHeader[i]) throw new IOException("Respons query tidak cocok");
        }
        ByteBuffer buffer = ByteBuffer.wrap(data, 11, length - 11).order(ByteOrder.LITTLE_ENDIAN);
        int count = buffer.getShort() & 0xffff;
        // At least 10 bytes per entry, even for an empty name.
        if (count > 1000 || count > buffer.remaining() / 10) throw new IOException("Daftar pemain tidak lengkap");
        List<Player> players = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            if (buffer.remaining() < 10) throw new IOException("Data pemain tidak lengkap");
            int id = buffer.get() & 0xff;
            int nameLength = buffer.get() & 0xff;
            if (buffer.remaining() < nameLength + 8) throw new IOException("Nama pemain tidak lengkap");
            byte[] name = new byte[nameLength];
            buffer.get(name);
            players.add(new Player(id, decodeName(name), buffer.getInt(), buffer.getInt()));
        }
        return players;
    }

    private static String decodeName(byte[] name) {
        try {
            return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(name)).toString();
        } catch (CharacterCodingException invalidUtf8) {
            return new String(name, Charset.forName("windows-1252"));
        }
    }
}
