#include "BrowserController.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QClipboard>
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QRegularExpression>
#ifdef TRAINER_WEBENGINE
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QtWebEngineQuick/QQuickWebEngineDownloadRequest>
#endif

namespace trainer {
BrowserController::BrowserController(QObject* parent):QObject(parent) {
    saveTimer_.setSingleShot(true);saveTimer_.setInterval(350);
    connect(&saveTimer_,&QTimer::timeout,this,&BrowserController::persist);
}
BrowserController::~BrowserController(){persist();persistDownloads();delete profile_;}
bool BrowserController::available() const {
#ifdef TRAINER_WEBENGINE
    return true;
#else
    return false;
#endif
}
void BrowserController::configure(const QString& directory,const QString& owner) {
    const QString next=owner.isEmpty()?QString():directory+"/browser/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
    if(next==path_)return;
    persist();persistDownloads();saveTimer_.stop();emit profileChanging();
    delete profile_;profile_=nullptr;field_=nullptr;tabs_.clear();bookmarks_.clear();history_.clear();jobs_.clear();selected_.clear();textTarget_.clear();path_=next;owner_=owner;
    if(!path_.isEmpty()) {
        QDir().mkpath(path_);QFile::setPermissions(path_,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QFile file(path_+"/session.json");
        if(file.open(QIODevice::ReadOnly)) {
            const auto data=QJsonDocument::fromJson(file.readAll()).object();
            tabs_=data["tabs"].toArray().toVariantList();bookmarks_=data["bookmarks"].toArray().toVariantList();history_=data["history"].toArray().toVariantList();selected_=data["selected"].toString();
            // Restore navigable pages only, never replay POST, blob or script URLs.
            for(auto& v:tabs_){auto row=v.toMap();const QUrl url(row["url"].toString());if(url.scheme()!="http"&&url.scheme()!="https")row["url"]="about:blank";v=row;}
        }
        QFile tasks(path_+"/downloads.json");if(tasks.open(QIODevice::ReadOnly))for(const auto& v:QJsonDocument::fromJson(tasks.readAll()).array()) {
            const auto r=v.toObject();Download d;d.id=r["id"].toString();d.name=r["name"].toString();d.path=r["path"].toString();d.state=r["state"].toString();d.received=r["received"].toInteger();d.total=r["total"].toInteger(-1);
            if(d.state=="running"||d.state=="paused"){d.state="error";d.error="Interrupted by restart. Download again from the original page.";}
            jobs_<<d;
        }
    }
    if(tabs_.isEmpty())newTab();
    if(std::none_of(tabs_.begin(),tabs_.end(),[this](const QVariant& v){return v.toMap()["id"]==selected_;}))selected_=tabs_.first().toMap()["id"].toString();
    emit profileChanged();emit changed();emit downloadsChanged();
}
void BrowserController::enter() {
    if(tabs_.isEmpty())newTab();
#ifdef TRAINER_WEBENGINE
    if(!profile_) {
        // Reuse Chromium's bounded range/validator-aware download jobs. Keep
        // authorization inside its network session, never copy cookies to QNAM.
        // This environment override exists only during engine creation and is
        // restored before any app/emulator child can be launched.
        const auto originalFlags=qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        auto flags=QString::fromUtf8(originalFlags);
        if(!flags.contains("ParallelDownloading")) {
            const QRegularExpression option("--enable-features=([^\\s]+)");
            const auto match=option.match(flags);
            if(match.hasMatch())flags.insert(match.capturedEnd(1),",ParallelDownloading");
            else flags+=" --enable-features=ParallelDownloading";
        }
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",flags.toUtf8());
        auto* p=new QQuickWebEngineProfile(this);
        if(originalFlags.isNull())qunsetenv("QTWEBENGINE_CHROMIUM_FLAGS");else qputenv("QTWEBENGINE_CHROMIUM_FLAGS",originalFlags);
        if(!path_.isEmpty()) {
            p->setStorageName("trainer-browser");p->setPersistentStoragePath(path_+"/web");p->setCachePath(path_+"/cache");p->setOffTheRecord(false);
            p->setPersistentCookiesPolicy(QQuickWebEngineProfile::AllowPersistentCookies);
        }
        p->setHttpCacheMaximumSize(64*1024*1024);p->setPushServiceEnabled(false);
        connect(p,&QQuickWebEngineProfile::downloadRequested,this,[this](QQuickWebEngineDownloadRequest* d){acceptDownload(d);});
        profile_=p;emit profileChanged();
    }
#endif
}
QString BrowserController::navigationUrl(QString input) {
    input=input.trimmed();if(input.isEmpty()||input=="about:blank")return "about:blank";
    QUrl url(input);
    if(url.scheme()=="http"||url.scheme()=="https")return url.isValid()&&!url.host().isEmpty()&&url.userInfo().isEmpty()?url.toString():QString();
    if(!url.scheme().isEmpty()&&!input.startsWith("localhost:"))return {};
    if(!input.contains(' ')&&(input.contains('.')||input.startsWith("localhost"))){url=QUrl("https://"+input);if(url.isValid()&&!url.host().isEmpty()&&url.userInfo().isEmpty())return url.toString();}
    return "https://duckduckgo.com/?q="+QString::fromLatin1(QUrl::toPercentEncoding(input));
}
QString BrowserController::newTab(QString url) {
    url=navigationUrl(url);if(url.isEmpty())return {};
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);tabs_<<QVariantMap{{"id",id},{"url",url},{"title","New tab"},{"icon",QString()}};selected_=id;saveTimer_.start();emit changed();return id;
}
void BrowserController::select(const QString& id){for(const auto& v:tabs_)if(v.toMap()["id"]==id){selected_=id;saveTimer_.start();emit changed();return;}}
void BrowserController::closeTab(const QString& id){for(int i=0;i<tabs_.size();++i)if(tabs_[i].toMap()["id"]==id){tabs_.removeAt(i);if(selected_==id)selected_=tabs_.isEmpty()?QString():tabs_[qMin(i,int(tabs_.size())-1)].toMap()["id"].toString();break;}if(tabs_.isEmpty())newTab();saveTimer_.start();emit changed();}
void BrowserController::updateTab(const QString& id,const QString& url,const QString& title,const QString& icon,bool visited){
    for(auto& v:tabs_)if(v.toMap()["id"]==id){auto row=v.toMap();row["url"]=url;row["title"]=title.isEmpty()?"New tab":title;row["icon"]=icon;v=row;break;}
    if(visited&&(url.startsWith("https://")||url.startsWith("http://"))) {
        history_.removeIf([&](const QVariant& v){return v.toMap()["url"]==url;});history_.prepend(QVariantMap{{"url",url},{"title",title},{"at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});while(history_.size()>500)history_.removeLast();
    }
    saveTimer_.start();emit changed();
}
void BrowserController::toggleBookmark(const QString& url,const QString& title){if(!url.startsWith("http://")&&!url.startsWith("https://"))return;bool removed=bookmarks_.removeIf([&](const QVariant& v){return v.toMap()["url"]==url;})>0;if(!removed)bookmarks_<<QVariantMap{{"url",url},{"title",title}};persist();emit changed();}
void BrowserController::removeHistory(){history_.clear();persist();emit changed();}
void BrowserController::persist(){if(path_.isEmpty())return;QSaveFile f(path_+"/session.json");if(f.open(QIODevice::WriteOnly)){f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);f.write(QJsonDocument(QJsonObject{{"tabs",QJsonArray::fromVariantList(tabs_)},{"selected",selected_},{"bookmarks",QJsonArray::fromVariantList(bookmarks_)},{"history",QJsonArray::fromVariantList(history_)}}).toJson());f.commit();}}
void BrowserController::requestAddress(const QString& url){textTarget_="address";emit textRequested("Address or search",url=="about:blank"?QString():url,4096,false);}
void BrowserController::applyText(const QString& text){const auto target=textTarget_;textTarget_.clear();if(target=="address"){const auto url=navigationUrl(text);if(!url.isEmpty())emit navigateRequested(url);}else if(target=="field"&&field_){QInputMethodQueryEvent query(Qt::ImCursorPosition|Qt::ImSurroundingText);QCoreApplication::sendEvent(field_,&query);QInputMethodEvent event;event.setCommitString(text,-query.value(Qt::ImCursorPosition).toInt(),query.value(Qt::ImSurroundingText).toString().size());QCoreApplication::sendEvent(field_,&event);field_=nullptr;}}
void BrowserController::editPageField(){auto* target=QGuiApplication::focusObject();if(!target)return;const auto enabled=QInputMethod::queryFocusObject(Qt::ImEnabled,{}).toBool();if(!enabled)return;field_=target;textTarget_="field";emit textRequested("Enter text",QInputMethod::queryFocusObject(Qt::ImSurroundingText,{}).toString(),8192,QInputMethod::queryFocusObject(Qt::ImHints,{}).toInt()&Qt::ImhHiddenText);}
void BrowserController::copyText(const QString& text){QGuiApplication::clipboard()->setText(text);}
void BrowserController::pointerEvent(QObject* object,qreal x,qreal y,int button){auto* item=qobject_cast<QQuickItem*>(object);if(!item||!item->window())return;const auto local=item->mapToScene(QPointF(x,y));const auto global=item->window()->mapToGlobal(local.toPoint());if(!button){QMouseEvent e(QEvent::MouseMove,local,global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(item->window(),&e);return;}const auto b=button==2?Qt::RightButton:Qt::LeftButton;QMouseEvent press(QEvent::MouseButtonPress,local,global,b,b,Qt::NoModifier);QMouseEvent release(QEvent::MouseButtonRelease,local,global,b,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(item->window(),&press);QCoreApplication::sendEvent(item->window(),&release);}
void BrowserController::wheel(QObject* object,qreal x,qreal y,qreal dx,qreal dy){auto* item=qobject_cast<QQuickItem*>(object);if(!item||!item->window())return;const auto local=item->mapToScene(QPointF(x,y));QWheelEvent e(local,item->window()->mapToGlobal(local.toPoint()),QPoint(int(dx),int(dy)),QPoint(int(dx),int(dy)),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(item->window(),&e);}
void BrowserController::dispatch(Action a){const char* command=nullptr;switch(a){case Action::Up:command="up";break;case Action::Down:command="down";break;case Action::Left:command="left";break;case Action::Right:command="right";break;case Action::Confirm:command="click";break;case Action::ContextMenu:command="context";break;case Action::Back:command="back";break;case Action::LocalAction:command="chrome";break;case Action::ScrollUp:command="scroll-up";break;case Action::ScrollDown:command="scroll-down";break;default:break;}if(command)emit commandRequested(command);}
QVariantList BrowserController::downloads() const {QVariantList rows;for(const auto& d:jobs_){QVariantList actions;
    if(d.request&&(d.state=="running"||d.state=="paused"))actions<<QVariantMap{{"id",d.paused?"resume":"pause"},{"label",d.paused?"Resume":"Pause"},{"enabled",true}};
    if(d.request&&(d.state=="running"||d.state=="paused"||d.state=="error"))actions<<QVariantMap{{"id","cancel"},{"label","Cancel"},{"enabled",true}};
    rows<<QVariantMap{{"id",d.id},{"title",d.name},{"detail",d.error.isEmpty()?"Browser":d.error},{"state",d.state},{"progress",d.total>0?double(d.received)/d.total:0},{"received",d.received},{"total",d.total},{"terminal",d.state=="done"||d.state=="error"||d.state=="cancelled"},{"actions",actions}};}return rows;}
void BrowserController::persistDownloads(){if(path_.isEmpty())return;QJsonArray rows;for(const auto& d:jobs_)rows.append(QJsonObject{{"id",d.id},{"name",d.name},{"path",d.path},{"state",d.state},{"received",d.received},{"total",d.total}});QSaveFile f(path_+"/downloads.json");if(f.open(QIODevice::WriteOnly)){f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);f.write(QJsonDocument(rows).toJson());f.commit();}}
void BrowserController::acceptDownload(QObject* object){
#ifdef TRAINER_WEBENGINE
    auto* request=qobject_cast<QWebEngineDownloadRequest*>(object);if(!request)return;
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);QDir().mkpath(directory);
    QString name=QFileInfo(request->suggestedFileName()).fileName();name.replace('\\','_');if(name.isEmpty()||name=="."||name=="..")name="download";
    const auto original=name;int suffix=1;auto reserved=[&]{return QFile::exists(directory+"/"+name)||std::any_of(jobs_.begin(),jobs_.end(),[&](const Download& d){return d.path==directory+"/"+name;});};while(reserved())name=QFileInfo(original).completeBaseName()+" ("+QString::number(suffix++)+")"+(QFileInfo(original).suffix().isEmpty()?QString():"."+QFileInfo(original).suffix());
    request->setDownloadDirectory(directory);request->setDownloadFileName(name);
    Download d;d.id=QUuid::createUuid().toString(QUuid::WithoutBraces);d.name=name;d.path=directory+"/"+name;d.state="running";d.request=request;jobs_<<d;
    const auto update=[this,request,id=d.id]{for(auto& job:jobs_)if(job.id==id){job.received=request->receivedBytes();job.total=request->totalBytes();job.paused=request->isPaused();switch(request->state()){case QWebEngineDownloadRequest::DownloadCompleted:job.state="done";break;case QWebEngineDownloadRequest::DownloadCancelled:job.state="cancelled";break;case QWebEngineDownloadRequest::DownloadInterrupted:job.state="error";job.error=request->interruptReasonString();break;default:job.state=job.paused?"paused":"running";break;}break;}persistDownloads();emit downloadsChanged();};
    connect(request,&QWebEngineDownloadRequest::receivedBytesChanged,this,update);connect(request,&QWebEngineDownloadRequest::totalBytesChanged,this,update);connect(request,&QWebEngineDownloadRequest::stateChanged,this,update);connect(request,&QWebEngineDownloadRequest::isPausedChanged,this,update);
    request->accept();update();
#else
    Q_UNUSED(object);
#endif
}
void BrowserController::controlDownload(const QString& id,const QString& command){
#ifdef TRAINER_WEBENGINE
    for(auto& d:jobs_)if(id.isEmpty()||d.id==id){auto* request=qobject_cast<QWebEngineDownloadRequest*>(d.request.data());if(!request)continue;if(command=="cancel"||command=="cancel-all")request->cancel();else if(command=="pause"||command=="pause-all")request->pause();else if(command=="resume"||command=="resume-all")request->resume();}
#else
    Q_UNUSED(id);Q_UNUSED(command);
#endif
}
}
