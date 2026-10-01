#include "../main.h"
#include "../game/game.h"
#include "../settings.h"
#include "../net/netgame.h"
#include "gui.h"
#include "../playertags.h"
#include "../net/playerbubblepool.h"
#include "vendor/str_obfuscator/str_obfuscator.hpp"
// voice
#include "../voice_new/Plugin.h"
#include "../voice_new/MicroIcon.h"
#include "../voice_new/SpeakerList.h"
#include "../voice_new/Network.h"

#include "../gui/samp_widgets/voicebutton.h"
#include "game/Textures/TextureDatabaseRuntime.h"
#include "game/Streaming.h"
#include "game/Pools.h"
#include "game/Widgets/TouchInterface.h"
#include "game/Widgets/WidgetButton.h"
#include "game/pad.h"
#include <algorithm>

extern CNetGame* pNetGame;
extern CPlayerTags* pPlayerTags;
extern CGame* pGame;
extern UI* pUI;
extern CSettings* pSettings;

class DefenseButton final : public Widget
{
public:
	void setNativeSource(CWidgetGta* source)
	{
		if (!source) return;
		auto* button = reinterpret_cast<CWidgetButton*>(source);
		m_fill = button->m_SpriteFill.m_pTexture ? button->m_SpriteFill.m_pTexture->raster : nullptr;
		m_icon = button->m_Sprite.m_pTexture ? button->m_Sprite.m_pTexture->raster : nullptr;
	}

	void draw(ImGuiRenderer* renderer) override
	{
		const ImVec2 topLeft = absolutePosition();
		const ImVec2 bottomRight = topLeft + size();
		renderer->drawRect(topLeft, bottomRight,
			focused() ? ImColor(120, 180, 230, 210) : ImColor(24, 28, 32, 175), true);
		if (m_fill)
			renderer->drawImage(topLeft, bottomRight, (ImTextureID)m_fill);

		if (m_icon)
		{
			const float iconWidth = size().x * 0.62f;
			const float iconHeight = size().y * 0.62f;
			const ImVec2 iconMin(topLeft.x + (size().x - iconWidth) * 0.5f,
				topLeft.y + size().y * 0.06f);
			renderer->drawImage(iconMin, iconMin + ImVec2(iconWidth, iconHeight), (ImTextureID)m_icon);
		}

		const float fontSize = std::min(size().y * 0.22f, UISettings::fontSize() / 3.5f);
		const ImVec2 textSize = renderer->calculateTextSize("DEF", fontSize);
		renderer->drawText(ImVec2(topLeft.x + (size().x - textSize.x) * 0.5f,
			topLeft.y + size().y * 0.73f), ImColor(255, 255, 255), "DEF", true, fontSize);
		Widget::draw(renderer);
	}

private:
	RwRaster* m_fill = nullptr;
	RwRaster* m_icon = nullptr;
};

UI::UI(const ImVec2& display_size, const std::string& font_path)
    : Widget(), ImGuiWrapper(display_size, font_path)
{
	UISettings::Initialize(display_size);
	this->setFixedSize(display_size);
}

UI::UI(const ImVec2& display_size, const std::vector<char>& fontData)
    : Widget(), ImGuiWrapper(display_size, fontData)
{
    UISettings::Initialize(display_size);
    this->setFixedSize(display_size);
}

