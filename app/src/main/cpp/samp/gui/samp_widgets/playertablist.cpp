#include "../gui.h"
#include "../../game/game.h"
#include "../../net/netgame.h"
#include "../../main.h"


#include <iomanip>

extern UI* pUI;
extern CGame* pGame;
extern CNetGame* pNetGame;

PlayerTabList::PlayerTabList()
{
	m_tabList = nullptr;
	m_lServerName = nullptr;
	m_lTotalPlayers = nullptr;
}

void PlayerTabList::show()
{
	if (visible()) return;
	if (!pNetGame) return;

	pNetGame->UpdatePlayerScoresAndPings();

	m_lServerName = new Label("", ImColor(0.94f, 0.71f, 0.16f), false, UISettings::fontSize() / 1.7f);
	this->addChild(m_lServerName);

	m_lTotalPlayers = new Label("", ImColor(0.7f, 0.7f, 0.7f), false, UISettings::fontSize() / 2.2f);
	this->addChild(m_lTotalPlayers);

	m_tabList = new TabListWidget();
	this->addChild(m_tabList);

	this->assemble();
	this->performLayout();

	pGame->EnableGameInput(false);
	pUI->keyboard()->setVisible(false);


	setVisible(true);
}

void PlayerTabList::hide()
{

	if (m_tabList != nullptr) {
		this->removeChild(m_tabList);
	}

	if (m_lServerName != nullptr) {
		this->removeChild(m_lServerName);
	}

	if (m_lTotalPlayers != nullptr) {
		this->removeChild(m_lTotalPlayers);
	}

	pGame->EnableGameInput(true);
	setVisible(false);
}

std::string int_to_hex(int val)
{
	char hex[20];
	sprintf(hex, "%02X", val);
	return std::string(hex);
}

void PlayerTabList::assemble()
{
	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();

	std::stringstream ss;
	ss << "Players: " << pPlayerPool->GetTotalPlayers();

	/*  */
	m_lServerName->setText(Encoding::cp2utf(pNetGame->m_szHostName));
	m_lTotalPlayers->setText(ss.str());

	std::stringstream data;
	/* head */
	data << "ID\tNickname\tScore\tPing\n";

	/* local player */
	ImColor lc = UI::fixcolor(pLocalPlayer->GetPlayerColorAsRGBA());
	std::string localColorTag = "{" +
		int_to_hex((int)(lc.Value.x * 255.0f)) +
		int_to_hex((int)(lc.Value.y * 255.0f)) +
		int_to_hex((int)(lc.Value.z * 255.0f)) + "}";

	data <<
		/* playerid */
		localColorTag << pPlayerPool->GetLocalPlayerID() << "\t" <<
		/* name */
		localColorTag << pPlayerPool->GetLocalPlayerName() << "\t" <<
		/* score */
		localColorTag << pPlayerPool->GetLocalPlayerScore() << "\t" <<
		/* ping */
		localColorTag << pPlayerPool->GetLocalPlayerPing() << "\n";

	std::string remoteColorTag;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// remote player активен 
		if (pPlayerPool->GetAt(i) && !pPlayerPool->IsPlayerNPC(i))
		{
			ImColor rc = UI::fixcolor(pPlayerPool->GetAt(i)->GetPlayerColor());
			remoteColorTag = "{" +
				int_to_hex((int)(rc.Value.x * 255.0f)) +
				int_to_hex((int)(rc.Value.y * 255.0f)) +
				int_to_hex((int)(rc.Value.z * 255.0f)) + "}";

			data <<
				/* playerid */
				remoteColorTag << i << "\t" <<
				/* name */
				remoteColorTag << pPlayerPool->GetPlayerName(i) << "\t" <<
				/* score */
				remoteColorTag << pPlayerPool->GetPlayerScore(i) << "\t" <<
				/* ping */
				remoteColorTag << pPlayerPool->GetPlayerPing(i) << "\n";
		}
	}

	m_tabList->assemble(data.str());
}

void PlayerTabList::performLayout()
{
	if (m_tabList == nullptr) return;

	float header_height = UISettings::padding() + UISettings::fontSize() + UISettings::padding();

	m_tabList->setMinSize(this->minSize() - ImVec2(0.0f, header_height));
	m_tabList->setMaxSize(this->maxSize() - ImVec2(0.0f, header_height));
	m_tabList->performLayout();

	m_lServerName->performLayout();
	m_lServerName->setPosition(ImVec2(UISettings::padding(), UISettings::padding()));

	m_lTotalPlayers->performLayout();
	m_lTotalPlayers->setPosition(ImVec2(m_tabList->width() - (m_lTotalPlayers->width() + UISettings::padding()), UISettings::padding()));

	m_tabList->setPosition(ImVec2(0.0f, header_height));

	FLog("m_tabList size: %d, %d", m_tabList->size().x, m_tabList->size().y);

	this->setSize(m_tabList->size() + ImVec2(0.0f, header_height));

	this->setPosition(ImVec2(
		(parent()->width() - width()) / 2,
		(parent()->height() - height()) / 2));
}

void PlayerTabList::draw(ImGuiRenderer* renderer)
{
	renderer->pushClipRect(absolutePosition(), absolutePosition() + size(), true);
	// Deep dark slate solid background
	renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(11, 15, 25, 240), true);
	// Crisp cyber-blue outer border
	renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(2, 132, 199, 255), false, 1.8f);
	// Top header separator line
	float header_height = UISettings::padding() + UISettings::fontSize() + UISettings::padding();
	renderer->drawLine(absolutePosition() + ImVec2(0.0f, header_height), absolutePosition() + ImVec2(size().x, header_height), ImColor(56, 189, 248, 160), 1.2f);
	Widget::draw(renderer);
	renderer->popClipRect();
}

void PlayerTabList::activateEvent(bool active)
{
	if (active == false) {
		this->hide();
	}
}