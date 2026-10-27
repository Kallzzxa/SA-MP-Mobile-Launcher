package com.sampmobile.xyvern.cef.omp;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * omp-cef browser logic for Android, ported from the PC client
 * (aurora-mp/omp-cef, MIT) src/client/core/app.cpp packet handling.
 *
 * This class owns no Android objects: the UI layer (CefJavaManager) only
 * has to implement Ui and forward its callbacks here. That keeps this
 * logic testable on a plain JDK.
 *
 * Server commands handled (CefEvent::Server names, sent as EmitEvent
 * packets with typed arguments):
 * - CreateBrowser: args [url String, focused Bool, controlsChat Bool,
 *   width Float, height Float] -> Ui.createBrowser
 * - CreateWorldBrowser / CreateWorld2DBrowser: NOT supported by an
 *   Android WebView (no DX9 world rendering / world-to-screen projection
 *   in the launcher). They get a BrowserCreateResult ERROR_GENERIC with
 *   reason "World browsers are not supported on Android", instead of
 *   silently becoming a misplaced overlay.
 * - SetWorld2DBrowserPos, AttachBrowserToObject, DetachBrowserFromObject:
 *   ignored (no world browser exists to move).
 * - SetBrowserVisible, SetBrowserLayer, DestroyBrowser, ReloadBrowser,
 *   FocusBrowser, LoadUrl -> Ui methods.
 * - EnableDevTools: ignored (WebView remote debugging is not exposed).
 * - MuteBrowser / SetAudioMode / SetAudioSettings: recorded, audio in a
 *   WebView is always 2D UI audio; no spatialisation is attempted.
 * - ToggleHudComponent / ToggleSpawnScreen / ClearChat / ToggleChatInput /
 *   SetKeyCapture / EnableKey / SetEscapeMenuMode / SetPlayerListMode:
 *   recorded in state; only ExitGame and chat-input state have a real
 *   Android effect, via Ui.
 * - Any other EmitEvent name: forwarded to the target browser's JS
 *   handlers as a user event (window.cef.on(name, ...)).
 *
 * Creates and emits received before resources finish downloading are
 * queued and flushed when DownloadComplete is sent, exactly like the PC
 * client (pending_creates_ / pending_emits_ in app.cpp).
 */
public final class OmpApp {
    public interface Ui {
        void createBrowser(int browserId, String url, boolean focused, boolean controlsChat, float width, float height);
        void destroyBrowser(int browserId);
        void setBrowserVisible(int browserId, boolean visible);
        void setBrowserLayer(int browserId, int layer);
        void setBrowserFocus(int browserId, boolean focused);
        void loadBrowserUrl(int browserId, String url);
        void reloadBrowser(int browserId, boolean ignoreCache);
        void emitToBrowser(int browserId, String name, List<OmpPacket.Argument> args);
        void exitGame();
        void setChatInputOpen(boolean open);
    }

    public static final class Browser {
        public final int id;
        public String url;
        public boolean focused;
        public boolean controlsChat = true;
        public float width;
        public float height;
        public boolean visible = true;
        public int layer;
        public boolean muted;
        public int audioMode; // 0 world, 1 ui (recorded only)

        Browser(int id, String url, boolean focused, boolean controlsChat, float width, float height) {
            this.id = id;
            this.url = url;
            this.focused = focused;
            this.controlsChat = controlsChat;
            this.width = width;
            this.height = height;
        }
    }

    private final Ui ui;
    private final Map<Integer, Browser> browsers = new HashMap<>();
    private final List<OmpPacket.Packet> pendingCreates = new ArrayList<>();
    private final List<OmpPacket.Packet> pendingEmits = new ArrayList<>();
    private boolean resourcesReady;
    private boolean downloadCompleteSent;
    private boolean chatInputOpen;
    private int escapeMenuMode;
    private int playerListMode;

    public OmpApp(Ui ui) {
        if (ui == null) throw new IllegalArgumentException("ui");
        this.ui = ui;
    }

