//
// Created by x1y2z on 03.02.2023.
//

#include "WidgetGta.h"
#include "main.h"
#include "game/game.h"
#include "net/netgame.h"
#include "vendor/shadowhook/patch.h"
#include "WidgetRegionLook.h"
#include "TouchInterface.h"
#include "WidgetButton.h"

extern CNetGame *pNetGame;
extern CGame *pGame;

CWidgetGta* m_pWidgets[WidgetIDs::NUM_WIDGETS];

enum eWidgetState {
    STATE_NONE,
    STATE_FIXED
};

WidgetIDs GetWidgetTypeFromWidget(CWidgetGta* pWidget)
{
    if(pWidget)
    {
        if(m_pWidgets[WidgetIDs::WIDGET_ATTACK] && pWidget == m_pWidgets[WidgetIDs::WIDGET_ATTACK]) return WidgetIDs::WIDGET_ATTACK;
        if(m_pWidgets[WidgetIDs::WIDGET_SPRINT] && pWidget == m_pWidgets[WidgetIDs::WIDGET_SPRINT]) return WidgetIDs::WIDGET_SPRINT;
        if(m_pWidgets[WidgetIDs::WIDGET_ACCELERATE] && pWidget == m_pWidgets[WidgetIDs::WIDGET_ACCELERATE]) return WidgetIDs::WIDGET_ACCELERATE;
        if(m_pWidgets[WidgetIDs::WIDGET_ENTER_CAR] && pWidget == m_pWidgets[WidgetIDs::WIDGET_ENTER_CAR]) return WidgetIDs::WIDGET_ENTER_CAR;
        if(m_pWidgets[WidgetIDs::WIDGET_BRAKE] && pWidget == m_pWidgets[WidgetIDs::WIDGET_BRAKE]) return WidgetIDs::WIDGET_BRAKE;
        if(m_pWidgets[WidgetIDs::WIDGET_HORN] && pWidget == m_pWidgets[WidgetIDs::WIDGET_HORN]) return WidgetIDs::WIDGET_HORN;
        if(m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_LEFT] && pWidget == m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_LEFT]) return WidgetIDs::WIDGET_VEHICLE_STEER_LEFT;
        if(m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT] && pWidget == m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT]) return WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT;
        if(m_pWidgets[WidgetIDs::WIDGET_HANDBRAKE] && pWidget == m_pWidgets[WidgetIDs::WIDGET_HANDBRAKE]) return WidgetIDs::WIDGET_HANDBRAKE;
        if(m_pWidgets[WidgetIDs::WIDGET_CAR_SHOOT] && pWidget == m_pWidgets[WidgetIDs::WIDGET_CAR_SHOOT]) return WidgetIDs::WIDGET_CAR_SHOOT;
        // The launcher's own registry only covers widgets registered by
        // name (accelerate, brake, horn, ...). Widgets like ATTACK and
        // SPRINT have no name entry, so their ProcessFixedWidget cases were
        // dead (always -1). Fall back to the game's own array, which is
        // indexed by widget type id, so those cases go live.
        if (CTouchInterface::m_pWidgets)
        {
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ATTACK]) return WidgetIDs::WIDGET_ATTACK;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_SPRINT]) return WidgetIDs::WIDGET_SPRINT;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ACCELERATE]) return WidgetIDs::WIDGET_ACCELERATE;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ENTER_CAR]) return WidgetIDs::WIDGET_ENTER_CAR;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_BRAKE]) return WidgetIDs::WIDGET_BRAKE;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_HORN]) return WidgetIDs::WIDGET_HORN;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_LEFT]) return WidgetIDs::WIDGET_VEHICLE_STEER_LEFT;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT]) return WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_HANDBRAKE]) return WidgetIDs::WIDGET_HANDBRAKE;
            if(pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_CAR_SHOOT]) return WidgetIDs::WIDGET_CAR_SHOOT;
        }
    }

    return static_cast<WidgetIDs>(-1);
}

void SetWidgetFromId(int idWidget, CWidgetGta* pWidget)
{
    m_pWidgets[idWidget] = pWidget;
}

