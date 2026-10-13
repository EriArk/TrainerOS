#include "LobbyWire.h"
#include <QJsonDocument>
#include <QUuid>

namespace trainer {
namespace {
const QString prefix="TrainerOS lobby\n```json\n";
constexpr int maxBytes=32768,partBytes=900,maxParts=(maxBytes+partBytes-1)/partBytes;
}
QStringList LobbyWire::encode(const QJsonObject& packet,qint64 now) {
    const auto bytes=QJsonDocument(packet).toJson(QJsonDocument::Compact);
    if(packet.isEmpty()||bytes.size()>maxBytes||now<=0)return {};
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int count=(bytes.size()+partBytes-1)/partBytes;QStringList messages;
    for(int i=0;i<count;++i) {
        const QJsonObject frame{{"v",1},{"id",id},{"part",i},{"count",count},{"expires",now+90},
            {"data",QString::fromLatin1(bytes.mid(i*partBytes,partBytes).toBase64())}};
        messages.append(prefix+QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact))+"\n```");
    }
    return messages;
}
QJsonObject LobbyWire::frame(const QString& content) {
    if(content.size()>2000||!content.startsWith(prefix)||!content.endsWith("\n```"))return {};
    const auto doc=QJsonDocument::fromJson(content.mid(prefix.size(),content.size()-prefix.size()-4).toUtf8());
    const auto f=doc.object();const auto id=f["id"].toString();
    if(f.size()!=6||f["v"]!=1||QUuid(id).isNull()||QUuid(id).toString(QUuid::WithoutBraces)!=id)return {};
    const int part=f["part"].toInt(-1),count=f["count"].toInt(-1);
    if(part<0||count<1||count>maxParts||part>=count||f["data"].toString().size()>1200||f["data"].toString().isEmpty())return {};
    return f;
}
QJsonObject LobbyWire::receive(const QString& peer,const QJsonObject& frame,qint64 now) {
    if(peer.isEmpty()||peer.size()>128||now<=0)return {};
    for(auto it=pending_.begin();it!=pending_.end();)if(it->expires<=now)it=pending_.erase(it);else ++it;
    const auto f=LobbyWire::frame(prefix+QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact))+"\n```");
    const auto expiry=f["expires"].toInteger();
    if(f.isEmpty()||expiry<=now||expiry>now+90)return {};
    const auto decoded=QByteArray::fromBase64Encoding(f["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded||decoded.decoded.isEmpty()||decoded.decoded.size()>partBytes)return {};
    const auto key=peer+"/"+f["id"].toString();
    if(!pending_.contains(key)&&pending_.size()>=64)return {};
    auto& pending=pending_[key];const auto count=f["count"].toInt(),part=f["part"].toInt();
    if(pending.count&&(pending.count!=count||pending.expires!=expiry)) {pending_.remove(key);return {};}
    if(pending.parts.contains(part)&&pending.parts[part]!=decoded.decoded) {pending_.remove(key);return {};}
    pending.count=count;pending.expires=expiry;pending.parts[part]=decoded.decoded;
    if(pending.parts.size()!=count)return {};
    QByteArray bytes;for(int i=0;i<count;++i)bytes+=pending.parts[i];pending_.remove(key);
    if(bytes.size()>maxBytes)return {};
    return QJsonDocument::fromJson(bytes).object();
}
}
