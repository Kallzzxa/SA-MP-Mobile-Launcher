#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "../audiostream.h"
#include "game/BuildingRemoval.h"
#include "dl/dlmanager.h"
#include "object_material_reader.h"
#include "object_move_reader.h"
#include <cmath>
#include <climits>
#include "../java/editobject.h"

extern CGame *pGame;
extern CNetGame *pNetGame;
extern CAudioStream* pAudioStream;


void ScrSetGravity(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fGravity;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fGravity)) return;
	
	pGame->SetGravity(fGravity);

	return;
}

void ScrSetCameraPos(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ)) return;

    CCamera::SetPosition(fX, fY, fZ, 0.0f, 0.0f, 0.0f);

	return;
}

void ScrSetCameraLookAt(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	uint8_t byteType;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ) || !bsData.Read(byteType)) return;
	if (byteType < 1 || byteType > 2) {
		byteType = 2;
	}

	CCamera::LookAtPoint(fX, fY, fZ, byteType);

	return;
}

void ScrInterpolateCamera(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool mode;
	CVector vecFrom;
	CVector vecTo;
	int iTime;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(mode) || !bsData.Read(vecFrom.x) || !bsData.Read(vecFrom.y) || !bsData.Read(vecFrom.z) || !bsData.Read(vecTo.x) || !bsData.Read(vecTo.y) || !bsData.Read(vecTo.z) || !bsData.Read(iTime) || !bsData.Read(byteMode)) return;

	if (byteMode < 1 || byteMode > 2) {
		byteMode = 2;
	}

	if (iTime > 0) {
		pNetGame->GetPlayerPool()->GetLocalPlayer()->m_bSpectateProcessed = true;
		if (mode) {
            CCamera::InterpolateCameraPos(&vecFrom, &vecTo, iTime, byteMode);
		}
		else {
            CCamera::InterpolateCameraLookAt(&vecFrom, &vecTo, iTime, byteMode);
		}
	}

	return;
}

void ScrTogglePlayerSpectating(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint32_t dwToggle; 
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(dwToggle)) return;

	FLog("TogglePlayerSpectating: %d", dwToggle);

	pNetGame->GetPlayerPool()->GetLocalPlayer()->ToggleSpectating(dwToggle);

	return;
}

void ScrSetSpawnInfo(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7 || !pNetGame ||
		!pNetGame->GetPlayerPool() || !pNetGame->GetPlayerPool()->GetLocalPlayer()) return;

	unsigned char *Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = static_cast<int>(rpcParams->numberOfBitsOfData);

	PLAYER_SPAWN_INFO spawnInfo{};
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

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

		spawnInfo.byteTeam = byteTeam;
		int actualSkin = (dCustomSkinID > 0) ? DLManager::Instance().GetVirtualModelId(dCustomSkinID) : iSkin;
		spawnInfo.iSkin = IsValidPedModel(actualSkin) ? actualSkin : iSkin;
		spawnInfo.unk = unk;
		spawnInfo.vecPos = vecPos;
		spawnInfo.fRotation = fRotation;
		memcpy(spawnInfo.iSpawnWeapons, iSpawnWeapons, sizeof(iSpawnWeapons));
		memcpy(spawnInfo.iSpawnWeaponsAmmo, iSpawnWeaponsAmmo, sizeof(iSpawnWeaponsAmmo));
	} else {
		if (!bsData.Read((char*)&spawnInfo, sizeof(PLAYER_SPAWN_INFO))) return;
	}

	if (!std::isfinite(spawnInfo.vecPos.x) || !std::isfinite(spawnInfo.vecPos.y) ||
		!std::isfinite(spawnInfo.vecPos.z) || !std::isfinite(spawnInfo.fRotation)) return;

	pNetGame->GetPlayerPool()->GetLocalPlayer()->SetSpawnInfo(&spawnInfo);

	return;
}

void ScrAddGangZone(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint16_t wZoneID;
	float minX, minY, maxX, maxY;
	uint32_t dwColor;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		if (!bsData.Read(wZoneID) || !bsData.Read(minX) || !bsData.Read(minY) || !bsData.Read(maxX) || !bsData.Read(maxY) || !bsData.Read(dwColor)) return;
		if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) || !std::isfinite(maxY)) return;
		pGangZonePool->New(wZoneID, minX, minY, maxX, maxY, dwColor);
	}
}

void ScrGangZoneDestroy(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		if (!bsData.Read(wZoneID)) return;
		pGangZonePool->Delete(wZoneID);
	}
}

void ScrGangZoneFlash(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		uint32_t dwColor;
		if (!bsData.Read(wZoneID) || !bsData.Read(dwColor)) return;
		pGangZonePool->Flash(wZoneID, dwColor);
	}
}

void ScrGangZoneStopFlash(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		if (!bsData.Read(wZoneID)) return;
		pGangZonePool->StopFlash(wZoneID);
	}
}

int iTotalObjects = 0;

// Read every material in the object creation packet, not just the first one.
// Keep the packet reader bounded; malformed material data must not reach rendering.
static bool ReadAndApplyObjectMaterial(RakNet::BitStream& data, CObject* object, bool creating) {
	ObjectMaterialData material{};
	auto decode = [](char* text, size_t length, RakNet::BitStream& stream) {
		return stringCompressor && stringCompressor->DecodeString(text, static_cast<int>(length), &stream);
	};
	if (!ReadObjectMaterial(data, decode, material)) return false;
	if (!object || material.index >= 16) return true;
	if (material.type == 1) {
		if (strlen(material.txd) < 32 && strlen(material.texture) < 32) {
			if (creating && material.model > 20000) material.model = 0xFFFF;
			object->SetMaterial(material.model, material.index, material.txd, material.texture, material.color);
		}
	} else if (strlen(material.font) < 32 && (creating || material.font[0] != '\0')) {
		object->SetMaterialText(material.index, material.text, material.size, material.font,
				material.fontSize, material.bold, material.fontColor, material.backgroundColor, material.alignment);
	}
	return true;
}

