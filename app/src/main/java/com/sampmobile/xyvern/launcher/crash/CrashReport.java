package com.hopepride.samp.launcher.crash;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Reads a native crash dump or a session log and says what can be said for sure.
 *
 * It reports the signal, the fault address, the faulting symbol and which of the
 * known failure patterns appear in the text. It does not invent a root cause: a
 * single fault address is not proof, so findings are written as observations.
 */
public final class CrashReport {

    private static final Pattern SIGNAL = Pattern.compile("signal:\\s*(0x[0-9a-fA-F]+)");
    private static final Pattern FAULT = Pattern.compile("fault:\\s*(0x[0-9a-fA-F]+)");
    private static final Pattern PC = Pattern.compile("pc-symbol:\\s*(.+)");
    private static final Pattern LR = Pattern.compile("lr-symbol:\\s*(.+)");
    private static final Pattern RLE = Pattern.compile("rle-guard:\\s*(.+)");
    private static final Pattern TEXDB = Pattern.compile("texdb:\\s*(.+)");
    private static final Pattern BUILD = Pattern.compile("build(:|\\s)\\s*(.+)");
    private static final Pattern RLE_UNTRANSLATED = Pattern.compile("RLEDecompress");
    private static final Pattern SORT_ENTRIES = Pattern.compile("SortEntries");
    private static final Pattern UNTRANSLATED_UI = Pattern.compile("UI-UNTRANSLATED:\\s*(.+)");

    public final String signalCode;
    public final String signalName;
    public final String fault;
    public final String pcSymbol;
    public final String lrSymbol;
    public final String rleGuard;
    public final String texdb;
    public final List<String> findings;
    public final List<String> untranslatedUi;
    public final boolean empty;

    private CrashReport(String signalCode, String fault, String pcSymbol, String lrSymbol,
                        String rleGuard, String texdb, List<String> findings, List<String> untranslatedUi) {
        this.signalCode = signalCode;
        this.signalName = signalNameOf(signalCode);
        this.fault = fault;
        this.pcSymbol = pcSymbol;
        this.lrSymbol = lrSymbol;
        this.rleGuard = rleGuard;
        this.texdb = texdb;
        this.findings = Collections.unmodifiableList(findings);
        this.untranslatedUi = Collections.unmodifiableList(untranslatedUi);
        this.empty = pcSymbol == null && lrSymbol == null && findings.isEmpty();
    }

    public static CrashReport parse(String text) {
        List<String> findings = new ArrayList<>();
        List<String> untranslated = new ArrayList<>();
        if (text == null) text = "";

        String signal = match(SIGNAL, text);
        String fault = match(FAULT, text);
        String pc = match(PC, text);
        String lr = match(LR, text);
        String rle = match(RLE, text);
        String texdb = match(TEXDB, text);

        String placement = (pc == null ? "" : pc) + " " + (lr == null ? "" : lr);

        if (SORT_ENTRIES.matcher(placement).find()) {
            findings.add("Crash terjadi di TextureDatabaseRuntime::SortEntries(bool).");
        }
        if (RLE_UNTRANSLATED.matcher(placement).find()) {
            findings.add("Crash terjadi di jalur RLEDecompress (dekompresi tekstur terkompresi).");
        }
        if ("0x58".equalsIgnoreCase(fault)) {
            findings.add("Alamat fault 0x58: pada TextureDatabase, offset itu jatuh di thumbs[2].dataPtr. "
                    + "Akses ke 0x58 berarti dasar pointer-nya nol, jadi objek database yang dipakai sudah tidak valid.");
        }
        if (rle != null && rle.contains("padded=0") && !rle.contains("calls=0")) {
            findings.add("Guard RLE terpasang dan dipanggil, tetapi tidak pernah menambah padding.");
        }
        if (texdb != null && texdb.contains("variant-fallback=1")) {
            findings.add("Build ini masih mengganti varian database tekstur saat runtime "
                    + "(variant-fallback=1); pada build terbaru penggantian varian dimatikan.");
        }
        if (texdb != null && texdb.contains("load-failure=1")) {
            findings.add("Ada database tekstur yang gagal dimuat.");
        }

        Matcher ui = UNTRANSLATED_UI.matcher(text);
        while (ui.find() && untranslated.size() < 40) {
            String label = ui.group(1).trim();
            if (!untranslated.contains(label)) untranslated.add(label);
        }
        if (!untranslated.isEmpty()) {
            findings.add(untranslated.size() + " teks antarmuka belum ada di tabel Bahasa Indonesia "
                    + "(daftarnya ada di bagian bawah laporan).");
        }

        if (findings.isEmpty() && signal == null) {
            findings.add("Tidak ada jejak crash native di berkas ini.");
        }

        return new CrashReport(signal, fault, pc, lr, rle, texdb, findings, untranslated);
    }

    private static String match(Pattern pattern, String text) {
        Matcher matcher = pattern.matcher(text);
        return matcher.find() ? matcher.group(1).trim() : null;
    }

    public static String signalNameOf(String code) {
        if (code == null) return "tidak diketahui";
        switch (code.toLowerCase(java.util.Locale.ROOT)) {
            case "0xb": return "SIGSEGV (akses memori tidak valid)";
            case "0x6": return "SIGABRT (proses dibatalkan)";
            case "0x7": return "SIGBUS (akses bus tidak valid)";
            case "0x4": return "SIGILL (instruksi tidak valid)";
            case "0x8": return "SIGFPE (kesalahan aritmetika)";
            default: return code;
        }
    }

    public String summary() {
        StringBuilder builder = new StringBuilder();
        builder.append("Sinyal: ").append(signalCode == null ? "tidak ada" : signalCode + " — " + signalName).append('\n');
        builder.append("Alamat fault: ").append(fault == null ? "-" : fault).append('\n');
        builder.append("Simbol PC: ").append(pcSymbol == null ? "-" : pcSymbol).append('\n');
        builder.append("Simbol LR: ").append(lrSymbol == null ? "-" : lrSymbol).append('\n');
        if (rleGuard != null) builder.append("RLE guard: ").append(rleGuard).append('\n');
        if (texdb != null) builder.append("Texdb: ").append(texdb).append('\n');

        builder.append("\nTemuan:\n");
        for (String finding : findings) builder.append("• ").append(finding).append('\n');

        if (!untranslatedUi.isEmpty()) {
            builder.append("\nTeks belum diterjemahkan:\n");
            for (String label : untranslatedUi) builder.append("• ").append(label).append('\n');
        }

        return builder.toString();
    }
}
