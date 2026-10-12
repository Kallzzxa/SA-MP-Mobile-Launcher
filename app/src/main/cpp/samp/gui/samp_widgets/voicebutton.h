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
		float width = size().x;
		float height = size().y;
		float rounding = width * 0.35f;

		if (recording()) {
			drawList->AddRectFilled(pMin, pMax, IM_COL32(220, 38, 38, 235), rounding);
			drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 220), rounding, 0, 1.5f);
		} else {
			drawList->AddRectFilled(pMin, pMax, IM_COL32(15, 17, 24, 205), rounding);
			drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 45), rounding, 0, 1.2f);
		}

		// Centered microphone glyph
		float centerX = (pMin.x + pMax.x) * 0.5f;
		float centerY = pMin.y + height * 0.42f;
		float micScale = width * 0.22f;

		ImU32 glyphColor = IM_COL32(255, 255, 255, 245);

		// 1. Microphone Capsule (rounded body)
		ImVec2 capMin = ImVec2(centerX - micScale * 0.7f, centerY - micScale * 1.5f);
		ImVec2 capMax = ImVec2(centerX + micScale * 0.7f, centerY + micScale * 0.3f);
		drawList->AddRectFilled(capMin, capMax, glyphColor, micScale * 0.7f);

		// 2. Microphone U-cradle arc
		drawList->PathArcTo(ImVec2(centerX, centerY - micScale * 0.1f), micScale * 1.35f, 0.0f, 3.14159265f, 16);
		drawList->PathStroke(glyphColor, 0, 2.0f);

		// 3. Vertical stem
		float stemTop = centerY - micScale * 0.1f + micScale * 1.35f;
		float stemBottom = stemTop + micScale * 0.8f;
		drawList->AddLine(ImVec2(centerX, stemTop), ImVec2(centerX, stemBottom), glyphColor, 2.0f);

		// 4. Base foot
		drawList->AddLine(ImVec2(centerX - micScale * 1.0f, stemBottom), ImVec2(centerX + micScale * 1.0f, stemBottom), glyphColor, 2.0f);

		// 5. Small crisp label
		const char* labelText = recording() ? "REC" : "MIC";
		ImVec2 textSize = ImGui::CalcTextSize(labelText);
		ImVec2 textPos = ImVec2(centerX - textSize.x * 0.5f, pMax.y - textSize.y - 4.0f);
		drawList->AddText(textPos, glyphColor, labelText);
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