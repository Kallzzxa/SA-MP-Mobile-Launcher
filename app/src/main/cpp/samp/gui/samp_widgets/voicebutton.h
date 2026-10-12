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
			// Modern recording state: glowing dark-red circle with pulse border
			drawList->AddCircleFilled(center, radius, IM_COL32(239, 68, 68, 220), 32);
			drawList->AddCircle(center, radius + 2.0f, IM_COL32(255, 255, 255, 200), 32, 1.5f);
		} else {
			// Modern sleek dark glass badge
			drawList->AddCircleFilled(center, radius, IM_COL32(18, 19, 26, 200), 32);
			drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 50), 32, 1.2f);
		}

		// Vector microphone icon rendering
		float micW = radius * 0.35f;
		float micH = radius * 0.65f;
		ImVec2 micMin = ImVec2(center.x - micW, center.y - micH * 0.6f);
		ImVec2 micMax = ImVec2(center.x + micW, center.y + micH * 0.2f);
		ImU32 iconColor = recording() ? IM_COL32(255, 255, 255, 255) : IM_COL32(212, 212, 216, 230);

		// Capsule body
		drawList->AddRectFilled(micMin, micMax, iconColor, micW);
		// Arc stand
		drawList->AddCircle(ImVec2(center.x, center.y - micH * 0.1f), micW * 1.5f, iconColor, 16, 1.5f);
		// Bottom stem
		drawList->AddLine(ImVec2(center.x, center.y + micH * 0.25f), ImVec2(center.x, center.y + micH * 0.65f), iconColor, 2.0f);
		// Base plate
		drawList->AddLine(ImVec2(center.x - micW, center.y + micH * 0.65f), ImVec2(center.x + micW, center.y + micH * 0.65f), iconColor, 2.0f);
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