void ScrCreateObject(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData <= 0 || !pNetGame) return;
	const int bits = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(rpcParams->input, (static_cast<unsigned int>(bits) + 7u) / 8u, false);
	data.SetWriteOffset(bits);
	OBJECTID objectId, attachedObject;
	VEHICLEID attachedVehicle;
	int model;
	CVector pos{}, rot{}, attachOffset{}, attachRot{};
	float drawDistance;
	uint8_t noCameraCollision, syncRotation = 0;
	if (!data.Read(objectId) || !data.Read(model) ||
		!data.Read(pos.x) || !data.Read(pos.y) || !data.Read(pos.z) ||
		!data.Read(rot.x) || !data.Read(rot.y) || !data.Read(rot.z) ||
		!data.Read(drawDistance) || !data.Read(noCameraCollision) ||
		!data.Read(attachedVehicle) || !data.Read(attachedObject)) return;
	if (attachedObject != INVALID_OBJECT_ID || attachedVehicle != INVALID_VEHICLE_ID) {
		if (!data.Read(attachOffset.x) || !data.Read(attachOffset.y) || !data.Read(attachOffset.z) ||
			!data.Read(attachRot.x) || !data.Read(attachRot.y) || !data.Read(attachRot.z) ||
			!data.Read(syncRotation)) return;
	}
	if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) ||
		!std::isfinite(rot.x) || !std::isfinite(rot.y) || !std::isfinite(rot.z) ||
		!std::isfinite(drawDistance) || !std::isfinite(attachOffset.x) ||
		!std::isfinite(attachOffset.y) || !std::isfinite(attachOffset.z) ||
		!std::isfinite(attachRot.x) || !std::isfinite(attachRot.y) || !std::isfinite(attachRot.z)) return;
	uint8_t materialCount;
	if (!data.Read(materialCount)) return;
	CObjectPool* pool = pNetGame->GetObjectPool();
	if (!pool || objectId >= MAX_OBJECTS) return;
	const int actualModel = model < 0 ? DLManager::Instance().GetVirtualModelId(model) : model;
	if (!pool->New(objectId, actualModel, pos, rot, drawDistance)) return;
	CObject* object = pool->GetAt(objectId);
	if (!object) return;
	++iTotalObjects;
	if (attachedObject != INVALID_OBJECT_ID)
		object->SetAttachedObject(attachedObject, &attachOffset, &attachRot, syncRotation);
	else if (attachedVehicle != INVALID_VEHICLE_ID)
		object->SetAttachedVehicle(attachedVehicle, &attachOffset, &attachRot);
	for (unsigned int i = 0; i < materialCount; ++i) {
		if (!ReadAndApplyObjectMaterial(data, object, true)) return;
	}
}

void ScrDestroyObject(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData <= 0 || !pNetGame) return;
	const int bits = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(rpcParams->input, (static_cast<unsigned int>(bits) + 7u) / 8u, false);
	data.SetWriteOffset(bits);
	OBJECTID objectId;
	CObjectPool* pool = pNetGame->GetObjectPool();
	if (!pool || !data.Read(objectId) || !pool->GetAt(objectId)) return;
	pool->Delete(objectId);
	--iTotalObjects;
}

void ScrSetObjectMaterial(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData <= 0 || !pNetGame) return;
	const int bits = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(rpcParams->input, (static_cast<unsigned int>(bits) + 7u) / 8u, false);
	data.SetWriteOffset(bits);
	OBJECTID objectId;
	CObjectPool* pool = pNetGame->GetObjectPool();
	if (!pool || !data.Read(objectId)) return;
	ReadAndApplyObjectMaterial(data, pool->GetAt(objectId), false);
}

void ScrEditObject(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	uint16_t wObjectID;
	if (!bsData.Read(wObjectID)) return;

	FLog("RPC_ScrEditObject(%d)", (int)wObjectID);

	CObjectEditor::startEditObject(wObjectID);
}

void ScrRemoveBuilding(RPCParameters *rpcParams)
{
    if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
        rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

    auto* Data = reinterpret_cast<unsigned char *>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
    bsData.SetWriteOffset(iBitLength);

    uint32_t modelId;
    CVector pos;
    float radius;

    if (!bsData.Read(modelId) || !bsData.Read((char*)&pos, sizeof(CVector)) || !bsData.Read(radius)) return;

    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) ||
        !std::isfinite(radius) || radius < 0.0f) return;

    // Store in struct array with bounds checking
    if (CBuildingRemoval::m_TotalRemovedObjects < CBuildingRemoval::MAX_REMOVALS) {
        CBuildingRemoval::m_RemoveBuildings[CBuildingRemoval::m_TotalRemovedObjects] = {
            modelId,
            pos,
            radius
        };
        CBuildingRemoval::m_TotalRemovedObjects++;
    }

    // Process removal immediately
    CBuildingRemoval::ProcessRemoveBuilding(modelId, pos, radius);
}

