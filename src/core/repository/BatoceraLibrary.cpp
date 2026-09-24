#include "BatoceraLibrary.h"
#include "CollectionRepository.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamReader>
#include <QSet>
#include <QPointer>
#include <memory>

namespace trainer {
namespace {
const QStringList mediaTags{"image","screenshot","thumbnail","marquee","fanart","titleshot","video","manual","magazine","map","bezel","cartridge","boxart","boxback","wheel","mix"};
const QHash<QString,QStringList>& formats() {
    static const QHash<QString,QStringList> value{
        {"gb",{"gb","zip","7z"}}, {"gbc",{"gb","gbc","zip","7z"}}, {"gba",{"gba","zip","7z"}},
        {"nds",{"nds","srl","zip","7z"}}, {"n3ds",{"3ds","cci","cxi","cia"}},
        {"n64",{"n64","z64","v64","zip","7z"}}, {"gc",{"iso","gcm","rvz","gcz","ciso"}},
        {"wii",{"iso","rvz","gcz","wbfs","wad","ciso"}}, {"wiiu",{"wua","wud","wux","rpx"}},
        {"switch",{"nsp","xci","nro"}}, {"pokemini",{"min","zip","7z"}},
        {"nes",{"nes","zip","7z"}}, {"snes",{"sfc","smc","zip","7z"}},
        {"mastersystem",{"sms","zip","7z"}}, {"megadrive",{"md","bin","gen","zip","7z"}},
        {"gamegear",{"gg","zip","7z"}}, {"sega32x",{"32x","bin","zip","7z"}},
        {"segacd",{"chd","cue","m3u"}}, {"saturn",{"chd","cue","m3u"}},
        {"dreamcast",{"chd","gdi","cdi","m3u"}}, {"psx",{"chd","cue","pbp","m3u","iso"}},
        {"ps2",{"chd","iso","cso","bin"}}, {"psp",{"iso","cso","pbp"}},
        {"pcengine",{"pce","zip","7z"}}, {"pcenginecd",{"chd","cue","m3u"}},
        {"ngp",{"ngp","zip","7z"}}, {"ngpc",{"ngc","ngp","zip","7z"}},
        {"neogeo",{"zip","7z"}}, {"neogeocd",{"chd","cue","ccd","m3u"}},
        {"fbneo",{"zip","7z"}}, {"mame",{"zip","7z"}},
        {"naomi",{"zip","7z"}}, {"atomiswave",{"zip","7z"}}};
    return value;
}
QString key(const QString& path) {
    const QFileInfo file(path);
    const auto canonical=file.canonicalFilePath();
    return canonical.isEmpty()?QDir::cleanPath(file.absoluteFilePath()):canonical;
}
QString resolve(const QString& base, QString path) {
    path=path.trimmed();
    if(path.isEmpty() || path.contains("://"))return {};
    if(path.startsWith("~/"))path=QDir::home().filePath(path.mid(2));
    return QDir::cleanPath(QFileInfo(path).isAbsolute()?path:QDir(base).absoluteFilePath(path));
}
QString plain(QString value, int limit) {
    value.replace(QRegularExpression("[\\x{0000}-\\x{001f}\\x{007f}]")," ");
    return value.simplified().left(limit);
}
QString nameKey(QString value) {
    value=value.normalized(QString::NormalizationForm_D).toCaseFolded();
    value.remove(QRegularExpression("\\p{M}"));
    value.remove(QRegularExpression("^\\d{4}\\s*[-.]\\s*"));
    // Strip only known dump annotations; hack/translation titles remain distinct.
    value.remove(QRegularExpression("\\((?:usa|us|au|u|e|j|europe|world|australia|korea|japan|player's choice|lodgenet|rev [0-9.]+|v[0-9.]+|[a-z]{2}(?:,[a-z]{2})+)(?:, (?:usa|europe|japan))*\\)"));
    value.remove(QRegularExpression("\\[!\\]"));
    value.remove(QRegularExpression("\\bversion\\b"));
    value.replace("pocket monsters","pokemon");
    value.replace('&',"and");
    value.remove(QRegularExpression("[^a-z0-9]"));
    return value;
}
Adventure identify(const QString& filename,const QString& platform,bool hack) {
    const QString stem=QFileInfo(filename).completeBaseName();
    const auto normalized=nameKey(stem);
    const bool japanese=stem.contains(QRegularExpression("\\((?:Japan|J)(?:,|\\))",QRegularExpression::CaseInsensitiveOption));
    QList<Adventure> matches;
    for(const auto& a:collectionCatalogue(true)) {
        if(hack || a.platformId!=platform)continue;
        const bool jp=a.title.contains("(Japan)");
        if(jp!=japanese && (a.catalogueId=="red-gb" || a.catalogueId=="red-jp-gb" || a.catalogueId=="blue-gb" || a.catalogueId=="blue-jp-gb"))continue;
        auto title=nameKey(a.title);
        const bool yellow=a.catalogueId=="yellow-gb" && normalized=="pokemonyellow";
        if(title==normalized || yellow)matches.append(a);
    }
    if(matches.size()==1) {
        auto a=matches.front();a.collectionOnly=false;
        QStringList edition;
        auto annotations=QRegularExpression("\\(([^()]*)\\)").globalMatch(stem);
        while(annotations.hasNext())edition.append(annotations.next().captured(1));
        a.variant=plain(edition.join(" · "),96);
        return a;
    }
    Adventure a;
    a.title=plain(stem,96);a.platformId=platform;a.adapterId="unconfigured";
    // Unidentified Pokémon editions/hacks are never merged into a base game.
    if(normalized.contains("pokemon") || normalized.contains("pocketmonsters"))a.worldId="unclassified-pokemon";
    else a.domain="multiverse";
    if(hack || stem.contains("hack",Qt::CaseInsensitive))a.kind=AdventureKind::RomHack;
    return a;
}
using Metadata=QHash<QString,QVariantMap>;
Metadata readGamelist(const QString& directory,QStringList& warnings) {
    QFile file(QDir(directory).filePath("gamelist.xml"));
    if(!file.exists())return {};
    if(!file.open(QIODevice::ReadOnly) || file.size()>32*1024*1024) {warnings.append(directory+": cannot read gamelist.xml");return {};}
    QXmlStreamReader xml(&file);Metadata result;
    if(!xml.readNextStartElement() || xml.name()!=u"gameList")xml.raiseError("Expected gameList");
    while(!xml.hasError() && xml.readNextStartElement()) {
        if(xml.name()!=u"game") {xml.skipCurrentElement();continue;}
        QVariantMap item;QString path;
        while(xml.readNextStartElement()) {
            const auto tag=xml.name().toString();
            if(tag=="path")path=resolve(directory,xml.readElementText());
            else if(mediaTags.contains(tag)) {
                const auto value=resolve(directory,xml.readElementText());
                const QFileInfo media(value);
                if(!value.isEmpty() && media.isFile() && media.isReadable())item[tag]=QUrl::fromLocalFile(media.absoluteFilePath()).toString();
            } else if(QStringList{"name","desc","genre","players","releasedate","developer","publisher","hidden"}.contains(tag))
                item[tag]=plain(xml.readElementText(),tag=="desc"?4096:256);
            else xml.skipCurrentElement();
        }
        if(!path.isEmpty())result.insert(key(path),item);
        if(result.size()>20000)xml.raiseError("Too many entries");
    }
    while(!xml.atEnd())xml.readNext();
    if(xml.hasError()) {warnings.append(directory+": invalid gamelist.xml");return {};}
    return result;
}
}
QStringList batoceraPlatforms() { return formats().keys(); }
FolderScan scanBatoceraLibrary(const QString& roms,const QList<AdventureRegistration>& existing) {
    FolderScan result;QDir root(roms);
    if(!root.exists()) {result.warnings.append("The ROM folder is unavailable.");return result;}
    root=QDir(root.canonicalPath());
    QHash<QString,AdventureRegistration> known;
    for(const auto& r:existing)known.insert(key(r.contentPath),r);
    QSet<QString> seen;
    for(const auto& folder:root.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)) {
        QString platform=folder.fileName();
        if(platform=="gamecube")platform="gc";
        if(platform=="3ds")platform="n3ds";
        const auto metadata=readGamelist(folder.absoluteFilePath(),result.warnings);
        QStringList paths;QSet<QString> parts;
        QDirIterator files(folder.absoluteFilePath(),QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
        int visited=0;
        while(files.hasNext()) {
            if(QThread::currentThread()->isInterruptionRequested())return {};
            files.next();const auto file=files.fileInfo();
            // Dot directories are not automatically hidden on every filesystem.
            if(QDir::fromNativeSeparators(file.absoluteFilePath()).contains("/.traineros-trash/"))continue;
            if(++visited>50000) {result.warnings.append(folder.fileName()+": folder scan limit reached");break;}
            if(!file.isFile() || !file.isReadable())continue;
            const auto path=key(file.absoluteFilePath());
            if(!known.contains(path)) {
                // Batocera arcade sets keep dependencies next to their games.
                // CHDs on NAOMI/Atomiswave are companions, not separate entries.
                const bool arcade=QStringList{"fbneo","mame","neogeo","neogeocd","naomi","atomiswave"}.contains(platform);
                const bool archive=QStringList{"zip","7z"}.contains(file.suffix().toLower());
                if(arcade && archive && QStringList{"neogeo","neocdz","awbios","naomi","naomigd","naomi2",
                    "hod2bios","f355bios","f355dlx","airlbios","qsound","qsound_hle","pgm","skns","midssio"}.contains(file.completeBaseName().toLower()))continue;
                if(file.fileName().contains(QRegularExpression("[\\[(](?:DLC|Update|Update Data)[\\])]",QRegularExpression::CaseInsensitiveOption)))continue;
            }
            if(file.suffix().compare("ccd",Qt::CaseInsensitive)==0 && !known.contains(path)
                && QFileInfo::exists(file.absolutePath()+"/"+file.completeBaseName()+".cue"))continue;
            if(!formats().value(platform).contains(file.suffix().toLower()) && !known.contains(path)) {
                if(QStringList{"naomi","atomiswave"}.contains(platform) && file.suffix().compare("chd",Qt::CaseInsensitive)==0)continue;
                // A misplaced ROM must remain reachable so Move can correct its
                // platform folder. Do not silently hide another known format.
                bool recognized=false;
                // Raw BINs can be BIOS or disc tracks. They remain visible only
                // where the platform explicitly treats them as launch content.
                if(file.suffix().compare("bin",Qt::CaseInsensitive)!=0)
                    for(const auto& extensions:formats())if(extensions.contains(file.suffix().toLower())){recognized=true;break;}
                if(!recognized)continue;
            }
            paths.append(path);
            if(file.suffix().compare("m3u",Qt::CaseInsensitive)==0 && file.size()<1024*1024) {
                QFile playlist(path);if(playlist.open(QIODevice::ReadOnly))for(const auto& line:QString::fromUtf8(playlist.readAll()).split('\n')) {
                    if(!line.trimmed().isEmpty() && !line.trimmed().startsWith('#'))parts.insert(key(resolve(file.absolutePath(),line)));
                }
            }
        }
        // A trashed playlist still owns its disc entries; don't rediscover them
        // as new games while its original path is intentionally absent.
        for(const auto& record:known) if(record.removed && record.adventure.platformId==platform
                && QFileInfo(record.contentPath).suffix().compare("m3u",Qt::CaseInsensitive)==0) {
            QFile playlist(record.trashPath);
            if(playlist.size()<1024*1024 && playlist.open(QIODevice::ReadOnly))
                for(const auto& line:QString::fromUtf8(playlist.readAll()).split('\n'))
                    if(!line.trimmed().isEmpty() && !line.trimmed().startsWith('#'))
                        parts.insert(key(resolve(QFileInfo(record.contentPath).absolutePath(),line)));
        }
        paths.sort();
        for(const auto& path:paths) {
            if(seen.contains(path) || (parts.contains(path) && !known.contains(path)))continue;
            const auto data=metadata.value(path);
            if(data.value("hidden").toString()=="true" && !known.contains(path))continue;
            seen.insert(path);FolderEntry entry;entry.existing=known.contains(path);
            if(entry.existing)entry.record=known.value(path);
            else {
                const auto relative=root.relativeFilePath(path).toLower();
                entry.record.adventure=identify(path,platform,relative.contains("/romhacks/"));
                if(relative.contains("/pokemon/") && entry.record.adventure.catalogueId.isEmpty()) {
                    entry.record.adventure.domain="pokemon";entry.record.adventure.worldId="unclassified-pokemon";
                }
                entry.record.contentPath=path;
                entry.record.adventure.id="folder-"+QString::fromLatin1(QCryptographicHash::hash(root.relativeFilePath(path).toUtf8(),QCryptographicHash::Sha256).toHex().left(32));
                if(!data.value("name").toString().isEmpty())entry.record.adventure.title=plain(data.value("name").toString(),96);
                if(entry.record.adventure.catalogueId.isEmpty())entry.record.adventure.description=plain(data.value("desc").toString(),160);
                if(entry.record.adventure.worldId=="unclassified-pokemon")entry.record.newWorld=World{"unclassified-pokemon","Other Pokémon",{}};
            }
            entry.media=data;
            for(const auto& tag:QStringList{"image","thumbnail","boxart","titleshot","fanart","mix"})
                if(!data.value(tag).toString().isEmpty()) {entry.media["cover"]=data.value(tag);break;}
            result.entries.append(entry);
            if(result.entries.size()>=20000) {result.warnings.append("Library scan limit reached.");return result;}
        }
    }
    return result;
}
BatoceraLibrary::BatoceraLibrary(LibraryRepository& library,QString roms,QObject* parent)
    :QObject(parent),library_(library),roms_(std::move(roms)) {
    deferredScan_.setSingleShot(true);
    connect(&deferredScan_,&QTimer::timeout,this,&BatoceraLibrary::rescan);
}
BatoceraLibrary::~BatoceraLibrary() {
    if(thread_) {thread_->requestInterruption();thread_->wait();delete thread_;}
}
QString BatoceraLibrary::storageRootFor(const QString& id) const {
    const auto record=library_.registration(id);if(!record)return {};
    const auto file=QFileInfo(record->contentPath).canonicalFilePath();if(file.isEmpty())return {};
    for(const auto& entry:QDir(roms_).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot)) {
        auto platform=entry.fileName();if(platform=="gamecube")platform="gc";if(platform=="3ds")platform="n3ds";
        if(!formats().contains(platform))continue;
        const auto root=entry.canonicalFilePath();
        if(!root.isEmpty() && file.startsWith(root+'/'))return QFileInfo(roms_).canonicalFilePath();
    }
    return {};
}
void BatoceraLibrary::editLibraryAsync(const LibraryEdit& edit,QObject* context,std::function<void(QString)> done) {
    if(edit.kind!=LibraryEditKind::MoveFile){library_.editLibraryAsync(edit,context,std::move(done));return;}
    if(busy_){done("The library is refreshing. Try the move again in a moment.");return;}
    auto request=edit;request.storageRoot=storageRootFor(edit.id);
    const auto record=library_.registration(edit.id);
    if(!record || request.storageRoot.isEmpty()){done("Reconnect this game's library storage before moving it.");return;}
    const auto relative=QDir(request.storageRoot).relativeFilePath(request.text);
    const auto folder=relative.section('/',0,0);auto platform=folder;
    if(platform=="gamecube")platform="gc";if(platform=="3ds")platform="n3ds";
    if(!formats().contains(platform) || !formats().value(platform).contains(QFileInfo(record->contentPath).suffix().toLower())) {
        done("Choose the correct platform folder for this file type.");return;
    }
    request.relocated=*record;
    if(platform!=record->adventure.platformId) {
        auto& moved=*request.relocated;moved.adventure.platformId=platform;
        moved.adventure.adapterId="unconfigured";moved.integrationConfig={};
        const auto identified=identify(record->contentPath,platform,record->adventure.kind==AdventureKind::RomHack);
        moved.adventure.catalogueId=identified.catalogueId;
        // Keep explicit World membership and personal identity/history.
        moved.contentPath=QDir(request.text).filePath(QFileInfo(record->contentPath).fileName());
        if(prepareInstallation)prepareInstallation(moved);
    } else if(prepareFileMove) {
        const auto error=prepareFileMove(*record,request);
        if(!error.isEmpty()){done(error);return;}
    }
    deferredScan_.stop();busy_=true;writing_=true;emit busyChanged();emit writingChanged();
    library_.editLibraryAsync(request,this,[this,guard=QPointer<QObject>(context),done=std::move(done)](const QString& error){
        busy_=false;writing_=false;emit busyChanged();emit writingChanged();
        if(guard)done(error);
        if(error.isEmpty())rescan();
    });
}
void BatoceraLibrary::refreshContentAvailability() {
    if(busy_ || !library_.editable())return;
    // Page/focus changes may request repeatedly. Keep navigation independent of
    // storage work, while still observing files copied during the current run.
    if(lastScan_.isValid() && lastScan_.elapsed()<10000) {
        if(!deferredScan_.isActive())deferredScan_.start(int(10000-lastScan_.elapsed()));
        return;
    }
    rescan();
}
void BatoceraLibrary::rescan() {
    if(busy_ || !library_.editable())return;
    deferredScan_.stop();lastScan_.start();
    library_.refreshContentAvailability();
    const auto existing=library_.registrations();
    busy_=true;emit busyChanged();
    auto result=std::make_shared<FolderScan>();
    thread_=QThread::create([result,root=roms_,existing]{*result=scanBatoceraLibrary(root,existing);});
    connect(thread_,&QThread::finished,this,[this,result] {
        thread_->wait();delete thread_;thread_=nullptr;
        scan_=std::move(*result);index_=0;added_=0;nextMedia_.clear();importNext();
    });
    thread_->start();
}
void BatoceraLibrary::importNext() {
    while(index_<scan_.entries.size()) {
        auto entry=scan_.entries[index_++];auto r=entry.record;
        // Recheck after asynchronous discovery: do not replace edits or bindings.
        const auto current=library_.registration(r.adventure.id);
        if(current) {
            if(QDir::cleanPath(current->contentPath)==QDir::cleanPath(r.contentPath))nextMedia_.insert(r.adventure.id,entry.media);
            // A newly copied ROM may reuse its old identity and history. Legacy
            // trash is restored explicitly and never overwrites a replacement.
            if(current->removed && current->trashPath.isEmpty() && QFileInfo(current->contentPath).isFile()) {
                if(!writing_) {writing_=true;emit writingChanged();}
                library_.editLibraryAsync({LibraryEditKind::RestoreGame,current->adventure.id,current->revision},this,[this](const QString& error){
                    if(error.isEmpty())++added_;else scan_.warnings.append(error);
                    QTimer::singleShot(0,this,&BatoceraLibrary::importNext);
                });
                return;
            }
            continue;
        }
        if(entry.existing)continue;
        if(!collectionExclusion(r.adventure,r.contentPath).isEmpty())continue;
        bool duplicate=false;
        for(const auto& other:library_.registrations())
            if(QDir::cleanPath(other.contentPath)==QDir::cleanPath(r.contentPath)){duplicate=true;break;}
        if(duplicate)continue;
        if(r.newWorld)for(const auto& w:library_.worlds())if(w.id==r.newWorld->id){r.newWorld.reset();break;}
        if(prepareInstallation)prepareInstallation(r);
        if(!writing_) {writing_=true;emit writingChanged();}
        library_.saveAdventureAsync(r,this,[this,id=r.adventure.id,media=entry.media](const auto& write) {
            if(write.success){++added_;nextMedia_.insert(id,media);}
            else scan_.warnings.append(write.error);
            QTimer::singleShot(0,this,&BatoceraLibrary::importNext);
        });
        return;
    }
    const bool mediaChanged=media_!=nextMedia_;
    media_=std::move(nextMedia_);
    if(writing_) {writing_=false;emit writingChanged();}
    busy_=false;emit busyChanged();
    if(mediaChanged || added_)emit changed();
    emit scanFinished(added_,scan_.warnings);
    scan_={};
}
}
