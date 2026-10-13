#include "../gui.h"
#include "../../main.h"

SplashScreen::SplashScreen()
{
	int x = rand() % 2;
	switch (x)
	{
		case 0:
			m_logo = new Image("menu", "menu_adjback");
			break;

		case 1:
			m_logo = new Image("menu", "menu_adjback2");
			break;
	}

	//-----------//
	//m_logo = new Image("samp", "mylogo");
	//m_logo = new Image("fm", "mylogo");
	this->addChild(m_logo);

	m_progressBar = new ProgressBar(ImColor(0.0f, 0.0f, 0.0f),
		ImColor(190, 0, 0), ImColor(255, 100, 0),
		ImColor(255, 100, 0), ImColor(190, 0, 0));
	this->addChild(m_progressBar);
}

void SplashScreen::performLayout()
{
}

void SplashScreen::draw(ImGuiRenderer* renderer)
{
	ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
		screenSize = size();
	}
	if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) return;

	// Fill background solid black
	renderer->drawRect(ImVec2(0.0f, 0.0f), screenSize, ImColor(0, 0, 0, 255), true);

	// 1. Center the SA-MP Logo perfectly in the screen
	float logoWidth = screenSize.x * 0.50f;
	float logoHeight = logoWidth * 0.42f;
	if (logoHeight > screenSize.y * 0.40f) {
		logoHeight = screenSize.y * 0.40f;
		logoWidth = logoHeight / 0.42f;
	}

	float logoX = (screenSize.x - logoWidth) * 0.5f;
	float logoY = (screenSize.y - logoHeight) * 0.40f;

	m_logo->setPosition(ImVec2(logoX, logoY));
	m_logo->setFixedSize(ImVec2(logoWidth, logoHeight));
	m_logo->draw(renderer);

	// 2. Center the loading progress bar directly below the logo
	float pbWidth = screenSize.x * 0.38f;
	float pbHeight = 6.0f;
	float pbX = (screenSize.x - pbWidth) * 0.5f;
	float pbY = logoY + logoHeight + 24.0f;

	// Auto-advancing smooth loading progress
	static float currentProgress = 0.08f;
	if (currentProgress < 1.0f) {
		currentProgress += 0.015f;
		if (currentProgress > 1.0f) currentProgress = 1.0f;
	}

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	// Background slot
	drawList->AddRectFilled(ImVec2(pbX, pbY), ImVec2(pbX + pbWidth, pbY + pbHeight), IM_COL32(24, 30, 42, 220), 3.0f);
	// Animated progress fill (Electric cyber blue)
	float filledWidth = pbWidth * currentProgress;
	if (filledWidth > 0.0f) {
		drawList->AddRectFilled(ImVec2(pbX, pbY), ImVec2(pbX + filledWidth, pbY + pbHeight), IM_COL32(2, 132, 199, 255), 3.0f);
		// Glowing edge tip
		drawList->AddCircleFilled(ImVec2(pbX + filledWidth, pbY + pbHeight * 0.5f), 3.5f, IM_COL32(56, 189, 248, 255));
	}
}