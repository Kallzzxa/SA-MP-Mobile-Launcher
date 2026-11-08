#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "../gui/gui.h"
#include "../vendor/encoding/encoding.h"
#include "../settings.h"
#include "dl/dlmanager.h"
#include <cstdio>
#include <climits>
#include <cmath>

extern UI* pUI;
extern CGame *pGame;
extern CNetGame *pNetGame;
extern CSettings *pSettings;
//extern CVoice* pVoice;

extern int iNetModeNormalOnFootSendRate;
extern int iNetModeNormalInCarSendRate;
extern int iNetModeFiringSendRate;
extern int iNetModeSendMultiplier;

int g_iLagCompensationMode = 0;

#define REJECT_REASON_BAD_VERSION   1
#define REJECT_REASON_BAD_NICKNAME  2
#define REJECT_REASON_BAD_MOD		3
#define REJECT_REASON_BAD_PLAYERID	4

void ProcessIncommingEvent(PLAYERID playerID, int iEventType,
	uint32_t dwParam1, uint32_t dwParam2, uint32_t dwParam3);


void InitGame(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] InitGame");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->m_pNetSet || !pGame || !pNetGame->GetPlayerPool() ||
		!pNetGame->GetPlayerPool()->GetLocalPlayer()) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	CNetGame::NET_SETTINGS settings{};
	PLAYERID MyPlayerID = 0;
	int iLagCompensation = 0;
	int onFootSendRate = 0, inCarSendRate = 0, firingSendRate = 0, sendMultiplier = 0;
	bool bStuntBonus = false, bLanMode = false;
	uint8_t byteStrLen = 0, byteVehicleModels[212]{};
	char hostName[sizeof(pNetGame->m_szHostName)]{};
	RakNet::BitStream bsInitGame(Data, (iBitLength + 7) / 8, false);
	bsInitGame.SetWriteOffset(iBitLength);

	if (!bsInitGame.ReadCompressed(settings.bZoneNames) ||
		!bsInitGame.ReadCompressed(settings.bUseCJWalk) ||
		!bsInitGame.ReadCompressed(settings.bAllowWeapons) ||
		!bsInitGame.ReadCompressed(settings.bLimitGlobalChatRadius) ||
		!bsInitGame.Read(settings.fGlobalChatRadius) ||
		!bsInitGame.ReadCompressed(bStuntBonus) ||
		!bsInitGame.Read(settings.fNameTagDrawDistance) ||
		!bsInitGame.ReadCompressed(settings.bDisableInteriorEnterExits) ||
		!bsInitGame.ReadCompressed(settings.bNameTagLOS) ||
		!bsInitGame.ReadCompressed(settings.bManualVehicleEngineAndLight) ||
		!bsInitGame.Read(settings.iSpawnsAvailable) ||
		!bsInitGame.Read(MyPlayerID) ||
		!bsInitGame.ReadCompressed(settings.bShowNameTags) ||
		!bsInitGame.Read(settings.iShowPlayerMarkers) ||
		!bsInitGame.Read(settings.byteWorldTime_Hour) ||
		!bsInitGame.Read(settings.byteWeather) ||
		!bsInitGame.Read(settings.fGravity) ||
		!bsInitGame.ReadCompressed(bLanMode) ||
		!bsInitGame.Read(settings.iDeathDropMoney) ||
		!bsInitGame.ReadCompressed(settings.bInstagib) ||
		!bsInitGame.Read(onFootSendRate) || !bsInitGame.Read(inCarSendRate) ||
		!bsInitGame.Read(firingSendRate) || !bsInitGame.Read(sendMultiplier) ||
		!bsInitGame.Read(iLagCompensation) || !bsInitGame.Read(byteStrLen) ||
		(byteStrLen && !bsInitGame.Read(hostName, byteStrLen)) ||
		!bsInitGame.Read((char*)byteVehicleModels, sizeof(byteVehicleModels)) ||
		!bsInitGame.Read(settings.iVehicleFriendlyFire)) return;

	// Apply the initialization only after the entire RPC has been read.
	pNetGame->m_pNetSet->bZoneNames = settings.bZoneNames;
	pNetGame->m_pNetSet->bUseCJWalk = settings.bUseCJWalk;
	pNetGame->m_pNetSet->bAllowWeapons = settings.bAllowWeapons;
	pNetGame->m_pNetSet->bLimitGlobalChatRadius = settings.bLimitGlobalChatRadius;
	pNetGame->m_pNetSet->fGlobalChatRadius = settings.fGlobalChatRadius;
	pNetGame->m_pNetSet->fNameTagDrawDistance = settings.fNameTagDrawDistance;
	pNetGame->m_pNetSet->bDisableInteriorEnterExits = settings.bDisableInteriorEnterExits;
	pNetGame->m_pNetSet->bNameTagLOS = settings.bNameTagLOS;
	pNetGame->m_pNetSet->bManualVehicleEngineAndLight = settings.bManualVehicleEngineAndLight;
	pNetGame->m_pNetSet->iSpawnsAvailable = settings.iSpawnsAvailable;
	pNetGame->m_pNetSet->bShowNameTags = settings.bShowNameTags;
	pNetGame->m_pNetSet->iShowPlayerMarkers = settings.iShowPlayerMarkers;
	pNetGame->m_pNetSet->byteWorldTime_Hour = settings.byteWorldTime_Hour;
	pNetGame->m_pNetSet->byteWeather = settings.byteWeather;
	pNetGame->m_pNetSet->fGravity = settings.fGravity;
	pNetGame->m_pNetSet->iDeathDropMoney = settings.iDeathDropMoney;
	pNetGame->m_pNetSet->bInstagib = settings.bInstagib;
	pNetGame->m_pNetSet->iVehicleFriendlyFire = settings.iVehicleFriendlyFire;
	iNetModeNormalOnFootSendRate = onFootSendRate;
	iNetModeNormalInCarSendRate = inCarSendRate;
	iNetModeFiringSendRate = firingSendRate;
	iNetModeSendMultiplier = sendMultiplier;
	memcpy(pNetGame->m_szHostName, hostName, sizeof(hostName));

	Log::addParameter("bUseCJWalk", pNetGame->m_pNetSet->bUseCJWalk);
	Log::addParameter("bZoneNames", pNetGame->m_pNetSet->bZoneNames);
	Log::addParameter("bAllowWeapons", pNetGame->m_pNetSet->bAllowWeapons);
	Log::addParameter("bLimitGlobalChatRadius", pNetGame->m_pNetSet->bLimitGlobalChatRadius);
	Log::addParameter("fGlobalChatRadius", pNetGame->m_pNetSet->fGlobalChatRadius);
	Log::addParameter("bStuntBonus", bStuntBonus);
	Log::addParameter("fNameTagDrawDistance", pNetGame->m_pNetSet->fNameTagDrawDistance);
	Log::addParameter("bDisableInteriorEnterExits", pNetGame->m_pNetSet->bDisableInteriorEnterExits);
	Log::addParameter("bNameTagLOS", pNetGame->m_pNetSet->bNameTagLOS);
	Log::addParameter("bManualVehicleEngineAndLight", pNetGame->m_pNetSet->bManualVehicleEngineAndLight);
	Log::addParameter("iSpawnsAvailable", pNetGame->m_pNetSet->iSpawnsAvailable);
	Log::addParameter("MyPlayerID", MyPlayerID);
	Log::addParameter("bShowNameTags", pNetGame->m_pNetSet->bShowNameTags);
	Log::addParameter("iShowPlayerMarkers", pNetGame->m_pNetSet->iShowPlayerMarkers);
	Log::addParameter("byteWorldTime_Hour", pNetGame->m_pNetSet->byteWorldTime_Hour);
	Log::addParameter("byteWeather", pNetGame->m_pNetSet->byteWeather);
	Log::addParameter("fGravity", pNetGame->m_pNetSet->fGravity);
	Log::addParameter("bLanMode", bLanMode);
	Log::addParameter("iDeathDropMoney", pNetGame->m_pNetSet->iDeathDropMoney);
	Log::addParameter("bInstagib", pNetGame->m_pNetSet->bInstagib);
	Log::addParameter("iNetModeNormalOnFootSendRate", iNetModeNormalOnFootSendRate);
	Log::addParameter("iNetModeNormalInCarSendRate", iNetModeNormalInCarSendRate);
	Log::addParameter("iNetModeFiringSendRate", iNetModeFiringSendRate);
	Log::addParameter("iNetModeSendMultiplier", iNetModeSendMultiplier);
	Log::addParameter("iLagCompensation", iLagCompensation);

	switch (iLagCompensation)
	{
		case 1:
			g_iLagCompensationMode = 0;
			break;
		case 2:
			g_iLagCompensationMode = 1;
			break;
		default:
			g_iLagCompensationMode = 2;
			break;
	}

	//InstallWeaponFireHooks();
	if (pGame && pSettings && !pSettings->Get().bAutoAim) {
		//DisableAutoAim();
	}

	pPlayerPool->SetLocalPlayerID(MyPlayerID);
	pGame->EnableStuntBonus(bStuntBonus);

	if (bLanMode) {
		pNetGame->m_bLanMode = true;
	}

	pNetGame->InitGameLogic();
	pGame->SetGravity(pNetGame->m_pNetSet->fGravity);

	if (pNetGame->m_pNetSet->bDisableInteriorEnterExits) {
		pGame->DisableEnterExits();
	}

	if (pNetGame->m_pNetSet->iVehicleFriendlyFire) {
		//InstallHook_VehicleFriendlyFire();
	}

	pGame->SetWorldWeather(pNetGame->m_pNetSet->byteWeather);
	pGame->ToggleCJWalk(pNetGame->m_pNetSet->bUseCJWalk);

	Log::addParameter("szHostName", pNetGame->m_szHostName);
	Log::addParameter("iVehicleFriendlyFire", pNetGame->m_pNetSet->iVehicleFriendlyFire);

	if (pUI) pUI->chat()->addDebugMessage("Terhubung ke {B9C9BF}%.64s", Encoding::cp2utf(pNetGame->m_szHostName).c_str());

	pNetGame->SetGameState(GAMESTATE_CONNECTED);

	pPlayerPool->GetLocalPlayer()->HandleClassSelection();

	
	if (pUI) {
		pUI->voicebutton()->setVisible(true);
	}

	/*if (pVoice && pVoice->recorder()->active())
	{
		if (pUI) {
			pUI->voicebutton()->setVisible(true);
		}
	}
	else
	{
		//pUI->dialog()->show(-1, DialogStyle::MSGBOX, u8"Voice", u8"Voice chat not initalized. \nPlease allow the app to access the microphone!", u8"OK", "");
		// or
		pUI->chat()->addInfoMessage(u8"{ff0000}Voice chat not inited. Please allow the app to access the microphone!");
	}*/
}

