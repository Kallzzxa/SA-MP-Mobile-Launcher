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
	ImVec2 screenSize = parent() ? parent()->size() : size();
	if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
		screenSize = ImGui::GetIO().DisplaySize;
	}

	// 1. Center the SA-MP Logo perfectly in the screen
	// Base aspect ratio of SA-MP logo is ~4:3 or ~16:9 box
	float logoWidth = screenSize.x * 0.55f;
	float logoHeight = logoWidth * 0.45f;
	if (logoHeight > screenSize.y * 0.45f) {
		logoHeight = screenSize.y * 0.45f;
		logoWidth = logoHeight / 0.45f;
	}

	ImVec2 logoPos = ImVec2(
		(screenSize.x - logoWidth) * 0.5f,
		(screenSize.y - logoHeight) * 0.42f
	);

	m_logo->setFixedSize(ImVec2(logoWidth, logoHeight));
	m_logo->setPosition(logoPos);

	// 2. Center the loading progress bar horizontally below the logo
	float pbarWidth = screenSize.x * 0.40f;
	float pbarHeight = 6.0f;
	ImVec2 pbarPos = ImVec2(
		(screenSize.x - pbarWidth) * 0.5f,
		logoPos.y + logoHeight + 25.0f
	);

	m_progressBar->setFixedSize(ImVec2(pbarWidth, pbarHeight));
	m_progressBar->setPosition(pbarPos);
}

void SplashScreen::draw(ImGuiRenderer* renderer)
{
	// background
	renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(0.0f, 0.0f, 0.0f), true);
	// logo
	m_logo->draw(renderer);

	// Auto-advancing smooth loading progress
	static float currentProgress = 0.08f;
	if (currentProgress < 1.0f) {
		currentProgress += 0.015f;
		if (currentProgress > 1.0f) currentProgress = 1.0f;
	}

	// Direct vector rendering for guaranteed visible & moving loading bar
	ImVec2 pbPos = m_progressBar->absolutePosition();
	ImVec2 pbSize = m_progressBar->size();
	if (pbSize.x > 0.0f && pbSize.y > 0.0f)
	{
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		// Background slot
		drawList->AddRectFilled(pbPos, ImVec2(pbPos.x + pbSize.x, pbPos.y + pbSize.y), IM_COL32(24, 30, 42, 220), 3.0f);
		// Animated progress fill (Electric cyber blue)
		float filledWidth = pbSize.x * currentProgress;
		if (filledWidth > 0.0f) {
			drawList->AddRectFilled(pbPos, ImVec2(pbPos.x + filledWidth, pbPos.y + pbSize.y), IM_COL32(2, 132, 199, 255), 3.0f);
			// Glowing edge tip
			drawList->AddCircleFilled(ImVec2(pbPos.x + filledWidth, pbPos.y + pbSize.y * 0.5f), 3.5f, IM_COL32(56, 189, 248, 255));
		}
	}
}