void ScrSetPlayerSkin(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	int iPlayerID;
	int iModel;
	uint32_t dCustomSkinID = 0;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iPlayerID) || !bsData.Read(iModel)) return;
	if (bsData.GetNumberOfUnreadBits() >= 32) {
		if (!bsData.Read(dCustomSkinID)) return;
	}

	int actualModel = iModel;
	if (dCustomSkinID > 0) {
		int16_t virtId = DLManager::Instance().GetVirtualModelId(dCustomSkinID);
		if (IsValidPedModel(virtId)) {
			actualModel = virtId;
		}
	}

	if (IsValidPedModel(actualModel))
	{
		if (pPlayerPool->GetLocalPlayerID() == iPlayerID)
		{
			pPlayerPool->GetLocalPlayer()->GetPlayerPed()->SetModelIndex(actualModel);
		}
		else
		{
			if (pPlayerPool->GetSlotState(iPlayerID) == true)
			{
				CRemotePlayer* pPlayer = pPlayerPool->GetAt(iPlayerID);
				if (!pPlayer || pPlayer->GetState() == PLAYER_STATE_NONE) return;
				CPlayerPed* pPlayerPed = pPlayer->GetPlayerPed();
				if (!pPlayerPed) return;
				pPlayerPed->SetModelIndex(actualModel);
			}
		}
	}
	else if (IsValidPedModel(iModel))
	{
		if (pPlayerPool->GetLocalPlayerID() == iPlayerID)
		{
			pPlayerPool->GetLocalPlayer()->GetPlayerPed()->SetModelIndex(iModel);
		}
		else
		{
			if (pPlayerPool->GetSlotState(iPlayerID) == true)
			{
				CRemotePlayer* pPlayer = pPlayerPool->GetAt(iPlayerID);
				if (!pPlayer || pPlayer->GetState() == PLAYER_STATE_NONE) return;
				CPlayerPed* pPlayerPed = pPlayer->GetPlayerPed();
				if (!pPlayerPed) return;
				pPlayerPed->SetModelIndex(iModel);
			}
		}
	}
}

void ScrSetPlayerMapIcon(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteIconID;
	CVector vecPos;
	uint8_t byteType;
	uint32_t dwColor;
	uint8_t byteStyle;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteIconID) || !bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z) || !bsData.Read(byteType) || !bsData.Read(dwColor) || !bsData.Read(byteStyle)) return;

	// fix crash (invalid GTASA map icons)
	if (byteIconID == 1 or
		byteIconID == 2 or
		byteIconID == 4 or
		byteIconID == 56) byteIconID = 52;

	pNetGame->SetMapIcon(byteIconID, vecPos.x, vecPos.y, vecPos.z, byteType, dwColor, byteStyle);
}

void ScrRemovePlayerMapIcon(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteIconID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteIconID)) return;

	pNetGame->DisableMapIcon(byteIconID);
}

void ScrShowNameTag(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteShowNameTag;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(byteShowNameTag)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (pPlayerPool->GetSlotState(PlayerID))
	{
		CRemotePlayer* pPlayer = pPlayerPool->GetAt(PlayerID);
		if (pPlayer) {
			pPlayer->m_bShowNameTag = byteShowNameTag;
		}
	}
}

void ScrApplyPlayerAnimation(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	char szAnimLib[256];
	char szAnimName[256];
	memset(szAnimLib, 0, sizeof(szAnimLib));
	memset(szAnimName, 0, sizeof(szAnimName));

	PLAYERID PlayerID;
	uint8_t byteLength;
	float fDelta;
	bool bLoop, bLockX, bLockY, bFreeze;
	uint32_t dwTime;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID)) return;
	
	if (!bsData.Read(byteLength) || !bsData.Read(szAnimLib, byteLength)) return;
	szAnimLib[byteLength] = '\0';

	if (!bsData.Read(byteLength) || !bsData.Read(szAnimName, byteLength)) return;
	szAnimName[byteLength] = '\0';

	if (!bsData.Read(fDelta) || !bsData.Read(bLoop) || !bsData.Read(bLockX) || !bsData.Read(bLockY) || !bsData.Read(bFreeze) || !bsData.Read(dwTime)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	CPlayerPed* pPlayerPed = nullptr;

	if (pPlayerPool)
	{
		if (PlayerID == pPlayerPool->GetLocalPlayerID()) {
			pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
		}
		else {
			if (pPlayerPool->GetSlotState(PlayerID)) {
				pPlayerPed = pPlayerPool->GetAt(PlayerID)->GetPlayerPed();
				pPlayerPool->GetAt(PlayerID)->m_bAppliedAnimation = true;
			}
		}

		if (pPlayerPed) {
			FLog("ApplyAnimation: %s:%s", szAnimLib, szAnimName);
			pPlayerPed->ApplyAnimation(szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, dwTime);
		}
	}
}

void ScrClearPlayerAnimations(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	FLog("ClearAnimation");

	PLAYERID playerId;
	if (!bsData.Read(playerId)) return;
	RwMatrix mat;

	CPlayerPool * pPlayerPool = NULL;
	CPlayerPed * pPlayerPed = NULL;

	pPlayerPool = pNetGame->GetPlayerPool();

	if (pPlayerPool)
	{
		if (playerId == pPlayerPool->GetLocalPlayerID()) {
			pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
		}
		else {
			if (pPlayerPool->GetSlotState(playerId)) {
				pPlayerPed = pPlayerPool->GetAt(playerId)->GetPlayerPed();
				pPlayerPool->GetAt(playerId)->m_bAppliedAnimation = false;
			}
		}
		if (pPlayerPed && pPlayerPed->m_pPed) {
            mat = pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
			pPlayerPed->m_pPed->SetPosn(mat.pos.x, mat.pos.y, mat.pos.z);
		}
	}
}

void ScrSetPlayerHealth(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fHealth;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fHealth)) return;

	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->SetHealth(fHealth);
}

void ScrGivePlayerWeapon(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iWeapon;
	int iAmmo;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iWeapon) || !bsData.Read(iAmmo)) return;
	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->GiveWeapon(iWeapon, iAmmo);
}

void ScrSetPlayerInterior(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteInteriorId;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteInteriorId)) return;

	CPlayerPed* pLocalPed = pGame->FindPlayerPed();
	if (!pLocalPed || !pLocalPed->m_pPed) return;
	pLocalPed->m_pPed->SetInterior(byteInteriorId, true);
}