#include "java/jniutil.h"
extern CJavaWrapper* pJavaWrapper;
int Button2Show;
void DialogBox(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] DialogBox");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 16) return;
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);

	int16_t wDialogID;
	uint8_t byteDialogStyle;
	uint8_t byteLength;
	char szTitle[257] = {};
	char szButton1[257] = {};
	char szButton2[257] = {};
	char szMessage[4097] = {};

	if (!bsData.Read(wDialogID)) return;
	if (wDialogID < 0) {
		if (pJavaWrapper) pJavaWrapper->HideDialog();
		return;
	}
	if (!bsData.Read(byteDialogStyle) || byteDialogStyle > 5) return;
	if (!bsData.Read(byteLength) || !bsData.Read(szTitle, byteLength)) return;
	if (!bsData.Read(byteLength) || !bsData.Read(szButton1, byteLength)) return;
	if (!bsData.Read(byteLength) || !bsData.Read(szButton2, byteLength)) return;
	if (!stringCompressor->DecodeString(szMessage, 4096, &bsData)) return;

	Log::addParameter("wDialogID", wDialogID);
	Log::addParameter("byteDialogStyle", (int)byteDialogStyle);
	Log::addParameter("szTitle", szTitle);
	Log::addParameter("szButton1", szButton1);
	Log::addParameter("szButton2", szButton2);
	Log::addParameter("szMessage", szMessage);
	
	if (pUI && pJavaWrapper)
	{
		if (strcmp(szButton2, "") == 0)
		{
			Button2Show = 2;
			//MyLog("lol %s", szButton2);
		}
		else
		{
			Button2Show = 1;
			//MyLog("lol2 %s", szButton2);
		}
		//pUI->dialog()->show(wDialogID, style, Encoding::cp2utf(szTitle), Encoding::cp2utf(szMessage), Encoding::cp2utf(szButton1), Encoding::cp2utf(szButton2));
		pJavaWrapper->ShowDialog(byteDialogStyle, wDialogID, (char*)Encoding::cp2utf(szTitle).c_str(), (char*) Encoding::cp2utf(szMessage).c_str(), (char*) Encoding::cp2utf(szButton1).c_str(), (char*) Encoding::cp2utf(szButton2).c_str());
	}

	return;
}

void Chat(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Chat");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	PLAYERID playerId;
	uint8_t byteMsgLen;
	char szMessage[256];
	memset(szMessage, 0, sizeof(szMessage));

	if (pNetGame->GetGameState() != GAMESTATE_CONNECTED) return;

	if (!bsData.Read(playerId) || !bsData.Read(byteMsgLen) || !bsData.Read(szMessage, byteMsgLen)) return;
	szMessage[byteMsgLen] = '\0';

	Log::addParameter("playerId", playerId);
	Log::addParameter("szMessage", szMessage);

	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	if (playerId == pPlayerPool->GetLocalPlayerID()) 
	{
		if (pUI) pUI->chat()->addChatMessage(Encoding::cp2utf(szMessage),
			pPlayerPool->GetLocalPlayerName(), UI::fixcolor(pPlayerPool->GetLocalPlayer()->GetPlayerColorAsRGBA()));
	}
	else {
		CRemotePlayer *pRemotePlayer = pPlayerPool->GetAt(playerId);
		if (pRemotePlayer) {
			pRemotePlayer->Say(szMessage);
		}
	}
}

