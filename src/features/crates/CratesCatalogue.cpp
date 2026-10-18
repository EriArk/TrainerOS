#include "CratesCatalogue.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace trainer {
namespace {
bool identity(const QString& id) {
    static const QRegularExpression syntax("^[a-z0-9][a-z0-9._-]{0,95}$");
    return syntax.match(id).hasMatch();
}
QString text(const QJsonObject& o,const char* key,int max=160) {
    return o.value(QLatin1String(key)).toString().trimmed().left(max);
}
QString art(const QJsonValue& value) {
    const auto s=value.toString();
    // Only compiled assets for now. A future verified cache owns remote image admission.
    if(s.size()>256 || s.contains("..") || s.contains('?') || s.contains('#'))return {};
    if(!s.startsWith("qrc:/crates/") && !s.startsWith("qrc:/series/"))return {};
    return s;
}
}
bool CratesCatalogue::read(const QByteArray& bytes,const QStringList& crateIds,CratesCatalogue& result) {
    if(bytes.size()>2*1024*1024)return false;
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())return false;
    const auto root=doc.object();
    if(root["schema"].toInt()!=1 || !root["projects"].isArray() || !root["contents"].isObject())return false;
    const auto rows=root["projects"].toArray();
    if(rows.size()>1000)return false;
    CratesCatalogue parsed;
    for(const auto& value:rows) {
        if(!value.isObject())return false;
        const auto o=value.toObject(); const auto id=text(o,"id",97),kind=text(o,"kind");
        if(!identity(id) || parsed.projects.contains(id) || text(o,"title").isEmpty() ||
           !QStringList{"game","app","module"}.contains(kind))return false;
        QVariantList screens,releases;
        const auto images=o["screenshots"].toArray();
        if(images.size()>8 || o["releases"].toArray().size()>64)return false;
        for(const auto& image:images) {const auto source=art(image);if(!source.isEmpty()&&!screens.contains(source))screens<<source;}
        QSet<QString> versions;
        for(const auto& item:o["releases"].toArray()) {
            if(!item.isObject())return false;
            const auto release=item.toObject();const auto version=text(release,"version",80);
            if(version.isEmpty() || versions.contains(version))return false;
            versions.insert(version);
            auto compatibility=text(release,"compatibility");
            if(!QStringList{"supported","untested","incompatible"}.contains(compatibility))compatibility="untested";
            auto channel=text(release,"channel");
            if(!QStringList{"Stable","Beta","Experimental"}.contains(channel))channel="";
            const auto size=release["sizeBytes"].toDouble(-1);
            releases<<QVariantMap{{"version",version},{"channel",channel},
                {"architecture",text(release,"architecture",80)},{"compatibility",compatibility},
                {"compatibilityNote",text(release,"compatibilityNote",600)},
                {"sizeBytes",size>=0&&size<1e13?size:-1},{"releasedOn",text(release,"releasedOn",40)},
                {"notes",text(release,"notes",6000)}};
        }
        parsed.projects[id]={{"id",id},{"title",text(o,"title")},{"kind",kind},
            {"license",text(o,"license",80)},{"sample",o["sample"].toBool(false)},
            {"creator",text(o,"creator")},{"summary",text(o,"summary",300)},
            {"description",text(o,"description",12000)},{"hero",art(o["hero"])},
            {"screenshots",screens},{"releases",releases}};
    }
    const auto contents=root["contents"].toObject();
    for(auto i=contents.begin();i!=contents.end();++i) {
        if(!crateIds.contains(i.key()) || !i.value().isArray() || i.value().toArray().size()>1000)return false;
        QStringList members;
        for(const auto& value:i.value().toArray()) {
            const auto id=value.toString();
            if(!parsed.projects.contains(id) || members.contains(id))return false;
            members<<id;
        }
        parsed.contents[i.key()]=members;
    }
    result=std::move(parsed);return true;
}
}
