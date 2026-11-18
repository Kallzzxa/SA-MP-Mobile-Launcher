#include "../main.h"
#include "game.h"
#include "util.h"
#include "RW/RenderWare.h"
#include "../gui/gui.h"
#include "../net/dl/dlmanager.h"
#include "../vendor/shadowhook/patch.h"
#include <map>
#include <set>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <cstdio>

extern UI* pUI;
extern CSnapShotHelper* pSnapShotHelper;

extern uintptr_t TextDrawTexture[];


CTextDraw::CTextDraw(TEXT_DRAW_TRANSMIT* pTextDrawTransmit, const char* szText)
{
    memset(&m_TextDrawData, 0, sizeof(TEXT_DRAW_DATA));

    m_TextDrawData.fLetterWidth = pTextDrawTransmit->fLetterWidth;
    m_TextDrawData.fLetterHeight = pTextDrawTransmit->fLetterHeight;

    m_TextDrawData.dwLetterColor = pTextDrawTransmit->dwLetterColor;
    m_TextDrawData.byteUnk12 = 0;
    m_TextDrawData.byteCentered = pTextDrawTransmit->byteCenter;
    m_TextDrawData.byteBox = pTextDrawTransmit->byteBox;

    m_TextDrawData.fLineWidth = pTextDrawTransmit->fLineWidth;
    m_TextDrawData.fLineHeight = pTextDrawTransmit->fLineHeight;

    m_TextDrawData.dwBoxColor = pTextDrawTransmit->dwBoxColor;
    m_TextDrawData.byteProportional = pTextDrawTransmit->byteProportional;
    m_TextDrawData.dwBackgroundColor = pTextDrawTransmit->dwBackgroundColor;
    m_TextDrawData.byteShadow = pTextDrawTransmit->byteShadow;
    m_TextDrawData.byteOutline = pTextDrawTransmit->byteOutline;
    m_TextDrawData.byteAlignLeft = pTextDrawTransmit->byteLeft;
    m_TextDrawData.byteAlignRight = pTextDrawTransmit->byteRight;
    m_TextDrawData.dwStyle = pTextDrawTransmit->byteStyle;

    m_TextDrawData.fX = pTextDrawTransmit->fX;
    m_TextDrawData.fY = pTextDrawTransmit->fY;

    m_TextDrawData.dwParam1 = 0xFFFFFFFF;
    m_TextDrawData.dwParam2 = 0xFFFFFFFF;
    m_TextDrawData.byteSelectable = pTextDrawTransmit->byteSelectable;
    m_TextDrawData.wModelID = pTextDrawTransmit->wModelID;
    m_TextDrawData.vecRot.x = pTextDrawTransmit->vecRot.x;
    m_TextDrawData.vecRot.y = pTextDrawTransmit->vecRot.y;
    m_TextDrawData.vecRot.z = pTextDrawTransmit->vecRot.z;
    m_TextDrawData.fZoom = pTextDrawTransmit->fZoom;
    m_TextDrawData.wColor1 = pTextDrawTransmit->wColor1;
    m_TextDrawData.wColor2 = pTextDrawTransmit->wColor2;
    m_TextDrawData.bHasKeyCode = false;
    m_TextDrawData.iTextureSlot = -1;
    SetText(szText);

    if (m_TextDrawData.dwStyle == 4) {
        m_TextDrawData.iTextureSlot = GetFreeTextDrawTextureSlot();
        LoadTexture();
    }

    m_TextDrawData.bHasRectArea = false;
    m_rectArea.left = 0.0f;
    m_rectArea.right = 0.0f;
    m_rectArea.top = 0.0f;
    m_rectArea.bottom = 0.0f;
    m_bHovered = false;
    m_dwHoverColor = 0;

}