    public Map<Integer, Browser> browsers() {
        return Collections.unmodifiableMap(browsers);
    }

    public boolean isChatInputOpen() {
        return chatInputOpen;
    }

    /** Called when the resource download finishes. Flushes queued creates/emits. */
    public Runnable onResourcesComplete() {
        resourcesReady = true;
        List<OmpPacket.Packet> creates = new ArrayList<>(pendingCreates);
        List<OmpPacket.Packet> emits = new ArrayList<>(pendingEmits);
        pendingCreates.clear();
        pendingEmits.clear();
        return () -> {
            for (OmpPacket.Packet p : creates) handleServerCommand(p, true);
            for (OmpPacket.Packet p : emits) handleServerCommand(p, true);
        };
    }

    public boolean needsDownloadComplete() {
        return resourcesReady && !downloadCompleteSent;
    }

    public void markDownloadCompleteSent() {
        downloadCompleteSent = true;
    }

    public void reset() {
        for (Integer id : new ArrayList<>(browsers.keySet())) {
            ui.destroyBrowser(id);
        }
        browsers.clear();
        pendingCreates.clear();
        pendingEmits.clear();
        resourcesReady = false;
        downloadCompleteSent = false;
        chatInputOpen = false;
    }

    /** Handle one EmitEvent packet from the server. Returns a ClientEmitEvent to send back, or null. */
    public OmpPacket.Packet handleEmit(OmpPacket.Packet p) {
        if (!resourcesReady && isServerCommand(p.eventName)) {
            // PC client queues creates/emits until resources are ready.
            if ("CreateBrowser".equals(p.eventName)) pendingCreates.add(p);
            else pendingEmits.add(p);
            return null;
        }
        return handleServerCommand(p, false);
    }

    private boolean isServerCommand(String name) {
        switch (name) {
            case "CreateBrowser":
            case "CreateWorldBrowser":
            case "CreateWorld2DBrowser":
            case "SetWorld2DBrowserPos":
            case "SetBrowserVisible":
            case "SetBrowserLayer":
            case "DestroyBrowser":
            case "ReloadBrowser":
            case "FocusBrowser":
            case "LoadUrl":
            case "EnableDevTools":
            case "AttachBrowserToObject":
            case "DetachBrowserFromObject":
            case "MuteBrowser":
            case "SetAudioMode":
            case "SetAudioSettings":
            case "ToggleHudComponent":
            case "ToggleSpawnScreen":
            case "ClearChat":
            case "ToggleChatInput":
            case "SetKeyCapture":
            case "EnableKey":
            case "ExitGame":
            case "SetEscapeMenuMode":
            case "SetPlayerListMode":
                return true;
            default:
                return false;
        }
    }

    private OmpPacket.Packet browserCreateResult(int browserId, boolean success, int code, String reason) {
        OmpPacket.Packet out = new OmpPacket.Packet();
        out.type = OmpPacket.TYPE_CLIENT_EMIT_EVENT;
        out.browserId = browserId;
        out.eventName = "BrowserCreateResult";
        out.args.add(OmpPacket.Argument.bool(success));
        out.args.add(OmpPacket.Argument.integer(code));
        out.args.add(OmpPacket.Argument.str(reason));
        return out;
    }

    private String argString(OmpPacket.Packet p, int i, String def) {
        if (i >= p.args.size() || p.args.get(i).type != OmpPacket.ARG_STRING) return def;
        return p.args.get(i).stringValue;
    }

    private boolean argBool(OmpPacket.Packet p, int i, boolean def) {
        if (i >= p.args.size() || p.args.get(i).type != OmpPacket.ARG_BOOL) return def;
        return p.args.get(i).boolValue;
    }

    private int argInt(OmpPacket.Packet p, int i, int def) {
        if (i >= p.args.size() || p.args.get(i).type != OmpPacket.ARG_INTEGER) return def;
        return p.args.get(i).intValue;
    }

