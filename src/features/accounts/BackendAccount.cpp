#include "BackendAccount.h"
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QUuid>
#include <QSet>
#include <cstring>
#include <qt6keychain/keychain.h>
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

namespace trainer {
namespace {
const QString Origin="https://traineros-api.abysstail.art";
QString randomSecret() {
    QByteArray value(32,Qt::Uninitialized);
    for(int i=0;i<32;i+=4){auto n=QRandomGenerator::system()->generate();memcpy(value.data()+i,&n,4);}
    return QString::fromLatin1(value.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
}
bool token(const QString& value) {return QRegularExpression("^[A-Za-z0-9_-]{43}$").match(value).hasMatch();}
bool uuid(const QString& value) {return !QUuid(value).isNull()&&value.size()==36;}
bool vaultRunning() {
#ifdef Q_OS_LINUX
    auto* bus=QDBusConnection::sessionBus().interface();
    return bus&&(bus->isServiceRegistered("org.freedesktop.secrets")||bus->isServiceRegistered("org.kde.kwalletd6"));
#else
    return true;
#endif
}
}
BackendAccount::BackendAccount(QObject* parent):QObject(parent) {
    connect(&listener_,&QTcpServer::newConnection,this,&BackendAccount::accept);
    expiry_.setSingleShot(true);refreshTimer_.setSingleShot(true);
    connect(&expiry_,&QTimer::timeout,this,[this]{fail("Sign-in expired. Try again.");});
    connect(&refreshTimer_,&QTimer::timeout,this,&BackendAccount::refresh);
}
BackendAccount::~BackendAccount(){reset();}
void BackendAccount::state(QString value,QString detail){state_=std::move(value);detail_=std::move(detail);emit changed();}
QJsonObject BackendAccount::snapshot() const {
    return {{"state",player_.isEmpty()?"signed_out":"signed_in"},{"player_id",player_}};
}
QString BackendAccount::actionLabel() const {
    if(state_=="waiting"||state_=="opening")return "Cancel TrainerOS sign-in";
    if(state_=="connected")return "Sign out of TrainerOS";
    if(state_=="restoring"||state_=="refreshing"||state_=="claiming"||state_=="signing_out")return "TrainerOS account — please wait";
    return "Sign in to TrainerOS with Fluxer";
}
void BackendAccount::reset() {
    ++epoch_;expiry_.stop();refreshTimer_.stop();listener_.close();encrypted_.cancel();
    for(auto* socket:findChildren<QTcpSocket*>(QString(),Qt::FindDirectChildrenOnly)){socket->abort();socket->deleteLater();}
    attempt_.clear();verifier_.clear();callbackPath_.clear();player_.clear();session_.clear();subject_.clear();access_.clear();refresh_.clear();
    logoutPending_=false;
}
void BackendAccount::configure(const QString& owner) {
    if(owner_==owner)return;
    reset();lock_.reset();owner_=owner;socialSubject_.clear();
    if(owner.isEmpty()){state("disabled","Open your Trainer to sign in");return;}
    key_=QString::fromLatin1(QCryptographicHash::hash(("traineros-backend-v1\n"+Origin+"\n"+owner).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/credentials";
    if(QFileInfo(directory).isSymLink()||!QDir().mkpath(directory)){state("disabled","Secure account storage is unavailable");return;}
    QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    lock_=std::make_shared<QLockFile>(directory+"/backend-"+key_+".lock");lock_->setStaleLockTime(0);
    if(!lock_->tryLock()){state("disabled","This Trainer's account is open in another process");return;}
    encryptedStore_=EncryptedCredentials::available()&&(!vaultRunning()||QFileInfo::exists(EncryptedCredentials::path(key_)));
    state("restoring","Restoring your TrainerOS session…");const auto epoch=epoch_;
    read([this,epoch](bool ok,QByteArray bytes){
        if(epoch!=epoch_)return;
        if(!ok){state("error","Could not open secure storage. Try again.");return;}
        const auto data=QJsonDocument::fromJson(bytes).object();
        if(data["pending"].toBool()||!token(data["refresh_token"].toString())){state("signed_out",data["pending"].toBool()?"Previous sign-in was interrupted. Sign in again.":"Use your Fluxer account — no new password");return;}
        refresh_=data["refresh_token"].toString().toLatin1();
        // Identity is published only after the server verifies this session.
        refresh();
    });
}
void BackendAccount::setSocialSubject(const QString& subject) {
    if(socialSubject_==subject)return;
    socialSubject_=subject;
    if(!subject.isEmpty()&&((!subject_.isEmpty()&&subject_!=subject)||state_=="opening"||state_=="waiting"||state_=="claiming")) {
        if(!access_.isEmpty())request("DELETE","/v1/auth/sessions/"+session_,{},[](int,QJsonObject){},access_);
        fail("Fluxer account changed. Sign in with the same account as Social.");
    }
}
void BackendAccount::activate() {
    if(owner_.isEmpty()||state_=="disabled")return;
    if(state_=="waiting"||state_=="opening"){cancel();return;}
    if(state_=="connected"){logout();return;}
    if(state_=="signed_out"||state_=="error")begin();
}
void BackendAccount::cancel(){reset();state("signed_out","Sign-in cancelled. You can continue offline.");}
void BackendAccount::fail(const QString& message){reset();state("error",message);}
void BackendAccount::begin() {
    if(loginGuard){const auto error=loginGuard();if(!error.isEmpty()){state("error",error);return;}}
    reset();verifier_=randomSecret();callbackPath_="/traineros/auth/"+randomSecret();
    if(!listener_.listen(QHostAddress::LocalHost,0)){fail("Could not prepare browser return. Try again.");return;}
    listener_.setMaxPendingConnections(4);
    state("opening","Opening Fluxer in your browser…");expiry_.start(600000);
    QJsonObject body{{"provider","fluxer"},{"device_label","TrainerOS"},{"claim_challenge_method","S256"},
        {"claim_challenge",QString::fromLatin1(QCryptographicHash::hash(verifier_.toLatin1(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))},
        {"return_uri",QString("http://127.0.0.1:%1%2").arg(listener_.serverPort()).arg(callbackPath_)}};
    if(!socialSubject_.isEmpty())body["expected_subject"]=socialSubject_;
    request("POST","/v1/auth/login-attempts",body,[this](int status,QJsonObject result){
        QUrl url(result["authorization_url"].toString());
        if(status!=201||!uuid(result["attempt_id"].toString())||url.scheme()!="https"||url.host()!="traineros-api.abysstail.art"||url.port(-1)!=-1||!url.userInfo().isEmpty()||url.path()!="/v1/auth/fluxer/start") {fail("Could not start sign-in. Check your connection and try again.");return;}
        if(loginGuard){const auto error=loginGuard();if(!error.isEmpty()){fail(error);return;}}
        attempt_=result["attempt_id"].toString();state("waiting","Finish sign-in in Fluxer. TrainerOS will return automatically.");
        const bool opened=services_.openBrowser?services_.openBrowser(url):QDesktopServices::openUrl(url);
        if(!opened)fail("Could not open the browser. Try again.");
    });
}
void BackendAccount::accept() {
    while(listener_.hasPendingConnections()) {
        auto* socket=listener_.nextPendingConnection();socket->setParent(this);socket->setReadBufferSize(8193);
        if(findChildren<QTcpSocket*>(QString(),Qt::FindDirectChildrenOnly).size()>4){socket->abort();socket->deleteLater();continue;}
        const auto epoch=epoch_;const auto port=listener_.serverPort();
        auto buffer=std::make_shared<QByteArray>();
        QTimer::singleShot(5000,socket,[socket]{socket->abort();socket->deleteLater();});
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket,epoch,port,buffer]{
            buffer->append(socket->readAll());
            if(epoch!=epoch_||buffer->size()>8192){socket->abort();return;}
            if(!buffer->contains("\r\n\r\n"))return;
            const auto lines=buffer->split('\n');const auto first=lines.first().trimmed().split(' ');
            const QByteArray host="127.0.0.1:"+QByteArray::number(port);int hosts=0;bool valid=true;
            for(int i=1;i<lines.size();++i){const auto line=lines[i].trimmed();if(line.toLower().startsWith("host:")){++hosts;valid&=line.mid(5).trimmed()==host;}if(line.toLower().startsWith("content-length:")||line.toLower().startsWith("transfer-encoding:"))valid=false;}
            if(first.size()!=3||first[0]!="GET"||first[2]!="HTTP/1.1"||hosts!=1||!valid){socket->abort();return;}
            QUrl url=QUrl::fromEncoded(first[1],QUrl::StrictMode);QUrlQuery query(url);
            auto items=query.queryItems(QUrl::FullyDecoded);QSet<QString> names;
            for(const auto& item:items)names.insert(item.first);
            const bool hasError=names.contains("error");
            valid=url.isRelative()&&url.path()==callbackPath_&&!url.hasFragment()&&items.size()==2&&names.size()==2&&names.contains("attempt_id")
                &&(names.contains("handoff_token")||hasError)&&query.queryItemValue("attempt_id")==attempt_&&!attempt_.isEmpty()&&state_=="waiting";
            const auto proof=query.queryItemValue("handoff_token");
            valid&=hasError||token(proof);
            if(!valid){socket->abort();return;}
            // Browser gets no credentials, third-party resources or reflected input.
            const QByteArray page="<!doctype html><meta charset=utf-8><title>TrainerOS</title><h1>Return to TrainerOS</h1><p>You can close this window.</p>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nConnection: close\r\nContent-Length: "+QByteArray::number(page.size())+"\r\n\r\n"+page);
            socket->disconnectFromHost();listener_.close();expiry_.stop();emit returnRequested();
            if(hasError){fail("Sign-in was cancelled or could not be completed. Try again.");return;}
            state("claiming","Finishing sign-in…");
            request("POST","/v1/auth/login-attempts/claim",{{"attempt_id",attempt_},{"claim_verifier",verifier_},{"handoff_token",proof}},[this](int status,QJsonObject result){
                if(status!=200){fail("Could not finish sign-in. Start again.");return;}tokens(result);
            });
        });
    }
}
QByteArray BackendAccount::saved(bool pending) const {
    return QJsonDocument(pending?QJsonObject{{"pending",true}}:QJsonObject{{"refresh_token",QString::fromLatin1(refresh_)}}).toJson(QJsonDocument::Compact);
}
void BackendAccount::tokens(const QJsonObject& result) {
    const auto subject=result["fluxer_subject"].toString();
    const auto expires=QDateTime::fromString(result["expires_at"].toString(),Qt::ISODateWithMs);
    if(!uuid(result["player_id"].toString())||!uuid(result["session_id"].toString())||!token(result["access_token"].toString())||!token(result["refresh_token"].toString())
        ||!QRegularExpression("^[0-9]{1,32}$").match(subject).hasMatch()||!expires.isValid()||expires<=QDateTime::currentDateTimeUtc()
        ||(!socialSubject_.isEmpty()&&subject!=socialSubject_)){fail("Account verification failed. Sign in again with your Social account.");return;}
    subject_=subject;session_=result["session_id"].toString();access_=result["access_token"].toString().toLatin1();refresh_=result["refresh_token"].toString().toLatin1();accessExpiry_=expires;
    const auto player=result["player_id"].toString();const auto epoch=epoch_;
    write(saved(),[this,epoch,player](bool ok){
        if(epoch!=epoch_)return;
        if(!ok){request("DELETE","/v1/auth/sessions/"+session_,{},[](int,QJsonObject){},access_);fail("Could not save the session securely. Try again.");return;}
        player_=player;attempt_.clear();verifier_.clear();
        if(logoutPending_){logoutPending_=false;logout();return;}
        state("connected","Connected through Fluxer");
        refreshTimer_.start(int(qBound(qint64(1000),QDateTime::currentDateTimeUtc().msecsTo(accessExpiry_)-60000,qint64(840000))));
    });
}
void BackendAccount::refresh() {
    if(refresh_.isEmpty())return;
    state("refreshing","Checking your TrainerOS session…");const auto epoch=epoch_;const auto credential=refresh_;
    // Durable tombstone BEFORE the one-shot request: no refresh replay after crash/response loss.
    write(saved(true),[this,epoch,credential](bool ok){
        if(epoch!=epoch_)return;
        if(!ok){fail("Could not update secure storage. Sign in again.");return;}
        refresh_.clear();
        request("POST","/v1/auth/refresh",{{"refresh_token",QString::fromLatin1(credential)}},[this](int status,QJsonObject result){
            if(status!=200){fail("Session could not be restored. Sign in again.");return;}tokens(result);
        });
    });
}
void BackendAccount::logout() {
    refreshTimer_.stop();state("signing_out","Signing out…");
    request("DELETE","/v1/auth/sessions/"+session_,{},[this](int status,QJsonObject){
        if(status==401&&!refresh_.isEmpty()){logoutPending_=true;refresh();return;}
        if(status!=204){state("connected","Could not sign out on the server. Try again.");refreshTimer_.start(30000);return;}
        const auto epoch=epoch_;write("{}",[this,epoch](bool ok){if(epoch!=epoch_)return;reset();state("signed_out",ok?"Signed out. Your local Trainer is unchanged.":"Signed out. Saved credential is no longer valid.");});
    },access_);
}
void BackendAccount::request(QByteArray method,QString path,QJsonObject body,Reply done,QByteArray access) {
    const auto epoch=epoch_;auto finish=[this,epoch,path,done=std::move(done)](int status,QJsonObject result){
        if(epoch==epoch_){done(status,std::move(result));return;}
        // A late old-owner grant is never adopted; revoke it if its response arrived.
        if(status==200&&(path.endsWith("/claim")||path=="/v1/auth/refresh")&&uuid(result["session_id"].toString())&&token(result["access_token"].toString()))
            request("DELETE","/v1/auth/sessions/"+result["session_id"].toString(),{},[](int,QJsonObject){},result["access_token"].toString().toLatin1());
    };
    if(services_.request){services_.request(method,path,body,access,std::move(finish));return;}
    QNetworkRequest request(QUrl(Origin+path));request.setRawHeader("User-Agent","TrainerOS-Backend/0.0.1");request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(15000);
    if(!access.isEmpty())request.setRawHeader("Authorization","Bearer "+access);
    if(path=="/v1/auth/login-attempts")request.setRawHeader("Idempotency-Key",QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1());
    auto* reply=network_.sendCustomRequest(request,method,body.isEmpty()?QByteArray():QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(65537);auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>65536)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,bytes,finish=std::move(finish)]{
        bytes->append(reply->readAll());const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok=bytes->size()<=65536&&reply->error()==QNetworkReply::NoError;
        finish(ok?status:(status==401?401:0),ok?QJsonDocument::fromJson(*bytes).object():QJsonObject());reply->deleteLater();
    });
}
void BackendAccount::read(std::function<void(bool,QByteArray)> done) {
    done=[lease=lock_,callback=std::move(done)](bool ok,QByteArray bytes){callback(ok,std::move(bytes));};
    if(services_.read){services_.read(key_,std::move(done));return;}
    if(encryptedStore_){encrypted_.read(key_,std::move(done));return;}
    if(!vaultRunning()){done(false,{});return;}
    auto* job=new QKeychain::ReadPasswordJob("TrainerOS.Backend",this);job->setKey(key_);job->setInsecureFallback(false);
    connect(job,&QKeychain::Job::finished,this,[job,done=std::move(done)]{done(job->error()==QKeychain::NoError||job->error()==QKeychain::EntryNotFound,job->binaryData());});job->start();
}
void BackendAccount::write(QByteArray bytes,std::function<void(bool)> done) {
    // Retain the old owner's lease until an asynchronous vault operation settles.
    // Switching back must not race a delayed write of an already rotated token.
    done=[lease=lock_,callback=std::move(done)](bool ok){callback(ok);};
    if(services_.write){services_.write(key_,std::move(bytes),std::move(done));return;}
    if(encryptedStore_){encrypted_.write(key_,std::move(bytes),[done=std::move(done)](bool ok,QByteArray){done(ok);});return;}
    if(!vaultRunning()){done(false);return;}
    auto* job=new QKeychain::WritePasswordJob("TrainerOS.Backend",this);job->setKey(key_);job->setInsecureFallback(false);job->setBinaryData(bytes);
    connect(job,&QKeychain::Job::finished,this,[job,done=std::move(done)]{done(job->error()==QKeychain::NoError);});job->start();
}
}
