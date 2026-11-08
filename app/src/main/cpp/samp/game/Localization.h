//
// Central localisation for the built in game interface.
//
// The engine ships its own menus (pause menu, map, settings) in whatever
// language the installed data set was built for. Everything the player sees
// there is drawn as a GXT (UTF-16) string through CFont::PrintString, so the
// translation lives in one table that is applied at draw time. Strings that are
// not part of the interface, such as player names, server messages, chat lines
// and commands, are never rewritten.
//

#pragma once

#include <cstdint>

enum Language
{
    LANGUAGE_INDONESIA = 0
};

class Localization
{
public:
    // Installs the font hook. Safe to call once, from InstallSpecialHooks.
    static void Initialise();

    static Language GetLanguage() { return LANGUAGE_INDONESIA; }

    // Returns the Indonesian text for a built in interface string, or nullptr
    // when the string is dynamic content and must be printed unchanged.
    static const uint16_t* Translate(const uint16_t* gxt);

    // Number of times a translation was applied, reported in diagnostics.
    static unsigned long long TranslatedCount();
    static unsigned long long UnmatchedCyrillicCount();
};
