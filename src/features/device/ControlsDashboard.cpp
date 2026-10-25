#include "ControlsDashboard.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <algorithm>
#include <limits>
#include <cmath>
#include <QMap>

namespace trainer {
namespace {
QString title(const QString& id){
    static const QMap<QString,QString> labels{{"volume","Volume"},{"brightness","Brightness"},{"wifi","Wi-Fi"},{"bluetooth","Bluetooth"},{"airplane","Airplane"},{"downloads","Downloads"},{"battery","Battery"},{"clock","Clock"},{"storage","Storage"},{"scraper","ScreenScraper"}};
    return labels.value(id,id);
}
}
ControlsDashboard::ControlsDashboard(QObject* parent):QObject(parent) {
    connect(this,&ControlsDashboard::changed,this,[this]{
        const auto current=widgets();
        if(current!=publishedLayout_){publishedLayout_=current;emit layoutChanged();}
    });
}
QList<ControlsDashboard::Widget> ControlsDashboard::defaults(){return {
    {"volume",{0,0,1,4}},{"brightness",{1,0,1,4}},{"wifi",{2,0,1,1}},{"bluetooth",{3,0,1,1}},{"airplane",{2,1,1,1}},{"downloads",{4,0,4,4}},
    {"battery",{2,2,2,1}},{"clock",{2,3,2,1}},{"storage",{8,0,4,4}},{"scraper",{3,1,1,1}}};}
QList<QSize> ControlsDashboard::sizes(const QString& id) const {
    if(external_.contains(id))return {{4,2},{2,2}};
    if(id=="volume"||id=="brightness")return {{3,2},{1,4},{2,2}};
    if(id=="wifi")return {{1,1},{2,2},{4,2}};
    if(id=="bluetooth"||id=="airplane")return {{1,1},{2,2},{4,2},{1,2}};
    if(id=="downloads")return {{4,4},{3,2},{6,2},{2,2}};
    if(id=="storage")return {{4,4},{3,2},{4,2},{2,2}};
    if(id=="battery"||id=="clock")return {{2,1},{2,2},{4,2}};
    if(id=="scraper")return {{1,1},{2,2},{4,2}};
    return {{2,2},{4,2}};
}
QString ControlsDashboard::selectedTitle() const {
    if(external_.contains(selected_))return external_[selected_].value("title").toString();
    return widgetSelected()?title(selected_):QString();
}
bool ControlsDashboard::widgetSelected() const {
    for(const auto& w:items_)if(w.id==selected_)return !w.hidden&&w.page==page_;
    return false;
}
ControlsDashboard::Widget* ControlsDashboard::find(const QString& id){for(auto& w:items_)if(w.id==id)return &w;return nullptr;}
bool ControlsDashboard::fits(const Widget& w,const QList<Widget>& items) const {
    if(w.page<0||w.page>7||w.rect.x()<0||w.rect.y()<0||w.rect.right()>=12||w.rect.bottom()>=4)return false;
    for(const auto& other:items)if(!other.hidden&&other.id!=w.id&&other.page==w.page&&other.rect.intersects(w.rect))return false;
    return true;
}
int ControlsDashboard::pages() const {int n=1;for(const auto& w:items_)if(!w.hidden)n=std::max(n,w.page+1);return std::max(n,moving_?page_+1:1);}
QVariantList ControlsDashboard::widgets() const {
    QVariantList result;
    for(const auto& w:items_)if(!w.hidden&&w.page==page_){
        auto row=external_.value(w.id);row.insert("id",w.id);row.insert("title",external_.contains(w.id)?row.value("title"):title(w.id));
        row.insert("x",w.rect.x());row.insert("y",w.rect.y());row.insert("w",w.rect.width());row.insert("h",w.rect.height());result.append(row);
    }
    return result;
}
QVariantList ControlsDashboard::available() const {
    QVariantList r;int i=0;for(const auto& w:items_)if(w.hidden)r.append(QVariantMap{{"id",w.id},{"title",external_.contains(w.id)?external_[w.id].value("title"):title(w.id)},{"selected",i++==pickIndex_}});return r;
}
void ControlsDashboard::setOwner(const QString& owner){
    const auto key=owner.isEmpty()?QString():"controls/v1/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
    if(key==key_&&!items_.isEmpty())return;
    leave();key_=key;items_=defaults();external_.clear();externalLayout_.clear();page_=0;selected_="volume";
    if(!key_.isEmpty())externalLayout_=QSettings().value(key_+"/extensions").toMap();
    const auto data=key_.isEmpty()?QByteArray():QSettings().value(key_).toByteArray();
    if(data.size()<=8192){
        const auto rows=QJsonDocument::fromJson(data).array();QList<Widget> loaded;QSet<QString> seen;bool valid=rows.size()==items_.size();
        for(const auto& v:rows){const auto r=v.toObject();const auto id=r["id"].toString();auto* def=find(id);
            if(!def||seen.contains(id)){valid=false;break;}seen.insert(id);
            Widget w{id,{r["x"].toInt(-1),r["y"].toInt(-1),r["w"].toInt(),r["h"].toInt()},r["page"].toInt(-1),r["hidden"].toBool()};
            if(!sizes(id).contains(w.rect.size())||(!w.hidden&&!fits(w,loaded))){valid=false;break;}
            loaded.append(w);
        }
        if(valid)items_=loaded;
    }
    if(!key_.isEmpty()&&QSettings().value(key_+"/layoutRevision",0).toInt()<2){
        // Owner's 25 October layout: compact radios, two full-height information columns.
        // Retain visibility and unrelated sizes/pages; keep the old serialized layout for recovery.
        if(!data.isEmpty())QSettings().setValue(key_+"/beforeRevision2",data);
        auto requested=items_;QList<Widget> placed;
        for(auto& w:requested){
            if(w.id=="wifi"||w.id=="bluetooth"||w.id=="airplane")w.rect.setSize({1,1});
            if(w.id=="downloads"||w.id=="storage"){w.rect={w.id=="downloads"?4:8,0,4,4};w.page=0;placed.append(w);}
        }
        for(auto& w:requested){
            if(w.id=="downloads"||w.id=="storage")continue;
            if(!w.hidden&&!fits(w,placed)){
                bool found=false;
                for(int offset=0;offset<8&&!found;++offset)for(int y=0;y<=4-w.rect.height()&&!found;++y)for(int x=0;x<=12-w.rect.width();++x){
                    auto candidate=w;candidate.page=(w.page+offset)%8;candidate.rect.moveTo(x,y);
                    if(fits(candidate,placed)){w=candidate;found=true;break;}
                }
            }
            placed.append(w);
        }
        // Keep stable enumeration/order for focus and the picker.
        for(auto& w:items_)for(const auto& p:placed)if(w.id==p.id){w=p;break;}
        normalize();save();QSettings().setValue(key_+"/layoutRevision",2);
    }
    normalize();emit changed();
}
void ControlsDashboard::save(){
    if(key_.isEmpty())return;QJsonArray rows;
    for(const auto& w:items_){
        const QJsonObject record{{"id",w.id},{"x",w.rect.x()},{"y",w.rect.y()},{"w",w.rect.width()},{"h",w.rect.height()},{"page",w.page},{"hidden",w.hidden}};
        if(external_.contains(w.id))externalLayout_[w.id]=record.toVariantMap();else rows.append(record);
    }
    QSettings().setValue(key_,QJsonDocument(rows).toJson(QJsonDocument::Compact));
    QSettings().setValue(key_+"/extensions",externalLayout_);
}
void ControlsDashboard::setExternalWidgets(const QVariantList& rows){
    QMap<QString,QVariantMap> next;
    for(const auto& value:rows){const auto row=value.toMap();const auto id=row["id"].toString();if(id.startsWith("sdk:"))next[id]=row;}
    if(next==external_)return;
    if(next.keys()!=external_.keys()){
        // Cancel a layout edit before removing its objects, so Back cannot revive
        // a disconnected process's widget or stale input target.
        leave();save();
        for(auto i=items_.begin();i!=items_.end();)if(external_.contains(i->id)&&!next.contains(i->id))i=items_.erase(i);else ++i;
        for(auto i=next.begin();i!=next.end();++i)if(!external_.contains(i.key())){
            Widget w{i.key(),{0,0,4,2},0,true};
            const auto saved=externalLayout_.value(i.key()).toMap();
            if(!saved.isEmpty()){
                Widget restored{i.key(),{saved["x"].toInt(),saved["y"].toInt(),saved["w"].toInt(),saved["h"].toInt()},saved["page"].toInt(),saved["hidden"].toBool()};
                if(restored.rect.size()==QSize(4,2)||restored.rect.size()==QSize(2,2)){
                    if(restored.hidden||fits(restored,items_))w=restored;
                    else {
                        bool found=false;
                        for(int p=0;p<8&&!found;++p)for(int y=0;y<=4-restored.rect.height()&&!found;++y)for(int x=0;x<=12-restored.rect.width();++x){
                            auto candidate=restored;candidate.page=p;candidate.rect.moveTo(x,y);
                            if(fits(candidate,items_)){w=candidate;found=true;break;}
                        }
                    }
                }
            }
            items_.append(w);
        }
    }
    external_=next;normalize();emit changed();
}
void ControlsDashboard::normalize(){
    QList<int> used;for(const auto& w:items_)if(!w.hidden&&!used.contains(w.page))used.append(w.page);std::sort(used.begin(),used.end());
    const auto old=page_;for(auto& w:items_)if(!w.hidden)w.page=used.indexOf(w.page);
    page_=std::max(0,int(used.indexOf(old)));
    const bool header=selected_=="edit"||(!editing_&&(selected_=="settings"||selected_=="power"||selected_=="switch"));
    if(auto* w=find(selected_);!header&&(!w||w->hidden||w->page!=page_)) {const auto rows=widgets();selected_=rows.isEmpty()?"edit":rows.first().toMap()["id"].toString();}
}
void ControlsDashboard::begin(){if(items_.isEmpty())items_=defaults();editing_=moving_=picking_=adjusting_=switching_=false;status_.clear();normalize();emit changed();}
void ControlsDashboard::leave(){if(moving_){items_=moveOriginal_;page_=original_.page;}editing_=moving_=picking_=adjusting_=switching_=false;normalize();emit changed();}
void ControlsDashboard::select(QString id){if(moving_||picking_||switching_)return;const auto* w=find(id);if(!w||w->hidden||w->page!=page_)return;selected_=id;adjusting_=false;status_.clear();emit changed();}
void ControlsDashboard::edit(){if(moving_){items_=moveOriginal_;page_=original_.page;moving_=false;}editing_=!editing_;picking_=adjusting_=false;status_.clear();normalize();save();emit changed();}
void ControlsDashboard::activate(QString id){
    if(switching_){if(id=="desktop"||id=="steam"){switching_=false;emit actionRequested(id);emit changed();}return;}
    if(picking_){restore(id);return;}
    if(id=="switch"&&!editing_){switching_=true;switchIndex_=0;emit changed();return;}
    if(id=="edit"){edit();return;}
    if(id=="add"){pick();return;}
    if(id=="resize"){resize();return;}if(id=="hide"){hide();return;}
    if(editing_&&(id=="settings"||id=="power"||id=="switch"))return;
    if(moving_){moving_=false;normalize();save();emit changed();return;}
    if(auto* w=find(id);w&&(w->hidden||w->page!=page_))return;
    if(!find(id)&&id!="settings"&&id!="power"&&id!="switch")return;
    selected_=id;status_.clear();
    if(editing_){if(auto* w=find(id)){original_=*w;moveOriginal_=items_;moving_=true;}emit changed();return;}
    if(id=="volume"||id=="brightness"){adjusting_=!adjusting_;emit changed();return;}
    if(id.startsWith("sdk:")){const auto* w=find(id);if(w&&!w->hidden&&external_.contains(id))emit contributionActivated(id);emit changed();return;}
    emit actionRequested(id);emit changed();
}
void ControlsDashboard::pick(){if(!editing_||moving_)return;picking_=true;pickIndex_=0;status_.clear();emit changed();}
void ControlsDashboard::restore(QString id){
    if(!editing_||!picking_)return;auto* w=find(id);if(!w||!w->hidden)return;
    auto candidate=*w;candidate.hidden=false;
    for(int offset=0;offset<8;++offset)for(int y=0;y<=4-w->rect.height();++y)for(int x=0;x<=12-w->rect.width();++x){const int p=(page_+offset)%8;candidate.page=p;candidate.rect.moveTo(x,y);if(fits(candidate,items_)){*w=candidate;selected_=id;page_=p;picking_=false;status_.clear();normalize();save();emit changed();return;}}
    status_="All pages are full. Hide a widget to make room.";emit changed();
}
void ControlsDashboard::hide(){if(!editing_||moving_)return;if(auto* w=find(selected_)){w->hidden=true;normalize();save();emit changed();}}
void ControlsDashboard::resize(){
    if(!editing_||moving_)return;auto* w=find(selected_);if(!w)return;
    auto candidate=*w;const auto allowed=sizes(w->id);
    candidate.rect.setSize(allowed[(allowed.indexOf(w->rect.size())+1)%allowed.size()]);
    if(candidate.rect==w->rect)return;
    if(fits(candidate,items_)){*w=candidate;status_.clear();save();emit changed();return;}
    for(int offset=0;offset<8;++offset)for(int y=0;y<=4-candidate.rect.height();++y)for(int x=0;x<=12-candidate.rect.width();++x){const int p=(page_+offset)%8;candidate.page=p;candidate.rect.moveTo(x,y);if(fits(candidate,items_)){*w=candidate;page_=p;status_.clear();normalize();save();emit changed();return;}}
    status_="No room for this size";emit changed();
}
bool ControlsDashboard::place(const Widget& candidate){
    auto* w=find(candidate.id);if(!w)return false;
    if(fits(candidate,items_)){*w=candidate;return true;}
    // Only equal rectangles exchange positions. Test the entire transaction first.
    for(const auto& other:items_)if(!other.hidden&&other.id!=w->id&&other.page==candidate.page&&other.rect==candidate.rect){
        auto displaced=other;displaced.rect=w->rect;displaced.page=w->page;
        auto trial=items_;for(auto& t:trial){if(t.id==other.id)t=displaced;else if(t.id==w->id)t=candidate;}
        if(fits(candidate,trial)&&fits(displaced,trial)){items_=trial;return true;}
    }
    return false;
}
void ControlsDashboard::drop(QString id,int x,int y){
    if(!editing_||picking_||moving_)return;auto* w=find(id);if(!w||w->hidden||w->page!=page_)return;
    auto c=*w;c.rect.moveTo(x,y);
    if(x<0||x+c.rect.width()>12){
        c.page=page_+(x<0?-1:1);c.rect.moveTo(x<0?12-c.rect.width():0,std::clamp(y,0,4-c.rect.height()));
    }
    if(place(c)){page_=c.page;selected_=id;status_.clear();normalize();save();}
    else status_="Choose a free space";
    emit changed();
}
void ControlsDashboard::move(int dx,int dy){
    const auto* w=find(selected_);if(!w)return;const auto original=*w;
    // Skip blocked cells so a filled board can exchange adjacent multi-cell widgets.
    for(int distance=1;distance<=(dx?12:4);++distance){auto c=original;c.rect.translate(dx*distance,dy*distance);
        if(place(c)){status_.clear();emit changed();return;}
    }
    status_="Free space needed · L1/R1 moves to another page";emit changed();
}
void ControlsDashboard::changePage(int delta){
    const int target=page_+delta;if(target<0||target>=std::min(8,pages()+(moving_?1:0)))return;
    if(moving_){auto* w=find(selected_);if(!w)return;auto c=*w;c.page=target;bool placed=false;for(int y=0;y<=4-c.rect.height()&&!placed;++y)for(int x=0;x<=12-c.rect.width();++x){c.rect.moveTo(x,y);if(fits(c,items_)){*w=c;placed=true;break;}}if(!placed)return;}
    page_=target;adjusting_=false;status_.clear();if(!moving_){const auto rows=widgets();selected_=rows.isEmpty()?"edit":rows.first().toMap()["id"].toString();}emit changed();
}
void ControlsDashboard::navigate(int dx,int dy){
    struct Target{QString id;QPointF center;};QList<Target> targets{{"edit",{2,-1}}};
    if(!editing_)targets.append({{"settings",{9,-1}},{"power",{10,-1}},{"switch",{11,-1}}});
    for(const auto& w:items_)if(!w.hidden&&w.page==page_)targets.append({w.id,{w.rect.x()+w.rect.width()/2.,w.rect.y()+w.rect.height()/2.}});
    QPointF from(1,-1);for(const auto& t:targets)if(t.id==selected_)from=t.center;
    double best=std::numeric_limits<double>::max();QString chosen=selected_;
    for(const auto& t:targets){auto diff=t.center-from;double along=dx?diff.x()*dx:diff.y()*dy;if(along<=0)continue;double across=dx?std::abs(diff.y()):std::abs(diff.x());double score=along+across*4;if(score<best){best=score;chosen=t.id;}}
    selected_=chosen;emit changed();
}
void ControlsDashboard::dispatch(Action a){
    if(a==Action::Home){leave();emit closeRequested();return;}
    if(a==Action::Back){if(switching_)switching_=false;else if(picking_)picking_=false;else if(moving_){items_=moveOriginal_;moving_=false;page_=original_.page;}else if(adjusting_)adjusting_=false;else if(editing_)editing_=false;else {emit closeRequested();return;}status_.clear();normalize();emit changed();return;}
    if(switching_){if(a==Action::Up||a==Action::Down)switchIndex_=a==Action::Up?0:1;else if(a==Action::Confirm)activate(switchIndex_?"steam":"desktop");emit changed();return;}
    if(picking_){const auto rows=available();if(a==Action::Up||a==Action::Down)pickIndex_=std::clamp(pickIndex_+(a==Action::Up?-1:1),0,std::max(0,int(rows.size())-1));else if(a==Action::Confirm&&!rows.isEmpty())restore(rows[pickIndex_].toMap()["id"].toString());emit changed();return;}
    if(a==Action::LocalAction){edit();return;}
    if(a==Action::PreviousPage||a==Action::NextPage){changePage(a==Action::NextPage?1:-1);return;}
    if(a==Action::Secondary&&adjusting_&&selected_=="volume"){emit actionRequested("volume");return;}
    if(a==Action::Secondary&&editing_){resize();return;}
    if(a==Action::ToggleContinue&&editing_){hide();return;}
    if(a==Action::PreviousFace&&editing_){pick();return;}
    if(a==Action::Confirm){activate(selected_);return;}
    const int dx=a==Action::Left?-1:a==Action::Right?1:0,dy=a==Action::Up?-1:a==Action::Down?1:0;
    if(!dx&&!dy)return;
    if(adjusting_)emit adjustRequested(selected_,(dx?dx:-dy)*5);
    else if(moving_)move(dx,dy);else navigate(dx,dy);
}
}