extern UI *pUI;
void ScrShowTextDraw(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

    FLog("ScrShowTextDraw");

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) {
        FLog("no textdraw pool");
        return;
    }

	uint16_t wTextDrawID;
	TEXT_DRAW_TRANSMIT textDrawTransmit;
	uint16_t wTextLength;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(wTextDrawID) || !bsData.Read((char*)& textDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT)) || !bsData.Read(wTextLength)) return;

	char szText[1024 + 1];

	if (wTextLength >= sizeof(szText)) return;
	if (!bsData.Read(szText, wTextLength)) return;
    szText[wTextLength] = 0;

    pTextDrawPool->New(wTextDrawID, &textDrawTransmit, szText);
}

void ScrHideTextDraw(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) return;

	uint16_t wTextDrawID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(wTextDrawID)) return;

	pTextDrawPool->Delete(wTextDrawID);
}

void ScrTextDrawSetString(RPCParameters * rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) return;

	uint16_t wTextDrawID;
	uint16_t wTextLength;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(wTextDrawID) || !bsData.Read(wTextLength)) return;

	if (wTextLength < 1024)
	{
		char szText[1024 + 1];
		if (!bsData.Read(szText, wTextLength)) return;
		szText[wTextLength] = '\0';

		CTextDraw* pTextDraw = pTextDrawPool->GetAt(wTextDrawID);
		if (pTextDraw) pTextDraw->SetText(szText);
	}

}

void ScrSelectTextDraw(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool bEnable = false;
	uint32_t dwColor = 0;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(bEnable) || !bsData.Read(dwColor)) return;

	pNetGame->GetTextDrawPool()->SetSelectState(bEnable ? true : false, dwColor);
}


void ScrSetPlayerAmmo(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	
	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	uint8_t byteWeapon;
	uint16_t wAmmo;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteWeapon) || !bsData.Read(wAmmo)) return;

	pLocalPlayer->GetPlayerPed()->SetAmmo(byteWeapon, wAmmo);
}

void ScrSetVehicleHealth(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fHealth;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(fHealth)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (pVehiclePool)
	{
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if (pVehicle) {
			pVehicle->SetHealth(fHealth);
		}
	}
}

void ScrAttachTrailerToVehicle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID TrailerID;
	VEHICLEID VehicleID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(TrailerID) || !bsData.Read(VehicleID)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pTrailer = pVehiclePool->GetAt(TrailerID);
	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);

	if (pTrailer && pVehicle)
	{
		pVehicle->SetTrailer(pTrailer);
		pVehicle->AttachTrailer();
	}
}

void ScrDetachTrailerFromVehicle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	VEHICLEID VehicleID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID)) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (pVehicle)
	{
		pVehicle->DetachTrailer();
		pVehicle->SetTrailer(nullptr);
	}
}

void ScrSetObjectPos(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	float fX;
	float fY;
	float fZ;
	float fUnused;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ObjectID) || !bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ) || !bsData.Read(fUnused)) return;
	if (!std::isfinite(fX) || !std::isfinite(fY) || !std::isfinite(fZ)) return;

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (pObject) {
		pObject->SetPos(fX, fY, fZ);
	}
}

void ScrSetObjectRotation(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	CVector vecRot;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ObjectID) || !bsData.Read(vecRot.x) || !bsData.Read(vecRot.y) || !bsData.Read(vecRot.z)) return;

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (!pObject) return;

	pObject->InstantRotate(vecRot.x, vecRot.y, vecRot.z);
}

void ScrCreateExplosion(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	uint32_t dwType;
	float fRadius;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ) || !bsData.Read(dwType) || !bsData.Read(fRadius)) return;

	ScriptCommand(&create_explosion_with_radius, fX, fY, fZ, dwType, fRadius);
}

void ScrSetVehicleNumberPlate(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteLength;
	char szPlateName[32+1];
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteLength)) return;
	if (byteLength > 32) return;

	if (!bsData.Read(szPlateName, byteLength)) return;
	szPlateName[byteLength] = '\0';

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetPlateText(szPlateName);
}

#define SPECTATE_TYPE_NORMAL	1
#define SPECTATE_TYPE_FIXED		2
#define SPECTATE_TYPE_SIDE		3


void ScrSpectatePlayer(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(byteMode)) return;

	switch (byteMode) 
	{
	case SPECTATE_TYPE_FIXED:
		byteMode = 15;
		break;
	case SPECTATE_TYPE_SIDE:
		byteMode = 14;
		break;
	default:
		byteMode = 4;
	}

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
	pLocalPlayer->m_byteSpectateMode = byteMode;
	pLocalPlayer->SpectatePlayer(PlayerID);
}

void ScrSpectateVehicle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteMode)) return;

	switch (byteMode)
	{
	case SPECTATE_TYPE_FIXED:
		byteMode = 15;
		break;
	case SPECTATE_TYPE_SIDE:
		byteMode = 14;
		break;
	default:
		byteMode = 3;
	}

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
	pLocalPlayer->m_byteSpectateMode = byteMode;
	pLocalPlayer->SpectateVehicle(VehicleID);
}

void ScrRemoveVehicleComponent(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	VEHICLEID VehicleID;
	uint16_t wComponent;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(wComponent)) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->RemoveComponent(wComponent);
}

void ScrAttachObjectToPlayer(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	PLAYERID PlayerID;
	float offsetX, offsetY, offsetZ;
	float rX, rY, rZ;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ObjectID) || !bsData.Read(PlayerID) || !bsData.Read(offsetX) || !bsData.Read(offsetY) || !bsData.Read(offsetZ) || !bsData.Read(rX) || !bsData.Read(rY) || !bsData.Read(rZ)) return;

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (!pObject) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (pPlayerPool->GetLocalPlayerID() == PlayerID)
	{
		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		ScriptCommand(&attach_object_to_actor,
			pObject->m_dwGTAId,
			pLocalPlayer->GetPlayerPed()->m_dwGTAId,
			offsetX, offsetY, offsetZ,
			rX, rY, rZ);
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		ScriptCommand(&attach_object_to_actor,
			pObject->m_dwGTAId,
			pRemotePlayer->GetPlayerPed()->m_dwGTAId,
			offsetX, offsetY, offsetZ,
			rX, rY, rZ);
	}
}

