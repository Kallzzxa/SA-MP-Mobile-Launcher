#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "game/World.h"

extern CGame* pGame;
extern CNetGame* pNetGame;


C3DTextLabelPool::C3DTextLabelPool()
{
	for (int i = 0; i < MAX_TEXT_LABELS; i++) {
		//m_TextLabels[i].pszText = nullptr;
		m_TextLabels[i] = nullptr;

		m_bSlotUsed[i] = false;
	}
}

C3DTextLabelPool::~C3DTextLabelPool()
{
	for (int i = 0; i < MAX_TEXT_LABELS; i++) {
		if (m_bSlotUsed[i]) {
			this->ClearLabel(i);
		}
	}
}

// Hapus kode warna {RRGGBB} dari teks untuk kalkulasi lebar
static void FilterLabelColors(std::string &str)
{
    std::string out;
    out.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '{' && i + 7 < str.size() && str[i + 7] == '}') {
            i += 8;
            continue;
        }
        out += str[i];
        i++;
    }
    str = out;
}

static bool ProcessLabelHexColor(const char* start, const char* end, ImVec4& color)
{
    const int hexCount = (int)(end - start);
    if (hexCount == 6 || hexCount == 8) {
        char hex[9];
        strncpy(hex, start, hexCount);
        hex[hexCount] = 0;
        unsigned int hexColor = 0;
        if (sscanf(hex, "%x", &hexColor) > 0) {
            color.x = static_cast<float>((hexColor & 0x00FF0000) >> 16) / 255.0f;
            color.y = static_cast<float>((hexColor & 0x0000FF00) >> 8) / 255.0f;
            color.z = static_cast<float>((hexColor & 0x000000FF)) / 255.0f;
            color.w = 1.0f;
            if (hexCount == 8)
                color.w = static_cast<float>((hexColor & 0xFF000000) >> 24) / 255.0f;
            return true;
        }
    }
    return false;
}

