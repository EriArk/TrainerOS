#pragma once
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace trainer {
// Device-local consent, independent of the directory and message delivery.
// UTC seconds survive timezone changes. Tick on resume; never wake the device.
class LobbyReminders {
public:
    bool configure(QString file) {
        if(file_==file)return true;
        file_=std::move(file);records_={};
        QFile input(file_);if(file_.isEmpty()||!input.exists())return true;
        if(!input.open(QIODevice::ReadOnly)||input.size()>65536)return false;
        const auto doc=QJsonDocument::fromJson(input.readAll());
        if(!doc.isObject()||doc.object()["v"]!=1)return false;
        const auto rows=doc.object()["rooms"].toObject();
        for(auto it=rows.begin();it!=rows.end();++it)
            if(!QUuid(it.key()).isNull()&&valid(it.value().toObject()))records_[it.key()]=it.value();
        return true;
    }
    bool contains(const QString& room) const {return records_.contains(room);}
    bool accept(const QJsonObject& room,qint64 now) {
        if(QUuid(room["id"].toString()).isNull()||room["startsAt"].toInteger()<=now)return false;
        QJsonObject record{{"owner",room["owner"]},{"startsAt",room["startsAt"]},
            {"title",room["title"].toString().isEmpty()?QString("Lobby"):room["title"].toString()},{"fired",false}};
        if(!valid(record))return false;
        const auto old=records_;records_[room["id"].toString()]=record;return commit(old);
    }
    bool observe(const QJsonObject& room) {
        const auto id=room["id"].toString();auto record=records_[id].toObject();
        if(record.isEmpty()||room["owner"]!=record["owner"])return true;
        if(room["startsAt"].toInteger()==0)return cancel(id);
        if(room["startsAt"]!=record["startsAt"])record["fired"]=false;
        record["startsAt"]=room["startsAt"];record["title"]=room["title"].toString().isEmpty()?QString("Lobby"):room["title"].toString();
        if(!valid(record))return false;
        if(record==records_[id].toObject())return true;
        const auto old=records_;records_[id]=record;return commit(old);
    }
    bool cancel(const QString& id) {
        if(!records_.contains(id))return true;
        const auto old=records_;records_.remove(id);return commit(old);
    }
    QJsonArray due(qint64 now) {
        QJsonArray due;const auto old=records_;
        for(const auto& id:records_.keys()) {
            auto record=records_[id].toObject();const auto start=record["startsAt"].toInteger();
            if(start<=now){records_.remove(id);continue;}
            if(start-now>900||record["fired"].toBool())continue;
            record["fired"]=true;records_[id]=record;record["id"]=id;due.append(record);
        }
        if(records_!=old&&!commit(old))return {};
        return due;
    }
private:
    static bool valid(const QJsonObject& r) {
        return !r["owner"].toString().isEmpty()&&r["owner"].toString().size()<=20
            &&r["startsAt"].isDouble()&&r["startsAt"].toInteger()>0&&r["startsAt"].toInteger()<=4102444800LL
            &&r["title"].isString()&&r["title"].toString().size()<=80;
    }
    bool commit(const QJsonObject& old) {
        if(file_.isEmpty()){records_=old;return false;}
        QSaveFile output(file_);
        const auto bytes=QJsonDocument(QJsonObject{{"v",1},{"rooms",records_}}).toJson(QJsonDocument::Compact);
        if(output.open(QIODevice::WriteOnly)&&output.write(bytes)==bytes.size()&&output.commit())return true;
        records_=old;return false;
    }
    QString file_;
    QJsonObject records_;
};
}