void ScrSetPlayerWantedLevel(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteWantedLevel;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteWantedLevel)) return;

	if (pGame) pGame->SetWantedLevel(byteWantedLevel);
}

void ScrSetPlayerSpecialAction(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteSpecialAction;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteSpecialAction)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	pPlayerPool->GetLocalPlayer()->ApplySpecialAction(byteSpecialAction);
}

void ScrEnableStuntBonus(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool bEnable;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(bEnable)) return;

	pGame->EnableStuntBonus(bEnable);
}

void ScrSetPlayerFightingStyle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteStyle;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(byteStyle)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CPlayerPed* pPlayerPed = nullptr;

	if (PlayerID == pPlayerPool->GetLocalPlayerID()) {
		pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
	}
	else {
		CRemotePlayer *pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pPlayerPed = pRemotePlayer->GetPlayerPed();
		}
	}

	if (pPlayerPed) {
		pPlayerPed->SetFightingStyle(byteStyle);
	}
}

void ScrSetPlayerVelocity(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecVelocity;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(vecVelocity.x) || !bsData.Read(vecVelocity.y) || !bsData.Read(vecVelocity.z)) return;
	if (!std::isfinite(vecVelocity.x) || !std::isfinite(vecVelocity.y) || !std::isfinite(vecVelocity.z)) return;

	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
	if (!pPlayerPed || !pPlayerPed->m_pPed) return;

	if (pPlayerPed->IsOnGround()) {
		//uint32_t dwState = pPlayerPed->GetStateFlags();
		//pPlayerPed->SetStateFlags(dwState ^ 3);
	}

	pPlayerPed->m_pPed->SetVelocity(vecVelocity);
}

void ScrSetVehicleVelocity(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteType;
	CVector vecVelocity;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteType) || !bsData.Read(vecVelocity.x) || !bsData.Read(vecVelocity.y) || !bsData.Read(vecVelocity.z)) return;
	if (!std::isfinite(vecVelocity.x) || !std::isfinite(vecVelocity.y) || !std::isfinite(vecVelocity.z)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
	if (!pPlayerPed) return;

	if (pPlayerPed->IsInVehicle())
	{
        CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
		VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr(pGtaVehicle);
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if (!pVehicle) return;

		if (byteType)
		{
			if (byteType == 1)
			{
				pVehicle->m_pVehicle->SetTurnSpeed(vecVelocity);
			}
		}
		else
		{
			pVehicle->m_pVehicle->SetVelocity(vecVelocity);
		}
	}

}

void ScrToggleWidescreen(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteToggle;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteToggle)) return;
	//if (gui) gui->chat()->addDebugMessage("Widescreen = %d", byteToggle);
	ScriptCommand(&toggle_widescreen, byteToggle);
}

void ScrSetVehicleTireDamageStatus(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteTireDamageStatus;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteTireDamageStatus)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetTireDamageStatus(byteTireDamageStatus);
}

void ScrSetPlayerTeam(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteTeam;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(byteTeam)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

	if (PlayerID == pPlayerPool->GetLocalPlayerID())
	{
		pLocalPlayer->SetTeam(byteTeam);
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pRemotePlayer->SetTeam(byteTeam);
		}
	}
}

void ScrSetPlayerName(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteLen;
	char szName[28];
	uint8_t byteSuccess;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(byteLen)) return;
	if (byteLen > MAX_PLAYER_NAME) return;

	if (!bsData.Read(szName, byteLen)) return;
	szName[byteLen] = '\0';
	if (!bsData.Read(byteSuccess)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (byteSuccess == 1) {
		pPlayerPool->SetPlayerName(PlayerID, szName);
	}

	if (pPlayerPool->GetLocalPlayerID() == PlayerID) {
		pPlayerPool->SetLocalPlayerName(szName);
	}
}

void ScrSetPlayerPos(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z)) return;
	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;
	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

    pLocalPlayer->DisableSurf();

    CPlayerPed* pLocalPed = pLocalPlayer->GetPlayerPed();
    if (!pLocalPed || !pLocalPed->m_pPed) return;
    if(pLocalPed->m_pPed->IsInVehicle())
        pLocalPed->RemoveFromVehicleAndPutAt(vecPos.x, vecPos.y, vecPos.z);
    else
        pLocalPed->m_pPed->SetPosn(vecPos.x, vecPos.y, vecPos.z);
}

void ScrSetPlayerPosFindZ(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z)) return;

	float fGroundZ = pGame->FindGroundZForCoord(vecPos.x, vecPos.y, vecPos.z);
	if (!std::isfinite(fGroundZ)) return;
	vecPos.z = fGroundZ + 1.5f;
    pLocalPlayer->DisableSurf();
	CPlayerPed* pLocalPed = pLocalPlayer->GetPlayerPed();
	if (!pLocalPed || !pLocalPed->m_pPed) return;
	pLocalPed->m_pPed->SetPosn(vecPos.x, vecPos.y, vecPos.z);
}

void ScrPutPlayerInVehicle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteSeatID;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteSeatID)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

    CPlayerPed *pPed = pGame->FindPlayerPed();
    if(!pPed || !pPed->m_pPed)return;

    //if(vehicleid == pPed->GetCurrentSampVehicleID()) return;

    if(pPed->m_pPed->IsInVehicle()) {
        pPed->m_pPed->RemoveFromVehicle();
    }
    CVehicle *pVehicle = pVehiclePool->GetAt(VehicleID);
    if(!pVehicle)return;
    //   DLOG("seatid = %d", vehicleid);
    if(byteSeatID == 0) {
        ScriptCommand(&put_actor_in_car, pPed->m_dwGTAId, pVehicle->m_dwGTAId);
        //	CCarEnterExit::SetPedInCarDirect(pPed->m_pPed, pVehicle->m_pVehicle, 0, true);
    } else {
        byteSeatID --;
        ScriptCommand(&put_actor_in_car2, pPed->m_dwGTAId, pVehicle->m_dwGTAId, byteSeatID);
//		seatid = CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(pVehicle->m_pVehicle, seatid);
//		CCarEnterExit::SetPedInCarDirect(pPed->m_pPed, pVehicle->m_pVehicle, seatid);
    }
}

