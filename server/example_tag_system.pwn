/*
    example_tag_system.pwn - Graffiti/Tag system for SA-MP Mobile Launcher
    ========================================================================
    Demonstrates CreateDynamicObject + SetDynamicObjectMaterialText +
    EditDynamicObject working with the mobile client.

    Requirements:
        - Streamer plugin (https://github.com/samp-incognito/samp-streamer-plugin)
        - Mobile client with RPC_ScrEditObject support (this repo)

    How it works:
        1. Player runs /createtag -> object (wall, model 19482) spawns in front
        2. SetDynamicObjectMaterialText applies colored text to the wall
        3. EditDynamicObject opens the mobile touch editor (move/rotate UI)
        4. OnPlayerEditDynamicObject saves the final position

    The mobile client handles:
        - RPC_ScrSetObjectMaterial (type 2) -> renders text to object texture
        - RPC_ScrEditObject -> shows touch edit UI (AttachEdit)
        - RPC_EditObject (response) -> sends final pos/rot back to server
*/

#include <a_samp>
#include <streamer>

#define MAX_TAGS 50

enum E_TAG_DATA
{
    tagObject,
    Float:tagX,
    Float:tagY,
    Float:tagZ,
    tagVW,
    tagIntID,
    tagColor[16],
    tagText[64]
}
new TagData[MAX_TAGS][E_TAG_DATA];
new tagCount = 0;

public OnGameModeInit()
{
    // Example: pre-create one tag
    CreateTag(0.0, 0.0, 3.0, "{FF0000}Hello {00FF00}Mobile!", 0, 0);
    return 1;
}

stock CreateTag(Float:x, Float:y, Float:z, const text[], color_idx = 0, vw = 0, interior = 0)
{
    if (tagCount >= MAX_TAGS) return -1;

    new slot = tagCount++;

    TagData[slot][tagX] = x;
    TagData[slot][tagY] = y;
    TagData[slot][tagZ] = z;
    TagData[slot][tagVW] = vw;
    TagData[slot][tagIntID] = interior;
    format(TagData[slot][tagColor], 16, "{FFFFFF}");
    format(TagData[slot][tagText], 64, "%s", text);

    // Wall object (model 19482 = tag wall)
    TagData[slot][tagObject] = CreateDynamicObject(19482,
        TagData[slot][tagX], TagData[slot][tagY], TagData[slot][tagZ],
        0.0, 0.0, 0.0,
        TagData[slot][tagVW], TagData[slot][tagIntID], -1, 300.0, 300.0);

    ApplyTagText(slot);
    return slot;
}

stock ApplyTagText(slot)
{
    new shstr[128];
    format(shstr, sizeof(shstr), "%s%s", TagData[slot][tagColor], TagData[slot][tagText]);
    // material 0, size 130, Arial 35, bold, transparent bg, centered
    SetDynamicObjectMaterialText(TagData[slot][tagObject], 0, shstr,
        130, "Arial", 35, 1, 0x00000000, 0x00000000, 1);
}

// /edittag [slot] - open mobile object editor for a tag
CMD:edittag(playerid, params[])
{
    new slot;
    if (sscanf(params, "d", slot)) return SendClientMessage(playerid, -1, "Usage: /edittag [slot]");
    if (slot < 0 || slot >= tagCount) return SendClientMessage(playerid, -1, "Invalid tag slot");

    // This triggers RPC_ScrEditObject -> mobile client shows touch editor
    EditDynamicObject(playerid, TagData[slot][tagObject]);
    SendClientMessage(playerid, -1, "Edit the tag position, then press SAVE.");
    return 1;
}

public OnPlayerEditDynamicObject(playerid, objectid, response, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz)
{
    // Find which tag slot this object belongs to
    for (new slot = 0; slot < tagCount; slot++)
    {
        if (TagData[slot][tagObject] == objectid)
        {
            if (response == EDIT_RESPONSE_FINAL)
            {
                TagData[slot][tagX] = x;
                TagData[slot][tagY] = y;
                TagData[slot][tagZ] = z;
                SetDynamicObjectPos(objectid, x, y, z);
                SetDynamicObjectRot(objectid, rx, ry, rz);
                SendClientMessage(playerid, -1, "Tag position saved.");
            }
            else
            {
                SendClientMessage(playerid, -1, "Tag edit cancelled.");
            }
            break;
        }
    }
    return 1;
}

// /settagtext [slot] [text] - change tag text (updates material instantly)
CMD:settagtext(playerid, params[])
{
    new slot, text[64];
    if (sscanf(params, "ds[64]", slot, text)) return SendClientMessage(playerid, -1, "Usage: /settagtext [slot] [text]");
    if (slot < 0 || slot >= tagCount) return SendClientMessage(playerid, -1, "Invalid tag slot");

    format(TagData[slot][tagText], 64, "%s", text);
    ApplyTagText(slot);
    SendClientMessage(playerid, -1, "Tag text updated.");
    return 1;
}
