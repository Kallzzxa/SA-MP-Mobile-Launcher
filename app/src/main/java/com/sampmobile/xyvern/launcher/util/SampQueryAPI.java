package com.sampmobile.xyvern.launcher.util;

import java.io.Closeable;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.Random;

/**
 * High-performance, memory-efficient SA-MP 0.3.7 UDP Query Client.
 * Supports IPv4 and domain names, automatic encoding fallback, and robust timeout handling.
 */
public final class SampQueryAPI implements Closeable {

    private static final byte[] SAMP_MAGIC = new byte[]{'S', 'A', 'M', 'P'};
    private static final Charset CP1252 = Charset.forName("windows-1252");

    public DatagramSocket f7277a = null;
    public InetAddress f7278b = null;
    public String f7279c = "";
    public int f7280d;
    public Random f7281e = new Random();
    public boolean f7282f = true;

    private byte[] ipBytes = new byte[4];

    public SampQueryAPI(String host, int port) {
        this.f7280d = port;
        try {
            this.f7278b = InetAddress.getByName(host);
            this.f7279c = this.f7278b.getHostAddress();
            byte[] rawAddr = this.f7278b.getAddress();
            if (rawAddr != null && rawAddr.length == 4) {
                System.arraycopy(rawAddr, 0, this.ipBytes, 0, 4);
            }
        } catch (UnknownHostException e) {
            this.f7282f = false;
        }

        try {
            this.f7277a = new DatagramSocket();
            this.f7277a.setSoTimeout(1800);
        } catch (SocketException e) {
            this.f7282f = false;
        }
    }

    private static String decodeString(int length, ByteBuffer buffer) {
        if (length <= 0 || buffer.remaining() < length) {
            return "";
        }
        byte[] bytes = new byte[length];
        buffer.get(bytes);

        // Try UTF-8 first
        try {
            return new String(bytes, StandardCharsets.UTF_8);
        } catch (Exception ignored) {
        }
        // Fallback to CP1252 / Western
        return new String(bytes, CP1252);
    }

    public static String m5851a(int i, ByteBuffer byteBuffer) {
        return decodeString(i, byteBuffer);
    }

    /**
     * Queries server information: [password(0/1), players, maxPlayers, hostname, gamemode, language]
     */
    public String[] mo7164b() {
        if (!this.f7282f || this.f7277a == null || this.f7278b == null) {
            return null;
        }

        try {
            DatagramPacket sendPacket = createPacket('i', null);
            if (sendPacket == null) return null;

            this.f7277a.send(sendPacket);

            byte[] recvBuffer = new byte[2048];
            DatagramPacket recvPacket = new DatagramPacket(recvBuffer, recvBuffer.length);
            this.f7277a.receive(recvPacket);

            if (recvPacket.getLength() < 11) {
                return null;
            }

            ByteBuffer wrap = ByteBuffer.wrap(recvBuffer, 0, recvPacket.getLength());
            wrap.order(ByteOrder.LITTLE_ENDIAN);
            wrap.position(11); // Skip 11-byte header

            if (wrap.remaining() < 5) return null;

            String[] result = new String[6];
            result[0] = wrap.get() == 0 ? "0" : "1"; // Has password
            result[1] = String.valueOf(wrap.getShort() & 0xFFFF); // Players
            result[2] = String.valueOf(wrap.getShort() & 0xFFFF); // Max players

            result[3] = wrap.remaining() >= 4 ? decodeString(wrap.getInt(), wrap) : ""; // Hostname
            result[4] = wrap.remaining() >= 4 ? decodeString(wrap.getInt(), wrap) : ""; // Gamemode
            result[5] = wrap.remaining() >= 4 ? decodeString(wrap.getInt(), wrap) : ""; // Language

            return result;
        } catch (Exception e) {
            return null;
        }
    }

    private DatagramPacket createPacket(char opcode, byte[] payload) {
        int payloadLen = (payload != null) ? payload.length : 0;
        byte[] packet = new byte[11 + payloadLen];

        // 0..3: SAMP
        packet[0] = 'S';
        packet[1] = 'A';
        packet[2] = 'M';
        packet[3] = 'P';

        // 4..7: Server IP in network byte order
        System.arraycopy(this.ipBytes, 0, packet, 4, 4);

        // 8..9: Port (little-endian)
        packet[8] = (byte) (this.f7280d & 0xFF);
        packet[9] = (byte) ((this.f7280d >> 8) & 0xFF);

        // 10: Opcode
        packet[10] = (byte) opcode;

        // Payload (if any, e.g. ping nonce)
        if (payloadLen > 0) {
            System.arraycopy(payload, 0, packet, 11, payloadLen);
        }

        return new DatagramPacket(packet, packet.length, this.f7278b, this.f7280d);
    }

    public DatagramPacket mo7165c(String str) {
        if (str == null || str.isEmpty()) return null;
        char opcode = str.charAt(0);
        byte[] payload = null;
        if (str.length() > 1) {
            payload = str.substring(1).getBytes(StandardCharsets.ISO_8859_1);
        }
        return createPacket(opcode, payload);
    }

    /**
     * Sends a ping packet with a 4-byte nonce and checks echo.
     */
    public boolean mo7166d() {
        if (!this.f7282f || this.f7277a == null || this.f7278b == null) {
            return false;
        }

        byte[] nonce = new byte[4];
        this.f7281e.nextBytes(nonce);

        try {
            DatagramPacket sendPacket = createPacket('p', nonce);
            if (sendPacket == null) return false;

            this.f7277a.send(sendPacket);

            byte[] recvBuffer = new byte[64];
            DatagramPacket recvPacket = new DatagramPacket(recvBuffer, recvBuffer.length);
            this.f7277a.receive(recvPacket);

            if (recvPacket.getLength() >= 15) {
                return recvBuffer[10] == 'p'
                        && recvBuffer[11] == nonce[0]
                        && recvBuffer[12] == nonce[1]
                        && recvBuffer[13] == nonce[2]
                        && recvBuffer[14] == nonce[3];
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    @Override
    public void close() {
        if (this.f7277a != null && !this.f7277a.isClosed()) {
            try {
                this.f7277a.close();
            } catch (Exception ignored) {
            }
        }
    }
}
