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
    {"volume",{0,0,1,4}},{"brightness",{1,0,1,4}},{"wifi",{2,0,2,2}},{"bluetooth",{4,0,1,2}},{"airplane",{5,0,1,2}},{"downloads",{6,0,6,2}},
    {"battery",{2,2,2,2}},{"clock",{4,2,2,2}},{"storage",{6,2,4,2}},{"scraper",{10,2,2,2}}};}
ControlsDashboard::Widget* ControlsDashboard::find(const QString& id){for(auto& w:items_)if(w.id==id)return &w;return nullptr;}
bool ControlsDashboard::fits(const Widget& w,const QList<Widget>& items) const {
    if(w.page<0||w.page>7||w.rect.x()<0||w.rect.y()<0||w.rect.right()>=12||w.rect.bottom()>=4)return false;
    for(const auto& other:items)if(!other.hidden&&other.id!=w.id&&other.page==w.page&&other.rect.intersects(w.rect))return false;
    return true;
}
int ControlsDashboard::pages() const {int n=1;for(const auto& w:items_)if(!w.hidden)n=std::max(n,w.page+1);return std::max(n,moving_?page_+1:1);}
QVariantList ControlsDashboard::widgets() const {
    QVariantList result;
    for(const auto& w:items_)if(!w.hidden&&w.page==page_)result.append(QVariantMap{{"id",w.id},{"title",title(w.id)},{"x",w.rect.x()},{"y",w.rect.y()},{"w",w.rect.width()},{"h",w.rect.height()}});
    return result;
}
QVariantList ControlsDashboard::available() const {
    QVariantList r;int i=0;for(const auto& w:items_)if(w.hidden)r.append(QVariantMap{{"id",w.id},{"title",title(w.id)},{"selected",i++==pickIndex_}});return r;
}
void ControlsDashboard::setOwner(const QString& owner){
    const auto key=owner.isEmpty()?QString():"controls/v1/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
    if(key==key_&&!items_.isEmpty())return;
    leave();key_=key;items_=defaults();page_=0;selected_="volume";
    const auto data=key_.isEmpty()?QByteArray():QSettings().value(key_).toByteArray();
    if(data.size()<=8192){
        const auto rows=QJsonDocument::fromJson(data).array();QList<Widget> loaded;QSet<QString> seen;bool valid=rows.size()==items_.size();
        for(const auto& v:rows){const auto r=v.toObject();const auto id=r["id"].toString();auto* def=find(id);
            if(!def||seen.contains(id)){valid=false;break;}seen.insert(id);
            Widget w{id,{r["x"].toInt(-1),r["y"].toInt(-1),r["w"].toInt(),r["h"].toInt()},r["page"].toInt(-1),r["hidden"].toBool()};
            const auto size=w.rect.size();const bool compact=size==QSize(2,2);
            if((size!=def->rect.size()&&!compact)||(!w.hidden&&!fits(w,loaded))){valid=false;break;}
            loaded.append(w);
        }
        if(valid)items_=loaded;
    }
    normalize();emit changed();
}
void ControlsDashboard::save(){
    if(key_.isEmpty())return;QJsonArray rows;
    for(const auto& w:items_)rows.append(QJsonObject{{"id",w.id},{"x",w.rect.x()},{"y",w.rect.y()},{"w",w.rect.width()},{"h",w.rect.height()},{"page",w.page},{"hidden",w.hidden}});
    QSettings().setValue(key_,QJsonDocument(rows).toJson(QJsonDocument::Compact));
}
void ControlsDashboard::normalize(){
    QList<int> used;for(const auto& w:items_)if(!w.hidden&&!used.contains(w.page))used.append(w.page);std::sort(used.begin(),used.end());
    const auto old=page_;for(auto& w:items_)if(!w.hidden)w.page=used.indexOf(w.page);
    page_=std::max(0,int(used.indexOf(old)));
    if(auto* w=find(selected_);!w||w->hidden||w->page!=page_) {const auto rows=widgets();selected_=rows.isEmpty()?"edit":rows.first().toMap()["id"].toString();}
}
void ControlsDashboard::begin(){if(items_.isEmpty())items_=defaults();editing_=moving_=picking_=adjusting_=switching_=false;status_.clear();normalize();emit changed();}
void ControlsDashboard::leave(){if(moving_)items_=moveOriginal_;editing_=moving_=picking_=adjusting_=switching_=false;normalize();emit changed();}
void ControlsDashboard::select(QString id){if(moving_||picking_)return;selected_=id;adjusting_=false;emit changed();}
void ControlsDashboard::edit(){if(moving_){items_=moveOriginal_;moving_=false;}editing_=!editing_;picking_=adjusting_=false;normalize();save();emit changed();}
void ControlsDashboard::activate(QString id){
    if(switching_){if(id=="desktop"||id=="steam"){switching_=false;emit actionRequested(id);emit changed();}return;}
    if(picking_){restore(id);return;}
    if(id=="switch"&&!editing_){switching_=true;switchIndex_=0;emit changed();return;}
    if(id=="edit"){edit();return;}
    if(id=="add"){pick();return;}
    if(id=="resize"){resize();return;}if(id=="hide"){hide();return;}
    if(moving_){moving_=false;normalize();save();emit changed();return;}
    selected_=id;status_.clear();
    if(editing_){if(auto* w=find(id)){original_=*w;moveOriginal_=items_;moving_=true;}emit changed();return;}
    if(id=="volume"||id=="brightness"){adjusting_=!adjusting_;emit changed();return;}
    emit actionRequested(id);emit changed();
}
void ControlsDashboard::pick(){if(!editing_)return;picking_=true;pickIndex_=0;emit changed();}
void ControlsDashboard::restore(QString id){
    if(!editing_||!picking_)return;auto* w=find(id);if(!w||!w->hidden)return;
    auto candidate=*w;candidate.hidden=false;
    for(int p=0;p<8;++p)for(int y=0;y<=4-w->rect.height();++y)for(int x=0;x<=12-w->rect.width();++x){candidate.page=p;candidate.rect.moveTo(x,y);if(fits(candidate,items_)){*w=candidate;selected_=id;page_=p;picking_=false;normalize();save();emit changed();return;}}
}
void ControlsDashboard::hide(){if(!editing_||moving_)return;if(auto* w=find(selected_)){w->hidden=true;normalize();save();emit changed();}}
void ControlsDashboard::resize(){
    if(!editing_||moving_)return;auto* w=find(selected_);if(!w)return;
    auto candidate=*w;QSize full;for(const auto& d:defaults())if(d.id==w->id)full=d.rect.size();
    candidate.rect.setSize(w->rect.size()==full?QSize(2,2):full);
    if(candidate.rect==w->rect)return;
    for(int p=page_;p<8;++p)for(int y=0;y<=4-candidate.rect.height();++y)for(int x=0;x<=12-candidate.rect.width();++x){candidate.page=p;candidate.rect.moveTo(x,y);if(fits(candidate,items_)){*w=candidate;page_=p;normalize();save();emit changed();return;}}
}
void ControlsDashboard::drop(QString id,int x,int y){if(!editing_)return;auto* w=find(id);if(!w)return;auto c=*w;c.rect.moveTo(x,y);c.page=page_;if(fits(c,items_)){*w=c;moving_=false;selected_=id;normalize();save();}else status_="Choose a free space";emit changed();}
void ControlsDashboard::move(int dx,int dy){auto* w=find(selected_);if(!w)return;auto c=*w;c.rect.translate(dx,dy);
    if(fits(c,items_)){*w=c;status_.clear();}
    else { // Equal-size neighbours can exchange places without losing either control.
        bool swapped=false;for(auto& other:items_)if(!other.hidden&&other.id!=w->id&&other.page==w->page&&other.rect==c.rect){auto o=other;o.rect=w->rect;auto trial=items_;for(auto& t:trial)if(t.id==other.id)t=o;if(fits(c,trial)){other=o;*w=c;swapped=true;break;}}
        if(!swapped)status_="Free space needed · L1/R1 moves to another page";
    }emit changed();}
void ControlsDashboard::changePage(int delta){
    const int target=page_+delta;if(target<0||target>=std::min(8,pages()+(moving_?1:0)))return;
    if(moving_){auto* w=find(selected_);if(!w)return;auto c=*w;c.page=target;bool placed=false;for(int y=0;y<=4-c.rect.height()&&!placed;++y)for(int x=0;x<=12-c.rect.width();++x){c.rect.moveTo(x,y);if(fits(c,items_)){*w=c;placed=true;break;}}if(!placed)return;}
    page_=target;if(!moving_){const auto rows=widgets();selected_=rows.isEmpty()?"edit":rows.first().toMap()["id"].toString();}emit changed();
}
void ControlsDashboard::navigate(int dx,int dy){
    struct Target{QString id;QPointF center;};QList<Target> targets{{"edit",{1,-1}},{"settings",{9,-1}},{"power",{10,-1}},{"switch",{11,-1}}};
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