CTextDraw::~CTextDraw()
{
    if (m_TextDrawData.iTextureSlot != -1 && m_TextDrawData.iTextureSlot != 0x0 && TextDrawTexture[m_TextDrawData.iTextureSlot]) RwTextureDestroy((RwTexture*)TextDrawTexture[m_TextDrawData.iTextureSlot]);
    DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);
}

// ---------------------------------------------------------------------------
// 0.3.DL: sprite textdraw "mdl-ID:nama_tekstur" mengambil tekstur dari TXD yang
// diunduh untuk model custom ID tersebut (AddSimpleModel(..., -ID, dff, txd)).
// TXD dibaca dari cache ke memori dan dipertahankan (satu kamus per file).
// ---------------------------------------------------------------------------
static void* ReadDictFromFile(const std::string& path)
{
    static std::map<std::string, void*> s_dicts;
    auto it = s_dicts.find(path);
    if (it != s_dicts.end()) return it->second;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 12 || size > 64 * 1024 * 1024) { fclose(f); return nullptr; }
    std::vector<uint8_t>* buf = new std::vector<uint8_t>((size_t)size);
    size_t got = fread(buf->data(), 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { delete buf; return nullptr; }

    void* dict = nullptr;
    RwMemory mem;
    mem.start = buf->data();
    mem.length = (RwUInt32)size;
    RwStream* stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
    if (stream)
    {
        uint32_t hdr[3] = {0, 0, 0};
        if (RwStreamRead(stream, hdr, 12) == 12 && hdr[0] == 0x16)
            dict = CHook::CallFunction<void*>("_Z25RwTexDictionaryStreamReadP8RwStream", stream);
        RwStreamClose(stream, &mem);
    }
    if (!dict) { delete buf; s_dicts[path] = nullptr; return nullptr; }
    s_dicts[path] = dict;
    return dict;
}

static RwTexture* FindInDict(void* dict, const char* szTexture)
{
    if (!dict) return nullptr;
    RwTexture* tex = CHook::CallFunction<RwTexture*>(
        "_Z31RwTexDictionaryFindNamedTextureP15RwTexDictionaryPKc", dict, szTexture);
    if (tex) ++tex->refCount;
    return tex;
}

RwTexture* FindCustomDlTextureByModel(int32_t netId, const char* szTexture)
{
    std::string path;
    if (!DLManager::Instance().GetTxdPathForNetworkModel(netId, path)) return nullptr;
    RwTexture* tex = FindInDict(ReadDictFromFile(path), szTexture);
    return tex;
}

RwTexture* FindTextureInNamedTxd(const char* szTxd, const char* szTexture)
{
    if (!g_pszStorage || !szTxd || !*szTxd) return nullptr;
    if (strchr(szTxd, '/') || strchr(szTxd, '\\') || strstr(szTxd, "..")) return nullptr;

    static std::set<std::string> s_missing;
    if (s_missing.count(szTxd)) return nullptr;
    bool anyFile = false;

    std::string lower = szTxd;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const char* dirs[] = { "models/txd/", "SAMP/txd/", "texdb/", "models/" };
    const std::string names[2] = { szTxd, lower };
    for (const char* d : dirs)
        for (const std::string& n : names)
        {
            std::string path = std::string(g_pszStorage) + d + n + ".txd";
            if (access(path.c_str(), R_OK) != 0) continue;
            anyFile = true;
            RwTexture* tex = FindInDict(ReadDictFromFile(path), szTexture);
            if (tex) return tex;
        }
    if (!anyFile) s_missing.insert(szTxd);
    return nullptr;
}

static RwTexture* FindCustomDlTexture(const char* szSlot, const char* szTexture)
{
    if (strncmp(szSlot, "mdl-", 4) != 0) return FindTextureInNamedTxd(szSlot, szTexture);
    const char* num = szSlot + 4;
    if (*num == '-') ++num;
    if (*num < '0' || *num > '9') return nullptr;
    long id = strtol(num, nullptr, 10);
    if (id <= 0) return nullptr;
    if (RwTexture* t = FindCustomDlTextureByModel((int32_t)-id, szTexture)) return t;
    return FindCustomDlTextureByModel((int32_t)id, szTexture);
}