void ClientMessage(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Client message ");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint32_t dwColor, dwStrLen;
	char szMsg[256];
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	
	memset(szMsg, 0, 256);
	if (!bsData.Read(dwColor) || !bsData.Read(dwStrLen)) return;

	Log::addParameter("dwColor", dwColor);
	Log::addParameter("dwStrLen", dwStrLen);

	if (dwStrLen <= 255)
	{
		if (!bsData.Read(szMsg, dwStrLen)) return;
		szMsg[dwStrLen] = '\0';
		Log::addParameter("szMsg", szMsg);
		if (pUI) pUI->chat()->addClientMessage(Encoding::cp2utf(szMsg), UI::fixcolor(dwColor));
	}
}

void RequestClass(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Request class");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool() || !pNetGame->GetPlayerPool()->GetLocalPlayer()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteRequestOutcome = 0;
	PLAYER_SPAWN_INFO SpawnInfo{};
	CLocalPlayer *pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	if (!bsData.Read(byteRequestOutcome)) return;
	if (!byteRequestOutcome) {
		pLocalPlayer->HandleClassSelectionOutcome(false);
		return;
	}
	if (bsData.GetNumberOfUnreadBits() >= (50 * 8)) {
		uint8_t byteTeam;
		int iSkin;
		uint32_t dCustomSkinID;
		uint8_t unk;
		CVector vecPos;
		float fRotation;
		int iSpawnWeapons[3];
		int iSpawnWeaponsAmmo[3];

		if (!bsData.Read(byteTeam) || !bsData.Read(iSkin) ||
			!bsData.Read(dCustomSkinID) || !bsData.Read(unk) ||
			!bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) ||
			!bsData.Read(vecPos.z) || !bsData.Read(fRotation) ||
			!bsData.Read((char*)iSpawnWeapons, sizeof(iSpawnWeapons)) ||
			!bsData.Read((char*)iSpawnWeaponsAmmo, sizeof(iSpawnWeaponsAmmo))) return;

		SpawnInfo.byteTeam = byteTeam;
		int actualSkin = (dCustomSkinID > 0) ? DLManager::Instance().GetVirtualModelId(dCustomSkinID) : iSkin;
		SpawnInfo.iSkin = IsValidPedModel(actualSkin) ? actualSkin : iSkin;
		SpawnInfo.unk = unk;
		SpawnInfo.vecPos = vecPos;
		SpawnInfo.fRotation = fRotation;
		memcpy(SpawnInfo.iSpawnWeapons, iSpawnWeapons, sizeof(iSpawnWeapons));
		memcpy(SpawnInfo.iSpawnWeaponsAmmo, iSpawnWeaponsAmmo, sizeof(iSpawnWeaponsAmmo));
	} else {
		if (!bsData.Read((char*)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO))) return;
	}

	if (!std::isfinite(SpawnInfo.vecPos.x) || !std::isfinite(SpawnInfo.vecPos.y) ||
		!std::isfinite(SpawnInfo.vecPos.z) || !std::isfinite(SpawnInfo.fRotation)) return;

	Log::addParameter("byteRequestOutcome", byteRequestOutcome);
	Log::addParameter("SpawnInfo.byteTeam", SpawnInfo.byteTeam);
	Log::addParameter("SpawnInfo.fRotation", SpawnInfo.fRotation);
	Log::addParameter("SpawnInfo.iSkin", SpawnInfo.iSkin);
	Log::addParameter("SpawnInfo.iSpawnWeapons", SpawnInfo.iSpawnWeapons);
	Log::addParameter("SpawnInfo.iSpawnWeaponsAmmo", SpawnInfo.iSpawnWeaponsAmmo);
	Log::addParameter("SpawnInfo.unk", SpawnInfo.unk);
	Log::addParameter("SpawnInfo.vecPos.x", SpawnInfo.vecPos.x);
	Log::addParameter("SpawnInfo.vecPos.y", SpawnInfo.vecPos.y);
	Log::addParameter("SpawnInfo.vecPos.z", SpawnInfo.vecPos.z);

	if (byteRequestOutcome) {
		pLocalPlayer->SetSpawnInfo(&SpawnInfo);
		pLocalPlayer->HandleClassSelectionOutcome(true);
	}
	else {
		pLocalPlayer->HandleClassSelectionOutcome(false);
	}
}

void RequestSpawn(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Request spawn");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool() || !pNetGame->GetPlayerPool()->GetLocalPlayer()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteRequestOutcome = false;
	CLocalPlayer *pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	if (!bsData.Read(byteRequestOutcome)) return;
	Log::addParameter("byteRequestOutcome", byteRequestOutcome);

	if (byteRequestOutcome == 2 || (byteRequestOutcome && pLocalPlayer->m_bWaitingForSpawnRequestReply)) {
		pLocalPlayer->Spawn();
	}
	else {
		pLocalPlayer->m_bWaitingForSpawnRequestReply = false;
	}
}

void ToggleClock(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Toggle clock");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteClock;

	if (!bsData.Read(byteClock)) return;
	Log::addParameter("byteClock", byteClock);

	if (byteClock) {
		pNetGame->m_pNetSet->byteHoldTime = false;
	}
	else {
		pNetGame->m_pNetSet->byteHoldTime = true;
		int iHour = 0, iMinute = 0;
		pGame->GetWorldTime(&iHour, &iMinute);
		pNetGame->m_pNetSet->byteWorldTime_Hour = static_cast<uint8_t>(iHour);
		pNetGame->m_pNetSet->byteWorldTime_Minute = static_cast<uint8_t>(iMinute);
	}
}

void SetTimeEx(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] SetTimeEx");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteHour;
	uint8_t byteMinute;

	if (!bsData.Read(byteHour) || !bsData.Read(byteMinute)) return;

	Log::addParameter("byteHour", byteHour);
	Log::addParameter("byteMinute", byteMinute);

	pGame->SetWorldTime(byteHour, byteMinute);
	pNetGame->m_pNetSet->byteWorldTime_Hour = byteHour;
	pNetGame->m_pNetSet->byteWorldTime_Minute = byteMinute;
}

void Weather(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Weather");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteWeather;
	
	if (!bsData.Read(byteWeather)) return;
	
	Log::addParameter("byteWeather", byteWeather);

	pNetGame->m_pNetSet->byteWeather = byteWeather;
	pGame->SetWorldWeather(byteWeather);
}

void WorldTime(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] World time");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint8_t byteWorldTIme;
	if (!bsData.Read(byteWorldTIme)) return;

	Log::addParameter("byteWorldTIme", byteWorldTIme);

	pNetGame->m_pNetSet->byteWorldTime_Hour = byteWorldTIme;
}

