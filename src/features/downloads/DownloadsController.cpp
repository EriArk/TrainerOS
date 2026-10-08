#include "DownloadsController.h"
#include <algorithm>
namespace trainer {
void DownloadsController::publish(const QString& provider,const QVariantList& rows) {
    const auto previous=tasks().value(focusIndex()).toMap();
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
    action_=std::clamp(action_,0,std::max(0,int(actions().size())-1));emit changed();
}
QVariantList DownloadsController::tasks() const {
    QVariantList list;
    for(auto source=sources_.begin();source!=sources_.end();++source)
        for(const auto& v:source.value()){auto row=v.toMap();row["provider"]=source.key();row["key"]=source.key()+":"+row["id"].toString();row["section"]=row["state"]=="done"?"Downloaded":"";list<<row;}
    const auto rank=[](const QVariant& v){const auto state=v.toMap()["state"].toString();return state=="running"?0:state=="done"?2:1;};
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
void DownloadsController::close(){open_=false;emit changed();}
void DownloadsController::select(int i){const auto list=tasks();if(i<0||i>=list.size())return;selected_=list[i].toMap()["key"].toString();action_=0;pane_=false;emit changed();}
void DownloadsController::activate(int i) {
    const auto row=tasks().value(focusIndex()).toMap();const auto commands=actions();
    if(i<0||i>=commands.size()||!commands[i].toMap().value("enabled",true).toBool())return;
    action_=i;pane_=true;emit commandRequested(row["provider"].toString(),row["id"].toString(),commands[i].toMap()["id"].toString());emit changed();
}
void DownloadsController::controlAll(const QString& command) {
    if(command!="pause-all"&&command!="resume-all"&&command!="cancel-all")return;
    const auto providers=sources_.keys();for(const auto& provider:providers)emit commandRequested(provider,{},command);
}
void DownloadsController::dispatch(Action a) {
    if(a==Action::Back||a==Action::SystemMenu){close();return;}
    if(a==Action::Left)pane_=false;
    if(a==Action::Right)pane_=true;
    if(a==Action::Up||a==Action::Down){const int d=a==Action::Up?-1:1;if(pane_)action_=std::clamp(action_+d,0,std::max(0,int(actions().size())-1));else select(std::clamp(focusIndex()+d,0,std::max(0,int(tasks().size())-1)));}
    if(a==Action::Confirm){if(pane_)activate(action_);else pane_=true;}
    if(a==Action::Secondary)controlAll("pause-all");
    if(a==Action::ToggleContinue)controlAll("resume-all");
    if(a==Action::LocalAction)controlAll("cancel-all");
    emit changed();
}
}
