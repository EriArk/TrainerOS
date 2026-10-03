#pragma once
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariantMap>

namespace trainer {
// Catalogue player counts describe the original game, not emulator transport.
// Only an unambiguous total of one excludes multiplayer. Unknown text stays unknown.
struct GamePlayers {
    int minimum=0, maximum=0;
    QString mode;
    bool solo() const { return maximum==1; }
    QString label() const {
        if(!maximum)return {};
        if(solo())return "1 player";
        return (minimum==maximum?QString::number(maximum):QString::number(minimum)+QStringLiteral("–")+QString::number(maximum))+" players";
    }
    QVariantMap presentation() const {
        return {{"minimum",minimum},{"maximum",maximum},{"solo",solo()},{"label",label()},{"mode",mode}};
    }
};
inline GamePlayers gamePlayers(const QVariantMap& metadata) {
    const auto text=metadata.value("players").toString().simplified().toLower();
    if(text=="single player"||text=="single-player"||text=="solo")return {1,1,{}};
    static const QRegularExpression pattern(QStringLiteral(
        "^(\\d{1,5})(?:\\s*[-–]\\s*(\\d{1,5}))?(?:\\s+players?)?(?:\\s*\\(?\\s*(simultaneous|alternating|alternate|at the same time|turns)\\s*\\)?)?$"));
    const auto match=pattern.match(text);
    if(!match.hasMatch())return {};
    const int low=match.captured(1).toInt(),high=match.captured(2).isEmpty()?low:match.captured(2).toInt();
    if(low<1||high<low)return {};
    const auto suffix=match.captured(3);
    return {low,high,suffix.isEmpty()?QString():suffix=="alternating"||suffix=="alternate"||suffix=="turns"?"Taking turns":"Together"};
}
inline bool permitsMultiplayer(const QVariantMap& metadata,const QJsonObject& runtimeProfile) {
    // Scraping never creates a transport or enlarges the runtime's party capacity.
    return !runtimeProfile.isEmpty()&&!gamePlayers(metadata).solo();
}
}
