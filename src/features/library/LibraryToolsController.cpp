#include "LibraryToolsController.h"
#include "core/repository/CollectionRepository.h"
#include <QFileInfo>
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
void LibraryToolsController::close() {if(!busy_){route_.clear();error_.clear();emit changed();}}
QString LibraryToolsController::title() const {
    if(route_=="trash")return "Game trash";
    if(route_=="world")return world_.name;
    if(route_=="move")return "Move to a World";
    if(route_=="remove")return "Удалить игру?";
    if(route_=="restore")return "Restore this game?";
    return game_.adventure.title;
}
QString LibraryToolsController::detail() const {
    if(route_=="remove")return game_.adventure.title+"\nROM будет удалён навсегда. Сохранения и история останутся.";
    if(route_=="restore")return game_.adventure.title;
    if(route_=="trash" && trash_.isEmpty())return "The trash is empty.";
    if(route_=="properties") {
        QString world;
        for(const auto& w:repository_.worlds())if(w.id==game_.adventure.worldId)world=w.name;
        QStringList information;
        if(!world.isEmpty())information.append("World: "+world);
        information.append("Platform: "+platformLabel(game_.adventure.platformId).name);
        information.append("File: "+QFileInfo(game_.contentPath).fileName());
        information.append("Folder: "+QFileInfo(game_.contentPath).absolutePath());
        return information.join('\n');
    }
    return {};
}
QVariantList LibraryToolsController::rows() const {
    const auto row=[](const QString& label,bool enabled=true){return QVariantMap{{"label",label},{"enabled",enabled}};};
    if(route_=="game")return {row("Rename"),row("Move",game_.adventure.domain=="pokemon"),row("Удалить"),row("Properties")};
    if(route_=="world")return {row("Rename World"),row("Done")};
    if(route_=="remove")return {row("Отмена"),row("Удалить")};
    if(route_=="restore")return {row("Cancel"),row("Restore game")};
    if(route_=="properties")return {row("Back")};
    QVariantList result;
    if(route_=="move")for(const auto& w:destinations_)result.append(row(w.name));
    if(route_=="trash")for(const auto& r:trash_)result.append(row(r.adventure.title));
    return result;
}
void LibraryToolsController::dispatch(Action action) {
    if(busy_)return;
    if(action==Action::Back) {
        if(route_=="move" || route_=="properties" || route_=="remove") {route_="game";focus_=0;error_.clear();emit changed();}
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
    if(route_=="game") {
        if(index==0)emit textRequested("Game name",game_.adventure.title,96);
        else if(index==1){destinations_=repository_.worlds();route_="move";focus_=0;}
        else {route_=index==2?"remove":"properties";focus_=0;}
    } else if(route_=="world") {
        if(index==0)emit textRequested("World name",world_.name,32);else close();
    } else if(route_=="move") {
        LibraryEdit edit{LibraryEditKind::MoveGame,game_.adventure.id,game_.revision};edit.world=destinations_[index];submit(edit);
    } else if(route_=="remove" || route_=="restore") {
        if(index==0){dispatch(Action::Back);return;}
        submit({route_=="remove"?LibraryEditKind::RemoveGame:LibraryEditKind::RestoreGame,game_.adventure.id,game_.revision});
    } else if(route_=="trash") {game_=trash_[index];route_="restore";focus_=0;}
    else if(route_=="properties")dispatch(Action::Back);
    emit changed();
}
void LibraryToolsController::applyText(const QString& text) {
    if(busy_)return;
    if(route_=="world") {
        LibraryEdit edit{LibraryEditKind::RenameWorld,world_.id};edit.text=text;edit.previousName=world_.name;submit(edit);
    } else if(route_=="game") {
        LibraryEdit edit{LibraryEditKind::RenameGame,game_.adventure.id,game_.revision};edit.text=text;submit(edit);
    }
}
void LibraryToolsController::submit(LibraryEdit edit) {
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