void ConnectionRejected(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Connection rejected");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteRejectReason;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteRejectReason)) return;

	Log::addParameter("byteRejectReason", byteRejectReason);

	switch (byteRejectReason)
	{
	case REJECT_REASON_BAD_VERSION:
		if (pUI) pUI->chat()->addInfoMessage("CONNECTION REJECTED: Incorrect Version.");
		break;

	case REJECT_REASON_BAD_NICKNAME:
		if (pUI) pUI->chat()->addInfoMessage("CONNECTION REJECTED: Unacceptable NickName");
		if (pUI) pUI->chat()->addInfoMessage("Please choose another nick between and 3-20 characters");
		if (pUI) pUI->chat()->addInfoMessage("Please use only a-z, A-z, 0-9");
		if (pUI) pUI->chat()->addInfoMessage("Use /quit to exit or press ESC and select Quit Game");
		pNetGame->SetGameState(GAMESTATE_WAIT_CONNECT);
		break;

	case REJECT_REASON_BAD_MOD:
		if (pUI) pUI->chat()->addInfoMessage("CONNECTION REJECTED: Bad mod version.");
		break;
	case REJECT_REASON_BAD_PLAYERID:
		if (pUI) pUI->chat()->addInfoMessage("CONNECTION REJECTED: Unable to allocate a player slot.");
		break;
	}

	pNetGame->GetRakClient()->Disconnect(500);
}

void GameModeRestart(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Gamemode restart");

	pNetGame->ShutdownForGameModeRestart();
}

void ServerJoin(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Server join");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	CRemotePlayer *pRemotePlayer = nullptr;
	uint8_t byteNameLen;
	uint8_t byteIsNPC;
	PLAYERID PlayerID;
	uint32_t dwMarkerColor;
	char szPlayerName[256];
	memset(szPlayerName, 0, sizeof(szPlayerName));

	if (!bsData.Read(PlayerID) || !bsData.Read(dwMarkerColor) ||
		!bsData.Read(byteIsNPC) || !bsData.Read(byteNameLen) ||
		!bsData.Read(szPlayerName, byteNameLen)) return;
	szPlayerName[byteNameLen] = '\0';

	Log::addParameter("PlayerID", PlayerID);
	Log::addParameter("dwMarkerColor", dwMarkerColor);
	Log::addParameter("byteIsNPC", byteIsNPC);
	Log::addParameter("szPlayerName", szPlayerName);

	if (strlen(szPlayerName) > MAX_PLAYER_NAME) {
		return;
	}

	pPlayerPool->New(PlayerID, szPlayerName, byteIsNPC);

	pRemotePlayer = pPlayerPool->GetAt(PlayerID);

	if (dwMarkerColor && pRemotePlayer) {
		pRemotePlayer->SetPlayerColor(dwMarkerColor);
	}
}

void ServerQuit(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Server quit");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	PLAYERID playerId;
	uint8_t byteReason;
	if (!bsData.Read(playerId) || !bsData.Read(byteReason)) return;

	Log::addParameter("playerId", playerId);
	Log::addParameter("byteReason", byteReason);

	pNetGame->GetPlayerPool()->Delete(playerId, byteReason);
}

void WorldPlayerAdd(RPCParameters *rpcParams)
{
	FLog("[RPC-IN] World player add");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	CRemotePlayer *pRemotePlayer = nullptr;
	CPlayerPed *pPlayerPed = nullptr;
	PLAYERID playerId = 0;
	uint8_t byteTeam = 0;
	int iSkin = 0;
	CVector vecPos{};
	float fRotation = 0.0f;
	uint32_t dwColor = 0;
	uint8_t byteFightingStyle = 0;
	uint16_t wUnkData[11]{};

	if (!bsData.Read(playerId) || !bsData.Read(byteTeam) || !bsData.Read(iSkin)) return;
	if (bsData.GetNumberOfUnreadBits() >= (47 * 8)) {
		uint32_t dCustomSkinID = 0;
		if (!bsData.Read(dCustomSkinID)) return;
		if (dCustomSkinID > 0) {
			int16_t virtId = DLManager::Instance().GetVirtualModelId(dCustomSkinID);
			if (IsValidPedModel(virtId)) {
				iSkin = virtId;
			}
		}
	}
	if (!bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z) ||
		!bsData.Read(fRotation) || !bsData.Read(dwColor) ||
		!bsData.Read(byteFightingStyle) || !bsData.Read((char*)wUnkData, sizeof(wUnkData))) return;
	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z) ||
		!std::isfinite(fRotation)) return;

	Log::addParameter("playerId", playerId);
	Log::addParameter("byteTeam", byteTeam);
	Log::addParameter("iSkin", iSkin);
	Log::addParameter("vecPos.x", vecPos.x);
	Log::addParameter("vecPos.y", vecPos.y);
	Log::addParameter("vecPos.z", vecPos.z);
	Log::addParameter("fRotation", fRotation);
	Log::addParameter("dwColor", dwColor);
	Log::addParameter("byteFightingStyle", byteFightingStyle);
	Log::addParameter("wUnkData[0]", wUnkData[0]);
	Log::addParameter("wUnkData[1]", wUnkData[1]);
	Log::addParameter("wUnkData[2]", wUnkData[2]);
	Log::addParameter("wUnkData[3]", wUnkData[3]);
	Log::addParameter("wUnkData[4]", wUnkData[4]);
	Log::addParameter("wUnkData[5]", wUnkData[5]);
	Log::addParameter("wUnkData[6]", wUnkData[6]);
	Log::addParameter("wUnkData[7]", wUnkData[7]);
	Log::addParameter("wUnkData[8]", wUnkData[8]);
	Log::addParameter("wUnkData[9]", wUnkData[9]);
	Log::addParameter("wUnkData[10]", wUnkData[10]);

	if (pPlayerPool)
	{
		pRemotePlayer = pPlayerPool->GetAt(playerId);
		if (pRemotePlayer) 
		{
			if (pRemotePlayer->Spawn(byteTeam, iSkin, &vecPos, fRotation, dwColor, byteFightingStyle))
			{
                FLog("WorldPlayerAdd");
				pPlayerPed = pRemotePlayer->GetPlayerPed();
				if (pPlayerPed)
				{
					for (int i = 0; i < 11; i++) {
						//pPlayerPed->SetSkillLevel(i, wUnkData[i]);
					}
				}

				if (pPlayerPool->IsPlayerNPC(playerId)) {
					if (pPlayerPed) {
						pPlayerPed->SetImmunities(0, 1, 1, 0, 0);
					}
				}
			}
			else {
				if (pUI) pUI->chat()->addDebugMessage("Warning: Couldn't spawn player(%u)", playerId);
			}
		}
	}
}

