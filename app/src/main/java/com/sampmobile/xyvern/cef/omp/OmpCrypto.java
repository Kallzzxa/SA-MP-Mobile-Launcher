package com.sampmobile.xyvern.cef.omp;

import java.math.BigInteger;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.Arrays;

/**
 * Pure-Java, libsodium-compatible crypto primitives for the omp-cef Android port.
 *
 * <p>No Android imports, no BouncyCastle, no libsodium bindings. Only
 * {@code java.security}, {@code java.math.BigInteger} and {@code javax.crypto}
 * would be allowed by the spec; in practice this file only needs
 * {@code MessageDigest} (SHA-256) and {@code SecureRandom} from
 * {@code java.security} plus {@code BigInteger}.</p>
 *
 * <p>Implemented here from the specifications, not from a library:</p>
 * <ul>
 *   <li>X25519 (RFC 7748 Montgomery ladder, scalar clamping, little-endian).</li>
 *   <li>BLAKE2b-512, unkeyed (RFC 7693).</li>
 *   <li>HSalsa20 / Salsa20 (20 rounds) and Poly1305 (mod 2^130-5).</li>
 *   <li>XSalsa20-Poly1305 secretbox in libsodium {@code crypto_secretbox_easy}
 *       format: 16-byte Poly1305 tag followed by the ciphertext body.</li>
 *   <li>libsodium {@code crypto_kx_client_session_keys} derivation.</li>
 * </ul>
 *
 * <p>Performance note: X25519 and Poly1305 use {@code BigInteger} field
 * arithmetic. That is not constant-time, but it is more than fast enough for
 * a one-time handshake and for KCP packets up to ~64 KB, which is the stated
 * use case. Do not use this for high-throughput bulk encryption.</p>
 *
 * <p>Error contract: invalid lengths / null arguments throw
 * {@code IllegalArgumentException}. {@link #x25519(byte[], byte[])} also throws
 * {@code IllegalArgumentException} when the result is the all-zero point
 * (the small-order / contributory-behaviour rejection in the spec).
 * {@link #secretboxDecrypt(byte[], byte[], byte[])} is the exception: a bad
 * tag (or a box shorter than the 16-byte tag) returns {@code null} instead
 * of throwing, so callers can treat it as an authentication failure.</p>
 */
public final class OmpCrypto {

    private OmpCrypto() {
    }

    // ------------------------------------------------------------------
    // Generic argument checks
    // ------------------------------------------------------------------

    private static void requireLength(byte[] v, int len, String name) {
        if (v == null || v.length != len) {
            throw new IllegalArgumentException(name + " must be " + len + " bytes");
        }
    }

    private static void requireNotNull(byte[] v, String name) {
        if (v == null) {
            throw new IllegalArgumentException(name + " must not be null");
        }
    }

    // ------------------------------------------------------------------
    // X25519 (RFC 7748)
    // ------------------------------------------------------------------

    private static final BigInteger P25519 =
            BigInteger.ONE.shiftLeft(255).subtract(BigInteger.valueOf(19));
    private static final BigInteger A24 = BigInteger.valueOf(121665); // (486662 - 2) / 4

    private static final byte[] BASE_POINT = new byte[32];

    static {
        BASE_POINT[0] = 9;
    }

    /** Decode a 32-byte little-endian field element, reduced mod 2^255-19. */
    private static BigInteger decodeLittleEndianModP(byte[] le) {
        byte[] be = new byte[le.length];
        for (int i = 0; i < le.length; i++) {
            be[i] = le[le.length - 1 - i];
        }
        return new BigInteger(1, be).mod(P25519);
    }

    /** Encode a field element as 32 little-endian bytes. */
    private static byte[] encodeLittleEndian32(BigInteger v) {
        byte[] out = new byte[32];
        for (int i = 0; i < 32; i++) {
            out[i] = (byte) v.shiftRight(8 * i).intValue();
        }
        return out;
    }

    private static boolean isAllZero(byte[] v) {
        int acc = 0;
        for (byte b : v) {
            acc |= b;
        }
        return acc == 0;
    }

