package com.sampmobile.xyvern.cef.omp;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * Wire format for aurora-mp/omp-cef (src/shared/packet.hpp +
 * src/shared/packet-serializer.hpp), MIT licensed, Copyright (c) 2026 Aurora Team.
 *
 * All integers are little-endian. Strings are u16 length + bytes (truncated
 * to 4096 by the server, same cap applied here). Byte blobs are u32 length +
 * bytes. A packet is one type byte followed by its payload.
 *
 * Packet types:
 * 0 RequestJoin, 1 HandshakeChallenge, 2 HandshakeFinalize, 3 JoinResponse,
 * 4 ServerConfig, 5 RequestFiles, 6 FileData, 7 DownloadComplete,
 * 8 PressKey, 9 EmitEvent, 10 EmitBrowserEvent, 11 ClientEmitEvent.
 *
 * Argument types: 0 String, 1 Integer, 2 Float, 3 Bool.
 * Client version sent in RequestJoin is 1.3.0.0 (0x01030000); the omp-cef
 * server plugin rejects any other client version.
 */
public final class OmpPacket {
    public static final int TYPE_REQUEST_JOIN = 0;
    public static final int TYPE_HANDSHAKE_CHALLENGE = 1;
    public static final int TYPE_HANDSHAKE_FINALIZE = 2;
    public static final int TYPE_JOIN_RESPONSE = 3;
    public static final int TYPE_SERVER_CONFIG = 4;
    public static final int TYPE_REQUEST_FILES = 5;
    public static final int TYPE_FILE_DATA = 6;
    public static final int TYPE_DOWNLOAD_COMPLETE = 7;
    public static final int TYPE_PRESS_KEY = 8;
    public static final int TYPE_EMIT_EVENT = 9;
    public static final int TYPE_EMIT_BROWSER_EVENT = 10;
    public static final int TYPE_CLIENT_EMIT_EVENT = 11;

    public static final int ARG_STRING = 0;
    public static final int ARG_INTEGER = 1;
    public static final int ARG_FLOAT = 2;
    public static final int ARG_BOOL = 3;

    /** Version 1.3.0.0, packed as (major<<24)|(minor<<16)|(patch<<8)|build. */
    public static final int CLIENT_VERSION = (1 << 24) | (3 << 16) | (0 << 8) | 0;

    public static final class Argument {
        public final int type;
        public final String stringValue;
        public final int intValue;
        public final float floatValue;
        public final boolean boolValue;

        private Argument(int type, String s, int i, float f, boolean b) {
            this.type = type;
            this.stringValue = s;
            this.intValue = i;
            this.floatValue = f;
            this.boolValue = b;
        }

        public static Argument str(String v) { return new Argument(ARG_STRING, v, 0, 0, false); }
        public static Argument integer(int v) { return new Argument(ARG_INTEGER, null, v, 0, false); }
        public static Argument flt(float v) { return new Argument(ARG_FLOAT, null, 0, v, false); }
        public static Argument bool(boolean v) { return new Argument(ARG_BOOL, null, 0, 0, v); }
    }

    public static final class Packet {
        public int type;
        // RequestJoin
        public int playerId;
        public int clientVersion;
        // HandshakeChallenge / Finalize
        public byte[] cookie;
        public byte[] publicKey;
        // JoinResponse
        public boolean accepted;
        public int kcpConvId;
        public String manifestJson = "";
        public int rejectReason;
        public int serverVersion;
        public String message = "";
        // ServerConfig
        public byte[] masterResourceKey;
        public boolean resourcesLoaderUi;
        // RequestFiles
        public final List<String[]> files = new ArrayList<>();
        // FileData
        public String resourceName = "";
        public String relativePath = "";
        public String fileHash = "";
        public int chunkIndex;
        public int totalChunks;
        public byte[] data;
        // EmitEvent / ClientEmitEvent
        public int browserId;
        public String eventName = "";
        public final List<Argument> args = new ArrayList<>();
    }