void WorldPlayerRemove(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] World player remove");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 16 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	CRemotePlayer *pRemotePlayer = nullptr;
	PLAYERID playerId = 0;

	if (!bsData.Read(playerId)) return;
	Log::addParameter("playerId", playerId);

	pRemotePlayer = pPlayerPool->GetAt(playerId);
	if (pRemotePlayer) {
		pRemotePlayer->Remove();
	}
}

void WorldPlayerDeath(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] World player death");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 16 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	CRemotePlayer *pRemotePlayer = nullptr;
	PLAYERID playerId = 0;

	if (!bsData.Read(playerId)) return;
	Log::addParameter("playerId", playerId);

	pRemotePlayer = pPlayerPool->GetAt(playerId);
	if (pRemotePlayer) {
		pRemotePlayer->HandleDeath();
	}
}

void WorldVehicleAdd(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] World vehicle add");

	if (!rpcParams || !rpcParams->input ||
		rpcParams->numberOfBitsOfData < sizeof(NEW_VEHICLE) * 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetVehiclePool()) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	NEW_VEHICLE VehicleInfo;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	memset(&VehicleInfo, 0, sizeof(NEW_VEHICLE));

	if (!bsData.Read((char*)&VehicleInfo, sizeof(NEW_VEHICLE))) return;
	if (!std::isfinite(VehicleInfo.vecPos.x) || !std::isfinite(VehicleInfo.vecPos.y) ||
		!std::isfinite(VehicleInfo.vecPos.z) || !std::isfinite(VehicleInfo.fRotation) ||
		!std::isfinite(VehicleInfo.fHealth)) return;

	Log::addParameter("VehicleInfo.byteAddSiren", VehicleInfo.byteAddSiren);
	Log::addParameter("VehicleInfo.byteInterior", VehicleInfo.byteInterior);
	Log::addParameter("VehicleInfo.byteInteriorColor1", VehicleInfo.byteInteriorColor1);
	Log::addParameter("VehicleInfo.byteInreriorColor2", VehicleInfo.byteInreriorColor2);
	Log::addParameter("VehicleInfo.byteLightDamageStatus", VehicleInfo.byteLightDamageStatus);
	Log::addParameter("VehicleInfo.byteTireDamageStatus", VehicleInfo.byteTireDamageStatus);
	Log::addParameter("VehicleInfo.dwDoorDamageStatus", VehicleInfo.dwDoorDamageStatus);
	Log::addParameter("VehicleInfo.dwPanelDamageStatus", VehicleInfo.dwPanelDamageStatus);
	Log::addParameter("VehicleInfo.fHealth", VehicleInfo.fHealth);
	Log::addParameter("VehicleInfo.fRotation", VehicleInfo.fRotation);
	Log::addParameter("VehicleInfo.iVehicleType", VehicleInfo.iVehicleType);
	Log::addParameter("VehicleInfo.vecPos.x", VehicleInfo.vecPos.x);
	Log::addParameter("VehicleInfo.vecPos.y", VehicleInfo.vecPos.y);
	Log::addParameter("VehicleInfo.vecPos.z", VehicleInfo.vecPos.z);
	Log::addParameter("VehicleInfo.VehicleID", VehicleInfo.VehicleID);

	Log::addParameter("VehicleModinfo.byteModSlots", VehicleInfo.byteModSlots);
	Log::addParameter("VehicleModinfo.bytePaintJob", VehicleInfo.bytePaintJob);
	Log::addParameter("VehicleModinfo.iBodyColor1", VehicleInfo.iBodyColor1);
	Log::addParameter("VehicleModinfo.iBodyColor2", VehicleInfo.iBodyColor2);

	if (VehicleInfo.iVehicleType < 400 || VehicleInfo.iVehicleType > 611) return;

	pVehiclePool->New(&VehicleInfo);
	/*CVehicle* pVehicle = pVehiclePool->GetAt(VehicleInfo.VehicleID);
	if (!pVehicle) return;

	bool bIsTrain = false;

	if (VehicleInfo.iVehicleType == 537) {
		VehicleInfo.iVehicleType = 569;
		bIsTrain = true;
	}

	if (VehicleInfo.iVehicleType == 538) {
		VehicleInfo.iVehicleType = 570;
		bIsTrain = true;
	}

	if (bIsTrain)
	{
		VehicleInfo.VehicleID++;
		pVehiclePool->New(&VehicleInfo);
		VehicleInfo.VehicleID++;
		pVehiclePool->New(&VehicleInfo);
		VehicleInfo.VehicleID++;
		pVehiclePool->New(&VehicleInfo);
	}*/

	/*if (pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_CAR)
	{
		for (int i = 0; i < 14; i++)
		{
			uint8_t  byteMod = VehicleInfo.byteModSlots[i];
			if (byteMod) {
				pVehicle->AddComponent(byteMod + 999);
			}
		}

		if (VehicleInfo.bytePaintJob) {
			pVehicle->SetPaintJob(VehicleInfo.bytePaintJob - 1);
		}
	}

	if (VehicleInfo.iBodyColor1 != -1 || VehicleInfo.iBodyColor2 != -1) {
		pVehicle->SetColor(VehicleInfo.iBodyColor1, VehicleInfo.iBodyColor2);
	}*/
}

void WorldVehicleRemove(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] World vehicle remove");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 16 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetVehiclePool() || !pGame) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	VEHICLEID VehicleID = INVALID_VEHICLE_ID;
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();

	if (!bsData.Read(VehicleID)) return;

	Log::addParameter("VehicleID", VehicleID);

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
	if (!pPlayerPed) return;

	if (pPlayerPed->IsInVehicle())
	{
        CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
		PLAYERID MyVehicleID = pVehiclePool->FindIDFromGtaPtr(pGtaVehicle);

		if (MyVehicleID == VehicleID)
		{
			RwMatrix matPlayer = pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
			pPlayerPed->RemoveFromVehicleAndPutAt(
				matPlayer.pos.x,
				matPlayer.pos.y,
				matPlayer.pos.z);
		}
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(i);
		if (pRemotePlayer) 
		{
			CPlayerPed* pPlayerPed = pRemotePlayer->GetPlayerPed();

			if (pRemotePlayer->GetPlayerPed() &&
				pRemotePlayer->GetState() != PLAYER_STATE_NONE &&
				pRemotePlayer->m_pCurrentVehicle == pVehicle) 
			{
				pRemotePlayer->RemoveFromVehicle();
			}
		}
	}

	for (int i = 0; i < MAX_VEHICLES; i++)
	{
		CVehicle* pVeh = pVehiclePool->GetAt(i);
		if (pVeh) {
			if (pVeh->m_pTrailer == pVehicle)
			{
				pVeh->DetachTrailer();
				pVeh->SetTrailer(0);
			}
		}
	}

	int iModel = pVehicle->m_pVehicle->GetModelId();
	pVehiclePool->Delete(VehicleID);

	if (iModel == 537 || iModel == 538)
	{
		pVehiclePool->Delete(VehicleID + 1);
		pVehiclePool->Delete(VehicleID + 2);
		pVehiclePool->Delete(VehicleID + 3);
	}
}