void SetWidgetFromName(const char* name, CWidgetGta* pWidget)
{
    if(!name || !pWidget) return;
    if(!strcmp("accelerate", name)) SetWidgetFromId(WidgetIDs::WIDGET_ACCELERATE, pWidget);
    if(!strcmp("hud_car", name)) SetWidgetFromId(WidgetIDs::WIDGET_ENTER_CAR, pWidget);
    if(!strcmp("brake", name)) SetWidgetFromId(WidgetIDs::WIDGET_BRAKE, pWidget);
    if(!strcmp("horn", name) || strstr(name, "horn")) SetWidgetFromId(WidgetIDs::WIDGET_HORN, pWidget);
    if(!strcmp("steer_left", name) || strstr(name, "steer_left") || strstr(name, "steerl") || !strcmp("left", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_STEER_LEFT, pWidget);
    if(!strcmp("steer_right", name) || strstr(name, "steer_right") || strstr(name, "steerr") || !strcmp("right", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT, pWidget);
    if(!strcmp("handbrake", name) || strstr(name, "handbrake")) SetWidgetFromId(WidgetIDs::WIDGET_HANDBRAKE, pWidget);
    if(!strcmp("car_shoot", name) || strstr(name, "car_shoot")) SetWidgetFromId(WidgetIDs::WIDGET_CAR_SHOOT, pWidget);
}

static bool IsVehicleControlWidget(CWidgetGta* pWidget)
{
    if (!pWidget) return false;
    if (m_pWidgets[WidgetIDs::WIDGET_ACCELERATE] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_BRAKE] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_HORN] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_LEFT] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_HANDBRAKE] == pWidget ||
        m_pWidgets[WidgetIDs::WIDGET_CAR_SHOOT] == pWidget)
    {
        return true;
    }

    // The game's widget array is indexed by widget TYPE id (WidgetIDs), NOT
    // by layout position (WIDGET_POSITION_*). The old code used position ids
    // here and several of them collide with unrelated widget types:
    //   WIDGET_POSITION_CAR_SHOOT (0x1) == WIDGET_ATTACK (0x1) -> the punch
    //   button was treated as a vehicle widget and disabled on foot.
    //   WIDGET_POSITION_HORN_ALT (0x8) == WIDGET_PHONE (0x8) -> the phone
    //   button was disabled on foot too.
    //   WIDGET_POSITION_VEHICLE_SHOOT_LEFT (0xA) == WIDGET_KISS (0xA).
    // Only the vehicle-shoot / air-gun widgets are not covered by the
    // launcher's own name registry above, so check those by type id.
    if (CTouchInterface::m_pWidgets)
    {
        if (pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_LEFT] ||
            pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_RIGHT] ||
            pWidget == CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_AIR_GUN])
        {
            return true;
        }
    }
    return false;
}

eWidgetState ProcessFixedWidget(CWidgetGta* pWidget)
{
    if (!pWidget || !pGame) return STATE_NONE;
    CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
    if (!pPlayerPed) return STATE_NONE;

    bool inVehicle = pPlayerPed->IsInVehicle();

    if (IsVehicleControlWidget(pWidget))
    {
        if (!inVehicle && !pPlayerPed->IsInJetpackMode())
        {
            pWidget->SetEnabled(false);
            return STATE_FIXED;
        }
    }

    WidgetIDs widgetType = GetWidgetTypeFromWidget(pWidget);
    switch(widgetType)
    {
        case -1:
            return STATE_NONE;
        case WidgetIDs::WIDGET_ATTACK:
            if(inVehicle || pPlayerPed->IsInJetpackMode())
            {
                pWidget->SetEnabled(false);
                return STATE_FIXED;
            }
            break;

        case WidgetIDs::WIDGET_SPRINT:
            if(inVehicle)
            {
                pWidget->SetEnabled(false);
                return STATE_FIXED;
            }
            break;

        case WidgetIDs::WIDGET_ACCELERATE:
        case WidgetIDs::WIDGET_BRAKE:
        case WidgetIDs::WIDGET_HORN:
        case WidgetIDs::WIDGET_VEHICLE_STEER_LEFT:
        case WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT:
        case WidgetIDs::WIDGET_HANDBRAKE:
        case WidgetIDs::WIDGET_CAR_SHOOT:
            if(!inVehicle && !pPlayerPed->IsInJetpackMode())
            {
                pWidget->SetEnabled(false);
                return STATE_FIXED;
            }
            break;
        case WidgetIDs::WIDGET_ENTER_CAR:
            if(pPlayerPed->IsInJetpackMode()) return STATE_NONE;

            if(pNetGame)
            {
                CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
                if(pVehiclePool)
                {
                    VEHICLEID vehicleId = pVehiclePool->FindNearestToLocalPlayerPed();
                    if(vehicleId == INVALID_VEHICLE_ID) return STATE_FIXED;

                    if(vehicleId != INVALID_VEHICLE_ID)
                    {
                        CVehicle *pVehicle = pVehiclePool->GetAt(vehicleId);
                        if(pVehicle)
                        {
                            if(!pPlayerPed->IsInVehicle() &&
                               pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() > 10.0f)
                            {
                                return STATE_FIXED;
                            }
                        }
                    }
                }
            }
            break;
    }

    return STATE_NONE;
}

