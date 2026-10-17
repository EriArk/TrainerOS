#include "SocialController.h"
namespace trainer {
QVariantMap SocialController::discover() const {
    const bool home=query_.isEmpty()&&!discoverAll_;
    const int count=discoverAll_?recentPeople().size():searchResults().size(),size=home?3:6;
    return {{"home",home},{"all",discoverAll_},{"area",discoverArea_},{"page",discoverPage_},
        {"pageSize",size},{"pages",qMax(1,(count+size-1)/size)},{"busy",snapshot_["searching"]},{"status",snapshot_["searchStatus"]},
        {"displayPage",(discoverAll_?0:snapshot_["searchOffset"].toInt()/size)+discoverPage_+1},
        {"displayPages",qMax(1,((discoverAll_?count:snapshot_["searchTotal"].toInt())+size-1)/size)},
        {"more",snapshot_["searchOffset"].toInt()+24<snapshot_["searchTotal"].toInt()},{"previous",snapshot_["searchOffset"].toInt()>0}};
}
QVariantMap SocialController::discoverPreview() const {
    auto result=discoverPreview_;if(result.isEmpty())return result;
    for(const auto& v:searchResults())if(v.toMap()["id"]==result["id"]){result=v.toMap();break;}
    result["busy"]=snapshot_["searching"];result["status"]=snapshot_["searchStatus"];return result;
}
void SocialController::runSearch(int offset) {
    searchStarted_=true;searchFocus_=-1;QString mode=searchKind_,text=query_;
    if(text.isEmpty())mode="communities";
    else if(text.contains("://")||text.startsWith("fluxer.gg/",Qt::CaseInsensitive)||text.startsWith("fluxer.app/",Qt::CaseInsensitive))mode="invite";
    else if(text.startsWith("invite:",Qt::CaseInsensitive)){mode="invite";text=text.mid(7).trimmed();}
    emit commandRequested("search",{{"mode",mode},{"text",text},{"offset",offset}});
}
void SocialController::setSearchKind(QString kind) {
    if(kind=="traineros")kind="communities";
    if(!QStringList{"people","communities"}.contains(kind)||kind==searchKind_)return;
    searchKind_=kind;discoverPage_=0;discoverAll_=false;discoverPreview_.clear();runSearch();emit changed();
}
void SocialController::editSearch(){textPurpose_="search";emit textRequested("Find people, communities or paste an invite",query_,256);}
void SocialController::activateSearch(int index) {
    const auto row=searchResults().value(index).toMap();if(row.isEmpty())return;
    searchFocus_=index;discoverArea_="results";
    if(row["kind"]=="person")profile(row["id"].toString());
    else if(row["kind"]=="request-tag"&&!snapshot_["searching"].toBool()&&row["action"]!="Sent") {
        closeMenu();menuMode_="confirm";menuTitle_="Send friend request?";menuDetail_=row["id"].toString();menuSubject_=row["id"].toString();
        menu_={"Cancel","Send request"};menuCommands_={"cancel","request-discovered-tag"};menuFocus_=0;
    } else if(row["kind"]!="request-tag")discoverPreview_=row;
    emit changed();
}
void SocialController::discoverAction(QString action,int index) {
    if(action=="search"){discoverArea_="search";editSearch();}
    else if(action=="clear"){query_.clear();discoverPreview_.clear();discoverAll_=false;discoverPage_=0;discoverArea_="search";runSearch();}
    else if(action=="retry")runSearch(snapshot_["searchOffset"].toInt());
    else if(action=="all"){discoverAll_=true;discoverArea_="recent";discoverPage_=0;selectRecent(0);}
    else if(action=="recent"){discoverArea_="recent";selectRecent(index);recentAction("profile");}
    else if(action=="back") {
        if(!discoverPreview_.isEmpty())discoverPreview_.clear();
        else if(discoverAll_){discoverAll_=false;discoverPage_=0;discoverArea_="recent";}
        else discoverArea_="search";
    } else if(action=="join"&&!discoverPreview_.isEmpty()&&!snapshot_["searching"].toBool()&&discoverPreview()["action"]!="Joined")
        emit commandRequested("search-action",{{"id",discoverPreview_["id"]}});
    else if(action=="next"||action=="previous") {
        const int step=action=="next"?1:-1;const auto state=discover();const int target=discoverPage_+step;
        if(target>=0&&target<state["pages"].toInt())discoverPage_=target;
        else if(!discoverAll_&&((step>0&&state["more"].toBool())||(step<0&&state["previous"].toBool()))) {runSearch(qMax(0,snapshot_["searchOffset"].toInt()+step*24));discoverPage_=step<0?24/state["pageSize"].toInt()-1:0;}
        if(discoverAll_)selectRecent(discoverPage_*6);else searchFocus_=discoverPage_*state["pageSize"].toInt();
    }
    emit changed();
}
void SocialController::dispatchDiscover(Action action) {
    if(!discoverPreview_.isEmpty()) {if(action==Action::Back)discoverAction("back");else if(action==Action::Confirm)discoverAction("join");return;}
    const auto state=discover();const bool home=state["home"].toBool();
    if(action==Action::Back){discoverAction("back");return;}
    if(discoverArea_=="search") {
        if(action==Action::Confirm)editSearch();
        else if(action==Action::Left||action==Action::Right)setSearchKind(searchKind_=="people"?"communities":"people");
        else if(action==Action::Down){discoverArea_=(home||discoverAll_)&&!recentPeople().isEmpty()?"recent":"results";searchFocus_=searchResults().isEmpty()?-1:discoverPage_*state["pageSize"].toInt();}
    } else if(discoverArea_=="recent") {
        if(action==Action::Confirm)recentAction("profile");
        else if(action==Action::ContextMenu||action==Action::LocalAction){if(!discoverAll_)discoverAction("all");else dispatchRecent(action);}
        else if(action==Action::Left||action==Action::Right){selectRecent(qBound(0,recentFocus()+(action==Action::Right?1:-1),qMax(0,int(recentPeople().size())-1)));if(discoverAll_)discoverPage_=recentFocus()/6;}
        else if(action==Action::Up){if(discoverAll_&&recentFocus()>=3){selectRecent(recentFocus()-3);discoverPage_=recentFocus()/6;}else discoverArea_="search";}
        else if(action==Action::Down){if(discoverAll_){selectRecent(qMin(recentFocus()+3,int(recentPeople().size())-1));discoverPage_=recentFocus()/6;}else {discoverArea_="results";searchFocus_=searchResults().isEmpty()?-1:0;}}
    } else {
        const int columns=home?3:searchKind_=="people"?1:3,count=searchResults().size(),pageSize=state["pageSize"].toInt();
        if(action==Action::Confirm){if(count)activateSearch(searchFocus_);else runSearch();}
        else if(action==Action::Up){if(searchFocus_<columns)discoverArea_=home&&!recentPeople().isEmpty()?"recent":"search";else searchFocus_-=columns;}
        else if(action==Action::Down&&count)searchFocus_=qMin(count-1,searchFocus_+columns);
        else if(action==Action::Left||action==Action::Right) {
            const int step=action==Action::Right?1:-1;
            if((step>0&&searchFocus_==count-1)||(step<0&&searchFocus_==0))discoverAction(step>0?"next":"previous");
            else searchFocus_=qBound(0,searchFocus_+step,qMax(0,count-1));
        }
        if(searchFocus_>=0)discoverPage_=searchFocus_/pageSize;
    }
    emit changed();
}
}
