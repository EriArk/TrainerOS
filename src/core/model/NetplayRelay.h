#pragma once
#include <QJsonObject>
#include <QStringList>

namespace trainer::netplayRelay {
inline QStringList regions() {return {"madrid","nyc","saopaulo","singapore"};}
inline bool validRegion(const QString& value) {return value=="auto"||regions().contains(value);}
inline bool valid(const QString& value) {return value=="psp-host"||validRegion(value);}
inline QString label(const QString& value) {
    if(value=="psp-host")return "Relay on host handheld";
    if(value=="madrid")return "Europe · Madrid";
    if(value=="nyc")return "North America · New York";
    if(value=="saopaulo")return "South America · São Paulo";
    if(value=="singapore")return "Asia · Singapore";
    return "Automatic";
}
inline bool supported(const QJsonObject& game) {
    const auto id=game["id"].toString();
    return id.startsWith("runtime.retroarch.")||id.startsWith("runtime.handheld.")||
        id.startsWith("runtime.fceumm.")||id.startsWith("runtime.snes9x.")||id.startsWith("runtime.genesis.");
}
inline QStringList choices(const QJsonObject& game) {
    if(game["id"].toString().startsWith("runtime.ppsspp."))return {"auto","psp-host"};
    if(supported(game)){auto values=regions();values.prepend("auto");return values;}
    return {};
}
inline bool compatible(const QJsonObject& game,const QString& value) {
    return value=="auto"||choices(game).contains(value);
}
inline QString connectionLabel(const QJsonObject& game,const QString& value) {
    if(value=="auto"&&game["id"].toString().startsWith("runtime.ppsspp."))return "Standard internet relay";
    return label(value);
}
}