uintptr_t LoadFromTxdSlot(const char* szSlot, const char* szTexture)
{
    if (RwTexture* custom = FindCustomDlTexture(szSlot, szTexture))
        return (uintptr_t)custom;

    RwTexture* tex = nullptr;
    if (strncmp(szSlot, "none", 5u))
    {
        /*uintptr_t v10 = ((int (*)(const char*))(g_libGTASA + 0x55BB85))(szSlot);
        CallFunction<void>(g_libGTASA + 0x55BD6C + 1);
        CallFunction<void>(g_libGTASA + 0x55BD6C + 1, v10, 0);
        tex = CallFunction<RwTexture*>(g_libGTASA + 0x1B2558 + 1, szTexture, 0);
        CallFunction<void>(g_libGTASA + 0x55BDA8 + 1);*/
    }

    static char *texdb[7] = { "samp", "mobile", "txd", "menu", "gta3", "gta_int", "player" };
    bool FindedLibrary = false;
    for (int i = 0; i < 7; i++)
    {
        if (!strcmp(texdb[i], szSlot))
        {
            FindedLibrary = true;
            break;
        }
    }

    if (FindedLibrary == false)
    {
        if(!tex) tex = (RwTexture*)LoadTexture(szTexture);
    }

    if(!tex) tex = (RwTexture*)LoadTexture(std::string(std::string(szTexture) + "_" + szSlot).c_str());
    if(!tex) tex = (RwTexture*)LoadTexture(std::string(std::string(szSlot) + "_" + szTexture).c_str());
    if(!tex) tex = (RwTexture*)LoadTexture(szTexture);
    if(!tex)
    {
        std::string str = szTexture;
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        tex = (RwTexture*)LoadTexture(str.c_str());
    }
    if(!tex)
    {
        std::string str = szTexture;
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        tex = (RwTexture*)LoadTexture(std::string(str + "_" + szSlot).c_str());
    }

    if(!tex)
    {
        tex = (RwTexture*)CUtil::LoadTextureFromDB(szSlot, szTexture);
    }

   FLog("%s loaded from %s", szTexture, szSlot);
    return (uintptr_t)tex;
}

void CTextDraw::Draw()
{
    if (m_TextDrawData.iTextureSlot != -1) {
        return DrawTextured();
    }

    if (m_TextDrawData.dwStyle == 4) {
        return DrawTextured();
    }

    return DrawDefault();
}

