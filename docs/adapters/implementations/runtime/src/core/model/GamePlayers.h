#pragma once
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariantMap>

namespace trainer {
// Reviewed paired-machine routes prepare two opaque batteries before launch.
inline bool requiresLinkedSavePreparation(const QJsonObject& profile) {
    const auto mode=profile.value("settings").toString();
    return mode=="sameboy-linked-pair-battery-v1"||mode=="mgba-linked-party-v2";
}
// Local naming is presentation; all content/settings/runtime fields still match.
inline bool sameMultiplayerGame(QJsonObject left,QJsonObject right) {
    if(left.isEmpty()||right.isEmpty())return false;
    left.remove("label");right.remove("label");
    // Cross-edition links require identified retail builds. A hack can retain
    // its base cartridge's header while changing the wire/save data. Unknown
    // builds may still link to the identical content, never to another edition
    // solely because both headers advertise the same family.
    static const QStringList gen3Retail{
        "a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af",
        "3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac",
        "729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059"};
    if(left.value("transport")=="netpacket"&&right.value("transport")=="netpacket"&&
       left.value("id")=="runtime.handheld.gba-gen3-en.v1"&&left.value("id")==right.value("id")&&
       gen3Retail.contains(left.value("content").toString())&&gen3Retail.contains(right.value("content").toString())) {
        left.remove("content");right.remove("content");
    }
    return left==right;
}
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
    // Handheld boxes often say one LOCAL player; a reviewed cable profile
    // supplies the independent-machine capacity instead.
    if(runtimeProfile.value("transport")=="netpacket"&&
       runtimeProfile.value("id").toString().startsWith("runtime.handheld."))return true;
    return !runtimeProfile.isEmpty()&&!gamePlayers(metadata).solo();
}
}