    private float argFloat(OmpPacket.Packet p, int i, float def) {
        if (i >= p.args.size()) return def;
        OmpPacket.Argument a = p.args.get(i);
        if (a.type == OmpPacket.ARG_FLOAT) return a.floatValue;
        if (a.type == OmpPacket.ARG_INTEGER) return a.intValue;
        return def;
    }

    private OmpPacket.Packet handleServerCommand(OmpPacket.Packet p, boolean flushed) {
        String name = p.eventName;
        int id = p.browserId;
        switch (name) {
            case "CreateBrowser": {
                String url = argString(p, 0, "");
                boolean focused = argBool(p, 1, false);
                boolean controlsChat = argBool(p, 2, true);
                float w = argFloat(p, 3, 0);
                float h = argFloat(p, 4, 0);
                if (browsers.containsKey(id)) {
                    return browserCreateResult(id, false, 2, "ID already in use");
                }
                browsers.put(id, new Browser(id, url, focused, controlsChat, w, h));
                ui.createBrowser(id, url, focused, controlsChat, w, h);
                return browserCreateResult(id, true, 0, "Successfully created");
            }
            case "CreateWorldBrowser":
            case "CreateWorld2DBrowser":
                return browserCreateResult(id, false, 1, "World browsers are not supported on Android");
            case "SetBrowserVisible": {
                Browser b = browsers.get(id);
                boolean visible = argBool(p, 0, true);
                if (b != null) b.visible = visible;
                ui.setBrowserVisible(id, visible);
                return null;
            }
            case "SetBrowserLayer": {
                Browser b = browsers.get(id);
                int layer = argInt(p, 0, 0);
                if (b != null) b.layer = layer;
                ui.setBrowserLayer(id, layer);
                return null;
            }
            case "DestroyBrowser":
                browsers.remove(id);
                ui.destroyBrowser(id);
                return null;
            case "ReloadBrowser":
                ui.reloadBrowser(id, argBool(p, 0, false));
                return null;
            case "FocusBrowser": {
                boolean focused = argBool(p, 0, false);
                Browser b = browsers.get(id);
                if (b != null) b.focused = focused;
                ui.setBrowserFocus(id, focused);
                return null;
            }
            case "LoadUrl": {
                String url = argString(p, 0, "");
                Browser b = browsers.get(id);
                if (b != null) b.url = url;
                ui.loadBrowserUrl(id, url);
                return null;
            }
            case "MuteBrowser": {
                Browser b = browsers.get(id);
                if (b != null) b.muted = argBool(p, 0, false);
                return null;
            }
            case "SetAudioMode": {
                Browser b = browsers.get(id);
                if (b != null) b.audioMode = argInt(p, 0, 1);
                return null;
            }
            case "SetAudioSettings":
            case "SetWorld2DBrowserPos":
            case "AttachBrowserToObject":
            case "DetachBrowserFromObject":
            case "EnableDevTools":
            case "ToggleHudComponent":
            case "ToggleSpawnScreen":
            case "ClearChat":
                return null;
            case "ToggleChatInput": {
                boolean enabled = argBool(p, 0, true);
                if (!enabled) {
                    chatInputOpen = false;
                    ui.setChatInputOpen(false);
                }
                return null;
            }
            case "SetKeyCapture":
            case "EnableKey":
                return null;
            case "ExitGame":
                ui.exitGame();
                return null;
            case "SetEscapeMenuMode":
                escapeMenuMode = argInt(p, 0, 0);
                return null;
            case "SetPlayerListMode":
                playerListMode = argInt(p, 0, 0);
                return null;
            default:
                // User event -> browser JS handlers.
                ui.emitToBrowser(id, name, p.args);
                return null;
        }
    }

    /** Client-side JS event (window.cef.emit) -> ClientEmitEvent packet to send. */
    public OmpPacket.Packet clientEvent(int browserId, String name, List<OmpPacket.Argument> args) {
        OmpPacket.Packet out = new OmpPacket.Packet();
        out.type = OmpPacket.TYPE_CLIENT_EMIT_EVENT;
        out.browserId = browserId;
        out.eventName = name;
        out.args.addAll(args);
        return out;
    }
}
