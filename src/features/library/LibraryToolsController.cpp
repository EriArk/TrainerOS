#include "LibraryToolsController.h"
#include "core/repository/CollectionRepository.h"
#include <QFileInfo>
#include <QDir>
#include <algorithm>

namespace trainer {
void LibraryToolsController::beginGame(const QString& id) {
    if(busy_ || !repository_.editable())return;
    const auto record=repository_.registration(id);
    if(!record || record->removed)return;
    game_=*record;route_="game";focus_=0;error_.clear();emit changed();
}
void LibraryToolsController::beginWorld(const QString& id,bool enabled) {
    if(busy_ || !enabled || !repository_.editable())return;
    for(const auto& world:repository_.worlds())if(world.id==id) {
        world_=world;route_="world";focus_=0;error_.clear();emit changed();return;
    }
}
void LibraryToolsController::beginTrash() {
    if(busy_)return;
    trash_.clear();for(const auto& r:repository_.registrations())if(r.removed && !r.trashPath.isEmpty())trash_.append(r);
    route_="trash";focus_=0;error_.clear();emit changed();
}
void LibraryToolsController::close() {if(!busy_){emit reviewRequested("reviews-close",{});++browseGeneration_;route_.clear();error_.clear();emit changed();}}
QString LibraryToolsController::title() const {
    if(route_.startsWith("reviews"))return "Reviews · "+game_.adventure.title;
    if(route_=="trash")return "Game trash";
    if(route_=="world")return world_.name;
    if(route_=="move")return "Move to a World";
    if(route_=="move-kind")return "Move game";
    if(route_=="folder")return directory_.path==root_?"Choose a platform":"Choose a folder";
    if(route_=="move-file")return "Move here?";
    if(route_=="remove")return "Удалить игру?";
    if(route_=="restore")return "Restore this game?";
    return game_.adventure.title;
}
QString LibraryToolsController::detail() const {
    if(route_=="reviews-edit")return reviewDraft_.isEmpty()?"A few words about your adventure…":reviewDraft_;
    if(route_=="reviews-delete")return "Delete your review?";
    if(route_=="reviews-report")return "Report this review to Fluxer";
    if(route_=="reviews")return reviews_.value("status").toString().isEmpty()?(completion_.completed?QString("Shared impressions"):completion_.message):reviews_["status"].toString();
    if(route_=="folder")return QDir(root_).relativeFilePath(directory_.path)=="."?QFileInfo(root_).fileName():QDir(root_).relativeFilePath(directory_.path);
    if(route_=="move-file")return game_.adventure.title+"\n"+QFileInfo(root_).fileName()+" / "+QDir(root_).relativeFilePath(destination_);
    if(route_=="remove")return game_.adventure.title+"\nROM будет удалён навсегда. Сохранения и история останутся.";
    if(route_=="restore")return game_.adventure.title;
    if(route_=="trash" && trash_.isEmpty())return "The trash is empty.";
    if(route_=="properties") {
        QStringList information;
        information.append("Platform: "+platformLabel(game_.adventure.platformId).name);
        information.append("File: "+QFileInfo(game_.contentPath).fileName());
        information.append("Folder: "+QFileInfo(game_.contentPath).absolutePath());
        return information.join('\n');
    }
    return {};
}
QVariantList LibraryToolsController::rows() const {
    const auto row=[](const QString& label,bool enabled=true){return QVariantMap{{"label",label},{"enabled",enabled}};};
    if(route_=="reviews-edit")return {row("Edit text"),row(reviewSpoiler_?"Spoilers: Yes":"Spoilers: No"),row("Publish review",!reviewDraft_.trimmed().isEmpty()&&!reviews_["busy"].toBool()),row("Discard")};
    if(route_=="reviews-delete")return {row("Cancel"),row("Delete review")};
    if(route_=="reviews-report")return {row("Spam"),row("Harassment"),row("Other"),row("Cancel")};
    if(route_=="reviews") {
        QVariantList result;
        for(const auto& v:reviews_["rows"].toList()) {
            auto r=v.toMap();result.append(row(r["name"].toString()+(r["spoiler"].toBool()?" · Spoilers":"")));
        }
        result.append(row(reviews_["mine"].toMap().isEmpty()?"Write a review":"Edit my review",completion_.completed&&reviews_["fresh"].toBool()&&!reviews_["busy"].toBool()));
        if(!reviews_["mine"].toMap().isEmpty())result.append(row("Delete my review",reviews_["fresh"].toBool()&&!reviews_["busy"].toBool()));
        result.append(row("Refresh",!reviews_["busy"].toBool()));
        if(reviews_["more"].toBool())result.append(row("More reviews",!reviews_["busy"].toBool()));
        return result;
    }
    if(route_=="game")return {row("Rename"),row("Move",catalog_ && !repository_.storageRootFor(game_.adventure.id).isEmpty()),row("Удалить"),row("Properties"),row("Download information & artwork",!repository_.storageRootFor(game_.adventure.id).isEmpty()),row("Collections"),row("Play together",multiplayerAvailable && multiplayerAvailable(game_.adventure.id))};
    if(route_=="move-kind")return {row("Platform / folder",catalog_ && !repository_.storageRootFor(game_.adventure.id).isEmpty()),row("Another World")};
    if(route_=="move-file")return {row("Cancel"),row("Move")};
    if(route_=="world")return {row("Rename World"),row("Done"),row("Scrape this collection")};
    if(route_=="remove")return {row("Отмена"),row("Удалить")};
    if(route_=="restore")return {row("Cancel"),row("Restore game")};
    if(route_=="properties"){
        QVariantList result{row("Reviews")};for(const auto& capability:capabilities_)result.append(row(capability,false));
        result.append(row("Back"));return result;
    }
    QVariantList result;
    if(route_=="folder") {
        result={row("Move here",directory_.path!=root_ && directory_.path!=QFileInfo(game_.contentPath).canonicalPath()),row("New folder",directory_.path!=root_),row("Parent folder",directory_.path!=root_)};
        for(const auto& entry:directory_.entries) {
            auto platform=entry.name;if(platform=="gamecube")platform="gc";if(platform=="3ds")platform="n3ds";
            result.append(row(directory_.path==root_?platformLabel(platform).name:entry.name));
        }
        if(directory_.page>0)result.append(row("Previous folders"));
        if(directory_.page+1<directory_.pages)result.append(row("More folders"));
    }
    if(route_=="move")for(const auto& w:destinations_)result.append(row(w.name));
    if(route_=="trash")for(const auto& r:trash_)result.append(row(r.adventure.title));
    return result;
}
void LibraryToolsController::dispatch(Action action) {
    if(busy_)return;
    if(action==Action::LocalAction && route_=="game"){close();return;}
    if(route_=="reviews"&&(action==Action::Left||action==Action::Right)){emit scrollReview(action==Action::Right?1:-1);return;}
    if(route_=="reviews"&&action==Action::LocalAction&&reviewReportAvailable()){
        reportId_=selectedReview()["id"].toString();route_="reviews-report";focus_=0;emit changed();return;
    }
    if(action==Action::Back) {
        if(route_.startsWith("reviews")) {
            if(reviews_["busy"].toBool()&&route_!="reviews")return;
            if(route_=="reviews"){emit reviewRequested("reviews-close",{});++browseGeneration_;route_="properties";}else route_="reviews";
            focus_=0;error_.clear();emit changed();return;
        }
        if(route_=="move-file"){browse(directory_.path,directory_.page);return;}
        if(route_=="folder" && directory_.path!=root_){browse(QFileInfo(directory_.path).absolutePath());return;}
        if(route_=="folder" || route_=="move-kind" || route_=="move" || route_=="properties" || route_=="remove") {route_="game";focus_=0;error_.clear();emit changed();}
        else if(route_=="restore")beginTrash();else close();
        return;
    }
    if(action==Action::Up)focus_=std::max(0,focus_-1);
    if(action==Action::Down)focus_=std::min(std::max(0,int(rows().size())-1),focus_+1);
    if(action==Action::Confirm){activate(focus_);return;}emit changed();
}
void LibraryToolsController::activate(int index) {
    const auto values=rows();if(busy_ || index<0 || index>=values.size())return;
    focus_=index;if(!values[index].toMap().value("enabled").toBool())return;
    if(route_=="game"&&index==6){const auto game=game_.adventure.id;close();emit togetherRequested(game);return;}
    if(route_=="game"&&index==5){emit collectionsRequested(game_.adventure.id);return;}
    if(route_=="game"&&index==4){const auto id=game_.adventure.id;close();emit scrapeRequested(id,{});return;}
    if(route_=="world"&&index==2){const auto id=world_.id;close();emit scrapeRequested({},id);return;}
    if(route_.startsWith("reviews")){reviewAction(index);return;}
    if(route_=="properties"&&index==0){openReviews();return;}
    if(route_=="game") {
        if(index==0)emit textRequested("Game name",game_.adventure.title,96);
        else if(index==1){
            root_=repository_.storageRootFor(game_.adventure.id);browse(root_);return;
        }
        else {
            route_=index==2?"remove":"properties";focus_=0;
            if(index==3 && capabilityQuery){
                capabilities_={"Checking this Adventure…"};const auto generation=++browseGeneration_;
                capabilityQuery(game_,this,[this,generation](QStringList rows){
                    if(generation!=browseGeneration_ || route_!="properties")return;
                    capabilities_=std::move(rows);focus_=0;emit changed();
                });
            }
        }
    } else if(route_=="world") {
        if(index==0)emit textRequested("World name",world_.name,32);else close();
    } else if(route_=="move-kind") {
        if(index==1){destinations_=repository_.worlds();route_="move";focus_=0;}
        else {root_=repository_.storageRootFor(game_.adventure.id);browse(root_);return;}
    } else if(route_=="folder") {
        if(index==0){destination_=directory_.path;route_="move-file";focus_=0;}
        else if(index==1)emit textRequested("Folder name",{},40);
        else if(index==2){browse(QFileInfo(directory_.path).absolutePath());return;}
        else if(index-3<directory_.entries.size()){browse(directory_.entries[index-3].path);return;}
        else {
            const auto previous=directory_.page>0 && index==3+directory_.entries.size();
            browse(directory_.path,directory_.page+(previous?-1:1));return;
        }
    } else if(route_=="move-file") {
        if(index==0){dispatch(Action::Back);return;}
        LibraryEdit edit{LibraryEditKind::MoveFile,game_.adventure.id,game_.revision};edit.text=destination_;submit(edit);
    } else if(route_=="move") {
        LibraryEdit edit{LibraryEditKind::MoveGame,game_.adventure.id,game_.revision};edit.world=destinations_[index];submit(edit);
    } else if(route_=="remove" || route_=="restore") {
        if(index==0){dispatch(Action::Back);return;}
        submit({route_=="remove"?LibraryEditKind::RemoveGame:LibraryEditKind::RestoreGame,game_.adventure.id,game_.revision});
    } else if(route_=="trash") {game_=trash_[index];route_="restore";focus_=0;}
    else if(route_=="properties" && index==values.size()-1)dispatch(Action::Back);
    emit changed();
}
void LibraryToolsController::applyText(const QString& text) {
    if(busy_)return;
    if(route_=="reviews-edit"){reviewDraft_=text.left(800);emit changed();return;}
    if(route_=="folder") {
        const auto name=text.trimmed();
        if(name.isEmpty() || name.startsWith('.') || name.endsWith('.') || name.contains('/') || name.contains('\\')
            || name.contains(':') || name.size()>40 || std::any_of(name.begin(),name.end(),[](QChar c){return c.category()==QChar::Other_Control;})) {
            error_="Choose a short folder name without slashes or dots at its ends.";emit changed();return;
        }
        destination_=QDir(directory_.path).filePath(name);route_="move-file";focus_=0;error_.clear();emit changed();return;
    }
    if(route_=="world") {
        LibraryEdit edit{LibraryEditKind::RenameWorld,world_.id};edit.text=text;edit.previousName=world_.name;submit(edit);
    } else if(route_=="game") {
        LibraryEdit edit{LibraryEditKind::RenameGame,game_.adventure.id,game_.revision};edit.text=text;submit(edit);
    }
}
void LibraryToolsController::browse(const QString& path,int page) {
    if(!catalog_ || root_.isEmpty())return;
    const auto resolved=QFileInfo(path).canonicalFilePath();
    if(resolved!=root_ && !resolved.startsWith(root_+'/'))return;
    const auto generation=++browseGeneration_;busy_=true;error_.clear();route_="folder";emit changed();
    catalog_->list(resolved,page,this,[this,generation](DirectoryPage result){
        if(generation!=browseGeneration_)return;
        busy_=false;error_=result.error;
        result.entries.removeIf([this](const FileEntry& entry){
            const QFileInfo file(entry.path);const auto canonical=file.canonicalFilePath();
            return !entry.directory || file.isSymLink() || !canonical.startsWith(root_+'/');
        });
        if(result.path==root_)result.entries.removeIf([](const FileEntry& entry){
            auto platform=entry.name;if(platform=="gamecube")platform="gc";if(platform=="3ds")platform="n3ds";
            return platformLabel(platform).badge=="?";
        });
        directory_=std::move(result);
        focus_=directory_.path==root_ && !directory_.entries.isEmpty()?3:
            directory_.path==QFileInfo(game_.contentPath).canonicalPath()?1:0;
        emit changed();
    });
}
void LibraryToolsController::submit(LibraryEdit edit) {
    if(editGuard){error_=editGuard();if(!error_.isEmpty()){emit changed();return;}}
    busy_=true;error_.clear();emit changed();
    repository_.editLibraryAsync(edit,this,[this,edit](const QString& error){
        busy_=false;error_=error;
        if(error.isEmpty()) {
            emit saved();
            if(edit.kind==LibraryEditKind::RestoreGame)beginTrash();else close();
        }
        emit changed();
    });
}
}

