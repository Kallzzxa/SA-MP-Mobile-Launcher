/*
    MobileCEF.inc - Server-side CEF (WebView) include for SA-MP Mobile Launcher
    ============================================================================

    Compatible with the SA-MP Mobile Launcher client protocol
    (custom RakNet packet, default packet id 252).

    Requirements:
        - Pawn.RakNet  https://github.com/katursis/Pawn.RakNet

    Install:
        1. Copy this file to pawno/include/
        2. #include <MobileCEF> AFTER <Pawn.RakNet> in your gamemode
        3. In OnGameModeInit: CefSetPacketId(252);
        4. Put your CEF web UI in scriptfiles/cef/  (index.html + assets)
           - scriptfiles/cef/version.txt  -> UI version, e.g. "1" (bump it every
             time you change the UI so clients re-download)
           - scriptfiles/cef/files.txt    -> list of files to sync, one per line
             (if missing, only index.html is synced)

    Typical flow (see server/example_usage.pwn):
        OnPlayerConnect:
            CefInitBrowser(playerid, "cef://__internal/loading.html");
        forward OnCefBrowserInit(playerid, success, errorcode):
            if (success) CefSyncUI(playerid);   // checks client cache first:
                                               // cached  -> show UI immediately
                                               // missing -> download, then show
        forward OnCefUIReady(playerid):
            // cef://index.html is now loaded from the client cache

    Web UI JavaScript API (window.cef, omp-cef style):
        cef.on("event_name", function(arg1, arg2) { ... });  // server -> web
        cef.emit("event_name", arg1, arg2);                  // web -> server
        cef.set_focus(true/false);
        cef.isChatInputOpen();
        cef.exitGame();

    Server Pawn API:
        CefSendEvent(playerid, "event_name", "data");       // -> cef.on(...)
        CefRegisterEventCallback("event_name", "OnMyEvent"); // cef.emit(...) ->
                                                             // forward OnMyEvent(playerid, data[])

    Copyright (c) 2026 - protocol compatible with SA:MP Mobile CEF clients.
*/

#if defined _MOBILECEF_INC_
    #endinput
#endif
#define _MOBILECEF_INC_

#if !defined PAWNRAKNET_INC_
    #error "[MobileCEF] Pawn.RakNet is required: https://github.com/katursis/Pawn.RakNet"
#endif

// ---------------------------------------------------------------------------
// Protocol (must match the mobile client)
// ---------------------------------------------------------------------------
#define CEF_DEFAULT_PACKET_ID       (252)

enum e_CefRpc
{
    RPC_InitBrowser = 1,
    RPC_DestroyBrowser,
    RPC_ShowBrowser,
    RPC_HideBrowser,
    RPC_SetBrowserUrl,
    RPC_ChangeBrowserFocus,
    RPC_ServerEvent,

    RPC_LibraryInit = 8,
    RPC_BrowserInit,
    RPC_ClientEvent
};

#define CEF_MAX_EVENT_NAME_LEN      (48)
#define CEF_MAX_EVENT_DATA_LEN      (4000)   // client caps data < 4096
#define CEF_MAX_CALLBACKS           (32)
#define CEF_MAX_UI_FILES            (16)
#define CEF_UI_CHUNK_RAW            (1536)   // -> 2048 base64 chars per chunk
#define CEF_UI_CHUNKS_PER_TICK      (6)

// strcpy/strncmp bukan native SA-MP: sediakan versi lokal.
stock Cef_StrCpy(dest[], const src[], len = sizeof dest)
{
    dest[0] = EOS;
    strcat(dest, src, len);
    return 1;
}

static stock
    cef_packet_id = CEF_DEFAULT_PACKET_ID,
    bool:cef_player_ready[MAX_PLAYERS char],
    cef_cb_count = 0,
    cef_cb_event[CEF_MAX_CALLBACKS][CEF_MAX_EVENT_NAME_LEN],
    cef_cb_func[CEF_MAX_CALLBACKS][64],
    bool:cef_ui_inited = false;

// UI sync state per player
static stock
    cefUiFileCount[MAX_PLAYERS],
    cefUiFile[MAX_PLAYERS][CEF_MAX_UI_FILES][64],
    cefUiIdx[MAX_PLAYERS],
    bool:cefUiSending[MAX_PLAYERS],
    bool:cefUiReady[MAX_PLAYERS],
    cefUiVer[MAX_PLAYERS][32],
    File:cefUiHandle[MAX_PLAYERS],
    cefUiTimer = -1;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