void CWidgetGta::SetEnabled(bool bEnabled) {
    m_bEnabled = bEnabled;
}

bool (*CWidget__IsTouched)(uintptr_t *thiz, CVector2D *pVecOut);
bool CWidget__IsTouched_hook(uintptr_t *thiz, CVector2D *pVecOut) {
//    if(*thiz == CWidgetGta::pWidgets[WIDGET_POSITION_HORN]) {
//        return true;
//    }
//    if(!CHUD::bIsShow)
//        return false;


    return CWidget__IsTouched(thiz, pVecOut);
}

uintptr_t (*CWidget)(CWidgetButton* thiz, const char* name, uintptr_t* a3, int a4, uintptr_t* a5);
uintptr_t CWidget_hook(CWidgetButton* thiz, const char* name, uintptr_t*  a3, int a4, uintptr_t* a5)
{
    FLog("New Widget: \"%s\" 0x%X", name, thiz-g_libGTASA);

    SetWidgetFromName(name, thiz);
    return CWidget(thiz, name, a3, a4, a5);
}

void (*CWidget__SetEnabled)(CWidgetGta* pWidget, bool bEnabled);
void CWidget__SetEnabled_hook(CWidgetGta* pWidget, bool bEnabled)
{
    if(pNetGame)
    {
        switch(ProcessFixedWidget(pWidget))
        {
            case STATE_NONE: break;
            case STATE_FIXED:
                bEnabled = false;
                break;
        }
    }

    CWidget__SetEnabled(pWidget, bEnabled);
}

void (*CWidgetButton__Update)(CWidgetButton* thiz);
void CWidgetButton__Update_hook(CWidgetButton* thiz) {
    if(pNetGame)
    {
        switch(ProcessFixedWidget(thiz))
        {
            case STATE_NONE: break;
            case STATE_FIXED: return;
        }
    }

    CWidgetButton__Update(thiz);
}

void CWidgetGta::InjectHooks() {
    CHook::InstallPLT(g_libGTASA + (VER_x32 ? 0x00671424 : 0x842408), &CWidgetButton__Update_hook, &CWidgetButton__Update);
    CHook::InlineHook("_ZN13CWidgetButtonC2EPKcRK14WidgetPositionjj10HIDMapping", &CWidget_hook, &CWidget);
    CHook::InlineHook("_ZN7CWidget10SetEnabledEb", &CWidget__SetEnabled_hook, &CWidget__SetEnabled);
}

void CWidgetGta::SetTexture(const char *name) {
    m_Sprite.m_pTexture = CUtil::LoadTextureFromDB("mobile", name);
}

bool CWidgetGta::IsReleased(CVector2D *pVecOut) {
    return CHook::CallFunction<bool>(g_libGTASA + (VER_x32 ? 0x002B3484 + 1 : 0x372794), this, pVecOut);
}

bool CWidgetGta::IsTouched(CVector2D *pVecOut) {
    return CHook::CallFunction<bool>(g_libGTASA + (VER_x32 ? 0x002B3324 + 1 : 0x3725D0), this, pVecOut);
}


