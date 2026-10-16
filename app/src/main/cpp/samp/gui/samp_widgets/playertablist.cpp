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

	// PC SA-MP: hostname merah (255,0,0)
	m_lServerName = new Label("", ImColor(1.0f, 0.0f, 0.0f), false, UISettings::fontSize() / 1.7f);
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

	// PC SA-MP: "Players: X-Y of Z" abu-abu (150,150,150), rata kanan
	int totalPlayers = pPlayerPool->GetTotalPlayers();
	std::stringstream ss;
	ss << "Players: 1-" << totalPlayers << " of " << totalPlayers;

	/*  */
	m_lServerName->setText(Encoding::cp2utf(pNetGame->m_szHostName));
	m_lTotalPlayers->setText(ss.str());

	std::stringstream data;
	/* head - PC SA-MP: "id", "name", "score", "ping" biru muda (100,150,200) */
	data << "{6496C8}id\t{6496C8}name\t{6496C8}score\t{6496C8}ping\n";

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
	renderer->drawRect(absolutePosition(), absolutePosition() + size(), UISettings::dialogBackgroundColor(), true);
	// PC SA-MP: area header (hostname) hitam solid, body semi-transparan
	float header_height = UISettings::padding() + UISettings::fontSize() + UISettings::padding();
	renderer->drawRect(absolutePosition(), absolutePosition() + ImVec2(size().x, header_height), ImColor(0.0f, 0.0f, 0.0f, 1.0f), true);
	Widget::draw(renderer);
	renderer->popClipRect();
}

void PlayerTabList::activateEvent(bool active)
{
	if (active == false) {
		this->hide();
	}
}