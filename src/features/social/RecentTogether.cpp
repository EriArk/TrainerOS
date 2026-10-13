#include "RecentTogether.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <algorithm>
#include <limits>

namespace trainer {
namespace {
bool personId(const QString& id) {if(id.isEmpty()||id.size()>20)return false;for(auto c:id)if(c<'0'||c>'9')return false;return true;}
}
void RecentTogether::configure(QString file,QString self) {
    if(file_==file&&self_==self)return;
    file_=file;self_=self;people_={};lastRoster_.clear();
    if(file.isEmpty())return;
    QFile input(file);if(QFileInfo(file).isSymLink()||input.size()>256*1024||!input.open(QIODevice::ReadOnly))return;
    const auto data=QJsonDocument::fromJson(input.readAll()).object();
    if(data["v"]!=1||data["self"]!=self)return;
    const auto people=data["people"].toObject();
    for(auto it=people.begin();it!=people.end()&&people_.size()<300;++it) {
        const auto p=it.value().toObject();
        if(personId(it.key())&&it.key()!=self&&p["name"].toString().size()<=100&&p["last"].toInteger()>0)people_[it.key()]=p;
    }
}
bool RecentTogether::observe(const QJsonObject& lobby,qint64 now) {
    if(self_.isEmpty()||now<=0)return false;
    const auto members=lobby["members"].toArray();bool joined=false;QStringList roster;
    for(const auto& v:members) {const auto p=v.toObject();if(p["id"]==self_&&p["connected"].toBool())joined=true;
        if(p["connected"].toBool())roster.append(p["id"].toString());}
    if(!joined) {lastRoster_.clear();return false;}
    roster.sort();const auto key=lobby["id"].toString()+"/"+roster.join(',');
    if(key==lastRoster_)return false;
    lastRoster_=key;bool changed=false;
    for(const auto& v:members) {
        const auto p=v.toObject();const auto id=p["id"].toString();
        if(!personId(id)||id==self_||!p["connected"].toBool())continue;
        people_[id]=QJsonObject{{"name",p["name"].toString().left(100)},{"last",now}};changed=true;
    }
    while(people_.size()>300) {QString oldest; qint64 date=std::numeric_limits<qint64>::max();
        for(auto it=people_.begin();it!=people_.end();++it)if(it.value().toObject()["last"].toInteger()<date) {oldest=it.key();date=it.value().toObject()["last"].toInteger();}
        people_.remove(oldest);
    }
    if(changed)save();return changed;
}
QVariantList RecentTogether::rows() const {
    QVariantList result;for(auto it=people_.begin();it!=people_.end();++it) {auto row=it.value().toObject().toVariantMap();row["id"]=it.key();result.append(row);}
    std::sort(result.begin(),result.end(),[](const QVariant& a,const QVariant& b){const auto x=a.toMap(),y=b.toMap();
        return x["last"]==y["last"]?x["id"].toString()<y["id"].toString():x["last"].toLongLong()>y["last"].toLongLong();});return result;
}
bool RecentTogether::forget(const QString& peer) {if(!people_.contains(peer))return false;people_.remove(peer);save();return true;}
void RecentTogether::save() const {
    if(file_.isEmpty())return;
    const auto dir=QFileInfo(file_).absolutePath();if(QFileInfo(dir).isSymLink()||QFileInfo(file_).isSymLink()||!QDir().mkpath(dir))return;
    QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(file_);if(!file.open(QIODevice::WriteOnly))return;file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"v",1},{"self",self_},{"people",people_}}).toJson(QJsonDocument::Compact);
    if(file.write(bytes)==bytes.size())file.commit();
}
}
