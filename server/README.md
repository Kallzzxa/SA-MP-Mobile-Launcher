# MobileCEF — Server Side

Include Pawn + contoh UI untuk fitur CEF WebView di SA-MP Mobile Launcher.
Protokol kompatibel penuh dengan client (paket RakNet custom, default packet id **252**).

## Isi folder

| File | Keterangan |
|---|---|
| `MobileCEF.inc` | Include server. Copy ke `pawno/include/`, lalu `#include <MobileCEF>` **setelah** `#include <Pawn.RakNet>` |
| `scriptfiles/cef/` | Contoh UI web. Copy ke `scriptfiles/cef/` di server |
| `example_usage.pwn` | Contoh pemakaian di gamemode |

## Kebutuhan server

- Plugin [Pawn.RakNet](https://github.com/katursis/Pawn.RakNet) (wajib, buat kirim paket custom)

## Cara pakai

1. Pasang `MobileCEF.inc` dan `#include <MobileCEF>` di gamemode.
2. Di `OnGameModeInit`: `CefSetPacketId(252);`
3. Taruh UI web kamu di `scriptfiles/cef/`:
   - `index.html` — halaman utama UI
   - `files.txt` — daftar file yang di-sync (satu per baris, opsional; default cuma `index.html`)
   - `version.txt` — versi UI, mis. `1`. **Naikkan setiap kali kamu ubah UI** supaya client download ulang.
4. Alur di gamemode (lihat `example_usage.pwn`):

```pawn
public OnPlayerConnect(playerid)
{
    CefInitBrowser(playerid, "cef://__internal/loading.html");
    CefShowBrowser(playerid);
}

forward OnCefBrowserInit(playerid, success, errorcode);
public OnCefBrowserInit(playerid, success, errorcode)
{
    if (success) CefSyncUI(playerid); // cek cache -> download kalau perlu
}

forward OnCefUIReady(playerid);
public OnCefUIReady(playerid)
{
    CefSendEvent(playerid, "ui_welcome", "Halo!");
}
```

`CefSyncUI` otomatis: kirim versi ke client → client cek cache (`SAMP/cef_ui/.version`)
→ kalau sama, UI langsung tampil dari cache → kalau beda/belum ada, file di-download
chunk-by-chunk (base64), disimpan di HP, lalu UI tampil. Selama download, halaman
`cef://__internal/loading.html` menampilkan progress bar.

## API Pawn

```pawn
CefSetPacketId(252)
CefInitBrowser(playerid, url[])        // url bisa cef://..., http(s)://...
CefSetBrowserUrl(playerid, url[])
CefShowBrowser(playerid) / CefHideBrowser(playerid)
CefDestroyBrowser(playerid)
CefChangeBrowserFocus(playerid, bool:focus)
CefSendEvent(playerid, event[], data[])        // -> window.cef.on(event, cb)
CefRegisterEventCallback(event[], callback[])  // window.cef.emit(event, ...) -> callback(playerid, data[])
CefIsCEFReady(playerid)                        // true kalau client support CEF
CefSyncUI(playerid)                             // sync scriptfiles/cef/ ke client

forward OnCefBrowserInit(playerid, success, errorcode);
forward OnCefUIReady(playerid);
```

## API JavaScript di web UI (`window.cef`, ala omp-cef)

```js
cef.on("ui_welcome", function (text) { /* server -> web */ });
cef.emit("ui_hello", "data1", 123);   // web -> server (data = JSON array string)
cef.set_focus(true);
cef.isChatInputOpen();
cef.exitGame();
```

Catatan: file UI sebaiknya text-based (html/css/js). File biner besar lebih baik
di-load lewat URL http(s) biasa.