namespace trainer {
void LibraryToolsController::openReviews() {
    route_="reviews";focus_=0;completion_={};reviews_={{"status","Reading this Adventure…"},{"busy",true}};emit changed();
    if(!completionQuery){reviews_={{"status","Reviews are unavailable for this Adventure."}};emit changed();return;}
    const auto generation=++browseGeneration_;
    completionQuery(game_,this,[this,generation](AdventureCompletion result){
        if(generation!=browseGeneration_||route_!="reviews")return;
        completion_=result;reviews_["busy"]=false;
        if(result.identity.isEmpty()){reviews_["status"]=result.message;emit changed();return;}
        emit reviewRequested("reviews-open",{{"identity",result.identity}});emit changed();
    });
}
void LibraryToolsController::receiveReviews(const QString& identity,const QVariantMap& state) {
    if(identity!=completion_.identity||!route_.startsWith("reviews"))return;
    reviews_=state;
    if(!state["busy"].toBool()&&(state["status"]=="Review published"||state["status"]=="Review deleted"||state["status"]=="Report sent")){route_="reviews";focus_=0;}
    focus_=qBound(0,focus_,qMax(0,int(rows().size())-1));emit changed();
}
QVariantMap LibraryToolsController::selectedReview() const {
    if(route_!="reviews")return {};
    auto row=reviews_["rows"].toList().value(focus_).toMap();
    if(row.isEmpty())return {};
    if(row["spoiler"].toBool()&&row["id"]!=reviewReveal_)row["text"]="Contains spoilers. Select to reveal.";
    return row;
}
void LibraryToolsController::reviewAction(int index) {
    if(reviews_["busy"].toBool())return;
    QVariantMap args{{"identity",completion_.identity}};
    if(route_=="reviews-edit") {
        if(index==0){emit textRequested("Your review",reviewDraft_,800);return;}
        if(index==1)reviewSpoiler_=!reviewSpoiler_;
        if(index==3){route_="reviews";focus_=0;}
        if(index==2&&completionQuery) {
            busy_=true;emit changed();const auto expected=completion_.identity;
            completionQuery(game_,this,[this,expected](AdventureCompletion result){
                busy_=false;
                if(result.identity!=expected||!result.completed){completion_=result;error_="Completion could not be confirmed. Reopen Reviews.";emit changed();return;}
                emit reviewRequested("reviews-save",{{"identity",expected},{"text",reviewDraft_},{"spoiler",reviewSpoiler_},{"completed",true},{"policy",result.policy}});
            });
        }
    } else if(route_=="reviews-delete") {
        if(index==1)emit reviewRequested("reviews-delete",args);else {route_="reviews";focus_=0;}
    } else if(route_=="reviews-report") {
        if(index<3){args["id"]=reportId_;args["category"]=QStringList{"spam","harassment","other"}[index];emit reviewRequested("reviews-report",args);}
        else {route_="reviews";focus_=0;}
    } else {
        const auto entries=reviews_["rows"].toList();
        if(index<entries.size()) {
            const auto row=entries[index].toMap();
            if(row["spoiler"].toBool()&&reviewReveal_!=row["id"].toString())reviewReveal_=row["id"].toString();
        } else {
            const auto label=rows()[index].toMap()["label"].toString();
            if(label=="Write a review"||label=="Edit my review") {
                reviewDraft_=reviews_["mine"].toMap()["text"].toString();reviewSpoiler_=reviews_["mine"].toMap()["spoiler"].toBool();
                route_="reviews-edit";focus_=0;emit textRequested("Your review",reviewDraft_,800);
            } else if(label=="Delete my review"){route_="reviews-delete";focus_=0;}
            else emit reviewRequested(label=="More reviews"?"reviews-more":"reviews-refresh",args);
        }
    }
    emit changed();
}
}