void ScrRemovePlayerFromVehicle(RPCParameters* rpcParams)
{
	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

	pLocalPlayer->GetPlayerPed()->ExitCurrentVehicle();
}

void ScrSetPlayerColor(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint32_t dwColor;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(PlayerID) || !bsData.Read(dwColor)) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (pPlayerPool->GetLocalPlayerID() == PlayerID)
	{
		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		if (pLocalPlayer) {
			pLocalPlayer->SetPlayerColor(dwColor);
		}
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pRemotePlayer->SetPlayerColor(dwColor);
		}
	}
}

void ScrShowGameText(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	char szMsg[256];
	memset(szMsg, 0, sizeof(szMsg));

	int iSize;
	int iTime;
	int iLen;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iSize) || !bsData.Read(iTime) || !bsData.Read(iLen)) return;

	if (iLen < 0 || iLen > 200) return;

	if (!bsData.Read(szMsg, iLen)) return;
	szMsg[iLen] = '\0';
	pGame->DisplayGameText(szMsg, iTime, iSize);
}

void ScrSetVehiclePos(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fX;
	float fY;
	float fZ;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ)) return;
	if (!std::isfinite(fX) || !std::isfinite(fY) || !std::isfinite(fZ)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->m_pVehicle->SetPosn(fX, fY, fZ);
}

void ScrSetVehicleZAngle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fAngle;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(fAngle)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetZAngle(fAngle);
}

void ScrSetVehicleParams(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteObjective;
	uint8_t byteDoorsLocked;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteObjective) || !bsData.Read(byteDoorsLocked)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	pVehiclePool->AssignSpecialParamsToVehicle(VehicleID, byteObjective, byteDoorsLocked);
}

void ScrSetPlayerCameraBehindPlayer(RPCParameters* rpcParams)
{
    CCamera::SetBehindPlayer();
}

void ScrTogglePlayerControllable(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteControllable;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(byteControllable)) return;

	pGame->FindPlayerPed()->TogglePlayerControllable(byteControllable);
}

void ScrPlayerPlaySound(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

    unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;
    RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
    bsData.SetWriteOffset(iBitLength);

    int iSound;
    CVector vecPos; // ใช้ CVector แทน VECTOR เพื่อความชัวร์เรื่องโครงสร้างข้อมูล

    if (!bsData.Read(iSound) || !bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z)) return;

    if(pGame)
        pGame->PlaySound(iSound, vecPos.x, vecPos.y, vecPos.z);
}

void ScrSetWorldBounds(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float bounds[4];
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(bounds[0]) || !bsData.Read(bounds[1]) || !bsData.Read(bounds[2]) || !bsData.Read(bounds[3])) return;

	pNetGame->m_pNetSet->fWorldBounds[0] = bounds[0];
	pNetGame->m_pNetSet->fWorldBounds[1] = bounds[1];
	pNetGame->m_pNetSet->fWorldBounds[2] = bounds[2];
	pNetGame->m_pNetSet->fWorldBounds[3] = bounds[3];
}

void ScrGivePlayerMoney(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iMoney;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(iMoney)) return;

	pGame->AddToLocalMoney(iMoney);
}

void ScrSetPlayerFacingAngle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fRotation;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fRotation)) return;

	pGame->FindPlayerPed()->SetTargetRotation(fRotation);
}

void ScrResetPlayerMoney(RPCParameters* rpcParams)
{
	pGame->ResetLocalMoney();
}

void ScrResetPlayerWeapons(RPCParameters* rpcParams)
{
	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->ClearWeapons();
}

void ScrLinkVehicleToInterior(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteInterior;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(VehicleID) || !bsData.Read(byteInterior)) return;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	pVehiclePool->LinkToInterior(VehicleID, byteInterior);
}

void ScrSetPlayerArmour(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	float fArmour;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(fArmour)) return;
	pLocalPlayer->GetPlayerPed()->SetArmour(fArmour);
}

void ScrSetArmedWeapon(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint32_t dwWeapon;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(dwWeapon)) return;
	if (dwWeapon >= 0 && dwWeapon <= 46) {
		CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
		if (pPlayerPed) {
			pPlayerPed->SetArmedWeapon(dwWeapon, false);
		}
	}
}

#define ATTACH_BONE_SPINE	1
#define ATTACH_BONE_HEAD	2
#define ATTACH_BONE_LUPPER	3
#define ATTACH_BONE_RUPPER	4
#define ATTACH_BONE_LHAND	5
#define ATTACH_BONE_RHAND	6
#define ATTACH_BONE_LTHIGH	7
#define ATTACH_BONE_RTHIGH	8
#define ATTACH_BONE_LFOOT	9
#define ATTACH_BONE_RFOOT	10
#define ATTACH_BONE_RCALF	11
#define ATTACH_BONE_LCALF	12
#define ATTACH_BONE_LFARM	13
#define ATTACH_BONE_RFARM	14
#define ATTACH_BONE_LSHOULDER	15
#define ATTACH_BONE_RSHOULDER	16
#define ATTACH_BONE_NECK	17
#define ATTACH_BONE_JAW		18