stock Cef_SetPacketId(packetid = CEF_DEFAULT_PACKET_ID)
{
    cef_packet_id = packetid;
    printf("[MobileCEF] packet id: %d", cef_packet_id);
    return 1;
}
#define CefSetPacketId Cef_SetPacketId

static stock BitStream:Cef_BitStream(e_CefRpc:rpcid)
{
    new BitStream:bs = BS_New();
    BS_WriteValue(bs, PR_UINT16, cef_packet_id, PR_UINT32, _:rpcid);
    return bs;
}

static stock Cef_SendString(BitStream:bs, const str[])
{
    new len = strlen(str);
    BS_WriteValue(bs, PR_UINT16, len);
    if (len > 0) BS_WriteValue(bs, PR_STRING, str, len);
}

// ---------------------------------------------------------------------------
// Browser control
// ---------------------------------------------------------------------------
stock CefInitBrowser(playerid, const url[])
{
    new BitStream:bs = Cef_BitStream(RPC_InitBrowser);
    Cef_SendString(bs, url);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

stock CefDestroyBrowser(playerid)
{
    new BitStream:bs = Cef_BitStream(RPC_DestroyBrowser);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

stock CefShowBrowser(playerid)
{
    new BitStream:bs = Cef_BitStream(RPC_ShowBrowser);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

stock CefHideBrowser(playerid)
{
    new BitStream:bs = Cef_BitStream(RPC_HideBrowser);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

stock CefSetBrowserUrl(playerid, const url[])
{
    new BitStream:bs = Cef_BitStream(RPC_SetBrowserUrl);
    Cef_SendString(bs, url);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

stock CefChangeBrowserFocus(playerid, bool:focus)
{
    new BitStream:bs = Cef_BitStream(RPC_ChangeBrowserFocus);
    BS_WriteValue(bs, PR_BOOL, focus);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

// ---------------------------------------------------------------------------
// Events  (server -> web hits window.cef.on(...))
// ---------------------------------------------------------------------------
stock CefSendEvent(playerid, const event_name[], const event_data[])
{
    if (!(0 <= playerid < MAX_PLAYERS) || !IsPlayerConnected(playerid)) return 0;
    if (strlen(event_name) >= CEF_MAX_EVENT_NAME_LEN || strlen(event_data) >= CEF_MAX_EVENT_DATA_LEN)
    {
        printf("[MobileCEF] event too long, dropped: %s", event_name);
        return 0;
    }
    new BitStream:bs = Cef_BitStream(RPC_ServerEvent);
    Cef_SendString(bs, event_name);
    Cef_SendString(bs, event_data);
    PR_SendPacket(bs, playerid);
    BS_Delete(bs);
    return 1;
}

// ---------------------------------------------------------------------------
// Events  (web -> server via window.cef.emit(...))
// ---------------------------------------------------------------------------
stock CefRegisterEventCallback(const event_name[], const callback[])
{
    // __cefui_* is reserved for the built-in UI sync, do not override it
    if (!strcmp(event_name, "__cefui_", true, 8)) return 0;
    for (new i = 0; i < cef_cb_count; i++)
    {
        if (!strcmp(cef_cb_event[i], event_name, true))
        {
            Cef_StrCpy(cef_cb_func[i], callback, 64);
            return 1;
        }
    }
    if (cef_cb_count >= CEF_MAX_CALLBACKS) return 0;
    Cef_StrCpy(cef_cb_event[cef_cb_count], event_name, CEF_MAX_EVENT_NAME_LEN);
    Cef_StrCpy(cef_cb_func[cef_cb_count], callback, 64);
    cef_cb_count++;
    return 1;
}

stock bool:CefIsCEFReady(playerid)
{
    if (!(0 <= playerid < MAX_PLAYERS)) return false;
    return cef_player_ready{playerid};
}

// ---------------------------------------------------------------------------
// Base64 (for UI file transfer)
// ---------------------------------------------------------------------------
static stock Cef_Base64Encode(const data[], len, out[], outsize)
{
    static const b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    new o = 0;
    for (new i = 0; i < len; i += 3)
    {
        new b0 = data[i] & 0xFF,
            b1 = (i + 1 < len) ? (data[i + 1] & 0xFF) : 0,
            b2 = (i + 2 < len) ? (data[i + 2] & 0xFF) : 0,
            n = (b0 << 16) | (b1 << 8) | b2;
        if (o + 5 >= outsize) break;
        out[o++] = b64[(n >> 18) & 63];
        out[o++] = b64[(n >> 12) & 63];
        out[o++] = (i + 1 < len) ? b64[(n >> 6) & 63] : '=';
        out[o++] = (i + 2 < len) ? b64[n & 63] : '=';
    }
    out[o] = EOS;
    return o;
}

// ---------------------------------------------------------------------------
// Server-hosted CEF UI  (scriptfiles/cef/)
// ---------------------------------------------------------------------------
static stock Cef_UIReadVersion(out[], outsize = sizeof out)
{
    Cef_StrCpy(out, "1", outsize);
    if (!fexist("scriptfiles/cef/version.txt")) return 0;
    new File:f = fopen("scriptfiles/cef/version.txt", io_read);
    if (!f) return 0;
    new line[64];
    if (fread(f, line))
    {
        // trim whitespace
        new s = 0, e = strlen(line);
        while (line[s] == ' ' || line[s] == '\t' || line[s] == '\r' || line[s] == '\n') s++;
        while (e > s && (line[e - 1] == ' ' || line[e - 1] == '\t' || line[e - 1] == '\r' || line[e - 1] == '\n')) e--;
        line[e] = EOS;
        if (e > s) Cef_StrCpy(out, line[s], outsize);
    }
    fclose(f);
    return 1;
}

static stock Cef_UIReadManifest(playerid)
{
    cefUiFileCount[playerid] = 0;
    if (fexist("scriptfiles/cef/files.txt"))
    {
        new File:f = fopen("scriptfiles/cef/files.txt", io_read);
        if (f)
        {
            new line[96];
            while (fread(f, line) && cefUiFileCount[playerid] < CEF_MAX_UI_FILES)
            {
                new s = 0, e = strlen(line);
                while (line[s] == ' ' || line[s] == '\t' || line[s] == '\r' || line[s] == '\n') s++;
                while (e > s && (line[e - 1] == ' ' || line[e - 1] == '\t' || line[e - 1] == '\r' || line[e - 1] == '\n' || line[e - 1] == ';')) e--;
                line[e] = EOS;
                if (e <= s || line[s] == '#' || line[s] == '/') continue; // skip empty/comments
                // sanitize: no quotes, no traversal
                new clean[64], c = 0;
                for (new i = s; i < e && c < 63; i++)
                {
                    if (line[i] == '"' || line[i] == '\'') continue;
                    clean[c++] = line[i];
                }
                clean[c] = EOS;
                if (c == 0 || strfind(clean, "..") != -1) continue;
                Cef_StrCpy(cefUiFile[playerid][cefUiFileCount[playerid]], clean, 64);
                cefUiFileCount[playerid]++;
            }
            fclose(f);
        }
    }
    if (cefUiFileCount[playerid] == 0)
    {
        Cef_StrCpy(cefUiFile[playerid][0], "index.html", 64);
        cefUiFileCount[playerid] = 1;
    }
}

/**
 * Sync the server UI to the player.
 * The client checks its cache first: if the version matches, the UI is shown
 * immediately; otherwise the files are downloaded, cached on the device, and
 * then shown. forward OnCefUIReady(playerid) fires when done.
 */
stock CefSyncUI(playerid)
{
    if (!(0 <= playerid < MAX_PLAYERS) || !IsPlayerConnected(playerid)) return 0;
    new ver[32];
    Cef_UIReadVersion(ver);
    if (cefUiReady[playerid] && !strcmp(cefUiVer[playerid], ver))
    {
        // already have this version cached
        CefSetBrowserUrl(playerid, "cef://index.html");
        CallLocalFunction("OnCefUIReady", "d", playerid);
        return 1;
    }
    Cef_UIRegisterInternal();
    Cef_UIReadManifest(playerid);
    Cef_StrCpy(cefUiVer[playerid], ver, 32);
    cefUiIdx[playerid] = 0;
    cefUiSending[playerid] = false;
    cefUiReady[playerid] = false;
    if (cefUiHandle[playerid] != File:-1) { fclose(cefUiHandle[playerid]); cefUiHandle[playerid] = File:-1; }
    CefSendEvent(playerid, "__cefui_version", ver);
    return 1;
}

// --- internal: __cefui_* callbacks bypass the reservation guard
static stock Cef_UIRegisterInternal()
{
    if (cef_ui_inited) return 1;
    cef_ui_inited = true;
    for (new i = 0; i < 3; i++)
    {
        new const ev[][] = {"__cefui_uptodate", "__cefui_needfiles", "__cefui_ready"};
        new const fn[][] = {"Cef_UIOnUpToDate", "Cef_UIOnNeedFiles", "Cef_UIOnReady"};
        new exists = 0;
        for (new j = 0; j < cef_cb_count; j++) if (!strcmp(cef_cb_event[j], ev[i], true)) { exists = 1; break; }
        if (!exists && cef_cb_count < CEF_MAX_CALLBACKS)
        {
            Cef_StrCpy(cef_cb_event[cef_cb_count], ev[i], CEF_MAX_EVENT_NAME_LEN);
            Cef_StrCpy(cef_cb_func[cef_cb_count], fn[i], 64);
            cef_cb_count++;
        }
    }
    return 1;
}

forward Cef_UIOnUpToDate(playerid, const data[]);
public Cef_UIOnUpToDate(playerid, const data[])
{
    // client cache is fresh -> show UI
    cefUiReady[playerid] = true;
    CefSetBrowserUrl(playerid, "cef://index.html");
    CallLocalFunction("OnCefUIReady", "d", playerid);
    return 1;
}

forward Cef_UIOnNeedFiles(playerid, const data[]);
public Cef_UIOnNeedFiles(playerid, const data[])
{
    // client needs the files -> start chunked transfer
    cefUiSending[playerid] = true;
    cefUiIdx[playerid] = 0;
    Cef_UIBeginFile(playerid);
    if (cefUiTimer == -1) cefUiTimer = SetTimer("Cef_UITick", 100, true);
    return 1;
}

forward Cef_UIOnReady(playerid, const data[]);
public Cef_UIOnReady(playerid, const data[])
{
    // client finished caching -> show UI
    cefUiReady[playerid] = true;
    cefUiSending[playerid] = false;
    CefSetBrowserUrl(playerid, "cef://index.html");
    CallLocalFunction("OnCefUIReady", "d", playerid);
    return 1;
}

static stock Cef_UIBeginFile(playerid)
{
    new idx = cefUiIdx[playerid];
    if (idx >= cefUiFileCount[playerid]) return 0;
    new path[160];
    format(path, sizeof path, "scriptfiles/cef/%s", cefUiFile[playerid][idx]);
    if (cefUiHandle[playerid] != File:-1) { fclose(cefUiHandle[playerid]); cefUiHandle[playerid] = File:-1; }
    if (!fexist(path))
    {
        printf("[MobileCEF] UI file missing, skipped: %s", path);
        cefUiIdx[playerid]++;
        return Cef_UIBeginFile(playerid);
    }
    new File:f = fopen(path, io_read);
    if (!f)
    {
        printf("[MobileCEF] cannot open UI file: %s", path);
        cefUiIdx[playerid]++;
        return Cef_UIBeginFile(playerid);
    }
    cefUiHandle[playerid] = f;
    new size = flength(f);
    new js[128];
    format(js, sizeof js, "[\"%s\",%d]", cefUiFile[playerid][idx], size);
    CefSendEvent(playerid, "__cefui_begin", js);
    return 1;
}

static stock Cef_UISendChunk(playerid)
{
    if (cefUiHandle[playerid] == File:-1)
    {
        // move to next file / finish
        new idx = cefUiIdx[playerid];
        if (idx < cefUiFileCount[playerid])
            CefSendEvent(playerid, "__cefui_end", cefUiFile[playerid][idx]);
        cefUiIdx[playerid]++;
        if (cefUiIdx[playerid] >= cefUiFileCount[playerid])
        {
            cefUiSending[playerid] = false;
            CefSendEvent(playerid, "__cefui_done", cefUiVer[playerid]);
        }
        else Cef_UIBeginFile(playerid);
        return 1;
    }
    new data[CEF_UI_CHUNK_RAW];
    new got = fblockread(cefUiHandle[playerid], data, sizeof data);
    if (got <= 0)
    {
        fclose(cefUiHandle[playerid]);
        cefUiHandle[playerid] = File:-1;
        return Cef_UISendChunk(playerid); // recurse once to advance
    }
    new b64[CEF_UI_CHUNK_RAW * 4 / 3 + 8];
    Cef_Base64Encode(data, got, b64, sizeof b64);
    CefSendEvent(playerid, "__cefui_chunk", b64);
    return 1;
}

forward Cef_UITick();
public Cef_UITick()
{
    new active = 0;
    for (new p = 0; p < MAX_PLAYERS; p++)
    {
        if (!cefUiSending[p] || !IsPlayerConnected(p)) continue;
        active = 1;
        for (new c = 0; c < CEF_UI_CHUNKS_PER_TICK && cefUiSending[p]; c++)
            Cef_UISendChunk(p);
    }
    if (!active && cefUiTimer != -1)
    {
        KillTimer(cefUiTimer);
        cefUiTimer = -1;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------
public OnIncomingPacket(playerid, packetid, BitStream:bs)
{
    if (packetid == cef_packet_id)
    {
        new e_CefRpc:rpcid;
        BS_IgnoreBits(bs, 16);
        BS_ReadValue(bs, PR_UINT32, _:rpcid);

        switch (rpcid)
        {
            case RPC_LibraryInit:
            {
                cef_player_ready{playerid} = true;
            }
            case RPC_BrowserInit:
            {
                new success, errorcode;
                BS_ReadValue(bs, PR_BOOL, success);
                BS_ReadValue(bs, PR_INT16, errorcode);
                CallLocalFunction("OnCefBrowserInit", "ddd", playerid, success, errorcode);
            }
            case RPC_ClientEvent:
            {
                // event name (cap + skip overflow so the stream stays aligned)
                new namelen;
                BS_ReadValue(bs, PR_UINT16, namelen);
                new ev[CEF_MAX_EVENT_NAME_LEN], nread = namelen;
                if (nread >= CEF_MAX_EVENT_NAME_LEN) nread = CEF_MAX_EVENT_NAME_LEN - 1;
                if (nread > 0) BS_ReadValue(bs, PR_STRING, ev, nread);
                ev[nread] = EOS;
                if (namelen > nread) BS_IgnoreBits(bs, (namelen - nread) * 8);

                // event data (same treatment)
                new datalen;
                BS_ReadValue(bs, PR_UINT16, datalen);
                new data[CEF_MAX_EVENT_DATA_LEN], dread = datalen;
                if (dread >= CEF_MAX_EVENT_DATA_LEN) dread = CEF_MAX_EVENT_DATA_LEN - 1;
                if (dread > 0) BS_ReadValue(bs, PR_STRING, data, dread);
                data[dread] = EOS;
                if (datalen > dread) BS_IgnoreBits(bs, (datalen - dread) * 8);

                for (new i = 0; i < cef_cb_count; i++)
                {
                    if (!strcmp(cef_cb_event[i], ev, true))
                    {
                        CallLocalFunction(cef_cb_func[i], "is", playerid, data);
                        break;
                    }
                }
            }
        }
    }

    #if defined Cef_OnIncomingPacket
        return Cef_OnIncomingPacket(playerid, packetid, bs);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnIncomingPacket
    #undef OnIncomingPacket
#else
    #define _ALS_OnIncomingPacket
#endif
#define OnIncomingPacket Cef_OnIncomingPacket
#if defined Cef_OnIncomingPacket
    forward Cef_OnIncomingPacket(playerid, packetid, BitStream:bs);
#endif

public OnPlayerConnect(playerid)
{
    cef_player_ready{playerid} = false;
    cefUiReady[playerid] = false;
    cefUiSending[playerid] = false;
    cefUiHandle[playerid] = File:-1;
    #if defined Cef_OnPlayerConnect
        return Cef_OnPlayerConnect(playerid);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerConnect
    #undef OnPlayerConnect
#else
    #define _ALS_OnPlayerConnect
#endif
#define OnPlayerConnect Cef_OnPlayerConnect
#if defined Cef_OnPlayerConnect
    forward Cef_OnPlayerConnect(playerid);
#endif

public OnPlayerDisconnect(playerid, reason)
{
    cef_player_ready{playerid} = false;
    cefUiReady[playerid] = false;
    cefUiSending[playerid] = false;
    if (cefUiHandle[playerid] != File:-1) { fclose(cefUiHandle[playerid]); cefUiHandle[playerid] = File:-1; }
    #if defined Cef_OnPlayerDisconnect
        return Cef_OnPlayerDisconnect(playerid, reason);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerDisconnect
    #undef OnPlayerDisconnect
#else
    #define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect Cef_OnPlayerDisconnect
#if defined Cef_OnPlayerDisconnect
    forward Cef_OnPlayerDisconnect(playerid, reason);
#endif

// ---------------------------------------------------------------------------
// Forwards for the gamemode
// ---------------------------------------------------------------------------
forward OnCefBrowserInit(playerid, success, errorcode);
forward OnCefUIReady(playerid);
