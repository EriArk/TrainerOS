#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVariantMap>

namespace trainer {
// Connection availability is an installation capability, not game identity.
// Old profiles support both existing routes. Explicit lists fail closed.
inline bool multiplayerConnectionAllowed(const QJsonObject& profile,const QString& connection) {
    if(profile.isEmpty())return false;
    if(!profile.contains("connections"))return true;
    const auto list=profile["connections"].toArray();
    if(list.isEmpty()||list.size()>2)return false;
    for(const auto& value:list)if(value!="nearby"&&value!="online")return false;
    return list.contains(connection);
}
// Offline Lobby reuses implemented LAN runtime paths. Unknown/SDK/Remote Play
// modes do not inherit support from missing metadata. PS2 uses its existing
// secure direct helper with host ICE candidates only.
inline bool localLobbyRuntime(const QJsonObject& profile) {
    const auto id=profile["id"].toString();
    if(id=="runtime.armsx2.lan.v1")return true;
    const bool known=id.startsWith("runtime.retroarch.")||id.startsWith("runtime.handheld.")
        ||id.startsWith("runtime.fceumm.")||id.startsWith("runtime.snes9x.")||id.startsWith("runtime.genesis.")
        ||id.startsWith("runtime.ppsspp.")||id.startsWith("runtime.dolphin.")||id=="runtime.rmg.n64.v1"
        ||id=="runtime.flycast-dojo.dc.v1"||id.startsWith("runtime.melonds.")||id.startsWith("runtime.azahar.")||id.startsWith("runtime.vita3k.");
    return known&&multiplayerConnectionAllowed(profile,"nearby");
}
// Display only LOCAL admitted runtime profiles. Catalogue player counts cannot
// create a mode, and an unknown future runtime does not inherit an icon.
inline QVariantMap multiplayerPresentation(const QJsonObject& profile) {
    if(profile.isEmpty() || (!multiplayerConnectionAllowed(profile,"nearby") && !multiplayerConnectionAllowed(profile,"online")))return {};
    const auto id=profile["id"].toString();QString kind,label;
    if(id=="runtime.armsx2.lan.v1"){kind="netplay";label="LAN (in-game)";}
    else if(id.startsWith("runtime.handheld.") && profile["transport"]=="netpacket"){kind="link";label="Link";}
    else if(id.startsWith("runtime.retroarch.") || id.startsWith("runtime.dolphin.") || id=="runtime.rmg.n64.v1" || id=="runtime.flycast-dojo.dc.v1"){kind="netplay";label="Netplay";}
    else if(id.startsWith("runtime.ppsspp.") || id=="runtime.azahar.3ds.v1" || id=="runtime.melonds.nds.v1" || id=="runtime.vita3k.lan.v1"){kind="wireless";label="Wireless";}
    if(kind.isEmpty())return {};
    return {{"id",kind},{"label",label},{"nearby",multiplayerConnectionAllowed(profile,"nearby")},{"online",multiplayerConnectionAllowed(profile,"online")}};
}
// Reviewed paired-machine routes prepare two opaque batteries before launch.
inline bool requiresLinkedSavePreparation(const QJsonObject& profile) {
    const auto mode=profile.value("settings").toString();
    return mode=="sameboy-linked-pair-battery-v1"||mode=="mgba-linked-party-v2";
}
// Emulator admission requires the same runtime/core build, content and settings.
// A matching mode name alone does not certify cross-build compatibility.
inline bool sameMultiplayerGame(QJsonObject left,QJsonObject right) {
    if(left.isEmpty()||right.isEmpty())return false;
    left.remove("label");right.remove("label");
    left.remove("connections");right.remove("connections");
    if(left["id"]==right["id"]&&left["id"].toString().startsWith("runtime.")) {
        if(left["playMode"].toString("separate")=="separate"&&right["playMode"].toString("separate")=="separate") {
            left.remove("controllerModes");right.remove("controllerModes");
        }
    }
    // A runtime may declare a fixed native port ceiling separately from its
    // catalogue-derived party limit. Local metadata must not split identical
    // games into incompatible builds. Both peers must declare the same ceiling;
    // shared-controller modes retain their exact two-person contract.
    if(left.contains("runtimePlayers")||right.contains("runtimePlayers")) {
        const int ceiling=left["runtimePlayers"].toInt();
        if(ceiling<2||ceiling>4||right["runtimePlayers"].toInt()!=ceiling)return false;
        if(left["playMode"].toString().isEmpty()&&right["playMode"].toString().isEmpty()) {
            const int a=left["players"].toInt(),b=right["players"].toInt();
            if(a<2||a>ceiling||b<2||b>ceiling)return false;
            left.remove("players");right.remove("players");
        }
    }
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
// A selected controller mode is part of the strict session identity. Derive
// variants only from a LOCAL runtime's declared capability; a peer's mode field
// alone never enables another launch route or grants a local runtime capability.
inline QJsonObject multiplayerModeProfile(QJsonObject local,const QString& mode) {
    if(local.isEmpty())return {};
    if(mode=="separate") {local.remove("playMode");return local;}
    if(mode!="pass-p1"||local["players"].toInt(2)<2||
       !local["controllerModes"].toArray().contains(mode))return {};
    local["playMode"]=mode;local["players"]=2;local["lateJoin"]=false;local["temporaryProgress"]=true;
    return local;
}
inline bool supportsMultiplayerSession(const QJsonObject& local,const QJsonObject& offered) {
    return sameMultiplayerGame(multiplayerModeProfile(local,offered["playMode"].toString("separate")),offered);
}
inline bool usesSharedController(const QJsonObject& profile) {
    return profile["playMode"]=="pass-p1"&&profile["players"].toInt(2)==2&&
        profile["controllerModes"].toArray().contains("pass-p1");
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
