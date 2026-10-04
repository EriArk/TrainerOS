#include "RetroArchHandheldLink.h"
#include "RetroArchSave.h"
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QUuid>

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
    } else if((r.adventure.platformId=="gb"||r.adventure.platformId=="gbc")&&
              QStringList{"gb","gbc"}.contains(QFileInfo(r.contentPath).suffix().toLower())) {
        auto title=h.mid(0x134,15);title=title.left(title.indexOf('\0')<0?title.size():title.indexOf('\0')).trimmed();
        if(QList<QByteArray>{"POKEMON RED","POKEMON BLUE","POKEMON YELLOW"}.contains(title))family="gb-gen1-en";
        if(QList<QByteArray>{"POKEMON_GLDAAUE","POKEMON_SLVAAXE","PM_CRYSTAL","POKEMON GOLD","POKEMON SILVER"}.contains(title))family="gb-gen2-en";
        if(title=="POKECARD"||title=="POKEMON CARD GB")family="gb-tcg";
        mode="cable";
    }
    if(family.isEmpty())return {};
    return {{"id","runtime.handheld."+family+".v1"},{"label",r.adventure.title.left(96)},
        {"settings",mode+"-own-save-v1"},{"players",players},{"transport","netpacket"}};
}
QString handheldLinkCore(const QJsonObject& p) {
    if(p["transport"]!="netpacket")return {};
    return p["id"].toString().startsWith("runtime.handheld.gba-")?"gpsp":"DoubleCherryGB";
}
QByteArray handheldLinkOptions(const QJsonObject& p) {
    if(handheldLinkCore(p)=="gpsp")return "gpsp_serial = \""+p["settings"].toString().section('-',0,0).toLatin1()+
        "\"\ngpsp_rtc = \"system\"\n";
    // A disconnected friend must not be replaced by the core's distribution
    // machine when the network connection goes away.
    return "dcgb_emulated_gameboys = \"1\"\n"
        "dcgb_singleplayer_linked_devive = \"Off\"\n"
        "dcgb_pkmbuddyboy_auto_mew = \"0\"\n";
}
namespace {
bool write(const QString& path,const QByteArray& bytes) {
    QSaveFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
}
QByteArray read(const QString& path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return {};
    return f.readAll();
}
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
        const auto s=readSettings(i.configFile);
        if(!supportedConfiguration(r,i,s))return "Couldn't locate your game save.";
        QString base=enabled(s,"savefiles_in_content_dir")?QFileInfo(r.contentPath).absolutePath():configuredPath(s,"savefile_directory");
        if(enabled(s,"sort_savefiles_by_content_enable"))base=QDir(base).filePath(QFileInfo(r.contentPath).dir().dirName());
        if(enabled(s,"sort_savefiles_enable"))base=QDir(base).filePath("Gambatte");
        if(r.integrationConfig.contains("librarySaveBase")) {
            base=r.integrationConfig["librarySaveBase"].toString();
            if(r.integrationConfig["librarySaveSortCore"].toBool())base=QDir(base).filePath("Gambatte");
        }
        target=QDir(base).filePath(QFileInfo(r.contentPath).completeBaseName()+".srm");
    }
    if(cancel)return "Opening was cancelled.";
    if(!safePath(target)||QFileInfo(target).isSymLink()||!QDir().mkpath(QFileInfo(target).absolutePath()))
        return "Couldn't prepare your game save.";
    const bool existed=QFileInfo::exists(target);
    const auto before=existed?read(target):QByteArray();
    // gpSP exports the largest GBA SRAM even for smaller cartridge memories.
    // Preserve their ordinary byte count on return. GB profiles use raw SRAM;
    // their incompatible RTC files are deliberately never copied over Gambatte.
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
    const auto previous=cmd.finalize;
    cmd.finalize=[previous,target,staged,guest,before,existed,gba](const ProcessOutcome& outcome)->QString {
        if(previous){const auto error=previous(outcome);if(!error.isEmpty())return error;}
        if(!outcome.started)return {};
        auto after=read(staged);
        const auto guestAfter=read(guest);
        if(!guestAfter.isEmpty()&&guestAfter!=before) {
            if(!after.isEmpty()&&after!=before&&after!=guestAfter)
                return "Two different multiplayer saves were kept in the session folder. Your original save was kept.";
            after=guestAfter;
        } else if(after.isEmpty())after=guestAfter;
        if(after.isEmpty())return "Couldn't read the multiplayer save. Your original save was kept.";
        if(after==before)return {};
        if(gba&&existed&&after.size()==131072&&before.size()<after.size())after.truncate(before.size());
        if(after==before)return {};
        // Retain a durable recovery copy before any possible error/cleanup.
        const auto recovery=target+".link-"+QUuid::createUuid().toString(QUuid::Id128)+".srm";
        if(!write(recovery,after))return "Couldn't keep the multiplayer save. Check storage before playing again.";
        if(outcome.crashed||outcome.exitCode!=0||QFileInfo(target).isSymLink()||
           QFileInfo::exists(target)!=existed||(existed&&read(target)!=before)||
           after.size()>131072||(existed&&after.size()!=before.size()))
            return "Your original save was kept. The multiplayer save is beside it as a .link file.";
        if(existed&&!write(target+".before-link",before))return "Couldn't back up your save. The multiplayer copy was kept beside it.";
        if(!write(target,after))return "Couldn't update your save. The multiplayer copy was kept beside it.";
        QFile::remove(recovery);return {};
    };
    return {};
}
}
