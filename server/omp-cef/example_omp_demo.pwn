// omp-cef demo minimal untuk PC + Android (launcher ini).
// Butuh: plugin omp-cef di server.cfg, cef.inc upstream di pawno/include/,
// dan folder scriptfiles/cef/cef_demo/ (contoh di folder ini).
// HANYA Overlay2D yang dipakai di demo ini, supaya jalan di Android juga.
// World2D/World3D sengaja tidak dipakai: Android menolaknya dengan
// BrowserCreateResult ERROR_GENERIC.
#include <a_samp>
#include <cef>

#define DEMO_BROWSER_ID 9001
#define DEMO_URL "http://cef/cef_demo/index.html"

public OnGameModeInit()
{
    CEF_AddResource("cef_demo");
    CEF_RegisterEvent("demo_button", "Demo_OnButton", Argument_String);
    return 1;
}

public OnCefInitialize(playerid, bool:success, E_CEF_INIT_REASON:reason, const message[])
{
    if (!success) {
        printf("[omp-demo] CEF init gagal player %d reason %d msg %s", playerid, _:reason, message);
    }
    return 1;
}

public OnCefReady(playerid)
{
    CEF_CreateBrowser(playerid, DEMO_BROWSER_ID, DEMO_URL, true);
    return 1;
}

public OnCefBrowserCreated(playerid, browserid, bool:success, E_CEF_CREATE_STATUS:code, const reason[])
{
    if (!success) {
        printf("[omp-demo] browser gagal player %d id %d code %d %s", playerid, browserid, _:code, reason);
        return 1;
    }
    CEF_EmitEvent(playerid, browserid, "ui_welcome", CEF_STR("Selamat datang di server!"));
    return 1;
}

// JS: window.cef.emit("demo_button", "ok") -> callback ini (daftar di bawah).
forward Demo_OnButton(playerid, browserid, const action[]);
public Demo_OnButton(playerid, browserid, const action[])
{
    SendClientMessage(playerid, -1, "Tombol ditekan dari CEF!");
    CEF_EmitEvent(playerid, browserid, "ui_reply", CEF_STR("diterima server"));
    return 1;
}