int GetInternalBoneIDFromSampID(int sampid)
{
    switch (sampid)
    {
        case ATTACH_BONE_SPINE: // 3 or 2
            return 3;
        case ATTACH_BONE_HEAD: // ?
            return 5;
        case ATTACH_BONE_LUPPER: // left upper arm
            return 22;
        case ATTACH_BONE_RUPPER: // right upper arm
            return 32;
        case ATTACH_BONE_LHAND: // left hand
            return 34;
        case ATTACH_BONE_RHAND: // right hand
            return 24;
        case ATTACH_BONE_LTHIGH: // left thigh
            return 41;
        case ATTACH_BONE_RTHIGH: // right thigh
            return 51;
        case ATTACH_BONE_LFOOT: // left foot
            return 43;
        case ATTACH_BONE_RFOOT: // right foot
            return 53;
        case ATTACH_BONE_RCALF: // right calf
            return 52;
        case ATTACH_BONE_LCALF: // left calf
            return 42;
        case ATTACH_BONE_LFARM: // left forearm
            return 33;
        case ATTACH_BONE_RFARM: // right forearm
            return 23;
        case ATTACH_BONE_LSHOULDER: // left shoulder (claviacle)
            return 31;
        case ATTACH_BONE_RSHOULDER: // right shoulder (claviacle)
            return 21;
        case ATTACH_BONE_NECK: // neck
            return 4;
        case ATTACH_BONE_JAW: // jaw ???
            return 8; // i dont know
    }
    return 0;
}

void ScrSetPlayerAttachedObject(RPCParameters* rpcParams)
{
    FLog("ScrSetPlayerAttachedObject");
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

    unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;
    RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
    bsData.SetWriteOffset(iBitLength);

    PLAYERID id;
    uint32_t slot;
    bool create;
    ATTACHED_OBJECT_INFO info;

    if (!bsData.Read(id) || !bsData.Read(slot) || !bsData.Read(create)) return;

    if (!pNetGame || !pNetGame->GetPlayerPool()) return;
    if (id >= MAX_PLAYERS || slot >= (uint32_t)MAX_ATTACHED_OBJECTS) return;

    CPlayerPed* pPed = nullptr;
    if (id == pNetGame->GetPlayerPool()->GetLocalPlayerID())
    {
        pPed = pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed();
    }
    else
    {
        if (pNetGame->GetPlayerPool()->GetAt(id))
        {
            pPed = pNetGame->GetPlayerPool()->GetAt(id)->GetPlayerPed();
        }
    }
    if (!pPed) return;
    if (!create)
    {
        pPed->DeattachObject(slot);
        return;
    }
    if (!bsData.Read((char*)& info, sizeof(ATTACHED_OBJECT_INFO))) return;

    pPed->AttachObject(&info, slot);
}

void ScrApplyActorAnimation(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	char szAnimLib[256];
	char szAnimName[256];
	memset(szAnimLib, 0, 256);
	memset(szAnimName, 0, 256);

	PLAYERID ActorID;
	uint8_t byteAnimLibLen;
	uint8_t byteAnimNameLen;
	float fDelta;
	bool bLoop;
	bool bLockX;
	bool bLockY;
	bool bFreeze;
	int iTime;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ActorID) || !bsData.Read(byteAnimLibLen) || !bsData.Read(szAnimLib, byteAnimLibLen) || !bsData.Read(byteAnimNameLen) || !bsData.Read(szAnimName, byteAnimNameLen) || !bsData.Read(fDelta) || !bsData.Read(bLoop) || !bsData.Read(bLockX) || !bsData.Read(bLockY) || !bsData.Read(bFreeze) || !bsData.Read(iTime)) return;

	szAnimLib[byteAnimLibLen] = '\0';
	szAnimName[byteAnimNameLen] = '\0';

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->ApplyAnimation(szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, iTime);
	}
}

void ScrClearActorAnimation(RPCParameters* rpcParams)
{
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

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->ClearAnimation();
	}
}

void ScrSetActorFacingAngle(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	float fAngle;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ActorID) || !bsData.Read(fAngle)) return;

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->SetFacingAngle(fAngle);
	}
}

void ScrSetActorPos(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ActorID) || !bsData.Read(vecPos.x) || !bsData.Read(vecPos.y) || !bsData.Read(vecPos.z)) return;
	if (!std::isfinite(vecPos.x) || !std::isfinite(vecPos.y) || !std::isfinite(vecPos.z)) return;

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->m_pPed->SetPosn(vecPos.x, vecPos.y, vecPos.z);
	}
}

void ScrSetActorHealth(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	float fHealth;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	if (!bsData.Read(ActorID) || !bsData.Read(fHealth)) return;

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->SetHealth(fHealth);
	}
}

void ScrPlayAudioStream(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteUrlLen;
	char szUrl[256];
	float fX, fY, fZ;
	float fRadius;
	uint8_t byteUsePos;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);
	memset(szUrl, 0, sizeof(szUrl));

	if (!bsData.Read(byteUrlLen) || !bsData.Read(szUrl, byteUrlLen) || !bsData.Read(fX) || !bsData.Read(fY) || !bsData.Read(fZ) || !bsData.Read(fRadius) || !bsData.Read(byteUsePos)) return;

	if (pAudioStream)
		pAudioStream->Play(szUrl, fX, fY, fZ, fRadius, byteUsePos);
}

void ScrStopAudioStream(RPCParameters* rpcParams)
{
	if (pAudioStream)
		pAudioStream->Stop(false);
		//pAudioStream->Stop(true);
}

void ScrMoveObject(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData <= 0 || !pNetGame) return;
	const int bits = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(rpcParams->input, (static_cast<unsigned int>(bits) + 7u) / 8u, false);
	data.SetWriteOffset(bits);
	ObjectMoveData move{};
	if (!ReadObjectMove(data, move)) return;
	CObjectPool* pool = pNetGame->GetObjectPool();
	CObject* object = pool ? pool->GetAt(move.id) : nullptr;
	if (object) object->MoveTo(move.x, move.y, move.z, move.speed, move.rotX, move.rotY, move.rotZ);
}