void TimerUpdate(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Timer update");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < sizeof(uint32_t) * 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	uint32_t dwTime = 0;

	if (!bsData.Read(dwTime)) return;
	Log::addParameter("dwTime", dwTime);

	//pGame->UpdateGlobalTimer(dwTime);
}

void ScmEvent(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] SCM event");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 18 * 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	PLAYERID playerId = 0;
	int iEvent = 0;
	uint32_t dwParam1 = 0, dwParam2 = 0, dwParam3 = 0;

	if (!bsData.Read(playerId) || !bsData.Read(iEvent) || !bsData.Read(dwParam1) ||
		!bsData.Read(dwParam2) || !bsData.Read(dwParam3)) return;

	Log::addParameter("playerId", playerId);
	Log::addParameter("iEvent", iEvent);
	Log::addParameter("dwParam1", dwParam1);
	Log::addParameter("dwParam2", dwParam2);
	Log::addParameter("dwParam3", dwParam3);
	
	ProcessIncommingEvent(playerId, iEvent, dwParam1, dwParam2, dwParam3);
}

void UpdateScoresPingsIPs(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Update scores and pings");
	if (!rpcParams || !rpcParams->input || !pNetGame || !pNetGame->GetPlayerPool()) return;
	int bitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(reinterpret_cast<unsigned char*>(rpcParams->input), (bitLength + 7) / 8, false);
	CPlayerPool* players = pNetGame->GetPlayerPool();

	for (int i = 0; i < (bitLength / 8) / 10; ++i) {
		PLAYERID playerId;
		int score, ping;
		if (!data.Read(playerId) || !data.Read(score) || !data.Read(ping)) break;
		players->UpdatePlayerScore(playerId, score);
		players->UpdatePlayerPing(playerId, ping);
	}
	if (pUI && pUI->playertablist() && pUI->playertablist()->visible()) {
		pUI->playertablist()->refresh();
	}
}

void Pickup(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Pickup");

	if (!rpcParams || !rpcParams->input ||
		rpcParams->numberOfBitsOfData < (sizeof(int) + sizeof(PICKUP)) * 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPickupPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);

	int iIndex = 0;
	PICKUP Pickup{};
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iIndex) || !bsData.Read((char*)&Pickup, sizeof(PICKUP))) return;

	pNetGame->GetPickupPool()->New(&Pickup, iIndex);
}

void DestroyPickup(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Destroy pickup");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < sizeof(int) * 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPickupPool()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);

	int iIndex = 0;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iIndex)) return;

	pNetGame->GetPickupPool()->Destroy(iIndex);
}

void Create3DTextLabel(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Create 3D text label");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	C3DTextLabelPool* pTextLabelPool = pNetGame->GetTextLabelPool();
	if (pTextLabelPool == nullptr) return;

	uint16_t wLabelId;
	TEXT_LABEL label;
	char szBuffer[2048 + 1];
	memset(szBuffer, 0, sizeof(szBuffer));

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(wLabelId) || !bsData.Read(label.dwColor) || !bsData.Read(label.vecPos.x) || !bsData.Read(label.vecPos.y) || !bsData.Read(label.vecPos.z) || !bsData.Read(label.fDistance) || !bsData.Read(label.bTestLOS) || !bsData.Read(label.playerId) || !bsData.Read(label.vehicleId)) return;
	if (!std::isfinite(label.vecPos.x) || !std::isfinite(label.vecPos.y) ||
		!std::isfinite(label.vecPos.z) || !std::isfinite(label.fDistance)) return;

	stringCompressor->DecodeString(szBuffer, sizeof szBuffer, &bsData);
	label.text = szBuffer;

	if (wLabelId < MAX_TEXT_LABELS) {
		pTextLabelPool->NewLabel(wLabelId, &label);
	}
}

void Update3DTextLabel(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Update 3D text label");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	C3DTextLabelPool* pTextLabelPool = pNetGame->GetTextLabelPool();
	if (pTextLabelPool == nullptr)  return;

	uint16_t wLabelId;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(wLabelId)) return;
	
	if (pTextLabelPool->GetSlotState(wLabelId)) {
			pTextLabelPool->ClearLabel(wLabelId);
	}
}

void SetCheckpoint(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Set checkpoint");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecPos;
	float fSize;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z) || !bsData.Read(fSize)) return;
	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z) ||
		!std::isfinite(fSize)) return;

	CVector vecSize;
	vecSize.x = fSize;
	vecSize.y = fSize;
	vecSize.z = fSize;

	pGame->SetCheckpointInformation(&vecPos, &vecSize);
	pGame->m_bCheckpointsEnabled = true;
}

void DisableCheckpoint(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Disable checkpoint");

	pGame->m_bCheckpointsEnabled = false;
}

void SetRaceCheckpoint(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Set race checkpoint");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteType;
	CVector vecPos;
	CVector vecNextPos;
	float fRadius;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteType) || !bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z) || !bsData.Read(vecNextPos.x) || !bsData.Read(vecNextPos.y) || !bsData.Read(vecNextPos.z) || !bsData.Read(fRadius)) return;
	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z) ||
		!std::isfinite(vecNextPos.x) || !std::isfinite(vecNextPos.y) || !std::isfinite(vecNextPos.z) ||
		!std::isfinite(fRadius)) return;

	pGame->SetRaceCheckpointInformation(byteType, &vecPos, &vecNextPos, fRadius);
	pGame->m_bRaceCheckpointsEnabled = true;
}

void DisableRaceCheckpoint(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Disable race checkpoint");

	pGame->m_bRaceCheckpointsEnabled = false;
}

void DamageVehicle(RPCParameters *rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Damage vehicle");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetVehiclePool()) return;

	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);
	RakNet::BitStream bsData(reinterpret_cast<unsigned char*>(rpcParams->input), (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	VEHICLEID vehId = INVALID_VEHICLE_ID;
	if (!bsData.Read(vehId)) return;
	if (pNetGame->GetVehiclePool()->GetSlotState(vehId))
	{
		uint32_t dwPanelStatus, dwDoorStatus;
		uint8_t byteLightStatus, byteTireStatus;

		if (!bsData.Read(dwPanelStatus) || !bsData.Read(dwDoorStatus) || !bsData.Read(byteLightStatus) || !bsData.Read(byteTireStatus)) return;

		CVehicle* pVehicle = pNetGame->GetVehiclePool()->GetAt(vehId);
		if (pVehicle)
		{
			pVehicle->SetWheelPoppedStatus(byteTireStatus);
			pVehicle->UpdateDamageStatus(dwPanelStatus, dwDoorStatus, byteLightStatus);
		}
	}
}

void SetVehicleTireStatus(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	VEHICLEID vehicleId;
	uint8_t byteTire;

	if (!bsData.Read(vehicleId)) return;
	if(vehicleId < 0 || vehicleId >= MAX_VEHICLES)
		return;

	if (!bsData.Read(byteTire)) return;

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool)
	{
		CVehicle *pVehicle = pVehiclePool->GetAt(vehicleId);
		if(pVehicle)
			pVehicle->SetWheelPoppedStatus(byteTire);
	}
}


