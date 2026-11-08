package com.hopepride.samp.util;

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

/** Optional SA-MP 'r' query for server-provided rules, independent of joining. */
public final class ServerRulesQuery {
    private ServerRulesQuery() {}

    public static final class Rule {
        public final String name;
        public final String value;
        Rule(String name, String value) { this.name = name; this.value = value; }
    }

    public static List<Rule> query(String host, int port) throws IOException {
        if (host == null || host.isEmpty() || port < 1 || port > 65535) throw new IOException("Alamat tidak valid");
        InetAddress address = null;
        for (InetAddress candidate : InetAddress.getAllByName(host)) {
            if (candidate instanceof Inet4Address) { address = candidate; break; }
        }
        if (address == null) throw new IOException("Query memerlukan IPv4");
        byte[] ip = address.getAddress();
        byte[] header = {'S', 'A', 'M', 'P', ip[0], ip[1], ip[2], ip[3], (byte) port, (byte) (port >> 8), 'r'};
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.connect(address, port);
            socket.setSoTimeout(2500);
            socket.send(new DatagramPacket(header, header.length));
            byte[] data = new byte[65507];
            DatagramPacket packet = new DatagramPacket(data, data.length);
            socket.receive(packet);
            return parse(data, packet.getLength(), header);
        }
    }

    static List<Rule> parse(byte[] data, int length, byte[] header) throws IOException {
        if (data == null || header == null || header.length != 11 || length < 13 || length > data.length)
            throw new IOException("Respons aturan tidak lengkap");
        for (int i = 0; i < 11; i++) {
            if (data[i] != header[i]) throw new IOException("Respons query tidak cocok");
        }
        ByteBuffer buf = ByteBuffer.wrap(data, 11, length - 11).order(ByteOrder.LITTLE_ENDIAN);
        int count = buf.getShort() & 0xffff;
        if (count > 256 || count > buf.remaining() / 2) throw new IOException("Data aturan tidak lengkap");
        List<Rule> result = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            String name = readString(buf);
            String value = readString(buf);
            result.add(new Rule(name, value));
        }
        return result;
    }

    private static String readString(ByteBuffer buf) throws IOException {
        if (!buf.hasRemaining()) throw new IOException("Panjang data tidak tersedia");
        int length = buf.get() & 0xff;
        if (buf.remaining() < length) throw new IOException("Data terpotong");
        byte[] bytes = new byte[length];
        buf.get(bytes);
        try {
            return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString();
        } catch (CharacterCodingException invalidUtf8) {
            return new String(bytes, Charset.forName("windows-1252"));
        }
    }
}
