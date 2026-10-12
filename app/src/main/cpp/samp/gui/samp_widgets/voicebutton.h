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
		float radius = (size().x < size().y ? size().x : size().y) * 0.44f;

		if (recording()) {
			// Recording state: Premium glowing red gradient
			drawList->AddCircleFilled(center, radius + 3.0f, IM_COL32(239, 68, 68, 80), 32);
			drawList->AddCircleFilled(center, radius, IM_COL32(220, 38, 38, 240), 32);
			drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 220), 32, 1.8f);
		} else {
			// Sleek dark frosted glass disc
			drawList->AddCircleFilled(center, radius, IM_COL32(18, 20, 28, 225), 32);
			drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 45), 32, 1.2f);
		}

		// Professional Microphone Glyph Design
		ImU32 glyphColor = recording() ? IM_COL32(255, 255, 255, 255) : IM_COL32(244, 244, 245, 230);
		float capW = radius * 0.28f;
		float capTop = center.y - radius * 0.48f;
		float capBottom = center.y + radius * 0.08f;

		// 1. Microphone Capsule (rounded capsule with smooth caps)
		drawList->AddRectFilled(ImVec2(center.x - capW, capTop), ImVec2(center.x + capW, capBottom), glyphColor, capW);

		// 2. Microphone U-Shaped Cradle / Arc
		float arcR = capW * 1.65f;
		float arcCenterY = capBottom - capW * 0.5f;
		// Semi-circle cradle arc
		drawList->PathArcTo(ImVec2(center.x, arcCenterY), arcR, 0.0f, 3.14159265f, 20);
		drawList->PathStroke(glyphColor, 0, 2.0f);

		// 3. Central Vertical Stem
		float stemTop = arcCenterY + arcR;
		float stemBottom = stemTop + radius * 0.26f;
		drawList->AddLine(ImVec2(center.x, stemTop), ImVec2(center.x, stemBottom), glyphColor, 2.2f);

		// 4. Sturdy Base Foot
		float baseHalfW = capW * 1.35f;
		drawList->AddLine(ImVec2(center.x - baseHalfW, stemBottom), ImVec2(center.x + baseHalfW, stemBottom), glyphColor, 2.2f);
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