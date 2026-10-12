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
		/* 5:3 aspect ratio */
		m_texture_micro_on = (RwTexture*)CUtil::LoadTextureFromDB("samp", "voiceactive"); //default
		m_texture_micro_off = (RwTexture*)CUtil::LoadTextureFromDB("samp", "voicepassive"); //default
	}

	virtual void draw(ImGuiRenderer* renderer) override
	{
		if(!pSettings->Get().bVoiceChatEnable) return;

		if (countdown > 0 && recording() == 1) countdown--;
		if (countdown == 0 && recording() == 1) setRecording(0);

		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		ImVec2 pMin = absolutePosition();
		ImVec2 pMax = absolutePosition() + size();
		ImVec2 center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
		float radius = (size().x < size().y ? size().x : size().y) * 0.46f;

		if (recording()) {
			// Active recording: Glowing crimson disc
			drawList->AddCircle(center, radius + 2.5f, IM_COL32(239, 68, 68, 160), 32, 2.0f);
			drawList->AddCircleFilled(center, radius, IM_COL32(220, 38, 38, 240), 32);
			drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 220), 32, 1.5f);
		} else {
			// Idle: Deep slate navy circle with sleek electric blue rim
			drawList->AddCircleFilled(center, radius, IM_COL32(15, 23, 42, 215), 32);
			drawList->AddCircle(center, radius, IM_COL32(56, 189, 248, 130), 32, 1.5f);
		}

		// Centered crisp vector microphone glyph (No text labels)
		ImU32 glyphColor = IM_COL32(255, 255, 255, 245);
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
		countdown = 500;
		setRecording(0);
	}

	void touchPushEvent() override
	{
			//setRecording(recording() ^ 1);
		setRecording(1);
		countdown = 500;
	}

	void setRecording(bool recording) 
	{ 
		if (recording == 1) countdown = 200;
		m_recording = recording;
		this->setCaptionColor(m_recording ? ImColor(1.0f, 0.0f, 0.0f) : ImColor(1.0f, 1.0f, 1.0f));
	}

	bool recording() const { return m_recording; }

private:
	bool m_recording;
	RwTexture* m_texture_micro_on;
	RwTexture* m_texture_micro_off;
	//int countdown = 1000;
public:
	int countdown = 1000;
};