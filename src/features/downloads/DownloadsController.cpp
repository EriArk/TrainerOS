#include "DownloadsController.h"
#include <algorithm>
namespace trainer {
DownloadsController::DownloadsController(QObject* parent):QObject(parent) {
    clock_.start();sampleTimer_.setInterval(1000);
    connect(&sampleTimer_,&QTimer::timeout,this,[this]{
        const auto now=clock_.elapsed();bool active=false;
        for(auto source=sources_.cbegin();source!=sources_.cend();++source)for(const auto& value:source.value()) {
            const auto row=value.toMap();if(!row.contains("received"))continue;
            auto& s=samples_[source.key()+":"+row["id"].toString()];
            const auto bytes=row["received"].toLongLong();
            s.speed=row["state"]=="running"&&s.time>0&&bytes>=s.bytes?double(bytes-s.bytes)*1000/std::max<qint64>(1,now-s.time):0;
            s.bytes=bytes;s.time=now;
            if(!row["terminal"].toBool()){s.history<<s.speed;while(s.history.size()>60)s.history.removeFirst();active=true;}
        }
        if(open_||active)emit changed();
    });sampleTimer_.start();
    connect(&picker_,&FilePickerController::changed,this,&DownloadsController::changed);
    connect(&picker_,&FilePickerController::selected,this,[this](const QString& folder){
        const auto key=pickerKey_;pickerKey_.clear();
        for(const auto& v:tasks()){const auto row=v.toMap();if(row["key"]==key&&row["canChooseDestination"].toBool()){
            selected_=key;
            emit destinationRequested(row["provider"].toString(),row["id"].toString(),folder);break;
        }}
    });
}
void DownloadsController::publish(const QString& provider,const QVariantList& rows) {
    if(sources_.value(provider)==rows)return;
    const auto previous=tasks().value(focusIndex()).toMap();
    const auto previousAction=actions().value(action_).toMap()["id"].toString();
    sources_[provider]=rows;
    const auto list=tasks();
    if(std::none_of(list.begin(),list.end(),[this](const QVariant& v){return v.toMap()["key"]==selected_;}))
        selected_=list.value(0).toMap()["key"].toString();
    // Follow the active download when it completes, without moving a player's
    // deliberate selection of an edition/error or an open action pane.
    if(!pane_&&previous["state"]=="running") {
        const auto selected=tasks().value(focusIndex()).toMap();
        if(selected["terminal"].toBool()) {
            for(const auto& value:list){const auto row=value.toMap();
                if(row["state"]=="running"||row["state"]=="queued"){selected_=row["key"].toString();break;}
            }
        }
    }
    if(pane_) {
        const auto commands=actions();
        const auto found=std::find_if(commands.begin(),commands.end(),[&](const QVariant& v){return v.toMap()["id"]==previousAction;});
        if(selected_!=previous["key"].toString()||previousAction.isEmpty()||found==commands.end()) {
            pane_=false;action_=0;
        } else action_=int(std::distance(commands.begin(),found));
    }
    action_=std::clamp(action_,0,std::max(0,int(actions().size())-1));emit changed();
}
QVariantList DownloadsController::tasks() const {
    QVariantList list;
    for(auto source=sources_.begin();source!=sources_.end();++source)
        for(const auto& v:source.value()){auto row=v.toMap();row["provider"]=source.key();row["key"]=source.key()+":"+row["id"].toString();row["section"]=row["state"]=="done"?(row["kind"]=="file-operation"?"Finished":"Downloaded"):"";
            const auto sample=samples_.value(row["key"].toString());
            if(row.contains("received")){row["speed"]=row["state"]=="running"?sample.speed:0;row["history"]=sample.history;}
            auto actions=row["actions"].toList();if(row["canChooseDestination"].toBool())actions<<QVariantMap{{"id","destination"},{"label",row["state"]=="done"?"Move to folder…":"Save to folder…"}};
            row["actions"]=actions;list<<row;}
    const auto rank=[](const QVariant& v){const auto row=v.toMap();const auto state=row["state"].toString();return state=="running"||state=="moving"?0:row["terminal"].toBool()||state=="done"?2:1;};
    std::stable_sort(list.begin(),list.end(),[&](const QVariant& a,const QVariant& b){return rank(a)<rank(b);});
    return list;
}
int DownloadsController::focusIndex() const {
    const auto list=tasks();for(int i=0;i<list.size();++i)if(list[i].toMap()["key"]==selected_)return i;return 0;
}
QVariantList DownloadsController::actions() const {return tasks().value(focusIndex()).toMap()["actions"].toList();}
QString DownloadsController::summary() const {
    int pending=0,done=0;const auto list=tasks();
    for(const auto& row:list){if(row.toMap()["terminal"].toBool())++done;else ++pending;}
    return list.isEmpty()?"No downloads yet":QString("%1 pending · %2 finished").arg(pending).arg(done);
}
void DownloadsController::begin(){open_=true;pane_=false;emit changed();}
void DownloadsController::close(){picker_.cancel();pickerKey_.clear();open_=false;pane_=false;emit changed();}
void DownloadsController::select(int i){const auto list=tasks();if(i<0||i>=list.size())return;selected_=list[i].toMap()["key"].toString();action_=0;pane_=false;emit changed();}
void DownloadsController::showActions(int i) {
    if(i<0||i>=tasks().size())return;
    select(i);pane_=!actions().isEmpty();emit changed();
}
void DownloadsController::dismissActions(){pane_=false;emit changed();}
void DownloadsController::activate(int i) {
    const auto row=tasks().value(focusIndex()).toMap();const auto commands=actions();
    if(i<0||i>=commands.size()||!commands[i].toMap().value("enabled",true).toBool())return;
    action_=i;pane_=true;command(row["key"].toString(),commands[i].toMap()["id"].toString());emit changed();
}
void DownloadsController::command(const QString& key,const QString& id) {
    for(const auto& v:tasks()){const auto row=v.toMap();if(row["key"]!=key)continue;
        bool allowed=false;for(const auto& a:row["actions"].toList())if(a.toMap()["id"]==id&&a.toMap().value("enabled",true).toBool())allowed=true;
        if(!allowed)return;
        if(id=="destination"){pickerKey_=key;picker_.begin(row["destination"].toString(),true);return;}
        emit commandRequested(row["provider"].toString(),row["id"].toString(),id);return;
    }
}
void DownloadsController::moveBefore(const QString& key,const QString& before) {
    QVariantMap from,to;for(const auto& v:tasks()){const auto row=v.toMap();if(row["key"]==key)from=row;if(row["key"]==before)to=row;}
    if(key==before||!from["reorderable"].toBool()||(!before.isEmpty()&&(!to["reorderable"].toBool()||to["provider"]!=from["provider"])))return;
    emit reorderRequested(from["provider"].toString(),from["id"].toString(),to["id"].toString());
}
void DownloadsController::controlAll(const QString& command) {
    if(command!="pause-all"&&command!="resume-all"&&command!="cancel-all")return;
    const auto providers=sources_.keys();for(const auto& provider:providers)emit commandRequested(provider,{},command);
}
void DownloadsController::dispatch(Action a) {
    if(picker_.isOpen()){picker_.dispatch(a);return;}
    if(a==Action::SystemMenu){close();return;}
    if(a==Action::Back){if(pane_)dismissActions();else close();return;}
    if(a==Action::Left)pane_=false;
    if(a==Action::Right)pane_=!actions().isEmpty();
    if(a==Action::Up||a==Action::Down){const int d=a==Action::Up?-1:1;if(pane_)action_=std::clamp(action_+d,0,std::max(0,int(actions().size())-1));else select(std::clamp(focusIndex()+d,0,std::max(0,int(tasks().size())-1)));}
    if(a==Action::Confirm){if(pane_)activate(action_);else pane_=!actions().isEmpty();}
    if(a==Action::Secondary)controlAll("pause-all");
    if(a==Action::ToggleContinue)controlAll("resume-all");
    if(a==Action::LocalAction)controlAll("cancel-all");
    emit changed();
}
}