void EnterVehicle(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Enter vehicle");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	VEHICLEID VehicleID;
	uint8_t bytePassenger;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID)) return;
    if(PlayerID < 0 || PlayerID >= MAX_PLAYERS)
        return;

	if (!bsData.Read(VehicleID)) return;
    if(VehicleID < 0 || VehicleID >= MAX_VEHICLES)
        return;

	if (!bsData.Read(bytePassenger)) return;

	if (bytePassenger) {
		bytePassenger = 1;
	}

	CRemotePlayer* pRemotePlayer = pNetGame->GetPlayerPool()->GetAt(PlayerID);
	if (pRemotePlayer) {
		pRemotePlayer->EnterVehicle(VehicleID, bytePassenger);
	}
}

void ExitVehicle(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Exit vehicle");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	VEHICLEID VehicleID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(VehicleID)) return;

	CRemotePlayer* pRemotePlayer = pNetGame->GetPlayerPool()->GetAt(PlayerID);
	if (pRemotePlayer) {
		if (pRemotePlayer->GetDistanceFromLocalPlayer() < 200.0f)
			pRemotePlayer->ExitVehicle();
	}
}

void VehicleParamsEx(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] VehicleParamsEx");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	VEHICLEID VehicleID;
	VEHICLE_PARAMS_EX vehParamsEx;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read((char*)& vehParamsEx, sizeof(VEHICLE_PARAMS_EX))) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->ApplyEngineState(vehParamsEx.byteEngine);

	pVehicle->ApplyLightState(vehParamsEx.byteLight);

	if (vehParamsEx.byteDoors)
	{
		if (vehParamsEx.byteDoors == 1) {
			pVehicle->SetDoorState(true);
		}
	}
	else {
		pVehicle->SetDoorState(false);
	}

	if (vehParamsEx.byteObjective)
	{
		if (vehParamsEx.byteObjective == 1)
			pVehicle->m_byteObjectiveVehicle = 1;
	}
	else
	{
		pVehicle->m_byteObjectiveVehicle = 0;
	}

	if (vehParamsEx.byteBoot)
	{
		if (vehParamsEx.byteBoot == 1)
			pVehicle->SetComponentOpenState(1, 17, 1.0f);
	}
	else
	{
		pVehicle->SetComponentOpenState(1, 17, 0);
	}

	if (vehParamsEx.byteBonnet)
	{
		if (vehParamsEx.byteBonnet == 1)
			pVehicle->SetComponentOpenState(0, 16, 1.0);
	}
	else
	{
		pVehicle->SetComponentOpenState(0, 16, 0.0);
	}

	if (vehParamsEx.byteAlarm)
	{
		if (vehParamsEx.byteAlarm == 1)
		{
			pVehicle->EnableSiren(true);
			pVehicle->SetAlarmState(20000);
		}
	}
	else
	{
		pVehicle->EnableSiren(false);
		pVehicle->SetAlarmState(0);
	}

	if (vehParamsEx.byteSiren)
	{
		if (vehParamsEx.byteSiren == 1)
			pVehicle->EnableSiren(true);
	}
	else
	{
		pVehicle->EnableSiren(false);
	}

	if (vehParamsEx.byteDoor1)
	{
		if (vehParamsEx.byteDoor1 == 1)
			pVehicle->SetComponentOpenState(2, 10, 1.0);
	}
	else
	{
		pVehicle->SetComponentOpenState(2, 10, 0.0);
	}

	if (vehParamsEx.byteDoor2)
	{
		if (vehParamsEx.byteDoor2 == 1)
			pVehicle->SetComponentOpenState(3, 8, 1.0);
	}
	else
	{
		pVehicle->SetComponentOpenState(3, 8, 0.0);
	}

	if (vehParamsEx.byteDoor3)
	{
		if (vehParamsEx.byteDoor3 == 1)
			pVehicle->SetComponentOpenState(4, 11, 1.0);
	}
	else
	{
		pVehicle->SetComponentOpenState(4, 11, 0.0);
	}

	if (vehParamsEx.byteDoor4)
	{
		if (vehParamsEx.byteDoor4 == 1)
			pVehicle->SetComponentOpenState(5, 9, 1.0);
	}
	else
	{
		pVehicle->SetComponentOpenState(5, 9, 0.0);
	}

	if (vehParamsEx.byteWindow1)
	{
		if (vehParamsEx.byteWindow1 == 1)
			pVehicle->CloseWindow(10);
	}
	else
	{
		pVehicle->OpenWindow(10);
	}

	if (vehParamsEx.byteWindow2)
	{
		if (vehParamsEx.byteWindow2 == 1)
			pVehicle->CloseWindow(8);
	}
	else
	{
		pVehicle->OpenWindow(8);
	}

	if (vehParamsEx.byteWindow3)
	{
		if (vehParamsEx.byteWindow3 == 1)
			pVehicle->CloseWindow(11);
	}
	else
	{
		pVehicle->OpenWindow(11);
	}

	if (vehParamsEx.byteWindow4)
	{
		if (vehParamsEx.byteWindow4 == 1)
			pVehicle->CloseWindow(9);
	}
	else
	{
		pVehicle->OpenWindow(9);
	}
}

void ShowActor(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Show actor");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	NEW_ACTOR newActor{};
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	if (bsData.GetNumberOfUnreadBits() >= (31 * 8)) {
		if (!bsData.Read(newActor.ActorID) || !bsData.Read(newActor.iSkin)) return;
		uint32_t dCustomSkinID = 0;
		if (!bsData.Read(dCustomSkinID)) return;
		if (dCustomSkinID > 0) {
			int16_t virtId = DLManager::Instance().GetVirtualModelId(dCustomSkinID);
			if (IsValidPedModel(virtId)) {
				newActor.iSkin = virtId;
			}
		}
		if (!bsData.Read(newActor.vecPos.x) || !bsData.Read(newActor.vecPos.y) || !bsData.Read(newActor.vecPos.z) || !bsData.Read(newActor.fAngle) || !bsData.Read(newActor.fHealth) || !bsData.Read(newActor.bInvulnerable)) return;
	} else {
		if (!bsData.Read((char*)&newActor, sizeof(NEW_ACTOR))) return;
	}

	if (IsValidPedModel(newActor.iSkin))
	{
		pActorPool->New(&newActor);
	}
}

void HideActor(RPCParameters* rpcParams)
{
	Log::traceLastFunc("[RPC-IN] Hide actor");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ActorID)) return;

	pActorPool->Delete(ActorID);
}

