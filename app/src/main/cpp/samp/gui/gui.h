#pragma once

#include <unordered_set>

#define DEBUG_GUI 0

#include "../vendor/encoding/encoding.h"

#include "imguiwrapper.h"
#include "uisettings.h"

#include "widget.h"
#include "widgets/layout.h"
#include "widgets/label.h"
#include "widgets/button.h"
#include "widgets/image.h"
#include "widgets/progressbar.h"
#include "widgets/scrollpanel.h"
#include "widgets/listbox.h"
#include "widgets/editbox.h"

#include "samp_widgets/keyboard.h"
#include "samp_widgets/splashscreen.h"
#include "samp_widgets/chat.h"
#include "samp_widgets/spawn.h"
#include "samp_widgets/buttonpanel.h"
#include "samp_widgets/playerTabList.h"
#include "samp_widgets/voicebutton.h"

#include "samp_widgets/dialogs/dialog.h"

#include "..//vendor/raknet/SingleProducerConsumer.h"

#pragma pack(push, 1)
struct BUFFERED_COMMAND_TEXTDRAW
{
    uint16_t textdrawId;
};
#pragma pack(pop)

class DefenseButton;

class UI : public Widget, public ImGuiWrapper
{
public:
	UI(const ImVec2& display_size, const std::string& font_path);
    UI(const ImVec2& display_size, const std::vector<char>& fontData);

	bool initialize() override;
	void render() override;
	void shutdown() override;

	SplashScreen* splashscreen() const { return m_splashScreen; }
	Chat* chat() const { return m_chat; }
	Spawn* spawn() const { return m_spawn; }
	Keyboard* keyboard() const { return m_keyboard; }
	Dialog* dialog() const { return m_dialog; }
	ButtonPanel* buttonpanel() const { return m_buttonPanel; }
	PlayerTabList* playertablist() const { return m_playerTabList; }
	VoiceButton* voicebutton() const { return m_voiceButton; }

	static const ImColor fixcolor(uint32_t color) {
		return ImColor(
			(int)((color & 0xFF000000) >> 24),
			(int)((color & 0x00FF0000) >> 16),
			(int)((color & 0x0000FF00) >> 8));
	}

	float ScaleX(float x) {
		return x * displaySize().x * (1.0f / 1920.0f);
	}

	float ScaleY(float y) {
		return y * displaySize().y * (1.0f / 1080.0f);
	}

	float GetFontSize() {
		return UISettings::fontSize();
	}

	virtual void touchEvent(const ImVec2& pos, TouchType type) override;
	void HandleDefenseTouchEvent(int type, int pointerId, int x, int y);
	void ToggleDefenseButtonEditor();

	bool OnTouchEvent(int type, bool multi, int x, int y);

    void renderDebug();

    void ProcessPushedTextdraws();
    void PushToBufferedQueueTextDrawPressed(uint16_t textdrawId);

protected:
	void drawList() override;

private:
	void FinishDefenseButtonEditor(bool save);

	Keyboard* m_keyboard;
	SplashScreen* m_splashScreen;
	Chat* m_chat;
	Spawn* m_spawn;
	Dialog* m_dialog;
	ButtonPanel* m_buttonPanel;
	PlayerTabList* m_playerTabList;
	VoiceButton* m_voiceButton;
	DefenseButton* m_defenseButton = nullptr;
	Button* m_defenseScaleDown = nullptr;
	Button* m_defenseScaleUp = nullptr;
	Button* m_defenseSave = nullptr;
	Button* m_defenseCancel = nullptr;
	Label* m_defenseEditorHelp = nullptr;
	std::unordered_set<int> m_defenseTouchIds;
	bool m_defenseEditorActive = false;
	int m_defenseDragPointer = -1;
	ImVec2 m_defenseDefaultPosition = ImVec2(0.0f, 0.0f);
	ImVec2 m_defenseEditPosition = ImVec2(0.0f, 0.0f);
	ImVec2 m_defenseLastTouch = ImVec2(0.0f, 0.0f);
	float m_defenseEditScale = 1.0f;
	Label* label;
    Label* label2;
    Label* label3;
    Label* label4;

	bool m_bNeedClearMousePos = false;

    DataStructures::SingleProducerConsumer<BUFFERED_COMMAND_TEXTDRAW> m_BufferedCommandTextdraws;
};