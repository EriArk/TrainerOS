#include "FilesController.h"
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <algorithm>
namespace trainer {
namespace {
QVariantMap item(QString id,QString label,QString icon={}) {return {{"id",id},{"label",label},{"icon",icon}};}
QString normalize(QString value){QUrl url(value);if(url.scheme().isEmpty())url=QUrl::fromLocalFile(QDir::cleanPath(value));url.setPassword({});url.setFragment({});return url.adjusted(QUrl::NormalizePathSegments).toString();}
QString child(QString parent,QString name){QUrl url(parent);auto path=url.path();if(!path.endsWith('/'))path+='/';url.setPath(path+name);return url.toString();}
}
FilesController::FilesController(QObject* parent):QObject(parent) {
    connect(&service_,&FilesService::listed,this,[this](QString url,QVariantList entries,QString error){
        if(url!=url_)return;
        busy_=false;entries_=entries;if(!error.isEmpty())error_=error;rebuild();
    });
    connect(&service_,&FilesService::changed,this,[this]{
        bool refresh=false;for(const auto& value:service_.tasks()) {const auto task=value.toMap();const auto id=task["id"].toString();
            if(task["terminal"].toBool()&&!finished_.contains(id)){finished_.insert(id);refresh=true;if(task["state"]=="error")error_=task["subtitle"].toString();}
            if(!task["question"].toString().isEmpty()&&dialog_.isEmpty()) {
                dialog_={{"title","File already exists"},{"text",task["question"]},{"task",id},{"actions",QVariantList{item("skip","Skip"),item("keep","Keep both"),item("replace","Replace"),item("cancel","Cancel operation")}}};dialogIndex_=0;
            }
        }
        if(refresh&&!url_.isEmpty())service_.list(url_);
        Q_EMIT changed();
    });
}
void FilesController::configure(const QString& directory,const QString& owner){
    if(owner_==owner)return;
    persist();owner_=owner;preferences_.clear();url_.clear();clipboard_.clear();selected_.clear();history_.clear();historyIndex_=-1;
    dialog_.clear();menuOpen_=false;entries_.clear();rows_.clear();textAction_.clear();service_.reset();
    if(!directory.isEmpty()&&!owner.isEmpty()){
        preferences_=QDir(directory).filePath("files-"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex())+".json");
        QFile file(preferences_);if(file.open(QIODevice::ReadOnly)){const auto state=QJsonDocument::fromJson(file.readAll()).object();url_=state["url"].toString();grid_=state["grid"].toBool(true);hidden_=state["hidden"].toBool(false);}
    }
    Q_EMIT rowsChanged();Q_EMIT changed();
}
void FilesController::persist(){if(preferences_.isEmpty())return;QSaveFile file(preferences_);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"version",1},{"url",url_},{"grid",grid_},{"hidden",hidden_}}).toJson());file.commit();}}
void FilesController::enter(){places_=service_.places();if(url_.isEmpty())navigate(QUrl::fromLocalFile(QDir::homePath()).toString());else if(entries_.isEmpty())service_.list(url_);Q_EMIT changed();}
void FilesController::leave(){menuOpen_=false;Q_EMIT changed();}
void FilesController::load(const QString& url){url_=url;busy_=true;error_.clear();query_.clear();selected_.clear();index_=0;entries_.clear();rows_.clear();area_="files";menuOpen_=false;service_.list(url);persist();Q_EMIT rowsChanged();Q_EMIT changed();}
void FilesController::navigate(QString value){if(modal())return;const auto url=normalize(value);const QUrl parsed(url);if(!QStringList{"file","trash","smb","sftp","ftp","dav","davs","network"}.contains(parsed.scheme())){error_="This location is not supported";Q_EMIT changed();return;}
    if(url!=url_){history_=history_.mid(0,historyIndex_+1);history_<<url;historyIndex_=history_.size()-1;}load(url);
}
void FilesController::rebuild(){const auto chosen=rows_.value(index_).toMap()["url"].toString();rows_.clear();
    for(const auto& value:entries_){const auto row=value.toMap();if((hidden_||!row["hidden"].toBool())&&(query_.isEmpty()||row["name"].toString().contains(query_,Qt::CaseInsensitive)))rows_<<row;}
    std::stable_sort(rows_.begin(),rows_.end(),[](const auto& av,const auto& bv){const auto a=av.toMap(),b=bv.toMap();if(a["directory"]!=b["directory"])return a["directory"].toBool();return QString::localeAwareCompare(a["name"].toString(),b["name"].toString())<0;});
    index_=std::clamp(index_,0,std::max(0,int(rows_.size())-1));for(int i=0;i<rows_.size();++i)if(rows_[i].toMap()["url"]==chosen)index_=i;Q_EMIT rowsChanged();Q_EMIT changed();
}
QVariantList FilesController::rows() const{auto rows=rows_;for(auto& v:rows){auto row=v.toMap();row["selected"]=selected_.contains(row["url"].toString());row["sizeLabel"]=row["directory"].toBool()?QString():QLocale().formattedDataSize(row["size"].toLongLong());v=row;}return rows;}
QVariantMap FilesController::view() const{return {{"url",url_},{"busy",busy_},{"error",error_},{"query",query_},{"grid",grid_},{"controls",controls_},{"area",area_},{"index",index_},{"placeIndex",placeIndex_},{"controlIndex",controlIndex_},{"menuIndex",menuIndex_},{"dialogIndex",dialogIndex_},{"selected",selected_.size()},{"count",rows_.size()},{"menuOpen",menuOpen_}};}
QVariantList FilesController::breadcrumbs() const {QVariantList result;QUrl url(url_);auto parts=url.path().split('/',Qt::SkipEmptyParts);QUrl base=url;base.setPath("/");result<<QVariantMap{{"label",url.isLocalFile()?"Storage":url.scheme()=="trash"?"Trash":url.host().isEmpty()?url.scheme():url.host()},{"url",base.toString()}};QString path;for(const auto& part:parts){path+="/"+part;base.setPath(path);result<<QVariantMap{{"label",part},{"url",base.toString()}};}return result;}
QVariantList FilesController::controls() const{return {item("back","Back","arrow-left"),item("forward","Forward","arrow-right"),item("up","Up","arrow-up"),item("search",query_.isEmpty()?"Search":"Clear search","magnifying-glass"),item("view",grid_?"List":"Grid",grid_?"list":"squares-four"),item("new","New folder","folder-plus"),item("paste","Paste","clipboard"),item("more","More","dots-three")};}
QVariantList FilesController::menu() const {
    if(!menuOpen_)return {};
    QVariantList result;
    if(!menuUrl_.isEmpty()) {result<<item("open","Open")<<item("select",selected_.contains(menuUrl_)?"Deselect":"Select")<<item("copy","Copy")<<item("cut","Cut");if(selected_.size()<2)result<<item("rename","Rename");if(!url_.startsWith("trash:"))result<<item("trash","Move to Trash");result<<item("delete","Delete permanently")<<item("properties","Properties");}
    else {result<<item("location","Open location")<<item("select-all","Select all")<<item("clear-selection","Clear selection")<<item("hidden",hidden_?"Hide hidden files":"Show hidden files")<<item("refresh","Refresh");}
    return result;
}
void FilesController::select(int index){if(index<0||index>=rows_.size())return;if(index_==index && area_=="files")return;index_=index;area_="files";Q_EMIT changed();}
void FilesController::activate(int index){if(modal()||busy_||index<0||index>=rows_.size())return;select(index);const auto row=rows_[index].toMap();if(!selected_.isEmpty()){const auto url=row["url"].toString();if(!selected_.remove(url))selected_.insert(url);Q_EMIT rowsChanged();Q_EMIT changed();return;}
    if(row["directory"].toBool())navigate(row["url"].toString());else Q_EMIT openRequested(row["url"].toString());
}
void FilesController::context(int index){if(modal())return;if(index>=0)select(index);menuUrl_=index>=0?rows_.value(index).toMap()["url"].toString():QString();menuOpen_=true;menuIndex_=0;Q_EMIT changed();}
QStringList FilesController::selectedUrls() const {return selected_.isEmpty()?QStringList{menuUrl_}:selected_.values();}
void FilesController::operation(QString op,QStringList sources,QString destination){
    sources.removeAll({});if((sources.isEmpty()&&op!="mkdir")||!service_.available()){error_="File operations are unavailable in this development build";Q_EMIT changed();return;}
    service_.start(op,sources,destination);selected_.clear();Q_EMIT rowsChanged();Q_EMIT changed();
}
void FilesController::command(const QString& action){if(modal())return;menuOpen_=false;
    if(action=="back"){if(historyIndex_>0)load(history_[--historyIndex_]);else command("up");}
    else if(action=="forward"){if(historyIndex_+1<history_.size())load(history_[++historyIndex_]);}
    else if(action=="up"){QUrl url(url_);auto path=url.path();if(path.endsWith('/')&&path.size()>1)path.chop(1);path=path.left(path.lastIndexOf('/')+1);if(path.isEmpty())path="/";url.setPath(path);if(url.toString()!=url_)navigate(url.toString());}
    else if(action=="refresh") {places_=service_.places();service_.list(url_);}
    else if(action=="view"){grid_=!grid_;area_="files";persist();}
    else if(action=="more")context();
    else if(action=="hidden"){hidden_=!hidden_;persist();rebuild();}
    else if(action=="search"&&!query_.isEmpty()){query_.clear();rebuild();}
    else if(action=="search"||action=="location"||action=="new"||action=="rename"){
        textAction_=action;textBase_=action=="rename"?menuUrl_:url_;
        Q_EMIT textRequested(action=="new"?"New folder":action=="rename"?"Rename":action=="location"?"Open location":"Search this folder",action=="rename"?QUrl(menuUrl_).fileName():action=="location"?url_:QString(),action=="location"?2048:255);
    } else if(action=="open"){const auto url=menuUrl_;selected_.clear();for(int i=0;i<rows_.size();++i)if(rows_[i].toMap()["url"]==url){activate(i);break;}}
    else if(action=="select"){if(!selected_.remove(menuUrl_))selected_.insert(menuUrl_);}
    else if(action=="select-all"){for(const auto& value:rows_)selected_.insert(value.toMap()["url"].toString());}
    else if(action=="clear-selection")selected_.clear();
    else if(action=="copy"||action=="cut"){clipboard_=selectedUrls();cut_=action=="cut";}
    else if(action=="paste"&&!clipboard_.isEmpty()){operation(cut_?"move":"copy",clipboard_,url_);if(cut_)clipboard_.clear();}
    else if(action=="trash"||action=="delete"){
        pendingSources_=selectedUrls();pendingOperation_=action;dialogIndex_=0;
        dialog_={{"title",action=="trash"?"Move to Trash?":"Delete permanently?"},{"text",QString("%1 item(s)\n%2").arg(pendingSources_.size()).arg(pendingSources_.size()==1?QUrl(pendingSources_.first()).fileName():"Selected files and folders")},{"actions",QVariantList{item("cancel","Cancel"),item("confirm",action=="trash"?"Move to Trash":"Delete")}}};
    } else if(action=="properties"){
        QVariantMap row;for(const auto& value:rows_)if(value.toMap()["url"]==menuUrl_)row=value.toMap();dialogIndex_=0;
        dialog_={{"title",row["name"]},{"text",QUrl(menuUrl_).toDisplayString(QUrl::RemoveUserInfo)+"\n"+QLocale().formattedDataSize(row["size"].toLongLong())+" · "+row["modified"].toString()},{"actions",QVariantList{item("cancel","Close")}}};
    }
    Q_EMIT rowsChanged();Q_EMIT changed();
}
void FilesController::applyText(const QString& text){const auto action=textAction_;textAction_.clear();if(action=="search"){query_=text;rebuild();return;}if(action=="location"){navigate(text);return;}
    if(text.isEmpty()||text=="."||text==".."||text.contains('/')||text.contains('\\')||text.contains(QChar(0))){error_="Enter a single file or folder name";Q_EMIT changed();return;}
    if(action=="new")operation("mkdir",{},child(textBase_,text));
    if(action=="rename"){QUrl parent(textBase_);auto path=parent.path();parent.setPath(path.left(path.lastIndexOf('/')+1));operation("rename",{textBase_},child(parent.toString(),text));}
}
void FilesController::answer(int index){const auto actions=dialog_["actions"].toList();if(index<0||index>=actions.size())return;const auto id=actions[index].toMap()["id"].toString(),task=dialog_["task"].toString();dialog_.clear();
    if(!task.isEmpty())service_.answer(task,id);else if(id=="confirm")operation(pendingOperation_,pendingSources_,pendingDestination_);pendingSources_.clear();Q_EMIT changed();
}
void FilesController::hideControls(bool hide){if((area_=="controls"&&hide)||controls_==!hide)return;controls_=!hide;Q_EMIT changed();}
void FilesController::setColumns(int columns){columns_=std::max(1,columns);}
void FilesController::focus(const QString& area,int index){if(area=="places"){area_=area;placeIndex_=std::clamp(index,0,std::max(0,int(places_.size())-1));}else if(area=="crumbs"){area_=area;controlIndex_=std::clamp(index,0,std::max(0,int(breadcrumbs().size())-1));}else if(area=="controls"){area_=area;controlIndex_=std::clamp(index,0,7);controls_=true;}else select(index);Q_EMIT changed();}
void FilesController::dispatch(Action action){
    if(modal()){const auto count=dialog_["actions"].toList().size();if(action==Action::Up||action==Action::Left)dialogIndex_=std::max(0,dialogIndex_-1);if(action==Action::Down||action==Action::Right)dialogIndex_=std::min(int(count)-1,dialogIndex_+1);if(action==Action::Confirm)answer(dialogIndex_);if(action==Action::Back){if(dialog_["task"].toString().isEmpty())answer(0);else answer(count-1);}Q_EMIT changed();return;}
    if(menuOpen_){const auto list=menu();if(action==Action::Up)menuIndex_=std::max(0,menuIndex_-1);if(action==Action::Down)menuIndex_=std::min(int(list.size())-1,menuIndex_+1);if(action==Action::Confirm)command(list.value(menuIndex_).toMap()["id"].toString());if(action==Action::Back)menuOpen_=false;Q_EMIT changed();return;}
    if(action==Action::LocalAction){controls_=!controls_;if(controls_)area_="controls";else area_="files";}
    else if(action==Action::ContextMenu)context(index_);
    else if(action==Action::Back){if(!selected_.isEmpty()){selected_.clear();Q_EMIT rowsChanged();}else command("back");}
    else if(action==Action::ScrollUp||action==Action::ScrollDown){hideControls(action==Action::ScrollDown);Q_EMIT scrollRequested(action==Action::ScrollDown?1:-1);}
    else if(action==Action::Confirm){if(area_=="crumbs")navigate(breadcrumbs().value(controlIndex_).toMap()["url"].toString());else if(area_=="places")navigate(places_.value(placeIndex_).toMap()["url"].toString());else if(area_=="controls")command(controls().value(controlIndex_).toMap()["id"].toString());else activate(index_);}
    else if(action==Action::ChangeView)command("view");
    else if(area_=="crumbs"){if(action==Action::Left)controlIndex_=std::max(0,controlIndex_-1);if(action==Action::Right)controlIndex_=std::min(int(breadcrumbs().size())-1,controlIndex_+1);if(action==Action::Down)area_="files";}
    else if(area_=="places"){if(action==Action::Up)placeIndex_=std::max(0,placeIndex_-1);if(action==Action::Down)placeIndex_=std::min(int(places_.size())-1,placeIndex_+1);if(action==Action::Right)area_="files";}
    else if(area_=="controls"){if(action==Action::Left)controlIndex_=std::max(0,controlIndex_-1);if(action==Action::Right)controlIndex_=std::min(7,controlIndex_+1);if(action==Action::Up)area_="files";}
    else {const int stride=grid_?columns_:1;if(action==Action::Left){if(index_%stride==0)area_="places";else --index_;}if(action==Action::Right)index_=std::min(int(rows_.size())-1,index_+1);if(action==Action::Up){if(index_<stride){area_="crumbs";controlIndex_=std::max(0,int(breadcrumbs().size())-1);}else index_-=stride;}if(action==Action::Down){if(index_+stride>=rows_.size()){controls_=true;area_="controls";}else index_+=stride;}}
    index_=std::max(0,index_);Q_EMIT changed();
}
}
