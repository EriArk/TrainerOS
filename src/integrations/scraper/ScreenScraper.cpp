#include "ScreenScraper.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <array>
#include <algorithm>

namespace trainer::scraper {
namespace {
const QMap<QString,int> systems{{"gb",9},{"gbc",10},{"gba",12},{"nds",15},{"n3ds",17},
    {"n64",14},{"gc",13},{"wii",16},{"wiiu",18},{"switch",225},{"pokemini",211},
    {"nes",3},{"snes",4},{"mastersystem",2},{"megadrive",1},{"gamegear",21},{"sega32x",19},
    {"segacd",20},{"saturn",22},{"dreamcast",23},{"psx",57},{"ps2",58},{"psp",61},
    {"pcengine",31},{"pcenginecd",114},{"ngp",25},{"ngpc",82},{"neogeo",142},
    {"neogeocd",70},{"fbneo",75},{"mame",75},{"naomi",56},{"atomiswave",53}};
bool cancelled(const Cancellation& flag) { return flag && flag->load(); }
QString value(const QJsonValue& v) {
    if(v.isObject()) return value(v.toObject().value("text"));
    return v.isString()?v.toString():v.isDouble()?QString::number(v.toDouble(),'g',16):QString();
}
int integer(const QJsonValue& v) { bool ok=false;int n=value(v).toInt(&ok);return ok?std::max(0,n):0; }
QString preferred(const QJsonValue& list, const QString& field, const QStringList& preferences) {
    const auto items=list.toArray();
    for(const auto& preference:preferences) for(const auto& item:items)
        if(item.toObject()[field].toString()==preference) return value(item).left(8192);
    return {}; // Do not substitute an unrelated language.
}
Quota quota(const QJsonObject& user) {
    return {integer(user["maxthreads"]),integer(user["maxrequestspermin"]),integer(user["maxrequestsperday"]),
        integer(user["requeststoday"]),integer(user["maxrequestskoperday"]),integer(user["requestskotoday"])};
}
Game parseGame(const QJsonObject& data, const Fingerprint* file) {
    Game g;g.id=value(data["id"]);g.system=integer(data["systeme"].toObject()["id"]);
    if(integer(data["id"])<=0 || g.system<=0 || integer(data["notgame"])>0)return {};
    const QStringList regions{"us","wor","eu","uk","ss"};
    g.fields["name"]=preferred(data["noms"],"region",regions);
    g.fields["desc"]=preferred(data["synopsis"],"langue",{"en"});
    const auto date=preferred(data["dates"],"region",regions);
    QDate parsed=QDate::fromString(date,"yyyy-MM-dd");
    if(!parsed.isValid() && date.size()==4)parsed=QDate(date.toInt(),1,1);
    if(parsed.isValid())g.fields["releasedate"]=parsed.toString("yyyyMMdd")+"T000000";
    for(const auto& pair:QList<QPair<QString,QString>>{{"developer","developpeur"},{"publisher","editeur"},{"players","joueurs"}})
        g.fields[pair.first]=value(data[pair.second]).left(256);
    QStringList genres;
    for(const auto& genre:data["genres"].toArray()) {
        const auto name=preferred(genre.toObject()["noms"],"langue",{"en"});
        if(!name.isEmpty() && !genres.contains(name))genres.append(name);
    }
    g.fields["genre"]=genres.join(" / ").left(256);
    const QMap<QString,QStringList> types{{"image",{"box-2D","box-3D"}}, {"marquee",{"wheel-hd","wheel"}},
        {"screenshot",{"ss"}},{"fanart",{"fanart"}},{"video",{"video-normalized","video"}}};
    const auto media=data["medias"].toArray();
    if(media.size()>2048)return {};
    for(auto it=types.cbegin();it!=types.cend();++it) {
        for(const auto& type:it.value()) {
            for(const auto& region:QStringList{"us","wor","eu","uk","ss",""}) {
                for(const auto& item:media) {
                    const auto m=item.toObject();const QUrl url(m["url"].toString());
                    if(m["type"].toString()==type && m["region"].toString()==region && allowedUrl(url)) {g.media[it.key()]=url;break;}
                }
                if(g.media.contains(it.key()))break;
            }
            if(g.media.contains(it.key()))break;
        }
    }
    const auto rom=data["rom"].toObject();
    if(file && value(rom["romsize"]).toLongLong()==file->size) {
        bool any=false, all=true;
        for(const auto& pair:QList<QPair<QString,QString>>{{"rommd5",file->md5},{"romsha1",file->sha1},{"romcrc",file->crc}}) {
            const auto hash=value(rom[pair.first]);
            if(!hash.isEmpty()){any=true;all=all && hash.compare(pair.second,Qt::CaseInsensitive)==0;}
        }
        g.exactFile=any && all;
    }
    return g;
}
}
bool Credentials::valid() const {
    const auto good=[](const QString& s){return !s.isEmpty() && s.size()<=256 && !s.contains(QChar::Null);};
    return good(developerId) && good(developerPassword) && ((username.isEmpty() && password.isEmpty()) || (good(username) && good(password)));
}
Credentials readCredentials(const QString& path) {
    QFile f(path);
    if(QFileInfo(path).isSymLink() || !f.open(QIODevice::ReadOnly) || f.size()>8192)return {};
#ifdef Q_OS_UNIX
    if(f.permissions() & (QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther))return {};
#endif
    const auto o=QJsonDocument::fromJson(f.readAll()).object();
    Credentials c{o["developerId"].toString(),o["developerPassword"].toString(),o["username"].toString(),o["password"].toString()};
    return o["version"].toInt()==1 && c.valid()?c:Credentials{};
}
bool writeCredentials(const QString& path,const Credentials& c) {
    if(!c.valid() || QFileInfo(path).isSymLink() || !QDir().mkpath(QFileInfo(path).absolutePath()))return false;
    QSaveFile f(path);f.setDirectWriteFallback(false);
    if(!f.open(QIODevice::WriteOnly) || !f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner))return false;
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"developerId",c.developerId},{"developerPassword",c.developerPassword},{"username",c.username},{"password",c.password}}).toJson();
    return f.write(bytes)==bytes.size() && f.commit();
}
int systemId(const QString& platform) {
    const QMap<QString,QString> aliases{{"gamecube","gc"},{"3ds","n3ds"},{"ps","psx"},{"dc","dreamcast"}};
    return systems.value(aliases.value(platform,platform),0);
}
QStringList platforms() { return systems.keys(); }
bool Fingerprint::unchanged() const {
    const QFileInfo info(path);
    return valid() && info.isFile() && info.canonicalFilePath()==path && info.size()==size && info.lastModified().toMSecsSinceEpoch()==modified;
}
Fingerprint fingerprint(const QString& path,const Cancellation& cancel) {
    QFileInfo info(path);Fingerprint f;f.path=info.canonicalFilePath();f.size=info.size();f.modified=info.lastModified().toMSecsSinceEpoch();
    QFile input(f.path);if(!info.isFile() || f.size<=0 || !input.open(QIODevice::ReadOnly))return {};
    static const auto table=[] {std::array<quint32,256> a{};for(quint32 i=0;i<256;++i){quint32 c=i;for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320U:0U);a[i]=c;}return a;}();
    QCryptographicHash md5(QCryptographicHash::Md5),sha1(QCryptographicHash::Sha1);quint32 crc=0xffffffffU;qint64 read=0;
    while(!input.atEnd()) {
        if(cancelled(cancel))return {};
        const auto block=input.read(256*1024);if(block.isEmpty())return {};
        md5.addData(block);sha1.addData(block);for(unsigned char ch:block)crc=table[(crc^ch)&255]^(crc>>8);read+=block.size();
    }
    f.md5=md5.result().toHex();f.sha1=sha1.result().toHex();f.crc=QString::number(crc^0xffffffffU,16).rightJustified(8,'0');
    return !cancelled(cancel) && read==f.size && f.unchanged()?f:Fingerprint{};
}
bool allowedUrl(const QUrl& u) {
    return u.isValid() && u.scheme()=="https" && u.userInfo().isEmpty() && (u.port(-1)==-1 || u.port()==443)
        && (u.host()=="screenscraper.fr" || u.host().endsWith(".screenscraper.fr"));
}
Transport httpsTransport() {
    return [](const QUrl& url,qint64 limit,const Cancellation& cancel) {
        Reply result;
        if(!allowedUrl(url) || limit<=0 || limit>64*1024*1024 || cancelled(cancel))return result;
        QNetworkAccessManager network;QUrl current=url;
        for(int redirects=0;redirects<=3;++redirects) {
            QNetworkRequest request(current);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
            request.setHeader(QNetworkRequest::UserAgentHeader,"TrainerOS/0.1.0");
            auto* reply=network.get(request);reply->setReadBufferSize(limit+1);
            QEventLoop loop;QTimer timeout,stop;timeout.setSingleShot(true);timeout.start(45000);stop.start(80);
            QByteArray bytes;bool oversized=false;
            QObject::connect(reply,&QIODevice::readyRead,&loop,[&]{bytes+=reply->read(limit+1-bytes.size());if(bytes.size()>limit){oversized=true;reply->abort();}});
            QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
            QObject::connect(&stop,&QTimer::timeout,reply,[&]{if(cancelled(cancel))reply->abort();});
            QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
            if(!reply->isFinished())loop.exec();
            bytes+=reply->read(std::max<qint64>(0,limit+1-bytes.size()));
            result.status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            result.retryAfter=std::clamp(reply->rawHeader("Retry-After").toInt(),0,3600);
            const auto redirect=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            const bool success=reply->error()==QNetworkReply::NoError && !oversized && bytes.size()<=limit && !cancelled(cancel);
            delete reply;
            if(!success){result.bytes.clear();if(result.status==200)result.status=0;return result;}
            if(!redirect.isEmpty()) {
                const auto next=current.resolved(redirect);
                // Never forward credentials to another origin, even another SS host.
                if(!allowedUrl(next) || next.host()!=url.host()) {result.status=0;return result;}
                current=next;continue;
            }
            result.bytes=bytes;return result;
        }
        result.status=0;return result;
    };
}
bool Quota::exhausted() const { return (daily>0 && today>=daily) || (failedDaily>0 && failedToday>=failedDaily); }
Client::Client(Credentials credentials,Transport transport,Delay delay):credentials_(std::move(credentials)),transport_(std::move(transport)),delay_(std::move(delay)) {
    if(!delay_)delay_=[](qint64 ms,const Cancellation& cancel){QElapsedTimer timer;timer.start();while(timer.elapsed()<ms){if(cancelled(cancel))return false;QThread::msleep(40);}return !cancelled(cancel);};
}
bool Client::reserve(const Cancellation& cancel) {
    if(cancelled(cancel) || quota_.exhausted())return false;
    // A client is worker-confined and serial. Even without known quotas it
    // never opens multiple service threads; backoff is cancellation-aware.
    const qint64 spacing=std::max<qint64>(backoffSeconds_*1000LL,quota_.perMinute>0?(60000LL+quota_.perMinute-1)/quota_.perMinute:1000);
    if(spacing_.isValid() && !delay_(std::max<qint64>(0,spacing-spacing_.elapsed()),cancel))return false;
    spacing_.restart();backoffSeconds_=0;
    if(accountKnown_)++quota_.today;
    return true;
}
QUrl Client::url(const QString& operation,const QMap<QString,QString>& fields) const {
    QUrl u("https://api.screenscraper.fr/api2/"+operation);QUrlQuery q;
    const QMap<QString,QString> auth{{"devid",credentials_.developerId},{"devpassword",credentials_.developerPassword},{"ssid",credentials_.username},{"sspassword",credentials_.password},{"softname","TrainerOS"},{"output","json"}};
    for(auto i=auth.cbegin();i!=auth.cend();++i)if(!i.value().isEmpty())q.addQueryItem(i.key(),i.value());
    for(auto i=fields.cbegin();i!=fields.cend();++i)q.addQueryItem(i.key(),i.value());
    u.setQuery(q);return u;
}
Result Client::request(const QString& op,const QMap<QString,QString>& fields,int system,const Fingerprint* f,const Cancellation& cancel) {
    Result r;
    if(cancelled(cancel)){r.status=Status::Cancelled;return r;}
    if(!credentials_.valid()){r.status=Status::MissingCredentials;return r;}
    if(!reserve(cancel)){r.status=cancelled(cancel)?Status::Cancelled:Status::Quota;return r;}
    const auto reply=transport_(url(op,fields),2*1024*1024,cancel);r.retryAfter=reply.retryAfter;
    if(cancelled(cancel)){r.status=Status::Cancelled;return r;}
    if(reply.status!=200) {
        switch(reply.status) {case 401:case 403:case 426:r.status=Status::Denied;break;case 404:r.status=Status::NotFound;break;
        case 423:case 429:r.status=Status::Busy;break;case 430:case 431:r.status=Status::Quota;break;default:r.status=Status::Offline;}
        if(r.status==Status::Busy)backoffSeconds_=std::max(30,reply.retryAfter);
        if(r.status==Status::Quota){quota_.daily=1;quota_.today=1;}
        if(r.status==Status::NotFound && accountKnown_)++quota_.failedToday;
        return r;
    }
    if(reply.bytes.size()>2*1024*1024)return r;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(reply.bytes,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())return r;
    const auto response=doc.object()["response"].toObject();if(response.isEmpty())return r;
    if(response["ssuser"].isObject()){quota_=quota(response["ssuser"].toObject());accountKnown_=true;}
    r.quota=quota_;
    if(op=="ssuserInfos.php") {r.status=response["ssuser"].isObject()?Status::Ready:Status::InvalidResponse;return r;}
    QJsonArray games;
    if(response["jeu"].isObject())games.append(response["jeu"]);
    else if(response["jeux"].isArray())games=response["jeux"].toArray();
    else return r;
    if(games.size()>30)return r;
    for(const auto& entry:games) {auto g=parseGame(entry.toObject(),f);if(!g.id.isEmpty() && g.system==system)r.games.append(g);}
    r.status=r.games.isEmpty()?Status::NotFound:Status::Ready;return r;
}
Result Client::account(const Cancellation& cancel) { return request("ssuserInfos.php",{},0,nullptr,cancel); }
Result Client::lookup(const QString& platform,const Fingerprint& f,const Cancellation& cancel) {
    const int system=systemId(platform);if(!system || !f.unchanged())return {};
    return request("jeuInfos.php",{{"systemeid",QString::number(system)},{"romnom",QFileInfo(f.path).fileName()},
        {"romtaille",QString::number(f.size)},{"md5",f.md5},{"sha1",f.sha1},{"crc",f.crc}},system,&f,cancel);
}
Result Client::search(const QString& platform,const QString& title,const Cancellation& cancel) {
    const int system=systemId(platform);if(!system || title.trimmed().isEmpty() || title.size()>256)return {};
    return request("jeuRecherche.php",{{"systemeid",QString::number(system)},{"recherche",title.trimmed()}},system,nullptr,cancel);
}
Reply Client::media(const QUrl& source,bool video,const Cancellation& cancel) {
    if(!credentials_.valid() || !allowedUrl(source) || cancelled(cancel))return {};
    if(!reserve(cancel))return {cancelled(cancel)?0:430,{},0};
    // Media response URLs can carry SS secrets: never persist or log them.
    auto result=transport_(source,video?64*1024*1024:8*1024*1024,cancel);
    if(result.status==429 || result.status==423)backoffSeconds_=std::max(30,result.retryAfter);
    if(result.status==430 || result.status==431){quota_.daily=1;quota_.today=1;}
    return result;
}
}
