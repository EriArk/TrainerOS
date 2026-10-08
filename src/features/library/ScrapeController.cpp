#include "ScrapeController.h"
#include "core/repository/RomPlatforms.h"
#include "core/repository/CollectionRepository.h"
#include "core/model/SeriesCatalog.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace trainer {
namespace {
using namespace scraper;
QString statusMessage(Status s) {
    switch(s) {
    case Status::Ready:return "Connected to ScreenScraper.";
    case Status::MissingCredentials:return "ScreenScraper access is not configured for this installation.";
    case Status::Denied:return "ScreenScraper refused access. Check your login and password in Settings.";
    case Status::MembersOnly:return "ScreenScraper currently restricts guest or inactive accounts. Sign in or try again later.";
    case Status::ClientRejected:return "ScreenScraper requires an updated or approved TrainerOS client.";
    case Status::Quota:return "Today's ScreenScraper limit has been reached. Resume tomorrow.";
    case Status::Busy:return "ScreenScraper is busy. Pause and retry later.";
    case Status::NotFound:return "No matching game found. Try another title or skip this game.";
    case Status::Cancelled:return "Stopped. Completed games have been kept.";
    case Status::InvalidResponse:return "ScreenScraper returned an unreadable response. Try again later.";
    default:return "Could not reach ScreenScraper. Check your connection and retry.";
    }
}
QJsonObject readJson(const QString& path) {
    QFile f(path);if(QFileInfo(path).isSymLink()||!f.open(QIODevice::ReadOnly)||f.size()>256*1024)return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
bool saveJson(const QString& path,const QJsonObject& o) {
    if(QFileInfo(path).isSymLink()||!QDir().mkpath(QFileInfo(path).absolutePath()))return false;
    QSaveFile f(path);f.setDirectWriteFallback(false);const auto bytes=QJsonDocument(o).toJson();
    return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
}
QVariantMap row(const QString& label,const QString& detail={},bool enabled=true) {
    return {{"label",label},{"detail",detail},{"enabled",enabled}};
}
QMap<QString,QString> localFields(const QString& root,const QString& path) {
    QFile f(QDir(root).filePath("gamelist.xml"));QMap<QString,QString> fields;
    if(!f.open(QIODevice::ReadOnly)||f.size()>32*1024*1024)return fields;
    const auto bytes=f.readAll();QDomDocument doc;
    if(bytes.toUpper().contains("<!DOCTYPE")||!doc.setContent(bytes))return fields;
    for(auto game=doc.documentElement().firstChildElement("game");!game.isNull();game=game.nextSiblingElement("game")) {
        if(QFileInfo(QDir(root).filePath(game.firstChildElement("path").text())).canonicalFilePath()!=path)continue;
        for(auto node=game.firstChildElement();!node.isNull();node=node.nextSiblingElement())fields[node.tagName()]=node.text();
        break;
    }
    return fields;
}
bool cachedMediaValid(const QString& root,const QString& path) {
    const QFileInfo f(path);return f.isFile()&&f.isReadable()&&f.canonicalFilePath().startsWith(root+'/');
}
}
ScrapeController::ScrapeController(LibraryRepository& library,QObject* parent):QObject(parent),library_(library) {}
ScrapeController::~ScrapeController(){if(cancel_)cancel_->store(true);future_.waitForFinished();}
void ScrapeController::configure(const QString& directory) {
    if(busy())return;
    directory_=directory;credentialsPath_=QDir(directory).filePath("secrets/screenscraper.json");
    credentials_=readCredentials(credentialsPath_);
    const auto settings=readJson(QDir(directory).filePath("screenscraper-settings.json"));
    preferences_=Preferences::fromJson(settings);display_=settings["display"].toObject();
    emit changed();
}
void ScrapeController::setTransport(Transport t,Delay d){if(!busy()){transport_=std::move(t);delay_=std::move(d);}}
QVariantList ScrapeController::settingsRows() const {
    const bool enabled=!busy();
    return {row("Login",credentials_.username.isEmpty()?"Optional ScreenScraper account":credentials_.username,enabled),
        row("Password",credentials_.password.isEmpty()?"Not set":"Saved privately on this device",enabled),
        row("Save & check connection","ScreenScraper.fr account",enabled),
        row("Sign out","Remove the saved user login and password",enabled&&!credentials_.username.isEmpty()),
        row("Language",preferences_.language+" (English fallback)",enabled),row("Preferred region",preferences_.region,enabled),
        row("Game information",preferences_.metadata?"On · description, genre, players, date, studio":"Off",enabled),
        row("Covers",preferences_.cover.isEmpty()?"Off":preferences_.cover=="box-3D"?"3D box":"2D box",enabled),
        row("Game logos",preferences_.logos?"On":"Off",enabled),row("Screenshots",preferences_.screenshots?"On":"Off",enabled),
        row("Background artwork",preferences_.fanart?"On":"Off",enabled),
        row("Existing information",preferences_.refresh?"Replace selected fields · keep game names":"Fill empty fields only",enabled),
        row("Scrape library","Choose and review the job before starting",enabled),
        row("Display · main image",display_["picture"].toString()=="cover"?"Cover first":"Screenshot first",enabled),
        row("Display · logos",display_["logos"].toBool(true)?"Game logos":"Text titles",enabled),
        row("Display · background",display_["background"].toBool(false)?"Downloaded artwork":"TrainerOS theme",enabled),
        row("Display · game facts",display_["facts"].toBool(true)?"On · year, genre, developer, publisher":"Off",enabled),
        row("Display · descriptions",display_["description"].toBool(true)?"On":"Off",enabled)};
}
void ScrapeController::activateSetting(int index) {
    if(busy()||index<0||index>=settingsRows().size())return;
    settingsFocus_=index;
    const auto cycle=[](const QStringList& list,const QString& value){return list[(list.indexOf(value)+1)%list.size()];};
    if(index<2){textTarget_=index==0?"login":"password";emit textRequested(index==0?"ScreenScraper login":"ScreenScraper password",index==0?credentials_.username:QString(),index==1);return;}
    if(index==2){checkAccount(false);return;}
    if(index==3){credentials_.username.clear();credentials_.password.clear();if(!writeCredentials(credentialsPath_,credentials_))status_="Could not save the account change.";else status_="Signed out.";}
    if(index==4)preferences_.language=cycle({"en","ru","fr","de","es","it","pt","ja"},preferences_.language);
    if(index==5)preferences_.region=cycle({"us","eu","jp","wor"},preferences_.region);
    if(index==6)preferences_.metadata=!preferences_.metadata;
    if(index==7)preferences_.cover=cycle({"box-2D","box-3D",""},preferences_.cover);
    if(index==8)preferences_.logos=!preferences_.logos;
    if(index==9)preferences_.screenshots=!preferences_.screenshots;
    if(index==10)preferences_.fanart=!preferences_.fanart;
    if(index==11)preferences_.refresh=!preferences_.refresh;
    if(index==12){begin();return;}
    if(index==13)display_["picture"]=display_["picture"].toString()=="cover"?"screenshot":"cover";
    if(index==14)display_["logos"]=!display_["logos"].toBool(true);
    if(index==15)display_["background"]=!display_["background"].toBool(false);
    if(index==16)display_["facts"]=!display_["facts"].toBool(true);
    if(index==17)display_["description"]=!display_["description"].toBool(true);
    if(index>=4){savePreferences();if(index>=13)emit displayChanged();}
    emit changed();
}
void ScrapeController::savePreferences(){auto settings=preferences_.json();settings["display"]=display_;if(directory_.isEmpty()||!saveJson(QDir(directory_).filePath("screenscraper-settings.json"),settings))status_="Could not save ScreenScraper preferences.";}
void ScrapeController::applyText(const QString& text) {
    const auto target=std::exchange(textTarget_,{});
    if(target=="login"){credentials_.username=text.trimmed().left(256);status_="Use Save & check connection to apply this account.";}
    else if(target=="password"){credentials_.password=text.left(256);status_="Use Save & check connection to apply this account.";}
    else if(target=="search"&&active_&&!working_&&!text.trimmed().isEmpty()) {
        choosing_=false;phase_="Searching titles";const auto item=queue_.value(index_);auto client=client_;auto cancel=cancel_;auto pending=pending_;
        run([client,cancel,pending,item,text]() mutable {auto r=client->search(item.platform,text.left(256),cancel);pending.status=r.status;pending.games=r.games;return pending;},[this](Outcome o){found(std::move(o));});
    }
    emit changed();
}
void ScrapeController::dispatchSettings(Action a) {
    if(a==Action::Back){emit settingsBackRequested();return;}
    if(a==Action::Up)settingsFocus_=std::max(0,settingsFocus_-1);
    if(a==Action::Down)settingsFocus_=std::min(int(settingsRows().size())-1,settingsFocus_+1);
    if((a==Action::Left||a==Action::Right)&&!busy()&&settingsFocus_>=4&&settingsFocus_!=12) {
        const auto previous=[](const QStringList& values,const QString& current){return values[(values.indexOf(current)+values.size()-1)%values.size()];};
        if(a==Action::Left&&settingsFocus_==4)preferences_.language=previous({"en","ru","fr","de","es","it","pt","ja"},preferences_.language);
        else if(a==Action::Left&&settingsFocus_==5)preferences_.region=previous({"us","eu","jp","wor"},preferences_.region);
        else if(a==Action::Left&&settingsFocus_==7)preferences_.cover=previous({"box-2D","box-3D",""},preferences_.cover);
        else {activateSetting(settingsFocus_);return;}
        savePreferences();emit changed();return;
    }
    if(a==Action::Confirm)activateSetting(settingsFocus_);else emit changed();
}
void ScrapeController::run(std::function<Outcome()> task,std::function<void(Outcome)> done) {
    working_=true;auto* watcher=new QFutureWatcher<Outcome>(this);
    connect(watcher,&QFutureWatcher<Outcome>::finished,this,[this,watcher,done=std::move(done)]() mutable {
        working_=false;auto result=watcher->result();watcher->deleteLater();done(std::move(result));emit changed();
    });
    auto future=QtConcurrent::run(std::move(task));future_=future;watcher->setFuture(future);emit changed();
}
void ScrapeController::checkAccount(bool startJob) {
    if(!credentials_.valid()){
        status_=statusMessage(Status::MissingCredentials);
        if(startJob){for(const auto& item:queue_){failures_<<item;taskStates_[item.id]="failed";taskErrors_[item.id]=status_;}finish();}
        emit changed();return;
    }
    if(!startJob&&!writeCredentials(credentialsPath_,credentials_)){status_="Could not store this account privately.";emit changed();return;}
    cancel_=std::make_shared<std::atomic_bool>(false);client_=std::make_shared<Client>(credentials_,transport_,delay_);client_->setPreferences(preferences_);
    const auto client=client_;const auto cancel=cancel_;phase_="Checking connection";
    run([client,cancel]{auto r=client->account(cancel);Outcome o;o.status=r.status;o.quota=r.quota;return o;},[this,startJob](Outcome o){
        if(startJob&&handleCancellation())return;
        status_=statusMessage(o.status);
        if(o.status==Status::Ready)status_+=QString(" %1 / %2 daily requests used.").arg(o.quota.today).arg(o.quota.daily);
        if(startJob){if(o.status==Status::Ready){phase_="Preparing queue";next();}else {for(int i=index_;i<queue_.size();++i){failures_<<queue_[i];taskStates_[queue_[i].id]="failed";taskErrors_[queue_[i].id]=status_;}finish();}}
    });
}
QList<ScrapeController::Item> ScrapeController::items() const {
    QList<Item> result;const auto selected=library_.registration(gameId_);
    for(const auto& r:library_.registrations()) {
        if(r.removed||r.adventure.collectionOnly||r.contentPath.isEmpty()||!r.contentAvailable)continue;
        // Curated downloads own their metadata. ScreenScraper only updates ROM-folder entries.
        if(r.integrationConfig.contains("oddcrate"))continue;
        if(scope_==0&&!gameId_.isEmpty()&&r.adventure.id!=gameId_)continue;
        if(scope_==1&&selected&&r.adventure.platformId!=selected->adventure.platformId)continue;
        if(scope_==3&&selected) {
            if(r.adventure.domain!=selected->adventure.domain)continue;
            if(selected->adventure.domain!="pokemon"&&seriesForTitle(r.adventure.title)!=seriesForTitle(selected->adventure.title))continue;
        }
        if(!worldId_.isEmpty()&&r.adventure.worldId!=worldId_&&!r.adventure.additionalWorldIds.contains(worldId_))continue;
        const auto root=library_.storageRootFor(r.adventure.id);const auto path=QFileInfo(r.contentPath).canonicalFilePath();
        if(root.isEmpty()||path.isEmpty()||!path.startsWith(root+'/'))continue;
        const auto folder=QDir(root).relativeFilePath(path).section('/',0,0);
        if(scope_==4&&!selectedSystems_.contains(romPlatformId(folder)))continue;
        const auto system=QFileInfo(QDir(root).filePath(folder)).canonicalFilePath();
        if(system.isEmpty()||!path.startsWith(system+'/'))continue;
        result.append({r.adventure.id,r.adventure.title,path,romPlatformId(folder),system});
    }
    return result;
}
void ScrapeController::begin(const QString& game,const QString& world) {
    if(busy())return;
    open_=true;finished_=false;gameId_=game;worldId_=world;scope_=game.isEmpty()?2:0;focus_=0;status_.clear();results_.clear();emit changed();
}
void ScrapeController::beginSystems() {
    if(busy())return;
    gameId_.clear();worldId_.clear();scope_=2;
    QMap<QString,int> counts;
    for(const auto& item:items())++counts[item.platform];
    systems_.clear();selectedSystems_.clear();
    for(const auto& platform:romPlatforms()) {
        if(!counts.contains(platform.id))continue;
        const bool supported=scraper::systemId(platform.id)>0;
        systems_.append(QVariantMap{{"id",platform.id},{"label",platform.name},
            {"shape",platform.shape},{"count",counts.value(platform.id)},{"enabled",supported}});
    }
    std::sort(systems_.begin(),systems_.end(),[](const QVariant& a,const QVariant& b){
        return QString::localeAwareCompare(a.toMap()["label"].toString(),b.toMap()["label"].toString())<0;
    });
    scope_=4;open_=true;finished_=false;focus_=systemListFocus_=0;status_.clear();results_.clear();emit changed();
}
QVariantList ScrapeController::systemRows() const {
    auto result=systems_;
    for(auto& value:result){auto r=value.toMap();r["checked"]=selectedSystems_.contains(r["id"].toString());value=r;}
    return result;
}
int ScrapeController::selectedGameCount() const {
    int count=0;
    for(const auto& value:systems_){const auto r=value.toMap();if(selectedSystems_.contains(r["id"].toString()))count+=r["count"].toInt();}
    return count;
}
QString ScrapeController::title() const {return selectingSystems()?"ScreenScraper":choosing_?"Choose the matching game":"ScreenScraper";}
QString ScrapeController::detail() const {
    if(active_)return QString("%1 / %2 · %3 failed · %4 skipped\n%5\n%6").arg(done_).arg(queue_.size()).arg(failed_).arg(skipped_).arg(queue_.value(index_).title,paused_?"Paused after the current game":phase_);
    if(finished_)return QString("%1 / %2 processed · %3 failed · %4 skipped\n").arg(done_).arg(queue_.size()).arg(failed_).arg(skipped_)+results_.mid(std::max(0,int(results_.size())-3)).join('\n');
    if(selectingSystems())return QString("%1 systems selected · %2 games\n%3 · ScreenScraper").arg(selectedSystems_.size()).arg(selectedGameCount()).arg(preferences_.refresh?"Replace selected fields":"Fill empty fields only");
    return QString("%1 games · %2\nDescriptions and artwork from ScreenScraper.fr. ROMs, saves and your game names stay in place.").arg(items().size()).arg(preferences_.refresh?"Replace selected fields":"Fill empty fields only");
}
QVariantList ScrapeController::rows() const {
    if(choosing_) {
        QVariantList r;for(const auto& g:pending_.games)r<<row(g.fields.value("name"),g.fields.value("publisher")+" · "+g.fields.value("releasedate").left(4));
        r<<row("Search another title")<<row("Skip this game")<<row("Stop scraping");return r;
    }
    if(active_)return {row(paused_?"Resume":"Pause"),row("Stop scraping")};
    if(finished_)return {row("Retry failed games",{},!failures_.isEmpty()),row("Close")};
    if(selectingSystems()) {
        int supported=0;for(const auto& value:systems_)if(value.toMap()["enabled"].toBool())++supported;
        QVariantList result{row(supported>0&&selectedSystems_.size()==supported?"Clear selection":"Select all",{},supported>0)};
        for(const auto& value:systemRows()){
            const auto r=value.toMap();result<<row(r["label"].toString(),QString::number(r["count"].toInt())+" games",r["enabled"].toBool());
        }
        result<<row("Download information & artwork",{},selectedGameCount()>0)<<row("Cancel");return result;
    }
    const auto selected=library_.registration(gameId_);
    const auto collection=selected?seriesDefinition(selected->adventure.domain=="pokemon"?QString("pokemon"):seriesForTitle(selected->adventure.title)).name:QString();
    return {row("Scope",!worldId_.isEmpty()?"Current collection":scope_==0?"This game":scope_==1?"Current system":scope_==3?collection+" collection":"Whole library",!gameId_.isEmpty()),
        row("Start scraping",{},!items().isEmpty()),row("Cancel")};
}
void ScrapeController::close(){if(busy())return;const bool systems=selectingSystems();open_=false;emit changed();if(systems)emit systemSelectionClosed();}
void ScrapeController::dispatch(Action a) {
    if(a==Action::Back){if(active_){cancel_->store(true);if(!working_)finish();else {phase_="Stopping";emit changed();}}else close();return;}
    if(selectingSystems()&&(a==Action::Left||a==Action::Right)) {
        const int startIndex=int(systems_.size())+1;
        if(a==Action::Right){if(focus_<startIndex){systemListFocus_=focus_;focus_=startIndex;}else focus_=startIndex+1;}
        else if(focus_>startIndex)focus_=startIndex;else if(focus_==startIndex)focus_=systemListFocus_;
        emit changed();return;
    }
    if(a==Action::Up)focus_=std::max(0,focus_-1);
    if(a==Action::Down)focus_=std::min(int(rows().size())-1,focus_+1);
    if(a==Action::Confirm)activate(focus_);else emit changed();
}
void ScrapeController::activate(int i) {
    if(i<0||i>=rows().size()||!rows()[i].toMap()["enabled"].toBool())return;
    focus_=i;
    if(selectingSystems()) {
        if(i==0){
            QSet<QString> all;for(const auto& value:systems_)if(value.toMap()["enabled"].toBool())all.insert(value.toMap()["id"].toString());
            selectedSystems_=selectedSystems_==all?QSet<QString>{}:all;
        }else if(i<=systems_.size()){
            const auto id=systems_[i-1].toMap()["id"].toString();
            if(selectedSystems_.contains(id))selectedSystems_.remove(id);else selectedSystems_.insert(id);
        }else if(i==systems_.size()+1)start();else close();
        emit changed();return;
    }
    if(choosing_) {
        if(i<pending_.games.size()){choosing_=false;focus_=0;emit jobStarted();apply(pending_.games[i]);return;}
        if(i==pending_.games.size()){textTarget_="search";emit textRequested("Find this game",queue_.value(index_).title,false);return;}
        if(i==pending_.games.size()+1){choosing_=false;emit jobStarted();complete({},true);return;}
        cancel_->store(true);finish();return;
    }
    if(active_){if(i==0){paused_=!paused_;if(!paused_&&!working_)next();}else {cancel_->store(true);if(!working_)finish();else phase_="Stopping";}}
    else if(finished_){if(i==0)start(true);else close();}
    else if(i==0){scope_=scope_==0?1:scope_==1?3:scope_==3?2:0;}else if(i==1)start();else close();
    emit changed();
}
void ScrapeController::start(bool retry) {
    if(busy())return;
    if(directory_.isEmpty()||(canStart&&!canStart())){status_="Wait for the current game or library operation to finish.";emit changed();return;}
    if(!preferences_.metadata&&preferences_.mediaTags().isEmpty()){status_="Choose what to download in Settings > ScreenScraper first.";emit changed();return;}
    queue_=retry?failures_:items();failures_.clear();results_.clear();done_=failed_=skipped_=index_=0;wrote_=paused_=choosing_=finished_=false;active_=true;focus_=0;
    taskStates_.clear();taskErrors_.clear();skipCurrent_=false;
    for(const auto& item:queue_)taskStates_[item.id]="queued";
    emit jobStarted();
    checkAccount(true);
}
void ScrapeController::next() {
    if(!active_||working_||choosing_)return;
    if(handleCancellation())return;
    while(index_<queue_.size()&&taskStates_.value(queue_[index_].id)=="cancelled"){++index_;++done_;++skipped_;}
    if(index_>=queue_.size()){finish();return;}if(paused_){emit changed();return;}
    if(taskStates_.value(queue_[index_].id)=="paused") {
        int next=index_+1;while(next<queue_.size()&&taskStates_.value(queue_[next].id)!="queued")++next;
        if(next==queue_.size()){phase_="Queue paused";emit changed();return;}
        queue_.move(next,index_);
    }
    const auto item=queue_[index_];
    if(!systemId(item.platform)){complete("This system has no ScreenScraper catalogue.",true);return;}
    phase_="Identifying game";status_.clear();
    const auto client=client_;const auto cancel=cancel_;const auto prefs=preferences_;const auto directory=directory_;
    run([item,client,cancel,prefs,directory]{
        Outcome o;o.file=fingerprint(item.path,cancel);
        if(!o.file.valid()){o.error="Could not read a stable game file.";return o;}
        auto options=prefs.json();options.remove("refresh");
        const auto key=QCryptographicHash::hash((item.path+o.file.md5+QString::number(o.file.size)).toUtf8()+QJsonDocument(options).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();
        o.cache=QDir(directory).filePath("cache/screenscraper/"+QString(key)+".json");
        const auto cached=readJson(o.cache);
        if(!prefs.refresh&&cached["version"].toInt()==1) {
            Game g;g.id=cached["id"].toString();g.system=cached["system"].toInt();
            const auto fields=cached["fields"].toObject();for(auto it=fields.begin();it!=fields.end();++it)g.fields[it.key()]=it.value().toString();
            const auto local=cached["local"].toObject();bool valid=g.system==systemId(item.platform)&&!g.id.isEmpty();
            for(auto it=local.begin();it!=local.end();++it){const auto path=QDir(item.root).filePath(it.value().toString());valid&=cachedMediaValid(item.root,path);o.local[it.key()]=path;}
            if(valid){o.cachedId=g.id;if(cached["complete"].toBool()){o.cached=true;o.games={g};return o;}}else o.local.clear();
        }
        // ZIP/7z/CHD use their actual container bytes, as accepted by jeuInfos.
        // CUE/M3U are descriptors, never evidence of the referenced disc identity.
        const auto extension=QFileInfo(item.path).suffix().toLower();
        Result r;
        if(extension!="cue"&&extension!="m3u"&&extension!="m3u8")r=client->lookup(item.platform,o.file,cancel);
        else r.status=Status::NotFound;
        if(r.status==Status::Busy)r=client->lookup(item.platform,o.file,cancel); // one bounded, paced retry
        if(r.status==Status::NotFound)r=client->search(item.platform,QFileInfo(item.path).completeBaseName().left(256),cancel);
        o.status=r.status;o.games=r.games;return o;
    },[this](Outcome o){found(std::move(o));});
}
void ScrapeController::found(Outcome o) {
    if(handleCancellation())return;
    if(!o.error.isEmpty()){complete(o.error);return;}
    if(o.status==Status::Quota||o.status==Status::Denied||o.status==Status::MissingCredentials||o.status==Status::MembersOnly||o.status==Status::ClientRejected){status_=statusMessage(o.status);for(int i=index_;i<queue_.size();++i){failures_<<queue_[i];taskStates_[queue_[i].id]="failed";taskErrors_[queue_[i].id]=status_;}finish();return;}
    if(o.status!=Status::Ready&&o.status!=Status::NotFound){complete(statusMessage(o.status));return;}
    pending_=std::move(o);
    if(pending_.games.size()==1&&(pending_.cached||pending_.games.first().exactFile)){apply(pending_.games.first());return;}
    choosing_=true;focus_=0;phase_="Exact identity not confirmed";status_=pending_.games.isEmpty()?statusMessage(Status::NotFound):"Check the edition before choosing. A similar title is not an exact ROM match.";emit changed();
}
void ScrapeController::apply(const Game& game) {
    phase_=pending_.cached?"Applying cached artwork":"Downloading selected artwork";
    const auto item=queue_[index_];const auto client=client_;const auto cancel=cancel_;const auto prefs=preferences_;auto pending=pending_;
    run([item,client,cancel,prefs,pending,game]() mutable {
        Outcome o=pending;o.games={game};auto g=game;auto local=o.cachedId==g.id?o.local:QMap<QString,QString>{};const auto existing=localFields(item.root,item.path);
        const auto cache=[&](bool complete){
            QJsonObject fields,media;
            for(auto it=g.fields.begin();it!=g.fields.end();++it)fields[it.key()]=it.value();
            for(auto it=local.begin();it!=local.end();++it)media[it.key()]=QDir(item.root).relativeFilePath(it.value());
            return saveJson(o.cache,{{"version",1},{"complete",complete},{"id",g.id},{"system",g.system},{"fields",fields},{"local",media}});
        };
        for(const auto& tag:prefs.mediaTags()) {
            if(cancel->load())return o;
            if(local.contains(tag))continue;
            if(!prefs.refresh&&!existing.value(tag).isEmpty())continue; // preserve owner media, including intentionally unavailable mounts
            if(!g.media.contains(tag))continue;
            auto reply=client->media(g.media[tag],false,cancel);
            if(reply.status==423||reply.status==429)reply=client->media(g.media[tag],false,cancel);
            if(reply.status!=200){o.error=reply.status==430||reply.status==431?statusMessage(Status::Quota):"Artwork download failed. Retry this game later.";if(reply.status==430||reply.status==431)o.status=Status::Quota;return o;}
            const auto path=storeMedia(item.root,g.id,reply.bytes,false,cancel);
            if(path.isEmpty()){o.error="Could not validate or store the downloaded image.";return o;}local[tag]=path;cache(false);
        }
        if(!prefs.metadata)g.fields.clear();else g.fields["name"]=item.title;
        if(cancel->load())return o;
        o.error=writeGamelist(item.root,o.file,g,local,prefs.refresh,cancel);
        if(!o.error.isEmpty())return o;
        o.success=true;
        // No service URLs, credentials or ROM content in the cache.
        cache(true);
        return o;
    },[this](Outcome o){
        wrote_|=o.success;
        if(handleCancellation(o.success))return;
        if(o.status==Status::Quota){status_=o.error;for(int i=index_;i<queue_.size();++i){failures_<<queue_[i];taskStates_[queue_[i].id]="failed";taskErrors_[queue_[i].id]=status_;}finish();return;}
        complete(o.error);
    });
}
void ScrapeController::complete(const QString& error,bool skipped) {
    const auto id=queue_.value(index_).id;
    if(taskStates_.value(id)!="cancelled")taskStates_[id]=skipped?"skipped":error.isEmpty()?"done":"failed";
    taskErrors_[id]=error;
    if(skipped)++skipped_;else if(!error.isEmpty()){++failed_;failures_<<queue_.value(index_);}
    if(!error.isEmpty())results_<<queue_.value(index_).title+": "+error;
    ++done_;++index_;pending_={};choosing_=false;focus_=0;
    emit changed();QTimer::singleShot(0,this,[this]{if(active_)next();});
}
void ScrapeController::finish() {
    if(cancel_&&cancel_->load())for(int i=index_;i<queue_.size();++i)if(taskStates_.value(queue_[i].id)!="done")taskStates_[queue_[i].id]="cancelled";
    active_=false;choosing_=false;finished_=true;focus_=failures_.isEmpty()?1:0;
    if(cancel_&&cancel_->load())status_=statusMessage(Status::Cancelled);
    else if(index_>=queue_.size())status_="Scraping finished. Completed changes are available in your library.";
    if(wrote_){wrote_=false;emit saved();}emit changed();
}
bool ScrapeController::handleCancellation(bool committed) {
    if(!cancel_||!cancel_->load())return false;
    if(committed)taskStates_[queue_.value(index_).id]="done";
    if(skipCurrent_) {
        skipCurrent_=false;cancel_=std::make_shared<std::atomic_bool>(false);
        if(!committed)taskStates_[queue_.value(index_).id]="cancelled";
        complete({},!committed);
    } else finish();
    return true;
}
QVariantList ScrapeController::downloadTasks() const {
    QVariantList rows;
    const int firstMovable=index_+((working_||choosing_)?1:0);
    for(int i=0;i<queue_.size();++i) {
        const auto& item=queue_[i];auto state=taskStates_.value(item.id,"queued");
        const bool current=active_&&i==index_,pending=active_&&i>=index_&&(state=="queued"||state=="paused");
        QString detail=state=="done"?"Completed":state=="failed"?taskErrors_.value(item.id,"Failed"):state=="cancelled"?"Cancelled":state=="skipped"?"Skipped":state=="paused"?"Paused":"Waiting";
        if(current&&choosing_){state="attention";detail="Choose the matching edition";}
        else if(current&&working_){state="running";detail=skipCurrent_?"Cancelling":paused_?phase_+" · pausing after this game":phase_;}
        else if(pending&&paused_){state="paused";detail="Queue paused";}
        QVariantList actions;
        const auto add=[&](const QString& id,const QString& label){actions<<QVariantMap{{"id",id},{"label",label}};};
        if(state=="attention")add("open","Choose match");
        if(pending||state=="attention") {
            if(current&&working_)add(paused_?"resume-all":"pause-all",paused_?"Resume queue":"Pause after game");
            else add(state=="paused"||paused_?"resume":"pause",state=="paused"||paused_?"Resume":"Pause");
            if(i>firstMovable)add("earlier","Move up");
            if(i>=firstMovable&&i+1<queue_.size())add("later","Move down");
            if(phase_!="Checking connection"||!current)add("cancel","Cancel");
        }
        if(!active_&&state=="failed")add("retry","Retry failed games");
        const auto art=library_.artwork(item.id);QString picture;
        for(const auto& tag:QStringList{"cover","image","screenshot","thumbnail","marquee"})if(!art.value(tag).toString().isEmpty()){picture=art.value(tag).toString();break;}
        if(!picture.isEmpty()&&QUrl(picture).scheme().isEmpty())picture=QUrl::fromLocalFile(picture).toString();
        const auto platform=platformLabel(item.platform);
        rows<<QVariantMap{{"id",item.id},{"title",item.title},{"source","ScreenScraper"},{"platform",platform.badge},{"shape",platform.shape},{"picture",picture},{"state",state},{"detail",detail},{"terminal",!pending&&state!="running"&&state!="attention"},{"actions",actions}};
    }
    return rows;
}
void ScrapeController::downloadCommand(const QString& id,const QString& command) {
    if(command=="open"&&choosing_){open_=true;emit changed();return;}
    if(command=="retry"&&!busy()) {
        if(!failures_.isEmpty())start(true);
        return;
    }
    if(!active_)return;
    if(command=="pause-all"){paused_=true;emit changed();return;}
    if(command=="resume-all") {
        paused_=false;for(auto it=taskStates_.begin();it!=taskStates_.end();++it)if(it.value()=="paused")it.value()="queued";
        if(!working_&&!choosing_)next();
        emit changed();return;
    }
    if(command=="cancel-all"){skipCurrent_=false;cancel_->store(true);if(!working_)finish();else {phase_="Stopping";emit changed();}return;}
    int position=index_;while(position<queue_.size()&&queue_[position].id!=id)++position;
    if(position>=queue_.size()||taskStates_.value(id)=="cancelled")return;
    const bool current=position==index_;
    if(command=="cancel") {
        if(current&&working_){if(phase_=="Checking connection")return;skipCurrent_=true;cancel_->store(true);}
        else {taskStates_[id]="cancelled";if(current){choosing_=false;next();}}
    } else if(command=="pause") {
        if(current&&(working_||choosing_))paused_=true;else {taskStates_[id]="paused";if(current)next();}
    } else if(command=="resume") {
        taskStates_[id]="queued";paused_=false;if(!working_&&!choosing_)next();
    } else if(command=="earlier"||command=="later") {
        const int first=index_+((working_||choosing_)?1:0),target=position+(command=="earlier"?-1:1);
        if(position>=first&&target>=first&&target<queue_.size())queue_.move(position,target);
    }
    emit changed();
}
}
