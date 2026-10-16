#include "ScreensController.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

namespace trainer {
void ScreensController::configure(const QString& directory,const QString& owner) {
    const auto path=directory.isEmpty() || owner.isEmpty()?QString():QDir(directory).filePath("screens/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex())+".json");
    if(owner_==owner && path_==path)return;
    directory_=directory;owner_=owner;path_=path;favorites_.clear();source_.clear();items_.clear();filter_.clear();selected_.clear();error_.clear();index_=0;filtering_=false;
    QFile file(path_);
    if(file.exists()) {
        if(file.size()>1024*1024 || !file.open(QIODevice::ReadOnly))error_="Could not read album favorites.";
        else {
            QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
            if(error.error!=QJsonParseError::NoError || !doc.isArray())error_="Could not read album favorites.";
            else for(const auto& id:doc.array())if(id.isString())favorites_.insert(id.toString());
        }
    }
    emit itemsChanged();emit changed();
}
void ScreensController::setMedia(const QVariantList& media) {
    if(source_==media)return;
    source_=media;rebuild();
}
void ScreensController::rebuild() {
    items_.clear();
    for(const auto& item:source_) {
        auto row=item.toMap();const bool favorite=favorites_.contains(row["id"].toString());
        if(filter_=="favorites" && !favorite)continue;
        if(filter_.startsWith("game:") && row["adventure"].toString()!=filter_.mid(5))continue;
        row["favorite"]=favorite;items_.append(row);
    }
    index_=std::clamp(index_,0,qMax(0,count()-1));
    for(int i=0;i<count();++i)if(items_[i].toMap()["id"].toString()==selected_){index_=i;break;}
    selected_=current()["id"].toString();
    emit itemsChanged();emit changed();
}
QVariantMap ScreensController::current() const {return items_.value(index_).toMap();}
QVariantList ScreensController::filters() const {
    QVariantList result{QVariantMap{{"id",""},{"label","All games"}},QVariantMap{{"id","favorites"},{"label","Starred moments"}}};
    QSet<QString> seen;
    for(const auto& item:source_) {
        const auto row=item.toMap();const auto id=row["adventure"].toString();if(seen.contains(id))continue;seen.insert(id);
        result.append(QVariantMap{{"id","game:"+id},{"label",row["game"]}});
    }
    return result;
}
QString ScreensController::filterLabel() const {
    for(const auto& row:filters())if(row.toMap()["id"].toString()==filter_)return row.toMap()["label"].toString();
    return filter_.startsWith("game:")?QString("This game"):QString("All games");
}
void ScreensController::select(int index) {if(index<0 || index>=count())return;index_=index;selected_=current()["id"].toString();emit changed();}
void ScreensController::close() {filtering_=false;emit changed();}
void ScreensController::forGame(const QString& id) {filter_="game:"+id;selected_.clear();index_=0;filtering_=false;rebuild();}
void ScreensController::showFilters() {
    filtering_=!filtering_;filterFocus_=0;
    const auto list=filters();for(int i=0;i<list.size();++i)if(list[i].toMap()["id"].toString()==filter_)filterFocus_=i;
    emit changed();
}
void ScreensController::chooseFilter(int index) {
    const auto list=filters();if(index<0 || index>=list.size())return;
    filter_=list[index].toMap()["id"].toString();filtering_=false;index_=0;rebuild();
}
void ScreensController::toggleFavorite() {
    const auto id=current()["id"].toString();if(id.isEmpty())return;
    auto next=favorites_;if(next.contains(id))next.remove(id);else next.insert(id);
    if(path_.isEmpty()){error_="Choose a Trainer to save favorite moments.";emit changed();return;}
    QDir().mkpath(QFileInfo(path_).absolutePath());QSaveFile file(path_);
    QStringList ordered(next.begin(),next.end());ordered.sort();const auto bytes=QJsonDocument(QJsonArray::fromStringList(ordered)).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()) {error_="Could not save this star. Your favorites are unchanged.";emit changed();return;}
    favorites_=next;error_.clear();rebuild();
}
void ScreensController::dispatch(Action action) {
    if(filtering_) {
        if(action==Action::Back || action==Action::LocalAction)filtering_=false;
        else if(action==Action::Up || action==Action::Down)filterFocus_=std::clamp(filterFocus_+(action==Action::Down?1:-1),0,int(filters().size())-1);
        else if(action==Action::Confirm){chooseFilter(filterFocus_);return;}
        emit changed();return;
    }
    if(action==Action::LocalAction){showFilters();return;}
    if(action==Action::ToggleFavorite){toggleFavorite();return;}
    if(action==Action::Back){close();return;}
    int delta=action==Action::Left?-1:action==Action::Right?1:0;
    if(delta)select(std::clamp(index_+delta,0,qMax(0,count()-1)));
}
}
