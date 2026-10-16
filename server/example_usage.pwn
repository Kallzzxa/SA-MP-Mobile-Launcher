// ============================================================================
// MobileCEF - contoh pemakaian di gamemode
// ============================================================================
// 1. Copy MobileCEF.inc ke pawno/include/
// 2. Copy folder server/scriptfiles/cef/ ke scriptfiles/cef/ di server kamu
// 3. Pastikan plugin Pawn.RakNet terpasang di server.cfg
// ============================================================================

#include <a_samp>
#include <Pawn.RakNet>
#include <MobileCEF>

public OnGameModeInit()
{
    CefSetPacketId(252); // harus sama dengan default client (252)

    // Daftarkan callback web -> server (dipicu oleh cef.emit(...) di UI)
    CefRegisterEventCallback("ui_hello", "OnUiHello");
    CefRegisterEventCallback("ui_close", "OnUiClose");
    return 1;
}

public OnPlayerConnect(playerid)
{
    // Tampilkan halaman loading dulu, sync UI jalan setelah browser siap.
    CefInitBrowser(playerid, "cef://__internal/loading.html");
    CefShowBrowser(playerid);
    return 1;
}

forward OnCefBrowserInit(playerid, success, errorcode);
public OnCefBrowserInit(playerid, success, errorcode)
{
    if (!success)
    {
        printf("[CEF] browser init gagal untuk player %d (error %d)", playerid, errorcode);
        return 1;
    }
    // Cek cache client dulu: kalau versi sama -> langsung tampil,
    // kalau belum ada / beda versi -> download dulu otomatis.
    CefSyncUI(playerid);
    return 1;
}

forward OnCefUIReady(playerid);
public OnCefUIReady(playerid)
{
    // cef://index.html sudah tampil dari cache client.
    CefSendEvent(playerid, "ui_welcome", "Halo dari server! UI dimuat dari cache.");
    return 1;
}

// --- web -> server : dipicu oleh cef.emit("ui_hello", ...) di index.html ---
forward OnUiHello(playerid, const data[]);
public OnUiHello(playerid, const data[])
{
    // data = JSON array string, mis. ["Halo juga dari HP!"]
    printf("[CEF] player %d: %s", playerid, data);
    CefSendEvent(playerid, "ui_welcome", "Server menerima sapaanmu!");
    return 1;
}

forward OnUiClose(playerid, const data[]);
public OnUiClose(playerid, const data[])
{
    CefHideBrowser(playerid);
    return 1;
}

