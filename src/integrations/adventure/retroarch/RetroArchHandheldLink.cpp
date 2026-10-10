#include "RetroArchHandheldLink.h"
#include "RetroArchSave.h"
#include "HandheldSavePair.h"
#include "core/model/GamePlayers.h"
#include <QtEndian>
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QUuid>
#include <QCryptographicHash>

namespace trainer::retroarch {
QJsonObject handheldLinkProfile(const AdventureRegistration& r) {
    if(!QStringList{"gb","gbc","gba"}.contains(r.adventure.platformId))return {};
    QFile f(r.contentPath);if(!f.open(QIODevice::ReadOnly))return {};
    const auto h=f.read(512);if(h.size()<0x150)return {};
    QString family,mode;
    int players=2;
    if(r.adventure.platformId=="gba"&&QFileInfo(r.contentPath).suffix().toLower()=="gba") {
        const auto code=h.mid(0xac,4);
        // English Gen III all share the emulated cable protocol. In-game
        // edition/progression restrictions still apply; RFU is a different mode.
        if(QList<QByteArray>{"BPEE","BPRE","BPGE","AXVE","AXPE"}.contains(code)) {
            family="gba-gen3-en";mode="mul_poke";players=4;
        } else if(code=="AWRE"||code=="AWRP") {family="gba-aw1";mode="mul_aw1";players=4;}
        else if(code=="AW2E"||code=="AW2P") {family="gba-aw2";mode="mul_aw2";players=4;}
        if(family.isEmpty()) {
            quint8 checksum=0;
            for(int n=0xa0;n<=0xbc;++n)checksum=quint8(checksum-quint8(h[n]));
            checksum=quint8(checksum-0x19);
            if(quint8(h[0xb2])==0x96&&checksum==quint8(h[0xbd])&&f.size()>=32768&&f.size()<=32*1024*1024) {
                // gpSP 5819380's RFU entries with working upstream evidence.
                // Cartridge codes select the connection mechanism; the complete
                // content digest still gates peers. Do not infer cross-edition
                // compatibility or enable known failed/latency-sensitive RFU games.
                const QList<QByteArray> golfRfu{"BMGE","BMGJ","BMGP","BMGS","BMGF","BMGI","BMGD","BMGU"};
                const QList<QByteArray> battleRfu{"BRBE","BRKE","BR5E","BR6E"};
                if(golfRfu.contains(code)||battleRfu.contains(code))
                    return {{"id","runtime.handheld.gba-rfu.v1"},{"label",r.adventure.title.left(96)},
                        {"settings","rfu-own-save-v1"},{"players",golfRfu.contains(code)?4:2},
                        {"transport","netpacket"},{"lateJoin",false}};
                return {{"id","runtime.retroarch.gba.mGBALink.v2"},{"label",r.adventure.title.left(96)},
                    {"settings","mgba-linked-party-v2"},{"players",4},{"transport","rollback"},
                    {"saveBytes",131072},{"lateJoin",false}};
            }
        }
    } else if((r.adventure.platformId=="gb"||r.adventure.platformId=="gbc")&&
              QStringList{"gb","gbc"}.contains(QFileInfo(r.contentPath).suffix().toLower())) {
        auto title=h.mid(0x134,15);title=title.left(title.indexOf('\0')<0?title.size():title.indexOf('\0')).trimmed();
        if(QList<QByteArray>{"POKEMON RED","POKEMON BLUE","POKEMON YELLOW"}.contains(title))family="gb-gen1-en";
        if(QList<QByteArray>{"POKEMON_GLDAAUE","POKEMON_SLVAAXE","PM_CRYSTAL","POKEMON GOLD","POKEMON SILVER"}.contains(title))family="gb-gen2-en";
        if(title=="POKECARD"||title=="POKEMON CARD GB")family="gb-tcg";
        mode="cable";
        if(family.isEmpty()) {
            // Upstream's ordinary rollback runs two genuinely linked machines.
            // Battery pairs use SameBoy's existing subsystem SRAM API and
            // authenticated preparation, checked by netplayIdentity.
            // Select by cartridge hardware, never by the game's name or series.
            const auto type=quint8(h[0x147]),size=quint8(h[0x148]);
            const QList<quint8> volatileCartridges{0x00,0x01,0x02,0x05,0x08,0x11,0x12,0x19,0x1a,0x1c,0x1d};
            quint8 checksum=0;
            for(int n=0x134;n<=0x14c;++n)checksum=quint8(checksum-quint8(h[n])-1);
            const auto ram=quint8(h[0x149]);
            const bool battery=(type==0x03||type==0x09||type==0x13||type==0x1b||type==0x1e)&&
                ram>=2&&ram<=5&&((type==0x03||type==0x13)?ram<=3:type==0x09?ram==2:type==0x1e?ram!=4:true);
            if((volatileCartridges.contains(type)||battery)&&size<=8&&f.size()==(qint64(32768)<<size)&&
               checksum==quint8(h[0x14d])) {
                auto result=QJsonObject {{"id","runtime.retroarch."+r.adventure.platformId+".DoubleCherryGB.v1"},
                    {"label",r.adventure.title.left(96)},{"settings","dcgb-linked-pair-volatile-v1"},
                    {"players",2},{"transport","rollback"}};
                if(battery) {
                    result["id"]="runtime.retroarch."+r.adventure.platformId+".SameBoy.v1";
                    result["settings"]="sameboy-linked-pair-battery-v1";
                    result["saveBytes"]=ram==2?8192:ram==3?32768:ram==4?131072:65536;
                    result["lateJoin"]=false;
                }
                return result;
            }
        }
    }
    if(family.isEmpty())return {};
    return {{"id","runtime.handheld."+family+".v1"},{"label",r.adventure.title.left(96)},
        {"settings",mode+"-own-save-v1"},{"players",players},{"transport","netpacket"}};
}
QString handheldLinkCore(const QJsonObject& p) {
    if(p["transport"]=="rollback"&&p["settings"]=="mgba-linked-party-v2")return "mgba_splitscreen";
    if(p["transport"]=="rollback"&&p["settings"]=="sameboy-linked-pair-battery-v1")return "sameboy";
    if(p["transport"]=="rollback"&&p["settings"]=="dcgb-linked-pair-volatile-v1")return "DoubleCherryGB";
    if(p["transport"]!="netpacket")return {};
    return p["id"].toString().startsWith("runtime.handheld.gba-")?"gpsp":"DoubleCherryGB";
}
bool handheldLinkContentCompatible(const QJsonObject& p,const QString& sha256) {
    // This exact Hnefatafl build stalls its cable handshake in the pinned
    // SameBoy pair even locally. Do not advertise a known failed combination.
    // This is a negative compatibility exception, not a title-based allowlist.
    return p["settings"]!="sameboy-linked-pair-battery-v1"||
        sha256!="f76a1a8f9292bd68c9330dc9f2721d9b516e03b5ecba1f19cf81d540f528d3bb";
}
QByteArray handheldLinkOptions(const QJsonObject& p,int playerSlot,int players) {
    if(p["transport"]=="rollback") {
        if(handheldLinkCore(p)=="mgba_splitscreen"&&players>=2&&players<=4&&playerSlot>=1&&playerSlot<=players)
            return "splitscreen_players = \""+QByteArray::number(players)+"\"\nsplitscreen_layout = \"focus\"\n"
                "splitscreen_focus_player = \""+QByteArray::number(playerSlot)+"\"\n"
                "splitscreen_audio = \"player "+QByteArray::number(playerSlot)+"\"\n"
                "splitscreen_fs_assist = \"off\"\nsplitscreen_overlays = \"off\"\n";
        if(handheldLinkCore(p)=="sameboy"&&(playerSlot==1||playerSlot==2))
            return "sameboy_link = \"enabled\"\nsameboy_screen_layout = \"player "+QByteArray::number(playerSlot)+" only\"\n"
                "sameboy_audio_output = \"Game Boy #"+QByteArray::number(playerSlot)+"\"\n"
                "sameboy_model_1 = \"Auto\"\nsameboy_model_2 = \"Auto\"\n";
        if(handheldLinkCore(p)!="DoubleCherryGB"||(playerSlot!=1&&playerSlot!=2))return {};
        return "dcgb_emulated_gameboys = \"2\"\ndcgb_gblink_enable = \"enabled\"\n"
            "dcgb_single_screen_mp = \"player "+QByteArray::number(playerSlot)+" only\"\n"
            "dcgb_audio_output = \"Game Boy #"+QByteArray::number(playerSlot)+"\"\n"
            "dcgb_singleplayer_linked_devive = \"Off\"\ndcgb_pkmbuddyboy_auto_mew = \"0\"\n";
    }
    if(handheldLinkCore(p)=="gpsp")return "gpsp_serial = \""+p["settings"].toString().section('-',0,0).toLatin1()+
        "\"\ngpsp_rtc = \"system\"\n";
    // A disconnected friend must not be replaced by the core's distribution
    // machine when the network connection goes away.
    const auto clock = p["id"].toString().contains("gb-gen2-en") ? QByteArray("dcgb_rtc_use_system_clock = \"2\"\n") : QByteArray();
    return clock + "dcgb_emulated_gameboys = \"1\"\n"
        "dcgb_singleplayer_linked_devive = \"Off\"\n"
        "dcgb_pkmbuddyboy_auto_mew = \"0\"\n";
}
namespace {
QString gbSaveTarget(const AdventureRegistration& r,const RetroArchInstallation& i) {
        const auto s=readSettings(i.configFile);
        if(!supportedConfiguration(r,i,s))return {};
        QString base=enabled(s,"savefiles_in_content_dir")?QFileInfo(r.contentPath).absolutePath():configuredPath(s,"savefile_directory");
        if(enabled(s,"sort_savefiles_by_content_enable"))base=QDir(base).filePath(QFileInfo(r.contentPath).dir().dirName());
        if(enabled(s,"sort_savefiles_enable"))base=QDir(base).filePath("Gambatte");
        if(r.integrationConfig.contains("librarySaveBase")) {
            base=r.integrationConfig["librarySaveBase"].toString();
            if(r.integrationConfig["librarySaveSortCore"].toBool())base=QDir(base).filePath("Gambatte");
        }
        return QDir(base).filePath(QFileInfo(r.contentPath).completeBaseName()+".srm");
}
bool write(const QString& path,const QByteArray& bytes) {
    QSaveFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
}
QByteArray read(const QString& path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return {};
    return f.readAll();
}
}
QString recoverHandheldReturn(const AdventureRegistration& r,const RetroArchInstallation& i) {
    if(r.adventure.platformId!="gb"&&r.adventure.platformId!="gbc")return {};
    const auto target=gbSaveTarget(r,i);
    return target.isEmpty()?QString():handheld::recoverPair(target);
}
LinkedSaveSeed linkedSaveSeed(const AdventureRegistration& r,const RetroArchInstallation& i) {
    const auto profile=handheldLinkProfile(r);
    const int size=profile["saveBytes"].toInt();
    const bool gba=r.adventure.platformId=="gba";
    const auto save=gba?resolveRetroArchSave(r,i):SaveTarget();
    if(gba&&(!save.supported||!save.error.isEmpty()))return {{},false,save.error.isEmpty()?"Couldn't locate your game save.":save.error};
    const auto target=gba?save.savePath:gbSaveTarget(r,i);
    if(!requiresLinkedSavePreparation(profile)||size<8192||size>131072||
       target.isEmpty()||!safePath(target)||QFileInfo(target).isSymLink())
        return {{},false,"Couldn't locate a compatible game save."};
    const auto error=handheld::recoverPair(target);
    if(!error.isEmpty())return {{},false,error};
    const bool existed=QFileInfo::exists(target);
    auto bytes=existed?read(target):QByteArray(size,char(0xff));
    const int originalSize=existed?bytes.size():0;
    if(gba&&existed&&QList<int>{512,8192,32768,65536,131072}.contains(bytes.size()))
        bytes+=QByteArray(size-bytes.size(),char(0xff));
    if(bytes.size()!=size)return {{},existed,"This game's save format needs to be checked."};
    return {bytes,existed,{},originalSize};
}
QString prepareLinkedSave(ProcessCommand& cmd,const AdventureRegistration& r,const RetroArchInstallation& i,
        const QString& directory,const NetplayRequest& request,const std::atomic_bool& cancel) {
    const auto seed=linkedSaveSeed(r,i);
    if(!seed.error.isEmpty())return seed.error;
    const int size=seed.bytes.size(),slot=request.slot;
    if(seed.bytes!=request.ownSram||seed.existed!=request.ownSramExisted||
       (r.adventure.platformId=="gba"&&seed.originalSize!=request.ownSramOriginalSize))
        return "Your save changed. Invite your friend again.";
    const int players=request.linkedPlayers;
    if(players<2||players>(r.adventure.platformId=="gba"?4:2)||slot<1||slot>players||request.host!=(slot==1)||
       (request.host&&request.peerSrams.size()!=players-1)||(!request.host&&!request.peerSrams.isEmpty()))
        return "Couldn't prepare the players' saves.";
    if(request.host)for(int player=2;player<=players;++player)
        if(request.peerSrams.value(player).size()!=size)return "Couldn't prepare the players' saves.";
    const auto ownerDirectory=QDir(directory).filePath("owner");
    if(!QDir().mkpath(ownerDirectory))return "Couldn't prepare your game save.";
    const auto error=prepareHandheldSave(cmd,r,i,ownerDirectory,cancel);
    if(!error.isEmpty())return error;
    const bool gba=r.adventure.platformId=="gba";
    const auto suffix=gba?QString(".gba"):QString(".gb");
    const auto saveSuffix=[gba](int player){return gba?(player==1?QString(".sav"):QString(".sav")+QString::number(player)):QString(".srm");};
    const auto name=QFileInfo(r.contentPath).completeBaseName()+".srm";
    QFile rom(r.contentPath);
    if(!rom.open(QIODevice::ReadOnly)||rom.size()>(gba?32:8)*1024*1024)return "Couldn't read the linked game.";
    const auto content=rom.readAll();
    if(QString::fromLatin1(QCryptographicHash::hash(content,QCryptographicHash::Sha256).toHex())!=request.expected["content"].toString())
        return "Your game changed. Invite your friend again.";
    if(!QDir().mkpath(QDir(directory).filePath(".netplay")))return "Couldn't prepare the linked game.";
    // The upstream subsystem gives each cartridge its own standard SRAM API.
    // Distinct private ROM basenames prevent frontend save-name collisions.
    for(int player=1;player<=players;++player) {
        const auto stem="player-"+QString::number(player);
        const auto bytes=player==slot?seed.bytes:request.host?request.peerSrams.value(player):QByteArray(size,char(0xff));
        if(!write(QDir(directory).filePath(stem+suffix),content)||
           !write(QDir(directory).filePath(stem+saveSuffix(player)),bytes)||
           !write(QDir(directory).filePath(".netplay/"+stem+saveSuffix(player)),bytes))return "Couldn't prepare the linked game.";
    }
    const auto stem="player-"+QString::number(slot);
    const auto staged=QDir(directory).filePath(stem+saveSuffix(slot)),guest=QDir(directory).filePath(".netplay/"+stem+saveSuffix(slot));
    const auto previous=cmd.finalize;
    cmd.finalize=[previous,staged,guest,ownerDirectory,name,size,before=seed.bytes](const ProcessOutcome& outcome)->QString {
        if(!outcome.started)return previous?previous(outcome):QString();
        auto own=read(staged);const auto other=read(guest);
        if(own.size()!=size||other.size()!=size)
            return "Couldn't read the linked saves. Your original save and session folder were kept.";
        if(own!=before&&other!=before&&own!=other)
            return "Two different saves were kept in the session folder. Your original save was kept.";
        if(other!=before)own=other;
        if(!write(QDir(ownerDirectory).filePath(name),own))return "Couldn't keep your multiplayer save.";
        return previous?previous(outcome):QString();
    };
    return {};
}
QString prepareHandheldSave(ProcessCommand& cmd,const AdventureRegistration& r,
        const RetroArchInstallation& i,const QString& directory,const std::atomic_bool& cancel) {
    QString target;
    const bool gba=r.adventure.platformId=="gba";
    if(gba) {
        const auto save=resolveRetroArchSave(r,i);
        if(!save.supported||!save.error.isEmpty())return save.error.isEmpty()?"Couldn't locate your game save.":save.error;
        target=save.savePath;
    } else {
        target=gbSaveTarget(r,i);
        if(target.isEmpty())return "Couldn't locate your game save.";
    }
    if(cancel)return "Opening was cancelled.";
    if(!safePath(target)||QFileInfo(target).isSymLink()||!QDir().mkpath(QFileInfo(target).absolutePath()))
        return "Couldn't prepare your game save.";
    const auto recovered=handheld::recoverPair(target);
    if(!recovered.isEmpty())return recovered;
    const bool clock=handheldLinkProfile(r)["id"].toString().contains("gb-gen2-en");
    const auto rtc=handheld::rtcPath(target);
    const bool rtcExisted=clock&&QFileInfo::exists(rtc);
    const auto rtcBefore=rtcExisted?read(rtc):QByteArray();
    QByteArray clockSeed=rtcBefore;
    if(clock) {
        QFile core(i.cores.value("DoubleCherryGB"));
        if(!core.open(QIODevice::ReadOnly)||core.size()>32*1024*1024||
           !core.readAll().contains("traineros-mbc3-rtc-v1"))
            return "Update the multiplayer emulator before linking this game.";
        if(QFileInfo(rtc).isSymLink()||(rtcExisted&&rtcBefore.size()!=8))
            return "This game's clock format needs to be checked.";
        if(clockSeed.isEmpty()) {
            clockSeed.resize(8);
            qToLittleEndian<quint64>(QDateTime::currentSecsSinceEpoch(),clockSeed.data());
        }
        if(qFromLittleEndian<quint64>(clockSeed.constData())>quint64(QDateTime::currentSecsSinceEpoch()))
            return "Check the device's date and time before linking this game.";
    }
    const bool existed=QFileInfo::exists(target);
    const auto before=existed?read(target):QByteArray();
    // gpSP exports the largest GBA SRAM even for smaller cartridge memories.
    // Preserve their ordinary byte count on return. GB profiles use raw SRAM;
    // RTC is shared only through the explicit MBC3 bridge; other clocks stay untouched.
    if(existed&&(before.isEmpty()||before.size()>131072))return "This game's save format needs to be checked.";
    const auto staged=QDir(directory).filePath(QFileInfo(target).fileName());
    if(existed&&!write(staged,before))return "Couldn't prepare your game save.";
    // Some cores register netpacket during content loading, after RetroArch has
    // selected its guest .netplay save path. Seed both private locations with
    // this player's SRAM; neither location ever contains another player's save.
    const auto guestDirectory=QDir(directory).filePath(".netplay");
    const auto guest=QDir(guestDirectory).filePath(QFileInfo(target).fileName());
    if(!QDir().mkpath(guestDirectory)||(existed&&!write(guest,before)))
        return "Couldn't prepare your game save.";
    if(clock&&(!write(handheld::rtcPath(staged),clockSeed)||!write(handheld::rtcPath(guest),clockSeed)))
        return "Couldn't prepare your game clock.";
    const auto previous=cmd.finalize;
    cmd.finalize=[previous,target,staged,guest,before,existed,gba,clock,rtc,rtcBefore,rtcExisted,clockSeed](const ProcessOutcome& outcome)->QString {
        if(previous){const auto error=previous(outcome);if(!error.isEmpty())return error;}
        if(!outcome.started)return {};
        auto after=read(staged);
        auto selected=staged;
        const auto guestAfter=read(guest);
        if(!guestAfter.isEmpty()&&guestAfter!=before) {
            if(!after.isEmpty()&&after!=before&&after!=guestAfter)
                return "Two different multiplayer saves were kept in the session folder. Your original save was kept.";
            after=guestAfter;selected=guest;
        } else if(after.isEmpty()){after=guestAfter;selected=guest;}
        if(after.isEmpty())return "Couldn't read the multiplayer save. Your original save was kept.";
        QByteArray clockAfter;
        if(clock) {
            // SRAM may be unchanged while the cartridge clock has advanced its
            // epoch. Select the guest clock only when it owns the changed pair.
            const auto localClock=read(handheld::rtcPath(staged)),guestClock=read(handheld::rtcPath(guest));
            if(after==before&&guestClock!=clockSeed&&localClock==clockSeed)selected=guest;
            clockAfter=selected==guest?guestClock:localClock;
            if(clockAfter.size()!=8||qFromLittleEndian<quint64>(clockAfter.constData())>quint64(QDateTime::currentSecsSinceEpoch()))
                return "Couldn't read the multiplayer clock. Your original save and clock were kept.";
            if(localClock!=clockSeed&&guestClock!=clockSeed&&localClock!=guestClock)
                return "Two different multiplayer clocks were kept in the session folder. Your original save was kept.";
        }
        if(after==before&&(!clock||clockAfter==rtcBefore))return {};
        if(gba&&existed&&after.size()==131072&&before.size()<after.size())after.truncate(before.size());
        if(after==before&&(!clock||clockAfter==rtcBefore))return {};
        // Retain a durable recovery copy before any possible error/cleanup.
        const auto recovery=target+".link-"+QUuid::createUuid().toString(QUuid::Id128)+".srm";
        if(!write(recovery,after)||(clock&&!write(recovery+".rtc",clockAfter)))return "Couldn't keep the multiplayer save. Check storage before playing again.";
        if(outcome.crashed||outcome.exitCode!=0||QFileInfo(target).isSymLink()||
           QFileInfo::exists(target)!=existed||(existed&&read(target)!=before)||
           (clock&&(QFileInfo(rtc).isSymLink()||QFileInfo::exists(rtc)!=rtcExisted||(rtcExisted&&read(rtc)!=rtcBefore)))||
           after.size()>131072||(existed&&after.size()!=before.size()))
            return "Your original save was kept. The multiplayer save is beside it as a .link file.";
        if(clock) {
            const auto error=handheld::returnPair(target,before,after,rtcBefore,clockAfter);
            if(!error.isEmpty())return error;
            QFile::remove(recovery);QFile::remove(recovery+".rtc");return {};
        }
        if(existed&&!write(target+".before-link",before))return "Couldn't back up your save. The multiplayer copy was kept beside it.";
        if(!write(target,after))return "Couldn't update your save. The multiplayer copy was kept beside it.";
        QFile::remove(recovery);return {};
    };
    return {};
}
}