void CTextDraw::DrawDefault()
{
    if (!m_szText || !strlen(m_szText)) return;

    //CFont::AsciiToGxtChar(m_szText, m_gxtString);
    int iScreenWidth = RsGlobal->maximumWidth;
    int iScreenHeight = RsGlobal->maximumHeight;
    float fHorizHudScale = 1.0f / 640.0f;
    float fVertHudScale = 1.0f / 448.0f;

    float fScaleY = (float)iScreenHeight * fVertHudScale * m_TextDrawData.fLetterHeight * 0.5f;
    float fScaleX = (float)iScreenWidth * fHorizHudScale * m_TextDrawData.fLetterWidth;

    CFont::SetScale(fScaleX, fScaleY);

    if (m_bHovered)
    {
        uint32_t dwReversed = __builtin_bswap32(m_dwHoverColor | (0x000000FF));
        CFont::SetColor(&dwReversed);
    }
    else
    {
        CFont::SetColor(&m_TextDrawData.dwLetterColor);
    }

    CFont::SetJustify(0);

    if (m_TextDrawData.byteAlignRight) CFont::SetOrientation(2);
    else if (m_TextDrawData.byteCentered) CFont::SetOrientation(0);
    else CFont::SetOrientation(1);

    float fLineWidth = iScreenWidth * m_TextDrawData.fLineWidth * fHorizHudScale;
    CFont::SetWrapX(fLineWidth);

    float fLineHeight = iScreenWidth * m_TextDrawData.fLineHeight * fHorizHudScale;
    CFont::SetCentreSize(fLineHeight);

    CFont::SetBackground(m_TextDrawData.byteBox, 0);
    CFont::SetBackgroundColor(&m_TextDrawData.dwBoxColor);
    CFont::SetProportional(m_TextDrawData.byteProportional);
    CFont::SetDropColor(&m_TextDrawData.dwBackgroundColor);

    if (m_TextDrawData.byteOutline) {
        CFont::SetEdge(m_TextDrawData.byteOutline);
    }
    else {
        //CFont::SetEdge(m_TextDrawData.byteShadow);
        CFont::SetDropShadowPosition(m_TextDrawData.byteShadow);
    }

    CFont::SetFontStyle(m_TextDrawData.dwStyle);

    if (m_TextDrawData.bHasKeyCode) {
        //  CMessages_InsertPlayerControlKeysInString(m_szString);
    }

    float fUseY = iScreenHeight - ((448.0 - m_TextDrawData.fY) * (iScreenHeight * fVertHudScale));
    float fUseX = iScreenWidth - ((640.0 - m_TextDrawData.fX) * (iScreenWidth * fHorizHudScale));
    CFont::PrintString(fUseX, fUseY, (const char*)m_szText);

    CFont::SetEdge(0);

    if (m_TextDrawData.byteAlignRight)
    {
        m_rectArea.left = fUseX - (fLineWidth - fUseX);
        m_rectArea.right = fUseX;
        m_rectArea.bottom = fUseY;
        m_rectArea.top = fUseY + fLineHeight;
    }
    else if (m_TextDrawData.byteCentered)
    {
        m_rectArea.left = fUseX - (fLineHeight * 0.5f);
        m_rectArea.right = m_rectArea.left + fLineHeight;
        m_rectArea.bottom = fUseY;
        m_rectArea.top = fUseY + fLineWidth;
    }
    else
    {
        m_rectArea.left = fUseX;
        m_rectArea.right = fLineWidth - fUseX + fUseX;
        m_rectArea.bottom = fUseY;
        m_rectArea.top = fUseY + fLineHeight;
    }
    m_TextDrawData.bHasRectArea = true;
}

void CTextDraw::DrawTextured()
{
    // Sprite "mdl-ID:..." belum bisa dimuat sebelum TXD-nya selesai diunduh: coba lagi berkala.
    if (m_TextDrawData.iTextureSlot != -1 && !TextDrawTexture[m_TextDrawData.iTextureSlot]
        && strncmp(m_szText, "mdl-", 4) == 0)
    {
        static uint32_t s_lastTry = 0;
        uint32_t now = GetTickCount();
        if (now - s_lastTry > 1000) { s_lastTry = now; LoadTexture(); }
    }

    float scaleX = (float)(RsGlobal->maximumWidth) * (1.0f / 640.0f);
    float scaleY = (float)(RsGlobal->maximumHeight) * (1.0f / 448.0f);

    m_rectArea.left = m_TextDrawData.fX * scaleX;
    m_rectArea.bottom = m_TextDrawData.fY * scaleY;
    m_rectArea.right = (m_TextDrawData.fX + m_TextDrawData.fLineWidth) * scaleX;
    m_rectArea.top = (m_TextDrawData.fY + m_TextDrawData.fLineHeight) * scaleY;
    m_TextDrawData.bHasRectArea = true;

    static float uv_reflected[8] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f };

    static float uv_normal[8] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f };

    if (m_bHovered) {
        if(TextDrawTexture[m_TextDrawData.iTextureSlot] != 0)DrawTextureUV(TextDrawTexture[m_TextDrawData.iTextureSlot], &m_rectArea, m_dwHoverColor,
                                                                           m_TextDrawData.dwStyle == 5 ? uv_reflected : uv_normal);
    }
    else {
        if(TextDrawTexture[m_TextDrawData.iTextureSlot] != 0)DrawTextureUV(TextDrawTexture[m_TextDrawData.iTextureSlot], &m_rectArea, m_TextDrawData.dwLetterColor,
                                                                           m_TextDrawData.dwStyle == 5 ? uv_reflected : uv_normal);
    }
}

