#pragma once
#include "../../game/game.h"
#include "../../game/util.h"

#include "../../main.h"

#include "../../settings.h"
#include "util/CUtil.h"

extern CSettings* pSettings;

class VoiceButton : public Button
{
public:
	VoiceButton() : Button("TALK", UISettings::fontSize() / 2) {
		m_recording = false;
		m_editMode = false;
		m_dragging = false;
		m_texture_micro_on = (RwTexture*)CUtil::LoadTextureFromDB("samp", "voiceactive");
		m_texture_micro_off = (RwTexture*)CUtil::LoadTextureFromDB("samp", "voicepassive");
	}

	void toggleEditMode()
	{
		m_editMode = !m_editMode;
		if (m_editMode) {
			setRecording(0);
		} else {
			if (pSettings) {
				pSettings->SaveVoiceButtonSettings();
			}
		}
	}

	bool isEditMode() const { return m_editMode; }

	virtual void draw(ImGuiRenderer* renderer) override
	{
		if(!pSettings->Get().bVoiceChatEnable) return;

		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		ImVec2 pMin = absolutePosition();
		ImVec2 pMax = absolutePosition() + size();
		ImVec2 center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
		float radius = (size().x < size().y ? size().x : size().y) * 0.46f;

		if (m_editMode) {
			// EDIT MODE: Distinct Amber / Cyan Pulsing Ring
			drawList->AddCircleFilled(center, radius, IM_COL32(15, 23, 42, 235), 32);
			drawList->AddCircle(center, radius + 3.0f, IM_COL32(250, 204, 21, 230), 32, 2.0f);
			drawList->AddCircle(center, radius, IM_COL32(56, 189, 248, 255), 32, 1.5f);
		} else if (recording()) {
			// ACTIVE RECORDING: Glowing Electric Cyber-Blue Disc (ZERO RED!)
			drawList->AddCircle(center, radius + 3.0f, IM_COL32(56, 189, 248, 180), 32, 2.5f);
			drawList->AddCircleFilled(center, radius, IM_COL32(2, 132, 199, 245), 32);
			drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 230), 32, 1.5f);
		} else {
			// IDLE: Deep slate navy circle with sleek electric blue rim
			drawList->AddCircleFilled(center, radius, IM_COL32(15, 23, 42, 215), 32);
			drawList->AddCircle(center, radius, IM_COL32(56, 189, 248, 140), 32, 1.5f);
		}

		// Centered crisp vector microphone glyph
		ImU32 glyphColor = (m_editMode) ? IM_COL32(250, 204, 21, 255) : IM_COL32(255, 255, 255, 245);
		float micScale = radius * 0.36f;

		// 1. Microphone Capsule (rounded body)
		ImVec2 capMin = ImVec2(center.x - micScale * 0.5f, center.y - micScale * 0.95f);
		ImVec2 capMax = ImVec2(center.x + micScale * 0.5f, center.y + micScale * 0.3f);
		drawList->AddRectFilled(capMin, capMax, glyphColor, micScale * 0.5f);

		// 2. Microphone U-cradle arc
		drawList->PathArcTo(ImVec2(center.x, center.y - micScale * 0.05f), micScale * 0.95f, 0.0f, 3.14159265f, 20);
		drawList->PathStroke(glyphColor, 0, 2.0f);

		// 3. Vertical stem
		float stemTop = center.y - micScale * 0.05f + micScale * 0.95f;
		float stemBottom = stemTop + micScale * 0.55f;
		drawList->AddLine(ImVec2(center.x, stemTop), ImVec2(center.x, stemBottom), glyphColor, 2.0f);

		// 4. Base foot
		drawList->AddLine(ImVec2(center.x - micScale * 0.7f, stemBottom), ImVec2(center.x + micScale * 0.7f, stemBottom), glyphColor, 2.0f);
	}

	void touchPopEvent() override
	{
		if (m_editMode) {
			m_dragging = false;
			if (pSettings) {
				pSettings->SaveVoiceButtonSettings();
			}
			return;
		}

		// In normal mode: stop recording IMMEDIATELY when finger lifts!
		setRecording(0);
	}

	void touchPushEvent() override
	{
		if (m_editMode) {
			m_dragging = true;
			return;
		}

		// In normal mode: start recording
		setRecording(1);
	}

	void touchMoveEvent(const ImVec2& delta) override
	{
		if (!m_editMode) {
			// In normal mode, button is firmly locked in position!
			return;
		}

		// In edit mode: smoothly drag and update position
		ImVec2 newPos = position() + delta;

		ImVec2 dispSize = ImGui::GetIO().DisplaySize;
		if (dispSize.x > 0.0f && dispSize.y > 0.0f) {
			newPos.x = std::clamp(newPos.x, 5.0f, dispSize.x - size().x - 5.0f);
			newPos.y = std::clamp(newPos.y, 5.0f, dispSize.y - size().y - 5.0f);
		}

		setPosition(newPos);

		if (pSettings) {
			pSettings->Get().fVoiceChatPosX = newPos.x;
			pSettings->Get().fVoiceChatPosY = newPos.y;
		}
	}

	void setRecording(bool recording) 
	{ 
		if (m_editMode && recording) return;
		m_recording = recording;
		this->setCaptionColor(m_recording ? ImColor(56, 189, 248) : ImColor(1.0f, 1.0f, 1.0f));
	}

	bool recording() const { return m_recording; }

private:
	bool m_recording{ false };
	bool m_editMode{ false };
	bool m_dragging{ false };
	RwTexture* m_texture_micro_on{ nullptr };
	RwTexture* m_texture_micro_off{ nullptr };
public:
	int countdown{ 0 };
};