// Gambar teks dengan dukungan kode warna inline {RRGGBB}
static void DrawLabelTextWithColors(ImGuiRenderer* renderer, ImVec2 pos, uint32_t baseColor,
                                    const std::string& text, const std::string& textNoColor)
{
    // Hitung offset per baris untuk centering
    float lineOffsets[64];
    int lineCount = 0;
    {
        const char* curBegin = textNoColor.c_str();
        const char* curPos = curBegin;
        while (*curPos != '\0' && lineCount < 64) {
            if (*curPos == '\n') {
                float w = ImGui::CalcTextSize(curBegin, curPos).x / 2.0f;
                lineOffsets[lineCount++] = w;
                curBegin = curPos + 1;
            }
            curPos++;
        }
        if (lineCount < 64) {
            float w = ImGui::CalcTextSize(curBegin, curPos).x / 2.0f;
            lineOffsets[lineCount++] = w;
        }
    }

    char tmp[4096];
    strncpy(tmp, text.c_str(), sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    ImVec2 vecPos = pos;
    if (lineCount > 0) vecPos.x -= lineOffsets[0];

    ImColor col = __builtin_bswap32(baseColor | 0x000000FF);
    const char* textStart = tmp;
    const char* textCur = tmp;
    int curLine = 0;
    float fontSize = UISettings::fontSize() / 2;

    while (*textCur != '\0') {
        if (*textCur == '{') {
            if (textCur != textStart) {
                ImVec2 sz = renderer->calculateTextSize(std::string(textStart, textCur), fontSize);
                renderer->drawText(vecPos, col, std::string(textStart, textCur), true, fontSize);
                vecPos.x += sz.x;
            }
            const char* colorStart = textCur + 1;
            do { ++textCur; } while (*textCur != '\0' && *textCur != '}');
            ImVec4 tc;
            if (ProcessLabelHexColor(colorStart, textCur, tc)) {
                col = ImColor(tc);
            }
            textStart = textCur + 1;
        } else if (*textCur == '\n') {
            if (textCur != textStart) {
                ImVec2 sz = renderer->calculateTextSize(std::string(textStart, textCur), fontSize);
                renderer->drawText(vecPos, col, std::string(textStart, textCur), true, fontSize);
            }
            curLine++;
            vecPos.x = pos.x;
            if (curLine < lineCount) vecPos.x -= lineOffsets[curLine];
            vecPos.y += fontSize;
            textStart = textCur + 1;
        }
        ++textCur;
    }
    if (textCur != textStart) {
        ImVec2 sz = renderer->calculateTextSize(std::string(textStart, textCur), fontSize);
        renderer->drawText(vecPos, col, std::string(textStart, textCur), true, fontSize);
    }
}

void C3DTextLabelPool::NewLabel(uint16_t wLabelId, TEXT_LABEL* pLabel) {

	if (wLabelId < MAX_TEXT_LABELS) {

		if (m_TextLabels[wLabelId])
		{
			delete m_TextLabels[wLabelId];
			m_TextLabels[wLabelId] = nullptr;
			m_bSlotUsed[wLabelId] = false;
		}

		//labelInfo.dwColor = (labelInfo.dwColor >> 8) | (labelInfo.dwColor << 24);
		//pTextLabel->pszText = new char[strlen(pLabel->pszText) + 1];
		//strcpy(pTextLabel->pszText, pLabel->pszText);
		TEXT_LABEL* pTextLabel = new TEXT_LABEL;
		pTextLabel->text = Encoding::cp2utf(pLabel->text);
		pTextLabel->textWithoutColors = pTextLabel->text;
		FilterLabelColors(pTextLabel->textWithoutColors);

		pTextLabel->dwColor = pLabel->dwColor;
		pTextLabel->vecPos.x = pLabel->vecPos.x;
		pTextLabel->vecPos.y = pLabel->vecPos.y;
		pTextLabel->vecPos.z = pLabel->vecPos.z;
		pTextLabel->fDistance = pLabel->fDistance;
		pTextLabel->bTestLOS = pLabel->bTestLOS;
		pTextLabel->playerId = pLabel->playerId;
		pTextLabel->vehicleId = pLabel->vehicleId;

		m_TextLabels[wLabelId] = pTextLabel;
		m_bSlotUsed[wLabelId] = true;
	}
}

void C3DTextLabelPool::ClearLabel(uint16_t wLabelId) {
	if (wLabelId < 0 || wLabelId >= MAX_TEXT_LABELS)
	{
		return;
	}
	m_bSlotUsed[wLabelId] = false;
	if (m_TextLabels[wLabelId])
	{
		delete m_TextLabels[wLabelId];
		m_TextLabels[wLabelId] = nullptr;
	}
}

void C3DTextLabelPool::Render(ImGuiRenderer* renderer)
{
	CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
	if(!pPlayerPed) return;

    static CCamera& TheCamera = *reinterpret_cast<CCamera*>(g_libGTASA + (VER_x32 ? 0x00951FA8 : 0xBBA8D0));

	for (int i = 0; i < MAX_TEXT_LABELS; i++)
	{
		if (m_bSlotUsed[i]) {
            CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
            if (!pPlayerPool) break;

            TEXT_LABEL *pTextLabel = m_TextLabels[i];

            CVector vecTextPos = pTextLabel->vecPos;

            if (pTextLabel->playerId != INVALID_PLAYER_ID) {
                if (pTextLabel->playerId == pPlayerPool->GetLocalPlayerID()) continue;

                if (pPlayerPool && pPlayerPool->GetSlotState(pTextLabel->playerId)) {
                    CRemotePlayer *pPlayer = pPlayerPool->GetAt(pTextLabel->playerId);
                    if (pPlayer && pPlayer->GetDistanceFromLocalPlayer() < pTextLabel->fDistance) {
                        CPlayerPed *pPlayerPed = pPlayer->GetPlayerPed();
                        if (pPlayerPed && pPlayerPed->m_pPed->IsAdded()) {
                            CVector matBone;
                            pPlayerPed->GetBonePosition(8, &matBone);

                            vecTextPos.x = matBone.x + pTextLabel->vecPos.x;
                            vecTextPos.y = matBone.y + pTextLabel->vecPos.y;
                            vecTextPos.z = matBone.z + 0.23 + pTextLabel->vecPos.z;

                            this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
                                       pTextLabel->dwColor);

                        }
                    }
                }
            }
			if (pTextLabel->vehicleId != INVALID_VEHICLE_ID) {
				CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
				if (pVehiclePool && pVehiclePool->GetSlotState(pTextLabel->vehicleId)) {
					CVehicle *pVehicle = pVehiclePool->GetAt(pTextLabel->vehicleId);
					if (pVehicle && pVehicle->m_pVehicle->IsAdded() &&
						pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() < pTextLabel->fDistance) {
						RwMatrix matVehicle = pVehicle->m_pVehicle->GetMatrix().ToRwMatrix();

						vecTextPos.x = matVehicle.pos.x + pTextLabel->vecPos.x;
						vecTextPos.y = matVehicle.pos.y + pTextLabel->vecPos.y;
						vecTextPos.z = matVehicle.pos.z + pTextLabel->vecPos.z;

						this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
								   pTextLabel->dwColor);
					}
				}
			}

			if (pPlayerPed->m_pPed->GetDistanceFromPoint(pTextLabel->vecPos.x, pTextLabel->vecPos.y, pTextLabel->vecPos.z) <= pTextLabel->fDistance)
				this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
						   pTextLabel->dwColor);
        }
	}
}