    private OmpPacket() {}

    // ---- encoding helpers (little-endian) ----

    static void writeString(ByteArrayOutputStream out, String s) {
        byte[] b = s == null ? new byte[0] : s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        if (b.length > 4096) {
            byte[] t = new byte[4096];
            System.arraycopy(b, 0, t, 0, 4096);
            b = t;
        }
        out.write(b.length & 0xFF);
        out.write((b.length >> 8) & 0xFF);
        out.write(b, 0, b.length);
    }

    static void writeBytes(ByteArrayOutputStream out, byte[] b) {
        if (b == null) b = new byte[0];
        int n = b.length;
        out.write(n & 0xFF);
        out.write((n >> 8) & 0xFF);
        out.write((n >> 16) & 0xFF);
        out.write((n >> 24) & 0xFF);
        out.write(b, 0, n);
    }

    static void writeInt(ByteArrayOutputStream out, int v) {
        out.write(v & 0xFF);
        out.write((v >> 8) & 0xFF);
        out.write((v >> 16) & 0xFF);
        out.write((v >> 24) & 0xFF);
    }

    static void writeFloat(ByteArrayOutputStream out, float v) {
        writeInt(out, Float.floatToRawIntBits(v));
    }

    public static byte[] encodeRequestJoin(int playerId) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(TYPE_REQUEST_JOIN);
        writeInt(out, playerId);
        writeInt(out, CLIENT_VERSION);
        return out.toByteArray();
    }

    public static byte[] encodeHandshakeFinalize(byte[] cookie, byte[] clientPublicKey) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(TYPE_HANDSHAKE_FINALIZE);
        writeBytes(out, cookie);
        writeBytes(out, clientPublicKey);
        return out.toByteArray();
    }

    public static byte[] encodeRequestFiles(List<String[]> files) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(TYPE_REQUEST_FILES);
        int count = files.size();
        out.write(count & 0xFF);
        out.write((count >> 8) & 0xFF);
        for (String[] f : files) {
            if (f[0] == null || f[0].isEmpty() || f[1] == null || f[1].isEmpty()) continue;
            writeString(out, f[0]);
            writeString(out, f[1]);
        }
        return out.toByteArray();
    }

    public static byte[] encodeDownloadComplete() {
        return new byte[]{(byte) TYPE_DOWNLOAD_COMPLETE};
    }

    public static byte[] encodeClientEmitEvent(int browserId, String name, List<Argument> args) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(TYPE_CLIENT_EMIT_EVENT);
        writeInt(out, browserId);
        writeString(out, name);
        out.write(args.size() & 0xFF);
        for (Argument a : args) {
            out.write(a.type & 0xFF);
            switch (a.type) {
                case ARG_STRING: writeString(out, a.stringValue); break;
                case ARG_INTEGER: writeInt(out, a.intValue); break;
                case ARG_FLOAT: writeFloat(out, a.floatValue); break;
                case ARG_BOOL: out.write(a.boolValue ? 1 : 0); break;
                default: break;
            }
        }
        return out.toByteArray();
    }

    // ---- decoding ----

    private static final class Reader {
        final ByteBuffer buf;
        Reader(byte[] data) {
            buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        }
        boolean has(int n) { return buf.remaining() >= n; }
        int u8() { return buf.get() & 0xFF; }
        int u16() { return buf.getShort() & 0xFFFF; }
        int i32() { return buf.getInt(); }
        float f32() { return buf.getFloat(); }
        String readString() {
            if (!has(2)) return null;
            int n = u16();
            if (!has(n)) return null;
            byte[] b = new byte[n];
            buf.get(b);
            return new String(b, java.nio.charset.StandardCharsets.UTF_8);
        }
        byte[] readBytes() {
            if (!has(4)) return null;
            int n = buf.getInt();
            if (n < 0 || !has(n)) return null;
            byte[] b = new byte[n];
            buf.get(b);
            return b;
        }
    }

    /** Returns null if the buffer is not a well-formed packet of a known type. */
    public static Packet decode(byte[] data) {
        if (data == null || data.length < 1) return null;
        Reader r = new Reader(data);
        Packet p = new Packet();
        p.type = r.u8();
        switch (p.type) {
            case TYPE_REQUEST_JOIN: {
                if (!r.has(8)) return null;
                p.playerId = r.i32();
                p.clientVersion = r.i32();
                break;
            }
            case TYPE_HANDSHAKE_CHALLENGE: {
                p.cookie = r.readBytes();
                p.publicKey = r.readBytes();
                if (p.cookie == null || p.publicKey == null) return null;
                break;
            }
            case TYPE_HANDSHAKE_FINALIZE: {
                p.cookie = r.readBytes();
                p.publicKey = r.readBytes();
                if (p.cookie == null || p.publicKey == null) return null;
                break;
            }
            case TYPE_JOIN_RESPONSE: {
                if (!r.has(1 + 4)) return null;
                p.accepted = r.u8() != 0;
                p.kcpConvId = r.i32();
                String m = r.readString();
                if (m == null) return null;
                p.manifestJson = m;
                if (r.has(1)) {
                    p.rejectReason = r.u8();
                    if (r.has(4)) p.serverVersion = r.i32();
                    if (r.has(4)) p.clientVersion = r.i32();
                    if (r.has(2)) {
                        String msg = r.readString();
                        if (msg != null) p.message = msg;
                    }
                }
                break;
            }
            case TYPE_SERVER_CONFIG: {
                p.masterResourceKey = r.readBytes();
                if (p.masterResourceKey == null || !r.has(1)) return null;
                p.resourcesLoaderUi = r.u8() != 0;
                break;
            }
            case TYPE_REQUEST_FILES: {
                if (!r.has(2)) return null;
                int count = r.u16();
                for (int i = 0; i < count; i++) {
                    String res = r.readString();
                    String path = r.readString();
                    if (res == null || path == null) return null;
                    p.files.add(new String[]{res, path});
                }
                break;
            }
            case TYPE_FILE_DATA: {
                String res = r.readString();
                String path = r.readString();
                String hash = r.readString();
                if (res == null || path == null || hash == null || !r.has(8)) return null;
                p.resourceName = res;
                p.relativePath = path;
                p.fileHash = hash;
                p.chunkIndex = r.i32();
                p.totalChunks = r.i32();
                p.data = r.readBytes();
                if (p.data == null) return null;
                break;
            }
            case TYPE_DOWNLOAD_COMPLETE:
            case TYPE_PRESS_KEY:
                // No payload in the omp-cef serializer for these types.
                break;
            case TYPE_EMIT_EVENT:
            case TYPE_EMIT_BROWSER_EVENT:
            case TYPE_CLIENT_EMIT_EVENT: {
                if (!r.has(4)) return null;
                p.browserId = r.i32();
                String name = r.readString();
                if (name == null || !r.has(1)) return null;
                p.eventName = name;
                int count = r.u8();
                for (int i = 0; i < count; i++) {
                    if (!r.has(1)) return null;
                    int t = r.u8();
                    switch (t) {
                        case ARG_STRING: {
                            String s = r.readString();
                            if (s == null) return null;
                            p.args.add(Argument.str(s));
                            break;
                        }
                        case ARG_INTEGER:
                            if (!r.has(4)) return null;
                            p.args.add(Argument.integer(r.i32()));
                            break;
                        case ARG_FLOAT:
                            if (!r.has(4)) return null;
                            p.args.add(Argument.flt(r.f32()));
                            break;
                        case ARG_BOOL:
                            if (!r.has(1)) return null;
                            p.args.add(Argument.bool(r.u8() != 0));
                            break;
                        default:
                            return null;
                    }
                }
                break;
            }
            default:
                return null;
        }
        return p;
    }
}