void CTextDraw::SetText(const char* szText)
{
    memset(m_szText, 0, 800);
    strncpy(m_szText, szText, 800);
    m_szText[800] = 0;

    if (m_TextDrawData.dwStyle == 4 && m_TextDrawData.iTextureSlot != -1)
    {
        DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);
        LoadTexture();
    }
}

void CTextDraw::LoadTexture()
{
    char txdname[64 + 1];
    memset(txdname, 0, sizeof(txdname));
    char texturename[64 + 1];
    memset(texturename, 0, sizeof(texturename));

    char* szTexture = strchr(m_szText, ':');
    if (szTexture == nullptr) return;

    if (strlen(m_szText) < 64 && strchr(m_szText, '\\') == nullptr && strchr(m_szText, '/') == nullptr)
    {
        strncpy(txdname, m_szText, (size_t)(szTexture - m_szText));
        strcpy(texturename, ++szTexture);

        if (m_TextDrawData.iTextureSlot != -1)
            TextDrawTexture[m_TextDrawData.iTextureSlot] = ::LoadFromTxdSlot(txdname, texturename);
    }
}

#include "Models/ModelInfo.h"
void CTextDraw::SnapshotProcess()
{
    if (m_TextDrawData.dwStyle != 5 || m_TextDrawData.iTextureSlot != -1) {
        return;
    }

    uintptr_t snapshot = 0;

    // PED MODEL
    if (IsValidPedModel(m_TextDrawData.wModelID))
    {
        /*if (!IsExistinfoForModel(m_TextDrawData.wModelID))
            m_TextDrawData.wModelID = 0;*/

        snapshot = (uintptr_t)pSnapShotHelper->CreatePedSnapShot(
                m_TextDrawData.wModelID,
                m_TextDrawData.dwBackgroundColor,
                &m_TextDrawData.vecRot,
                m_TextDrawData.fZoom);
    }
        // VEHICLE MODEL
    else if (m_TextDrawData.wModelID >= 400 && m_TextDrawData.wModelID <= 611)
    {
        snapshot = (uintptr_t)pSnapShotHelper->CreateVehicleSnapShot(
                m_TextDrawData.wModelID,
                m_TextDrawData.dwBackgroundColor,
                &m_TextDrawData.vecRot,
                m_TextDrawData.fZoom,
                m_TextDrawData.wColor1,
                m_TextDrawData.wColor2
        );
    }
        // OBJECT MODEL
    else
    {
        uint16_t objModel = m_TextDrawData.wModelID;
        int16_t signedId = (int16_t)objModel;
        if (signedId < -1)
        {
            int16_t virt = DLManager::Instance().GetVirtualModelId((int32_t)signedId);
            if (virt > 0 && DLManager::Instance().IsModelRegistered(virt))
                objModel = (uint16_t)virt;
        }
        if (!CModelInfo::GetModelInfo(objModel))
            objModel = 18631;
        snapshot = (uintptr_t)pSnapShotHelper->CreateObjectSnapShot(
                objModel,
                m_TextDrawData.dwBackgroundColor,
                &m_TextDrawData.vecRot,
                m_TextDrawData.fZoom
        );
    }

    if (snapshot)
    {
        m_TextDrawData.iTextureSlot = GetFreeTextDrawTextureSlot();
        TextDrawTexture[m_TextDrawData.iTextureSlot] = snapshot;
    }
}