void ScrStopObject(RPCParameters* rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData <= 0 || !pNetGame) return;
	const int bits = rpcParams->numberOfBitsOfData;
	RakNet::BitStream data(rpcParams->input, (static_cast<unsigned int>(bits) + 7u) / 8u, false);
	data.SetWriteOffset(bits);
	OBJECTID id;
	if (!data.Read(id)) return;
	CObjectPool* pool = pNetGame->GetObjectPool();
	CObject* object = pool ? pool->GetAt(id) : nullptr;
	if (object) object->StopMoving();
}

void AttachCameraToObject(RPCParameters *rpcParams)
{
	if (!rpcParams || !rpcParams->input || rpcParams->numberOfBitsOfData < 8 ||
		rpcParams->numberOfBitsOfData > INT_MAX - 7) return;

	unsigned char* Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength + 7) / 8, false);
	bsData.SetWriteOffset(iBitLength);

	CObjectPool *pObjectPool = pNetGame->GetObjectPool();
	if(pObjectPool)
	{
		OBJECTID objectId;

		if (!bsData.Read(objectId)) return;
		if(objectId < 0 || objectId >= MAX_OBJECTS) 
			return;

		CObject *pObject = pObjectPool->GetAt(objectId);
		if(pObject)
		{
			//if(pGameCamera)
				//pGameCamera->AttachToEntity(pObject);
		}
	}
}

void RegisterScriptRPCs(RakClientInterface *pRakClient)
{
	FLog("Registering script RPC's..");

	// RPC_ScrDisableVehicleCollision
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon, ScrSetPlayerMapIcon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon, ScrRemovePlayerMapIcon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWeaponAmmo, ScrSetPlayerAmmo);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetGravity, ScrSetGravity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleHealth, ScrSetVehicleHealth);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachTrailerToVehicle, ScrAttachTrailerToVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDetachTrailerFromVehicle, ScrDetachTrailerFromVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectPos, ScrSetObjectPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectRotation, ScrSetObjectRotation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDestroyObject, ScrDestroyObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateExplosion, ScrCreateExplosion);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowNameTag, ScrShowNameTag);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrMoveObject, ScrMoveObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopObject, ScrStopObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrNumberPlate, ScrSetVehicleNumberPlate);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);
	
	// RPC_null - unused
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayerSpectatePlayer, ScrSpectatePlayer);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayerSpectateVehicle, ScrSpectateVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveComponent, ScrRemoveVehicleComponent);

	// RPC_ScrForceClassSelection - useless
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachObjectToPlayer, ScrAttachObjectToPlayer);

	// RPC_ScrInitMenu
	// RPC_ScrShowMenu
	// RPC_ScrHideMenu

	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerWantedLevel, ScrSetPlayerWantedLevel);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTextDrawSetString, ScrTextDrawSetString);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAddGangZone, ScrAddGangZone);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone, ScrGangZoneDestroy);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone, ScrGangZoneFlash);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone, ScrGangZoneStopFlash);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrApplyPlayerAnimation, ScrApplyPlayerAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrClearPlayerAnimations, ScrClearPlayerAnimations);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpecialAction, ScrSetPlayerSpecialAction);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEnableStuntBonus, ScrEnableStuntBonus);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetFightingStyle, ScrSetPlayerFightingStyle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerVelocity, ScrSetPlayerVelocity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleVelocity, ScrSetVehicleVelocity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrToggleWidescreen, ScrToggleWidescreen);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleTireStatus, ScrSetVehicleTireDamageStatus);

	// RPC_150 ???
	// RPC_92 ???
	// RPC_ScrPlayCrimeReport

	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerTeam, ScrSetPlayerTeam);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerName, ScrSetPlayerName);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPosFindZ, ScrSetPlayerPosFindZ);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPutPlayerInVehicle, ScrPutPlayerInVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemovePlayerFromVehicle, ScrRemovePlayerFromVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor, ScrSetPlayerColor);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrShowGameText);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetInterior, ScrSetPlayerInterior);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos, ScrSetCameraPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt, ScrSetCameraLookAt);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehiclePos, ScrSetVehiclePos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleZAngle, ScrSetVehicleZAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrVehicleParams, ScrSetVehicleParams);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraBehindPlayer, ScrSetPlayerCameraBehindPlayer);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerControllable, ScrTogglePlayerControllable);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlaySound, ScrPlayerPlaySound);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWorldBounds, ScrSetWorldBounds);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrGivePlayerMoney);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetPlayerMoney);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetPlayerWeapons, ScrResetPlayerWeapons);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrGivePlayerWeapon, ScrGivePlayerWeapon);

	// RPC_64 - unused
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrLinkVehicle, ScrLinkVehicleToInterior);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);

	// RPC_ScrSendDeathMessage
	// RPC_ScrSetShopName
	// RPC_ScrSetPlayerDrunkLevel

	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetArmedWeapon, ScrSetArmedWeapon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerAttachedObject, ScrSetPlayerAttachedObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayAudioStream, ScrPlayAudioStream);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopAudioStream, ScrStopAudioStream);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveBuilding, ScrRemoveBuilding);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachCameraToObject, AttachCameraToObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInterpolateCamera, ScrInterpolateCamera);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClickTextDraw, ScrSelectTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectMaterial, ScrSetObjectMaterial);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEditObject, ScrEditObject);

	// RPC_ScrObjectNoCameraCol
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrApplyActorAnimation, ScrApplyActorAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrClearActorAnimations, ScrClearActorAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorFacingAngle, ScrSetActorFacingAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorPos, ScrSetActorPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorHealth, ScrSetActorHealth);

	// RPC_SetPlayerVirtualWorld - 03DL only
}

void UnregisterScriptRPCs(RakClientInterface *pRakClient)
{
	LOGI("Unregistering script RPC's..");

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetGravity);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrEditObject);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrAddGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveBuilding);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInterpolateCamera);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrPlaySound);
}