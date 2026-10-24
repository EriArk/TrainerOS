#pragma once
#include "core/repository/LibraryRepository.h"
#include "core/repository/RomPlatforms.h"
#include <QHash>
#include <QSet>

namespace trainer {
// Lobby-owned routing of a copy; ordinary launch/save settings stay in the library.
// Native network bundles still enforce their manifest and content format gates.
inline AdventureRegistration multiplayerRegistration(AdventureRegistration r) {
    static const QHash<QString,QString> native{
        {"psp","ppsspp"},{"gc","dolphin"},{"wii","dolphin"},
        {"nds","melonds"},{"n3ds","azahar"},{"psvita","vita3k"},
        {"ps2","armsx2"},{"n64","retroarch"},{"dreamcast","retroarch"}};
    static const QSet<QString> libretro{
        "nes","fds","snes","sg1000","mastersystem","megadrive","segacd","sega32x",
        "pcengine","pcenginecd","supergrafx","atari2600","psx","fbneo","neogeo","mame",
        "gb","gbc","gba"};
    const auto platform=r.adventure.platformId;
    if(native.contains(platform))r.adventure.adapterId=native.value(platform);
    else if(libretro.contains(platform)) {
        r.adventure.adapterId="retroarch";
        r.integrationConfig["core"]=romCore(platform);
    }
    return r;
}
}