bool UI::initialize()
{
	if (!ImGuiWrapper::initialize()) return false;

	m_splashScreen = new SplashScreen();
	this->addChild(m_splashScreen);
	m_splashScreen->setFixedSize(size());
	m_splashScreen->setPosition(ImVec2(0.0f, 0.0f));
	m_splashScreen->setVisible(true);

	m_chat = new Chat();
	this->addChild(m_chat);
	m_chat->setFixedSize(UISettings::chatSize());
	m_chat->setPosition(UISettings::chatPos());
	m_chat->setItemSize(UISettings::chatItemSize());
	m_chat->setVisible(false);

	m_buttonPanel = new ButtonPanel();
	this->addChild(m_buttonPanel);
	m_buttonPanel->setFixedSize(UISettings::buttonPanelSize());
	m_buttonPanel->setPosition(UISettings::buttonPanelPos());
	m_buttonPanel->setVisible(false);

	m_defenseButton = new DefenseButton();
	this->addChild(m_defenseButton);
	m_defenseButton->setVisible(false);

	m_defenseEditorHelp = new Label("Drag DEF to move | Resize | Save or cancel",
		ImColor(255, 255, 255), true, UISettings::fontSize() / 2.5f);
	this->addChild(m_defenseEditorHelp);
	m_defenseEditorHelp->setVisible(false);

	auto makeEditorButton = [this](const char* caption, const ImVec2& position,
		const std::function<void()>& callback) {
		auto* button = new Button(caption, UISettings::fontSize() / 2.5f);
		button->performLayout();
		button->setFixedSize(ImVec2(UISettings::fontSize() * 2.4f, UISettings::fontSize() * 1.5f));
		Widget* buttonLabel = button->childAt(0);
		if (buttonLabel)
			buttonLabel->setPosition((button->size() - buttonLabel->size()) / 2.0f);
		button->setPosition(position);
		button->setCallback(callback);
		button->setVisible(false);
		this->addChild(button);
		return button;
	};
	m_defenseScaleDown = makeEditorButton("SIZE -", ImVec2(0.0f, 0.0f), [this]() {
		m_defenseEditScale = std::max(0.5f, m_defenseEditScale - 0.1f);
	});
	m_defenseScaleUp = makeEditorButton("SIZE +", ImVec2(0.0f, 0.0f), [this]() {
		m_defenseEditScale = std::min(1.8f, m_defenseEditScale + 0.1f);
	});
	m_defenseSave = makeEditorButton("SAVE", ImVec2(0.0f, 0.0f), [this]() {
		FinishDefenseButtonEditor(true);
	});
	m_defenseCancel = makeEditorButton("CANCEL", ImVec2(0.0f, 0.0f), [this]() {
		FinishDefenseButtonEditor(false);
	});

	m_voiceButton = new VoiceButton();
	this->addChild(m_voiceButton);
	m_voiceButton->setFixedSize(UISettings::buttonVoiceSize());
	m_voiceButton->setPosition(UISettings::buttonVoicePos());
	m_voiceButton->setVisible(false);

	m_spawn = new Spawn();
	this->addChild(m_spawn);
	m_spawn->setFixedSize(UISettings::spawnSize());
	m_spawn->setPosition(UISettings::spawnPos());
	m_spawn->setVisible(false);

	m_dialog = new Dialog();
	this->addChild(m_dialog);
	m_dialog->setVisible(false);
	m_dialog->setMinSize(UISettings::dialogMinSize());
	m_dialog->setMaxSize(UISettings::dialogMaxSize());

	m_keyboard = new Keyboard();
	this->addChild(m_keyboard);
	m_keyboard->setFixedSize(UISettings::keyboardSize());
	m_keyboard->setPosition(UISettings::keyboardPos());
	m_keyboard->setVisible(false);

	m_playerTabList = new PlayerTabList();
	this->addChild(m_playerTabList);
	m_playerTabList->setMinSize(UISettings::dialogMinSize());
	m_playerTabList->setMaxSize(UISettings::dialogMaxSize());
	m_playerTabList->setVisible(false);

    label = new Label(" ", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
    pUI->addChild(label);

    label2 = new Label(" ", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
    pUI->addChild(label2);

    label3 = new Label(" ", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
    pUI->addChild(label3);

    label4 = new Label(" ", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
    pUI->addChild(label4);

	// mem
	Label* d_label1;

	d_label1 = new Label(cryptor::create("SAMP-M 1.0B").decrypt(), ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 3);
	this->addChild(d_label1);
	d_label1->setPosition(ImVec2(3.0, 3.0));

    // ==== version ==== //
    //d_label = new Label("", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
    //this->addChild(d_label);
   // d_label->setPosition(ImVec2(3.0, 55.0));

	return true;
}

void UI::render()
{
	ImGuiWrapper::render();

    renderDebug();

    ProcessPushedTextdraws();

	if (m_bNeedClearMousePos) {
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = ImVec2(-1, -1);
		m_bNeedClearMousePos = false;
	}
}

void UI::shutdown()
{
	ImGuiWrapper::shutdown();
}

void UI::drawList()
{
	if (!visible()) return;

	/*Label* label;
	label = new Label("1.0.11", ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
	label->setPosition(ImVec2(0.0, 0.0));
	this->addChild(label);*/

	if (pPlayerTags) pPlayerTags->Render(renderer());
	if (pNetGame && pNetGame->GetTextLabelPool()) pNetGame->GetTextLabelPool()->Render(renderer());
	if (pNetGame && pNetGame->GetPlayerBubblePool()) pNetGame->GetPlayerBubblePool()->Render(renderer());

	CWidgetGta* attackWidget = CTouchInterface::m_pWidgets
		? CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ATTACK]
		: nullptr;
	if (pNetGame && attackWidget &&
		RsGlobal->maximumWidth > 0 && RsGlobal->maximumHeight > 0)
	{
		const CRect& attackRect = attackWidget->m_RectScreen;
		if (attackRect.right > attackRect.left && attackRect.top > attackRect.bottom)
		{
			const float scaleX = displaySize().x / RsGlobal->maximumWidth;
			const float scaleY = displaySize().y / RsGlobal->maximumHeight;
			const float buttonScale = m_defenseEditorActive
				? m_defenseEditScale
				: std::clamp(pSettings->Get().fDefenseButtonScale, 0.5f, 1.8f);
			const float buttonWidth = (attackRect.right - attackRect.left) * scaleX * 0.72f * buttonScale;
			const float buttonHeight = (attackRect.top - attackRect.bottom) * scaleY * 0.62f * buttonScale;
			const float centerX = (attackRect.left + attackRect.right) * scaleX * 0.5f;
			const float attackTop = (RsGlobal->maximumHeight - attackRect.top) * scaleY;
			const float top = attackTop - buttonHeight - 8.0f * scaleY;
			m_defenseDefaultPosition = ImVec2(centerX - buttonWidth * 0.5f, std::max(0.0f, top));

			ImVec2 buttonPosition = m_defenseDefaultPosition;
			if (m_defenseEditorActive)
			{
				buttonPosition = m_defenseEditPosition;
			}
			else if (pSettings->Get().fDefenseButtonPosX >= 0.0f &&
				pSettings->Get().fDefenseButtonPosY >= 0.0f)
			{
				buttonPosition = ImVec2(
					pSettings->Get().fDefenseButtonPosX * displaySize().x,
					pSettings->Get().fDefenseButtonPosY * displaySize().y);
			}
			buttonPosition.x = std::clamp(buttonPosition.x, 0.0f,
				std::max(0.0f, displaySize().x - buttonWidth));
			buttonPosition.y = std::clamp(buttonPosition.y, 0.0f,
				std::max(0.0f, displaySize().y - buttonHeight));

			m_defenseButton->setFixedSize(ImVec2(buttonWidth, buttonHeight));
			m_defenseButton->setPosition(buttonPosition);
			m_defenseButton->setNativeSource(attackWidget);

			CPlayerPed* localPlayer = pGame ? pGame->FindPlayerPed() : nullptr;
			bool canDefend = false;
			if (localPlayer && localPlayer->m_pPed)
			{
				const uint8_t weapon = localPlayer->GetCurrentWeapon();
				const bool supportedWeapon = weapon == WEAPON_UNARMED ||
					(weapon >= WEAPON_BRASSKNUCKLE && weapon <= WEAPON_KATANA);
				const bool running = localPlayer->m_pPed->m_nMoveState >= PEDMOVE_RUN ||
					LocalPlayerKeys.bKeys[ePadKeys::KEY_SPRINT];
				canDefend = !localPlayer->IsInVehicle() && supportedWeapon && !running;
			}
			m_defenseButton->setVisible(m_defenseEditorActive ||
				(attackWidget->m_bEnabled && canDefend && top >= 0.0f));
		}
		else
		{
			m_defenseButton->setVisible(false);
		}
	}
	else
	{
		m_defenseButton->setVisible(false);
	}

	const float editorX = UISettings::padding();
	const float editorY = UISettings::padding();
	const float editorGap = UISettings::padding() * 0.5f;
	if (m_defenseEditorHelp)
	{
		m_defenseEditorHelp->setVisible(m_defenseEditorActive);
		m_defenseEditorHelp->setPosition(ImVec2(editorX, editorY));
	}
	Button* editorButtons[] = { m_defenseScaleDown, m_defenseScaleUp, m_defenseSave, m_defenseCancel };
	for (int i = 0; i < 4; ++i)
	{
		Button* button = editorButtons[i];
		if (!button) continue;
		button->setVisible(m_defenseEditorActive);
		button->setPosition(ImVec2(editorX + i * (button->width() + editorGap),
			editorY + (m_defenseEditorHelp ? m_defenseEditorHelp->height() + editorGap : 0.0f)));
	}

	if (!m_defenseButton->visible())
	{
		m_defenseTouchIds.clear();
		m_defenseDragPointer = -1;
		LocalPlayerKeys.bKeys[ePadKeys::KEY_BLOCK] = false;
	}

	draw(renderer());
}

void UI::touchEvent(const ImVec2& pos, TouchType type)
{
	/* 
		� ������� ����������
		1 - ����������
		2 - ������
		3 - ���
	*/

	if (m_keyboard->visible() && m_keyboard->contains(pos))
	{
		m_keyboard->touchEvent(pos, type);
		return;
	}

	if (m_dialog->visible() && m_dialog->contains(pos))
	{
		m_dialog->touchEvent(pos, type);
		return;
	}

	Widget::touchEvent(pos, type);
}

void UI::ToggleDefenseButtonEditor()
{
	if (m_defenseEditorActive)
	{
		FinishDefenseButtonEditor(false);
		return;
	}
	if (!m_defenseButton || !pSettings)
		return;

	m_defenseEditScale = std::clamp(pSettings->Get().fDefenseButtonScale, 0.5f, 1.8f);
	if (pSettings->Get().fDefenseButtonPosX >= 0.0f && pSettings->Get().fDefenseButtonPosY >= 0.0f)
	{
		m_defenseEditPosition = ImVec2(
			pSettings->Get().fDefenseButtonPosX * displaySize().x,
			pSettings->Get().fDefenseButtonPosY * displaySize().y);
	}
	else
	{
		m_defenseEditPosition = m_defenseDefaultPosition;
		if (m_defenseEditPosition.x == 0.0f && m_defenseEditPosition.y == 0.0f)
			m_defenseEditPosition = ImVec2(displaySize().x * 0.78f, displaySize().y * 0.55f);
	}

	m_defenseEditorActive = true;
	m_defenseDragPointer = -1;
	m_defenseTouchIds.clear();
	LocalPlayerKeys.bKeys[ePadKeys::KEY_BLOCK] = false;
	if (m_chat)
		m_chat->addInfoMessage("Drag DEF to move, adjust size, then save.");
}

void UI::FinishDefenseButtonEditor(bool save)
{
	if (save && pSettings && displaySize().x > 0.0f && displaySize().y > 0.0f)
	{
		pSettings->Get().fDefenseButtonPosX = m_defenseEditPosition.x / displaySize().x;
		pSettings->Get().fDefenseButtonPosY = m_defenseEditPosition.y / displaySize().y;
		pSettings->Get().fDefenseButtonScale = m_defenseEditScale;
		pSettings->SaveDefenseButtonSettings();
		if (m_chat)
			m_chat->addInfoMessage("DEF button layout saved.");
	}
	else if (m_chat)
	{
		m_chat->addInfoMessage("DEF button edit cancelled.");
	}

	m_defenseEditorActive = false;
	m_defenseDragPointer = -1;
	m_defenseTouchIds.clear();
	LocalPlayerKeys.bKeys[ePadKeys::KEY_BLOCK] = false;
}

void UI::HandleDefenseTouchEvent(int type, int pointerId, int x, int y)
{
	if (m_defenseEditorActive)
	{
		if (!m_defenseButton || !m_defenseButton->visible())
			return;

		const ImVec2 touchPosition((float)x, (float)y);
		if (type == 2 && m_defenseDragPointer == -1 && m_defenseButton->contains(touchPosition))
		{
			m_defenseDragPointer = pointerId;
			m_defenseLastTouch = touchPosition;
		}
		else if (type == 3 && m_defenseDragPointer == pointerId)
		{
			m_defenseEditPosition.x += touchPosition.x - m_defenseLastTouch.x;
			m_defenseEditPosition.y += touchPosition.y - m_defenseLastTouch.y;
			m_defenseLastTouch = touchPosition;
			m_defenseEditPosition.x = std::clamp(m_defenseEditPosition.x, 0.0f,
				std::max(0.0f, displaySize().x - m_defenseButton->width()));
			m_defenseEditPosition.y = std::clamp(m_defenseEditPosition.y, 0.0f,
				std::max(0.0f, displaySize().y - m_defenseButton->height()));
			m_defenseButton->setPosition(m_defenseEditPosition);
		}
		else if (type == 1 && m_defenseDragPointer == pointerId)
		{
			m_defenseDragPointer = -1;
		}

		LocalPlayerKeys.bKeys[ePadKeys::KEY_BLOCK] = false;
		return;
	}

	if (!m_defenseButton || !m_defenseButton->visible())
	{
		m_defenseTouchIds.clear();
	}
	else if (type == 2 && m_defenseButton->contains(ImVec2(x, y)))
	{
		m_defenseTouchIds.insert(pointerId);
	}
	else if (type == 1)
	{
		m_defenseTouchIds.erase(pointerId);
	}

	LocalPlayerKeys.bKeys[ePadKeys::KEY_BLOCK] = !m_defenseTouchIds.empty();
}

enum eTouchType
{
	TOUCH_POP = 1,
	TOUCH_PUSH = 2,
	TOUCH_MOVE = 3
};

bool UI::OnTouchEvent(int type, bool multi, int x, int y)
{
	ImGuiIO& io = ImGui::GetIO();

	/*
	switch (type)
	{
	case 1://TOUCH_PUSH:
		io.MousePos = ImVec2(x, y);
		io.MouseDown[0] = true;
		MyLog2("TOUCH_PUSH");
		break;

	case 2://TOUCH_POP:
		io.MouseDown[0] = false;
		m_bNeedClearMousePos = true;
		MyLog2("TOUCH_POP");
		break;

	case 3://TOUCH_MOVE:
		io.MousePos = ImVec2(x, y);
		MyLog2("TOUCH_MOVE");
		break;
	}*/
	VoiceButton* vbutton = pUI->voicebutton();
	switch (type)
	{
	case TOUCH_PUSH:
		io.MousePos = ImVec2(x, y);
		io.MouseDown[0] = true;
		break;

	case TOUCH_POP:
		io.MouseDown[0] = false;
		m_bNeedClearMousePos = true;
		break;

	case TOUCH_MOVE:
		io.MousePos = ImVec2(x, y);
		//if (vbutton->countdown > 50) vbutton->countdown = 20;
		break;
	}

	return true;
}

#include "../settings.h"
extern CGame *pGame;
extern CSettings* pSettings;
void UI::renderDebug()
{
    if(!pSettings->Get().iFPSCounter) return;

    char szStr[30];
    char szStrMem[64];
    char szStrPos[64];

    ImVec2 pos = ImVec2(pUI->ScaleX(40.0f), pUI->ScaleY(540.0f));

    static float fps = 120.f;
        static auto lastTick = CTimer::m_snTimeInMillisecondsNonClipped;
        if(CTimer::m_snTimeInMillisecondsNonClipped - lastTick > 500) {
            lastTick = CTimer::m_snTimeInMillisecondsNonClipped;
            fps = std::clamp(CTimer::game_FPS, 10.f, (float) 120);
        }
        snprintf(&szStr[0], sizeof(szStr), "FPS: %.0f", fps);

        label->setText(&szStr[0]);
        label->setPosition(pos);

        /*auto &msUsed = CStreaming::ms_memoryUsed;
        auto &msAvailable = CStreaming::ms_memoryAvailable;

        struct mallinfo memInfo = mallinfo();
        int totalAllocatedMB  = memInfo.uordblks / (1024 * 1024);

        snprintf(&szStrMem[0], sizeof(szStrMem), "MEM: %d mb (stream %d/%d) (Tex %d MB)",
                 totalAllocatedMB,
                 msUsed / (1024 * 1024),
                 msAvailable / (1024 * 1024),
                 TextureDatabaseRuntime::storedTexels / (1024 * 1024)
        );

        pos = ImVec2(pUI->ScaleX(40.0f), pUI->ScaleY(1080.0f - UISettings::fontSize() * 9));

        label2->setText(&szStrMem[0]);
        label2->setPosition(pos);

        if (pGame->FindPlayerPed()->m_pPed)
        {
            snprintf(&szStrPos[0], sizeof(szStrPos), "POS: %.2f, %.2f, %.2f", pGame->FindPlayerPed()->m_pPed->m_matrix->m_pos.x, pGame->FindPlayerPed()->m_pPed->m_matrix->m_pos.y, pGame->FindPlayerPed()->m_pPed->m_matrix->m_pos.z);
            pos = ImVec2(pUI->ScaleX(40.0f), pUI->ScaleY(1080.0f - UISettings::fontSize() * 8));
            label3->setText(&szStrPos[0]);
            label3->setPosition(pos);
        }
        //Log("pools = %d mem = %d", GetPedPoolGta()->GetNoOfUsedSpaces(), totalAllocatedMB);
        char debugPools[250];
        snprintf(&debugPools[0], sizeof(debugPools), "NSingle: %d; NDouble: %d; Peds: %d; Veh's: %d; Obj: %d; EntryInf: %d; Dummies: %d, Buildings: %d",
                 GetPtrNodeSingleLinkPool()->GetNoOfUsedSpaces(),
                 GetPtrNodeDoubleLinkPool()->GetNoOfUsedSpaces(),
                 GetPedPoolGta()->GetNoOfUsedSpaces(),
                 GetVehiclePoolGta()->GetNoOfUsedSpaces(),
                 GetObjectPoolGta()->GetNoOfUsedSpaces(),
                 GetEntryInfoNodePool()->GetNoOfUsedSpaces(),
                 GetDummyPool()->GetNoOfUsedSpaces(),
                 GetBuildingPool()->GetNoOfUsedSpaces()
                 );

        pos = ImVec2(pUI->ScaleX(40.0f), pUI->ScaleY(1080.0f - UISettings::fontSize() * 1));

        label4->setText(&debugPools[0]);
        label4->setPosition(pos);*/
}

void UI::PushToBufferedQueueTextDrawPressed(uint16_t textdrawId)
{
    BUFFERED_COMMAND_TEXTDRAW* pCmd = m_BufferedCommandTextdraws.WriteLock();

    pCmd->textdrawId = textdrawId;

    m_BufferedCommandTextdraws.WriteUnlock();
}

void UI::ProcessPushedTextdraws()
{
    BUFFERED_COMMAND_TEXTDRAW* pCmd = nullptr;
    while (pCmd = m_BufferedCommandTextdraws.ReadLock())
    {
        RakNet::BitStream bs;
        bs.Write(pCmd->textdrawId);
        pNetGame->GetRakClient()->RPC(&RPC_ClickTextDraw, &bs, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, 0);
        m_BufferedCommandTextdraws.ReadUnlock();
    }
}