#pragma once
#include <QJsonObject>
#include <QStringList>

namespace trainer::netplayRelay {
inline QStringList regions() {return {"madrid","nyc","saopaulo","singapore"};}
inline bool valid(const QString& value) {return value=="auto"||regions().contains(value);}
inline QString label(const QString& value) {
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
}