void C3DTextLabelPool::Draw(ImGuiRenderer* renderer, TEXT_LABEL* label, CVector vecPos, const std::string& text, uint32_t dwColor)
{
	CVector vPos;
	vPos.x = vecPos.x;
	vPos.y = vecPos.y;
	vPos.z = vecPos.z;

    static CCamera& TheCamera = *reinterpret_cast<CCamera*>(g_libGTASA + (VER_x32 ? 0x00951FA8 : 0xBBA8D0));

	int hitEntity = 0;
    if (label->bTestLOS) {
		CAMERA_AIM *pCam = GameGetInternalAim();
		if (!pCam)
		{
			return;
		}

        RwMatrix matPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->m_pPed->GetMatrix().ToRwMatrix();

		CVector vec;
		vec.x = pCam->pos1x;
		vec.y = pCam->pos1y;
		vec.z = pCam->pos1z;

		//bool isLineOfSightClear = ((bool (*)(CVector*, CVector*, int, int, int, int, int, int, int))(g_libGTASA + 0x423418 + 1))(&vec, &matPlayer.pos, 1, 0, 0, 1, 0, 0, 0);

        hitEntity = CWorld::GetIsLineOfSightClear(vecPos, TheCamera.GetPosition(), true, false, false, true, false, false, false);
		/*if(!isLineOfSightClear)
		{
			LOGI("labelpool draw ok no render fuck you bitch");
			return;
		}*/
    }

	if (!label->bTestLOS || hitEntity) {
		if (pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->m_pPed->GetDistanceFromPoint(vecPos.x, vecPos.y, vecPos.z) <= label->fDistance) {
			CVector vecOut;
			// CSprite::CalcScreenCoors
			((void (*)(CVector *, CVector *, float *, float *, bool, bool)) (g_libGTASA + (VER_x32 ? 0x005C57E8 + 1 : 0x6E9DF8)))(
					&vPos, &vecOut, 0, 0, 0, 0);
			if (vecOut.z < 1.0f) return;

			DrawLabelTextWithColors(renderer, ImVec2(vecOut.x, vecOut.y), dwColor,
			                        text, label->textWithoutColors);
		}
	}
}