    /**
     * Montgomery ladder, scalar clamped as in RFC 7748, inputs/outputs
     * little-endian. Throws {@code IllegalArgumentException} if the shared
     * secret is all zero.
     */
    public static byte[] x25519(byte[] scalar32, byte[] point32) {
        requireLength(scalar32, 32, "scalar32");
        requireLength(point32, 32, "point32");

        // Clamp a copy of the scalar.
        byte[] s = Arrays.copyOf(scalar32, 32);
        s[0] = (byte) (s[0] & 0xF8);
        s[31] = (byte) ((s[31] & 0x7F) | 0x40);

        // Decode u, masking off the top bit of the last byte (RFC 7748).
        byte[] uBytes = Arrays.copyOf(point32, 32);
        uBytes[31] = (byte) (uBytes[31] & 0x7F);
        BigInteger x1 = decodeLittleEndianModP(uBytes);

        BigInteger x2 = BigInteger.ONE;
        BigInteger z2 = BigInteger.ZERO;
        BigInteger x3 = x1;
        BigInteger z3 = BigInteger.ONE;
        int swap = 0;

        for (int t = 254; t >= 0; t--) {
            int kt = ((s[t >> 3] & 0xFF) >> (t & 7)) & 1;
            swap ^= kt;
            if (swap == 1) {
                BigInteger tmp = x2; x2 = x3; x3 = tmp;
                tmp = z2; z2 = z3; z3 = tmp;
            }
            swap = kt;

            BigInteger a = x2.add(z2).mod(P25519);
            BigInteger aa = a.multiply(a).mod(P25519);
            BigInteger b = x2.subtract(z2).mod(P25519);
            BigInteger bb = b.multiply(b).mod(P25519);
            BigInteger e = aa.subtract(bb).mod(P25519);
            BigInteger c = x3.add(z3).mod(P25519);
            BigInteger d = x3.subtract(z3).mod(P25519);
            BigInteger da = d.multiply(a).mod(P25519);
            BigInteger cb = c.multiply(b).mod(P25519);

            BigInteger daPlusCb = da.add(cb).mod(P25519);
            x3 = daPlusCb.multiply(daPlusCb).mod(P25519);

            BigInteger daMinusCb = da.subtract(cb).mod(P25519);
            BigInteger diffSq = daMinusCb.multiply(daMinusCb).mod(P25519);
            z3 = x1.multiply(diffSq).mod(P25519);

            x2 = aa.multiply(bb).mod(P25519);
            z2 = e.multiply(aa.add(A24.multiply(e)).mod(P25519)).mod(P25519);
        }

        if (swap == 1) {
            BigInteger tmp = x2; x2 = x3; x3 = tmp;
            tmp = z2; z2 = z3; z3 = tmp;
        }

        if (z2.equals(BigInteger.ZERO)) {
            throw new IllegalArgumentException("x25519: degenerate point (z = 0)");
        }
        byte[] out = encodeLittleEndian32(x2.multiply(z2.modInverse(P25519)).mod(P25519));
        if (isAllZero(out)) {
            throw new IllegalArgumentException("x25519: all-zero shared secret rejected");
        }
        return out;
    }

    /**
     * Returns {@code {publicKey32, secretKey32}}. The secret key is 32 random
     * bytes; the public key is {@code x25519(secret, basePoint 9)}. Clamping
     * happens inside {@link #x25519}, so the stored secret is the raw random
     * value (as in libsodium, where the secret key is stored unclamped and
     * clamped on each use).
     */
    public static byte[][] generateKeypair(SecureRandom rnd) {
        SecureRandom r = (rnd != null) ? rnd : new SecureRandom();
        byte[] sk = new byte[32];
        r.nextBytes(sk);
        byte[] pk = x25519(sk, BASE_POINT);
        return new byte[][]{pk, sk};
    }

    // ------------------------------------------------------------------
    // BLAKE2b-512 (RFC 7693), unkeyed, no salt, no personal string
    // ------------------------------------------------------------------