void ChatBubble(RPCParameters* rpcParams)
{
	Log::traceLastFunc("RPC: ChatBubble");

	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CPlayerBubblePool* pPlayerBubblePool = pNetGame->GetPlayerBubblePool();
	if (pPlayerBubblePool == nullptr) return;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	PLAYERID playerId;
	uint32_t color;
	float drawDistance;
	uint32_t duration;
	uint8_t byteTextLen;

	char szText[256];
	memset(szText, 0, 256);

	if (!bsData.Read(playerId)) return;

	if (playerId == pNetGame->GetPlayerPool()->GetLocalPlayerID()) return;
	if (playerId >= MAX_PLAYERS) return;

	if (!bsData.Read(color) || !bsData.Read(drawDistance) || !bsData.Read(duration) || !bsData.Read(byteTextLen) || !bsData.Read((char*)szText, byteTextLen)) return;
	szText[byteTextLen] = '\0';

	pPlayerBubblePool->New(playerId, szText, color, drawDistance, duration);
}

void SetPlayerSkillLevel(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	PLAYERID playerId;
	uint32_t ucSkillType;
	uint16_t uiSkillLevel;

	if (!bsData.Read(playerId) || !bsData.Read(ucSkillType) || !bsData.Read(uiSkillLevel)) return;

	if(ucSkillType < 0 || ucSkillType > 10)
		return;
	if(uiSkillLevel < 0 || uiSkillLevel > 1000)
		return;

	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	if(pPlayerPool)
	{
		if(playerId == pPlayerPool->GetLocalPlayerID())
			pPlayerPool->GetLocalPlayer()->GetPlayerPed()->SetWeaponSkill(ucSkillType, uiSkillLevel);
		else if(pPlayerPool->GetSlotState(playerId))
			pPlayerPool->GetAt(playerId)->GetPlayerPed()->SetWeaponSkill(ucSkillType, uiSkillLevel);;
	}
}

#include "../java/editobject.h"
void EditAttachedObject(RPCParameters *rpcParams) {
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	uint32_t index;

	if (!bsData.Read(index)) return;

	CObjectEditor::startEditPlayerAttach(index);

	FLog("RPC: EditAttachedObject %d", index);
}

void EditObject(RPCParameters *rpcParams) {
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	bool bPlayerObj;
	uint16_t objectId;

	if (!bsData.Read(bPlayerObj) || !bsData.Read(objectId)) return;

	CObjectEditor::startEditObject(objectId);

	FLog("RPC: EditObject %d", objectId);
}

void DL_ModelRequest(RPCParameters* rpcParams)
{
	DLManager::Instance().OnModelRequest(rpcParams);
}

void DL_ModelFile(RPCParameters* rpcParams)
{
	int idx = DLManager::Instance().Next183DumpIndex();
	if (idx >= 0 && idx < 3) {
		char hex[128];
		int len = (rpcParams && rpcParams->input) ? static_cast<int>((rpcParams->numberOfBitsOfData + 7) / 8) : 0;
		int n = (len > 12) ? 12 : len;
		int pos = snprintf(hex, sizeof(hex), "183 raw:");
		for (int i = 0; i < n && pos < (int)sizeof(hex) - 4; ++i) {
			pos += snprintf(hex + pos, sizeof(hex) - pos, " %02X", rpcParams->input[i]);
		}
		DLManager::Instance().PushChat(hex);
	}
	DLManager::Instance().OnModelFile(rpcParams);
}

void DL_DownloadCompleted(RPCParameters* rpcParams)
{
	DLManager::Instance().OnDownloadCompleted(rpcParams);
}

void RegisterRPCs(RakClientInterface *pRakClient)
{
	Log::traceLastFunc("Registering RPC's..");
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_InitGame, InitGame);

	// RPC_SetPlayerSkillLevel
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetPlayerSkillLevel, SetPlayerSkillLevel);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel, Create3DTextLabel);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrUpdate3DTextLabel, Update3DTextLabel);
	//pRakClient->RegisterAsRemoteProcedureCall(&RPC_ChatBubble, ChatBubble);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DialogBox, DialogBox);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, SetCheckpoint);					// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, DisableCheckpoint);			// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetRaceCheckpoint, SetRaceCheckpoint);			// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableRaceCheckpoint, DisableRaceCheckpoint);	// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs, UpdateScoresPingsIPs);
	// RPC_SvrStats - useless
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_GameModeRestart, GameModeRestart);				// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, ConnectionRejected);			// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, ClientMessage);					// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldTime, WorldTime);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_Pickup, Pickup);									// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DestroyPickup, DestroyPickup);					// DONE
	// RPC_DestroyWeaponPickup - unused in 037
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScmEvent, ScmEvent);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_Weather, Weather);								// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetTimeEx, SetTimeEx);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ToggleClock, ToggleClock);						// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_TimerUpdate, TimerUpdate);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, WorldPlayerAdd);					// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath, WorldPlayerDeath);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, WorldPlayerRemove);			// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd, WorldVehicleAdd);				// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, WorldVehicleRemove);			// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DamageVehicle, DamageVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleTireStatus, SetVehicleTireStatus);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_VehicleParamsEx, VehicleParamsEx);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_EnterVehicle, EnterVehicle);						// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ExitVehicle, ExitVehicle);						// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, ServerJoin);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, ServerQuit);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_Chat, Chat);										// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RequestClass);						// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestSpawn, RequestSpawn);						// DONE
	// RPC_OnPlayerEditAttachedObject
	// RPC_EditObject
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_EditAttachedObject, EditAttachedObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_EditObject, EditObject);
	// RPC_EnterEditObject
	// RPC_CancelEdit
	// RPC_SetTargeting
	// RPC_ClientCheckResponse
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ShowActor, ShowActor);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_HideActor, HideActor);							// DONE
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ChatBubble, ChatBubble);

	// 0.3.DL Artwork
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ModelRequest, DL_ModelRequest);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ModelFile, DL_ModelFile);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_DownloadCompleted, DL_DownloadCompleted);
}

void UnregisterRPCs(RakClientInterface *pRakClient)
{
	Log::traceLastFunc("Unregistering RPC's..");
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_InitGame);

	// 0.3.DL Artwork
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ModelRequest);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ModelFile);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DownloadCompleted);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetPlayerSkillLevel);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrUpdate3DTextLabel);
	//pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ChatBubble);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DialogBox);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetCheckpoint);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DisableCheckpoint);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs);
	// RPC_SvrStats - useless
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_GameModeRestart);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ConnectionRejected);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ClientMessage);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldTime);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Pickup);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DestroyPickup);
	// RPC_DestroyWeaponPickup - unused in 037
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScmEvent);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Weather);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetTimeEx);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ToggleClock);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_TimerUpdate);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DamageVehicle);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetVehicleTireStatus);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_VehicleParamsEx);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_EnterVehicle);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ExitVehicle);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerJoin);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerQuit);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Chat);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestClass);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestSpawn);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ShowActor);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_HideActor);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ChatBubble);
}