    private static final long[] BLAKE2B_IV = {
        0x6a09e667f3bcc908L, 0xbb67ae8584caa73bL,
        0x3c6ef372fe94f82bL, 0xa54ff53a5f1d36f1L,
        0x510e527fade682d1L, 0x9b05688c2b3e6c1fL,
        0x1f83d9abfb41bd6bL, 0x5be0cd19137e2179L
    };

    private static final int[][] BLAKE2B_SIGMA = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
        {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
        {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
        {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
        {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
        {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
        {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
        {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
        {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0}
    };

    private static long rotr64(long x, int n) {
        return (x >>> n) | (x << (64 - n));
    }

    private static long load64Le(byte[] b, int off) {
        return ((long) (b[off] & 0xFF))
                | ((long) (b[off + 1] & 0xFF) << 8)
                | ((long) (b[off + 2] & 0xFF) << 16)
                | ((long) (b[off + 3] & 0xFF) << 24)
                | ((long) (b[off + 4] & 0xFF) << 32)
                | ((long) (b[off + 5] & 0xFF) << 40)
                | ((long) (b[off + 6] & 0xFF) << 48)
                | ((long) (b[off + 7] & 0xFF) << 56);
    }

    private static void store64Le(byte[] b, int off, long v) {
        b[off] = (byte) v;
        b[off + 1] = (byte) (v >>> 8);
        b[off + 2] = (byte) (v >>> 16);
        b[off + 3] = (byte) (v >>> 24);
        b[off + 4] = (byte) (v >>> 32);
        b[off + 5] = (byte) (v >>> 40);
        b[off + 6] = (byte) (v >>> 48);
        b[off + 7] = (byte) (v >>> 56);
    }

    private static void blake2bG(long[] v, int a, int b, int c, int d, long mx, long my) {
        v[a] = v[a] + v[b] + mx;
        v[d] = rotr64(v[d] ^ v[a], 32);
        v[c] = v[c] + v[d];
        v[b] = rotr64(v[b] ^ v[c], 24);
        v[a] = v[a] + v[b] + my;
        v[d] = rotr64(v[d] ^ v[a], 16);
        v[c] = v[c] + v[d];
        v[b] = rotr64(v[b] ^ v[c], 63);
    }

    private static void blake2bCompress(long[] h, byte[] block, long counter, boolean last) {
        long[] m = new long[16];
        for (int i = 0; i < 16; i++) {
            m[i] = load64Le(block, i * 8);
        }
        long[] v = new long[16];
        System.arraycopy(h, 0, v, 0, 8);
        v[8] = BLAKE2B_IV[0];
        v[9] = BLAKE2B_IV[1];
        v[10] = BLAKE2B_IV[2];
        v[11] = BLAKE2B_IV[3];
        v[12] = counter ^ BLAKE2B_IV[4];
        v[13] = 0L ^ BLAKE2B_IV[5];
        v[14] = (last ? 0xFFFFFFFFFFFFFFFFL : 0L) ^ BLAKE2B_IV[6];
        v[15] = 0L ^ BLAKE2B_IV[7];

        for (int r = 0; r < 12; r++) {
            int[] s = BLAKE2B_SIGMA[r % 10];
            // Column step.
            blake2bG(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
            blake2bG(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
            blake2bG(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
            blake2bG(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
            // Diagonal step.
            blake2bG(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
            blake2bG(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
            blake2bG(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
            blake2bG(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
        }
        for (int i = 0; i < 8; i++) {
            h[i] ^= v[i] ^ v[i + 8];
        }
    }

    /** BLAKE2b with a 64-byte digest, unkeyed, no salt, no personal string. */
    public static byte[] blake2b512(byte[] input) {
        requireNotNull(input, "input");
        long[] h = new long[8];
        // Parameter block, word 0 only (all other words are zero):
        // digest_length=64, key_length=0, fanout=1, depth=1, leaf_length=0.
        long param0 = 0x01010000L ^ 64L; // = 0x01010040
        h[0] = BLAKE2B_IV[0] ^ param0;
        for (int i = 1; i < 8; i++) {
            h[i] = BLAKE2B_IV[i];
        }

        int offset = 0;
        int remaining = input.length;
        while (remaining > 128) {
            byte[] block = Arrays.copyOfRange(input, offset, offset + 128);
            blake2bCompress(h, block, offset + 128L, false);
            offset += 128;
            remaining -= 128;
        }
        // Final block: remaining bytes (0..128), zero-padded, counter = total length.
        byte[] last = new byte[128];
        if (remaining > 0) {
            System.arraycopy(input, offset, last, 0, remaining);
        }
        blake2bCompress(h, last, input.length, true);

        byte[] out = new byte[64];
        for (int i = 0; i < 8; i++) {
            store64Le(out, i * 8, h[i]);
        }
        return out;
    }

    // ------------------------------------------------------------------
    // crypto_kx (client side)
    // ------------------------------------------------------------------

    /**
     * libsodium {@code crypto_kx_client_session_keys}: returns
     * {@code {rx32, tx32}} where {@code rx || tx =
     * BLAKE2b-512(sharedSecret || clientPk || serverPk)} and
     * {@code sharedSecret = x25519(clientSk, serverPk)}.
     */
    public static byte[][] kxClientSessionKeys(byte[] clientPk32, byte[] clientSk32, byte[] serverPk32) {
        requireLength(clientPk32, 32, "clientPk32");
        requireLength(clientSk32, 32, "clientSk32");
        requireLength(serverPk32, 32, "serverPk32");
        byte[] shared = x25519(clientSk32, serverPk32);
        byte[] material = new byte[96];
        System.arraycopy(shared, 0, material, 0, 32);
        System.arraycopy(clientPk32, 0, material, 32, 32);
        System.arraycopy(serverPk32, 0, material, 64, 32);
        byte[] hash = blake2b512(material);
        byte[] rx = Arrays.copyOfRange(hash, 0, 32);
        byte[] tx = Arrays.copyOfRange(hash, 32, 64);
        return new byte[][]{rx, tx};
    }

    // ------------------------------------------------------------------
    // Salsa20 / HSalsa20
    // ------------------------------------------------------------------

    private static int load32Le(byte[] b, int off) {
        return (b[off] & 0xFF)
                | ((b[off + 1] & 0xFF) << 8)
                | ((b[off + 2] & 0xFF) << 16)
                | ((b[off + 3] & 0xFF) << 24);
    }

    private static void store32Le(byte[] b, int off, int v) {
        b[off] = (byte) v;
        b[off + 1] = (byte) (v >>> 8);
        b[off + 2] = (byte) (v >>> 16);
        b[off + 3] = (byte) (v >>> 24);
    }

    private static int rotl32(int x, int n) {
        return (x << n) | (x >>> (32 - n));
    }

    /** One Salsa20 quarter-round on positions (a, b, c, d) of the state. */
    private static void salsaQr(int[] x, int a, int b, int c, int d) {
        x[b] ^= rotl32(x[a] + x[d], 7);
        x[c] ^= rotl32(x[b] + x[a], 9);
        x[d] ^= rotl32(x[c] + x[b], 13);
        x[a] ^= rotl32(x[d] + x[c], 18);
    }

    /** 20 rounds (10 double rounds), no final addition. Returns the working state. */
    private static int[] salsaCore(int[] input) {
        int[] x = Arrays.copyOf(input, 16);
        for (int round = 0; round < 10; round++) {
            // Column round.
            salsaQr(x, 0, 4, 8, 12);
            salsaQr(x, 5, 9, 13, 1);
            salsaQr(x, 10, 14, 2, 6);
            salsaQr(x, 15, 3, 7, 11);
            // Row round.
            salsaQr(x, 0, 1, 2, 3);
            salsaQr(x, 5, 6, 7, 4);
            salsaQr(x, 10, 11, 8, 9);
            salsaQr(x, 15, 12, 13, 14);
        }
        return x;
    }

    private static final int C0 = 0x61707865; // "expa"
    private static final int C1 = 0x3320646e; // "nd 3"
    private static final int C2 = 0x79622d32; // "2-by"
    private static final int C3 = 0x6b206574; // "te k"

    /** HSalsa20: 16-byte nonce -> 32-byte subkey. */
    private static byte[] hsalsa20(byte[] key32, byte[] nonce16) {
        int[] st = new int[16];
        st[0] = C0;
        st[1] = load32Le(key32, 0);
        st[2] = load32Le(key32, 4);
        st[3] = load32Le(key32, 8);
        st[4] = load32Le(key32, 12);
        st[5] = C1;
        st[6] = load32Le(nonce16, 0);
        st[7] = load32Le(nonce16, 4);
        st[8] = load32Le(nonce16, 8);
        st[9] = load32Le(nonce16, 12);
        st[10] = C2;
        st[11] = load32Le(key32, 16);
        st[12] = load32Le(key32, 20);
        st[13] = load32Le(key32, 24);
        st[14] = load32Le(key32, 28);
        st[15] = C3;
        int[] out = salsaCore(st);
        // HSalsa20 output word order: 0, 5, 10, 15, 6, 7, 8, 9.
        int[] order = {0, 5, 10, 15, 6, 7, 8, 9};
        byte[] subkey = new byte[32];
        for (int i = 0; i < 8; i++) {
            store32Le(subkey, i * 4, out[order[i]]);
        }
        return subkey;
    }

    /** Salsa20 keystream of {@code length} bytes, 8-byte nonce, counter from 0. */
    private static byte[] salsa20Stream(byte[] key32, byte[] nonce8, int length) {
        int[] base = new int[16];
        base[0] = C0;
        base[1] = load32Le(key32, 0);
        base[2] = load32Le(key32, 4);
        base[3] = load32Le(key32, 8);
        base[4] = load32Le(key32, 12);
        base[5] = C1;
        base[6] = load32Le(nonce8, 0);
        base[7] = load32Le(nonce8, 4);
        base[8] = 0; // block counter low
        base[9] = 0; // block counter high
        base[10] = C2;
        base[11] = load32Le(key32, 16);
        base[12] = load32Le(key32, 20);
        base[13] = load32Le(key32, 24);
        base[14] = load32Le(key32, 28);
        base[15] = C3;

        byte[] stream = new byte[length];
        long blockCounter = 0;
        int produced = 0;
        while (produced < length) {
            int[] st = Arrays.copyOf(base, 16);
            st[8] = (int) blockCounter;
            st[9] = (int) (blockCounter >>> 32);
            int[] core = salsaCore(st);
            byte[] block = new byte[64];
            for (int i = 0; i < 16; i++) {
                store32Le(block, i * 4, core[i] + st[i]);
            }
            int take = Math.min(64, length - produced);
            System.arraycopy(block, 0, stream, produced, take);
            produced += take;
            blockCounter++;
        }
        return stream;
    }

    // ------------------------------------------------------------------
    // Poly1305 (mod 2^130-5)
    // ------------------------------------------------------------------

    private static final BigInteger POLY_P =
            BigInteger.ONE.shiftLeft(130).subtract(BigInteger.valueOf(5));

    /** Interpret a little-endian byte array as an unsigned BigInteger. */
    private static BigInteger decodeLittleEndian(byte[] le) {
        byte[] be = new byte[le.length];
        for (int i = 0; i < le.length; i++) {
            be[i] = le[le.length - 1 - i];
        }
        return new BigInteger(1, be);
    }

    /** Poly1305 one-time authenticator: 16-byte tag for {@code msg} under {@code key32}. */
    private static byte[] poly1305(byte[] msg, byte[] key32) {
        byte[] rBytes = Arrays.copyOfRange(key32, 0, 16);
        // Clamp r.
        rBytes[3] = (byte) (rBytes[3] & 15);
        rBytes[7] = (byte) (rBytes[7] & 15);
        rBytes[11] = (byte) (rBytes[11] & 15);
        rBytes[15] = (byte) (rBytes[15] & 15);
        rBytes[4] = (byte) (rBytes[4] & 252);
        rBytes[8] = (byte) (rBytes[8] & 252);
        rBytes[12] = (byte) (rBytes[12] & 252);
        BigInteger r = decodeLittleEndian(rBytes);
        BigInteger s = decodeLittleEndian(Arrays.copyOfRange(key32, 16, 32));

        BigInteger acc = BigInteger.ZERO;
        int offset = 0;
        while (offset < msg.length) {
            int blockLen = Math.min(16, msg.length - offset);
            byte[] block = new byte[blockLen + 1];
            System.arraycopy(msg, offset, block, 0, blockLen);
            block[blockLen] = 1; // the appended 1 byte
            BigInteger n = decodeLittleEndian(block);
            acc = acc.add(n).multiply(r).mod(POLY_P);
            offset += blockLen;
        }
        BigInteger tagVal = acc.add(s).mod(BigInteger.ONE.shiftLeft(128));
        byte[] tag = new byte[16];
        for (int i = 0; i < 16; i++) {
            tag[i] = (byte) tagVal.shiftRight(8 * i).intValue();
        }
        return tag;
    }

    // ------------------------------------------------------------------
    // XSalsa20-Poly1305 secretbox (libsodium crypto_secretbox_easy format)
    // ------------------------------------------------------------------

    /**
     * Returns {@code tag(16) || ciphertextBody}, same length as plaintext
     * plus 16. The first 32 bytes of the XSalsa20 stream are the Poly1305
     * key; message encryption starts at stream offset 32.
     */
    public static byte[] secretboxEncrypt(byte[] plaintext, byte[] nonce24, byte[] key32) {
        requireNotNull(plaintext, "plaintext");
        requireLength(nonce24, 24, "nonce24");
        requireLength(key32, 32, "key32");
        byte[] subkey = hsalsa20(key32, Arrays.copyOfRange(nonce24, 0, 16));
        byte[] stream = salsa20Stream(subkey, Arrays.copyOfRange(nonce24, 16, 24), 32 + plaintext.length);
        byte[] polyKey = Arrays.copyOfRange(stream, 0, 32);
        byte[] body = new byte[plaintext.length];
        for (int i = 0; i < plaintext.length; i++) {
            body[i] = (byte) (plaintext[i] ^ stream[32 + i]);
        }
        byte[] tag = poly1305(body, polyKey);
        byte[] out = new byte[16 + body.length];
        System.arraycopy(tag, 0, out, 0, 16);
        System.arraycopy(body, 0, out, 16, body.length);
        return out;
    }

    /**
     * Verifies the tag and returns the plaintext, or {@code null} if the tag
     * does not verify or the box is shorter than 16 bytes.
     */
    public static byte[] secretboxDecrypt(byte[] boxed, byte[] nonce24, byte[] key32) {
        if (boxed == null || boxed.length < 16) {
            return null;
        }
        requireLength(nonce24, 24, "nonce24");
        requireLength(key32, 32, "key32");
        byte[] tag = Arrays.copyOfRange(boxed, 0, 16);
        byte[] body = Arrays.copyOfRange(boxed, 16, boxed.length);
        byte[] subkey = hsalsa20(key32, Arrays.copyOfRange(nonce24, 0, 16));
        byte[] stream = salsa20Stream(subkey, Arrays.copyOfRange(nonce24, 16, 24), 32 + body.length);
        byte[] polyKey = Arrays.copyOfRange(stream, 0, 32);
        byte[] expected = poly1305(body, polyKey);
        if (!MessageDigest.isEqual(tag, expected)) {
            return null;
        }
        byte[] plain = new byte[body.length];
        for (int i = 0; i < body.length; i++) {
            plain[i] = (byte) (body[i] ^ stream[32 + i]);
        }
        return plain;
    }

    // ------------------------------------------------------------------
    // SHA-256 convenience (for .pak hash checks)
    // ------------------------------------------------------------------

    public static byte[] sha256(byte[] input) {
        requireNotNull(input, "input");
        try {
            return MessageDigest.getInstance("SHA-256").digest(input);
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA-256 not available", e);
        }
    }
}
