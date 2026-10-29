#include "BackendAccount.h"
#include "platform/storage/KeychainRequest.h"
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
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonArray>
#include <cmath>
#include <utility>
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
    connect(this,&BackendAccount::changed,this,[this]{QTimer::singleShot(0,this,&BackendAccount::flushOutfits);});
    progressionTimer_.setInterval(120000);
    connect(&progressionTimer_,&QTimer::timeout,this,[this]{refreshProgression();});
    connect(&listener_,&QTcpServer::newConnection,this,&BackendAccount::accept);
    expiry_.setSingleShot(true);refreshTimer_.setSingleShot(true);
    connect(&expiry_,&QTimer::timeout,this,[this]{if(supportPending_)finishSupport("Patreon connection expired. Try again.");else fail("Sign-in expired. Try again.");});
    connect(&refreshTimer_,&QTimer::timeout,this,&BackendAccount::refresh);
    avatarRetry_.setSingleShot(true);
    connect(&avatarRetry_,&QTimer::timeout,this,&BackendAccount::syncPublicAvatar);
}
BackendAccount::~BackendAccount(){reset();}
QVariantMap BackendAccount::progression() const {
    auto result=progression_.toVariantMap();
    result["known"]=!progression_.isEmpty();result["stale"]=progressionStale_;result["busy"]=rewardBusy_;
    // A pending debit can already have settled on the server. Do not advertise
    // a prospective balance until its receipt and the wallet are reconciled.
    if(progressionStale_ || rewardBusy_ || !shopPending_.isEmpty())result["support_preview"]=QVariantMap{};
    return result;
}
QString BackendAccount::rewardActionLabel() const {
    if(rewardBusy_)return "Checking rewards…";
    if(!progression_["link"].toObject().isEmpty())return "Check earned rewards";
    if(!progression_["challenge"].toObject().isEmpty())return "Verify RA profile code";
    return "Enable RA rewards";
}
bool BackendAccount::adoptProgression(const QJsonObject& value) {
    auto integer=[](QJsonValue v,double minimum){return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble()>=minimum && v.toDouble()<=9007199254740991.0 && std::floor(v.toDouble())==v.toDouble();};
    const auto mastery=value["mastery"].toObject();
    if(player_.isEmpty() || value["player_id"].toString()!=player_ || !integer(value["balance"],0)
        || !integer(mastery["xp"],0) || !integer(mastery["level"],1) || !integer(mastery["level_xp"],0)
        || !integer(mastery["next_level_xp"],1) || mastery["level_xp"].toDouble()>=mastery["next_level_xp"].toDouble()
        || !value["history"].isArray() || value["history"].toArray().size()>50
        || !QDateTime::fromString(value["checked_at"].toString(),Qt::ISODateWithMs).isValid())return false;
    bool revisionOk=false;const auto revision=value["revision"].toString().toULongLong(&revisionOk);
    if(!revisionOk || (!progression_.isEmpty() && revision<progression_["revision"].toString().toULongLong()))return false;
    for(const auto item:value["history"].toArray()) {
        const auto row=item.toObject();
        if(!uuid(row["id"].toString()) || !integer(row["xp"],0) || !integer(row["sparks"],row["source"]=="shop.purchase"?-2147483647:0)
            || row["title"].toString().size()>256 || !QStringList{"ra.achievement","ra.completion","shop.purchase","support.topup","operator.test_grant"}.contains(row["source"].toString()))return false;
        if(row["source"]=="operator.test_grant" && (row["xp"].toDouble()!=0 || row["sparks"].toDouble()<=0 || row["sparks"].toDouble()>1000000))return false;
        if(row["source"]=="support.topup" && (row["xp"].toDouble()!=0 || row["sparks"].toDouble()<=0
            || !integer(row["support_multiplier"],2) || row["support_multiplier"].toInt()>3))return false;
    }
    const bool had=!progression_.isEmpty();
    const auto xpDelta=qint64(mastery["xp"].toDouble()-progression_["mastery"].toObject()["xp"].toDouble());
    const auto sparkDelta=qint64(value["balance"].toDouble()-progression_["balance"].toDouble());
    auto sanitized=value;
    const auto preview=value["support_preview"].toObject();
    bool previewValid=preview["policy"]=="support-preview-2026-10-v1" && preview["state"]=="calculation_only"
        && preview["activation_available"].isBool() && !preview["activation_available"].toBool()
        && integer(preview["eligible_rewards"],0) && integer(preview["base_sparks"],0)
        && integer(preview["credited_reward_sparks"],0) && preview["options"].isArray()
        && preview["options"].toArray().size()==2;
    QSet<int> multipliers;
    for(const auto option:preview["options"].toArray()) {
        const auto row=option.toObject();const int multiplier=row["multiplier"].toInt();
        const auto credited=preview["credited_reward_sparks"].toDouble(),base=preview["base_sparks"].toDouble();
        previewValid &= integer(row["multiplier"],2) && multiplier<=3 && !multipliers.contains(multiplier)
            && row["id"]=="support-x"+QString::number(multiplier)
            && !row["title"].toString().isEmpty() && row["title"].toString().size()<=64
            && credited>=base && credited<=3*base
            && integer(row["top_up"],0) && row["top_up"].toDouble()<=base*(multiplier-1)
            && integer(row["potential_balance"],0)
            && row["potential_balance"].toDouble()==value["balance"].toDouble()+row["top_up"].toDouble();
        multipliers.insert(multiplier);
    }
    if(!previewValid)sanitized["support_preview"]=QJsonValue();
    const auto support=value["support"].toObject();
    if(!integer(support["multiplier"],1) || support["multiplier"].toInt()>3
        || (support["multiplier"].toInt()>1 && !QDateTime::fromString(support["verified_until"].toString(),Qt::ISODateWithMs).isValid()))
        sanitized["support"]=QJsonValue();
    progression_=sanitized;progressionStale_=false;
    if(had && xpDelta>=0 && sparkDelta>0)emit rewardsEarned(xpDelta,sparkDelta);
    return true;
}
void BackendAccount::rewardRequest(QString action,QJsonObject body,bool sync) {
    if(player_.isEmpty() || state_!="connected" || rewardBusy_)return;
    rewardBusy_=true;emit changed();
    const auto path="/v1/me/progression"+(action.isEmpty()?QString():"/ra/"+action);
    request(action.isEmpty()?"GET":"POST",path,body,[this,action,sync](int status,QJsonObject result){
        rewardBusy_=false;
        if(status==200 && adoptProgression(result)) {
            refreshPrizes(); if(action=="sync")refreshShop();
            rewardStatus_=action=="verify"?"RA linked. New verified achievements now earn XP and Sparks. You can remove the code from your RA motto.":"Account progress is up to date";
            emit changed();
            if(sync && !progression_["link"].toObject().isEmpty()) {
                const auto now=QDateTime::currentMSecsSinceEpoch();
                if(now-lastRewardSync_<120000)return;
                auto source=rewardSource?rewardSource():QJsonObject{};
                if(source["username"].toString().compare(progression_["link"].toObject()["username"].toString(),Qt::CaseInsensitive)!=0)return;
                source.remove("username");
                if(source["web_key"].toString().isEmpty())return;
                lastRewardSync_=now;rewardRequest("sync",source);
            }
            return;
        }
        progressionStale_=!progression_.isEmpty();
        const auto code=result["code"].toString();
        if(code=="ra_challenge_missing")rewardStatus_="Add the exact code to your RA profile motto, save it, then verify again.";
        else if(code=="ra_already_linked")rewardStatus_="This RA profile is already bound to a TrainerOS account. Existing rewards stay with their owner.";
        else if(code=="ra_challenge_expired")rewardStatus_="The link code expired. Refresh account progress and start again.";
        else if(status==429)rewardStatus_="Please wait a few seconds before checking again.";
        else if(code=="ra_account_ineligible")rewardStatus_="This RA account is not eligible for rewards. Local play is unchanged.";
        else rewardStatus_="Could not verify account progress. Retry when connected; earned rewards remain on the server.";
        emit changed();
        if(code=="ra_challenge_expired")refreshProgression(false);
    },access_);
}
void BackendAccount::refreshProgression(bool sync) { rewardRequest({}, {},sync); }
void BackendAccount::activateRewards() {
    if(player_.isEmpty() || state_!="connected" || rewardBusy_)return;
    auto source=rewardSource?rewardSource():QJsonObject{};
    if(source["web_key"].toString().isEmpty()) {
        rewardStatus_="Open RetroAchievements account settings and add your own Web API key to enable rewards.";emit changed();return;
    }
    if(progression_.isEmpty()){refreshProgression(false);return;}
    if(!progression_["link"].toObject().isEmpty()) {
        if(source["username"].toString().compare(progression_["link"].toObject()["username"].toString(),Qt::CaseInsensitive)!=0){rewardStatus_="The current RA account differs from the linked rewards profile. Select the linked RA account first.";emit changed();return;}
        source.remove("username");
        lastRewardSync_=QDateTime::currentMSecsSinceEpoch();rewardRequest("sync",source);
    } else {
        source.remove("games");
        if(!progression_["challenge"].toObject().isEmpty()){source.remove("username");rewardRequest("verify",source);}
        else rewardRequest("challenge",source);
    }
}
void BackendAccount::copyRewardChallenge() {
    const auto code=progression_["challenge"].toObject()["challenge"].toString();
    if(!code.isEmpty() && QGuiApplication::clipboard()){QGuiApplication::clipboard()->setText(code);rewardStatus_="Code copied. Add it to your RA profile motto and save.";emit changed();}
}
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
void BackendAccount::viewProfile(QString player,QString subject,QString before,Reply done) {
    if(state_!="connected"||player_.isEmpty()){done(401,{});return;}
    QString path;
    if(!subject.isEmpty() && QRegularExpression("^[0-9]{1,32}$").match(subject).hasMatch())path="/v1/profiles/fluxer/"+subject;
    else if(subject.isEmpty() && !QUuid(player).isNull())path="/v1/profiles/"+QUuid(player).toString(QUuid::WithoutBraces);
    else {done(422,{});return;}
    if(!before.isEmpty()) {if(!QRegularExpression("^[1-9][0-9]{0,18}$").match(before).hasMatch()){done(422,{});return;}path+="?before="+before;}
    request("GET",path,{},std::move(done),access_);
}
void BackendAccount::profileSharing(QJsonObject update,Reply done) {
    if(state_!="connected"||player_.isEmpty()){done(401,{});return;}
    request(update.isEmpty()?"GET":"PUT","/v1/me/profile/sharing",update,std::move(done),access_);
}
void BackendAccount::publishProfileFavorites(QJsonArray games,QString revision,Reply done) {
    if(state_!="connected"||player_.isEmpty()){done(401,{});return;}
    request("PUT","/v1/me/profile/favorites",{{"games",games},{"revision",revision}},std::move(done),access_);
}
void BackendAccount::reset() {
    ++supportGeneration_;support_={};supportClaim_={};supportMessage_.clear();supportBusy_=false;supportPending_=false;supportReview_=false;
    prizeBusy_=false;prizeStale_=true;celebrationIds_={};celebrationGroups_={};
    shopBusy_=false;shopReview_={};shopStale_=true;outfitFlushing_=false;
    progressionTimer_.stop();progression_={};rewardStatus_.clear();rewardBusy_=false;progressionStale_=false;lastRewardSync_=0;
    avatarRetry_.stop();publicAvatar_={};publishedAvatar_={};avatarBusy_=false;
    const auto channels=playGrants_.keys();for(const auto& channel:channels)closePlay(channel);
    ++epoch_;expiry_.stop();refreshTimer_.stop();listener_.close();encrypted_.cancel();
    for(auto* socket:findChildren<QTcpSocket*>(QString(),Qt::FindDirectChildrenOnly)){socket->abort();socket->deleteLater();}
    attempt_.clear();verifier_.clear();callbackPath_.clear();player_.clear();session_.clear();subject_.clear();access_.clear();refresh_.clear();
    logoutPending_=false;
}
void BackendAccount::configure(const QString& owner) {
    if(owner_==owner)return;
    reset();lock_.reset();shopSnapshot_={};shopPending_={};prizeSnapshot_={};celebrationIds_={};prizeIndex_=0;wardrobeState_={};outfitWrites_={};outfitFlushing_=false;shopStatus_.clear();shopIndex_=0;owner_=owner;socialSubject_.clear();
    if(owner.isEmpty()){state("disabled","Open your Trainer to sign in");return;}
    key_=QString::fromLatin1(QCryptographicHash::hash(("traineros-backend-v1\n"+Origin+"\n"+owner).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/credentials";
    if(QFileInfo(directory).isSymLink()||!QDir().mkpath(directory)){state("disabled","Secure account storage is unavailable");return;}
    QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    lock_=std::make_shared<QLockFile>(directory+"/backend-"+key_+".lock");lock_->setStaleLockTime(0);
    if(!lock_->tryLock()){state("disabled","This Trainer's account is open in another process");return;}
    loadCommerce();
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
QVariantMap BackendAccount::support() const {
    auto value=support_.toVariantMap();value["busy"]=supportBusy_;value["pending"]=supportPending_;
    value["review"]=supportReview_;value["message"]=supportMessage_;return value;
}
void BackendAccount::finishSupport(QString message) {
    ++supportGeneration_;supportClaim_={};supportBusy_=false;supportPending_=false;supportReview_=false;
    listener_.close();expiry_.stop();attempt_.clear();verifier_.clear();callbackPath_.clear();
    supportMessage_=std::move(message);emit changed();
}
void BackendAccount::supportRequest(QByteArray method,QString path,QJsonObject body) {
    if(state_!="connected"||supportBusy_)return;
    supportBusy_=true;supportReview_=false;supportMessage_="Checking Patreon…";emit changed();
    const auto generation=supportGeneration_;
    request(method,path,body,[this,generation](int status,QJsonObject value){
        if(generation!=supportGeneration_)return;
        supportBusy_=false;
        const auto linked=value["state"].toString();
        if(status==200 && value["provider"]=="patreon" && value["available"].isBool()
            && QStringList{"unlinked","connected","disconnected","needs_relink"}.contains(linked)
            && value["multiplier"].isDouble() && value["multiplier"].toDouble()==value["multiplier"].toInt()
            && value["multiplier"].toInt()>=1 && value["multiplier"].toInt()<=3) {
            support_=value;
            supportMessage_=!value["available"].toBool()?"Patreon activation is being prepared."
                :linked=="connected"?"Patreon connected · current rate ×"+QString::number(value["multiplier"].toInt())
                :linked=="needs_relink"?"Reconnect Patreon to refresh your membership."
                :"Connect Patreon to verify your supporter level.";
            refreshProgression(false);
        }else {
            const auto code=value["code"].toString();
            supportMessage_=code=="support_identity_conflict"?"This Patreon account is linked to another Trainer, or differs from your previous link."
                :code=="support_check_pending"?"A membership check is already in progress. Try again shortly."
                :"Could not verify Patreon. Try again when connected.";
        }
        emit changed();
    },access_);
}
void BackendAccount::refreshSupport() {supportRequest("GET","/v1/me/support");}
void BackendAccount::activateSupport(int action) {
    if(state_!="connected")return;
    if(action==0) {
        if(supportPending_){finishSupport("Patreon connection cancelled.");return;}
        if(supportBusy_)return;
        if(support_["state"]=="connected")supportRequest("POST","/v1/me/support/patreon/check");
        else if(support_["available"].toBool())beginSupport();
        else refreshSupport();
    }else if(action==1 && !supportBusy_) {
        if(loginGuard && !loginGuard().isEmpty()){supportMessage_=loginGuard();emit changed();return;}
        const QUrl url("https://www.patreon.com/AbyssTail/membership");
        if(!(services_.openBrowser?services_.openBrowser(url):QDesktopServices::openUrl(url))) {
            supportMessage_="Could not open Patreon.";emit changed();
        }
    }else if(action==2 && !supportBusy_ && (support_["state"]=="connected"||support_["state"]=="needs_relink")) {
        if(supportReview_)supportRequest("DELETE","/v1/me/support/patreon");
        else {supportReview_=true;supportMessage_="Disconnect TrainerOS? This does not cancel billing on Patreon. Earned Sparks and purchases stay yours.";emit changed();}
    }else if(action==3){supportReview_=false;emit changed();}
}
void BackendAccount::beginSupport() {
    if(loginGuard){const auto error=loginGuard();if(!error.isEmpty()){supportMessage_=error;emit changed();return;}}
    ++supportGeneration_;const auto generation=supportGeneration_;
    verifier_=randomSecret();callbackPath_="/traineros/auth/"+randomSecret();
    if(!listener_.listen(QHostAddress::LocalHost,0)){finishSupport("Could not prepare browser return.");return;}
    listener_.setMaxPendingConnections(4);supportBusy_=true;supportPending_=true;supportReview_=false;
    supportMessage_="Opening Patreon…";expiry_.start(600000);emit changed();
    QJsonObject body{{"claim_challenge",QString::fromLatin1(QCryptographicHash::hash(verifier_.toLatin1(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))},
        {"return_uri",QString("http://127.0.0.1:%1%2").arg(listener_.serverPort()).arg(callbackPath_)}};
    request("POST","/v1/me/support/patreon/attempts",body,[this,generation](int status,QJsonObject result){
        if(generation!=supportGeneration_)return;
        const QUrl url(result["authorization_url"].toString());
        if(status!=201||!uuid(result["attempt_id"].toString())||url.scheme()!="https"||url.host()!="traineros-api.abysstail.art"||url.port(-1)!=-1||!url.userInfo().isEmpty()||url.hasFragment()||url.path()!="/v1/support/patreon/start") {
            finishSupport("Could not start Patreon connection. Try again.");return;
        }
        if(loginGuard){const auto error=loginGuard();if(!error.isEmpty()){finishSupport(error);return;}}
        attempt_=result["attempt_id"].toString();supportMessage_="Confirm in Patreon. You will return here automatically.";emit changed();
        if(!(services_.openBrowser?services_.openBrowser(url):QDesktopServices::openUrl(url)))finishSupport("Could not open Patreon.");
    },access_);
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
                &&(names.contains("handoff_token")||hasError)&&query.queryItemValue("attempt_id")==attempt_&&!attempt_.isEmpty()&&(state_=="waiting"||supportPending_);
            const auto proof=query.queryItemValue("handoff_token");
            valid&=hasError||token(proof);
            if(!valid){socket->abort();return;}
            // Browser gets no credentials, third-party resources or reflected input.
            const QByteArray page="<!doctype html><meta charset=utf-8><title>TrainerOS</title><h1>Return to TrainerOS</h1><p>You can close this window.</p>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nConnection: close\r\nContent-Length: "+QByteArray::number(page.size())+"\r\n\r\n"+page);
            socket->disconnectFromHost();listener_.close();expiry_.stop();emit returnRequested();
            if(supportPending_) {
                if(hasError){finishSupport("Patreon connection was cancelled or could not be completed.");return;}
                const QJsonObject body{{"attempt_id",attempt_},{"claim_verifier",verifier_},{"handoff_token",proof}};
                supportPending_=false;supportBusy_=false;attempt_.clear();verifier_.clear();
                if(state_=="refreshing") {
                    supportClaim_=body;supportBusy_=true;supportMessage_="Finishing your connection…";emit changed();
                }else supportRequest("POST","/v1/me/support/patreon/claim",body);
                return;
            }
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
        player_=player;if(!supportPending_){attempt_.clear();verifier_.clear();}
        if(!prizeSnapshot_.isEmpty()&&prizeSnapshot_["player_id"].toString()!=player_){prizeSnapshot_={};saveCommerce();}
        if(!shopSnapshot_.isEmpty()&&shopSnapshot_["player_id"].toString()!=player_){shopSnapshot_={};shopIndex_=0;saveCommerce();}
        if(logoutPending_){logoutPending_=false;logout();return;}
        state("connected","Connected through Fluxer");
        if(!supportClaim_.isEmpty()) {
            const auto body=std::exchange(supportClaim_,QJsonObject{});supportBusy_=false;
            supportRequest("POST","/v1/me/support/patreon/claim",body);
        }
        syncPublicAvatar();
        progressionTimer_.start();refreshProgression();refreshShop();
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
        const auto epoch=epoch_;write("{}",[this,epoch](bool ok){if(epoch!=epoch_)return;shopSnapshot_={};saveCommerce();reset();state("signed_out",ok?"Signed out. Your local Trainer is unchanged.":"Signed out. Saved credential is no longer valid.");});
    },access_);
}
void BackendAccount::request(QByteArray method,QString path,QJsonObject body,Reply done,QByteArray access) {
    const auto epoch=epoch_;auto finish=[this,epoch,path,done=std::move(done)](int status,QJsonObject result){
        if(epoch==epoch_){done(status,std::move(result));return;}
        // A late old-owner grant is never adopted; revoke it if its response arrived.
        if(status==200&&(path.endsWith("/claim")||path=="/v1/auth/refresh")&&uuid(result["session_id"].toString())&&token(result["access_token"].toString()))
            request("DELETE","/v1/auth/sessions/"+result["session_id"].toString(),{},[](int,QJsonObject){},result["access_token"].toString().toLatin1());
        if(status==200&&path=="/v1/play/grants"&&token(result["grant_token"].toString()))
            request("POST","/v1/play/commands",{{"action","revoke"},{"operation_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"payload",QJsonObject{}}},[](int,QJsonObject){},result["grant_token"].toString().toLatin1());
    };
    if(services_.request){services_.request(method,path,body,access,std::move(finish));return;}
    QNetworkRequest request(QUrl(Origin+path));request.setRawHeader("User-Agent","TrainerOS-Backend/0.0.1");request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(path.endsWith("/progression/ra/sync")?80000:15000);
    if(!access.isEmpty())request.setRawHeader("Authorization","Bearer "+access);
    if(path=="/v1/auth/login-attempts")request.setRawHeader("Idempotency-Key",QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1());
    auto* reply=network_.sendCustomRequest(request,method,body.isEmpty()?QByteArray():QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(65537);auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>2097152)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,bytes,finish=std::move(finish)]{
        bytes->append(reply->readAll());const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok=bytes->size()<=2097152&&(reply->error()==QNetworkReply::NoError||(status>=400&&status<500));
        finish(ok?status:0,ok?QJsonDocument::fromJson(*bytes).object():QJsonObject());reply->deleteLater();
    });
}
void BackendAccount::closePlay(const QString& channel) {
    const auto grant=playGrants_.take(channel);
    if(grant.token.isEmpty())return;
    request("POST","/v1/play/commands",{{"action","revoke"},{"operation_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"payload",QJsonObject{}}},[](int,QJsonObject){},grant.token);
}
void BackendAccount::setPublicAvatar(const QString& owner,const QJsonObject& recipe) {
    if(owner!=owner_)return;
    publicAvatar_=recipe;
    syncPublicAvatar();
}
void BackendAccount::syncPublicAvatar() {
    // Missing/uninstalled assets are not a request to erase the online appearance.
    if(state_!="connected"||avatarBusy_||avatarRetry_.isActive()||publicAvatar_.isEmpty()||publicAvatar_==publishedAvatar_)return;
    const auto recipe=publicAvatar_;avatarBusy_=true;
    request("PUT","/v1/me/profile/avatar",{{"avatar",recipe}},[this,recipe](int status,QJsonObject){
        avatarBusy_=false;
        if(status==200){publishedAvatar_=recipe;syncPublicAvatar();}
        else avatarRetry_.start(60000);
    },access_);
}
void BackendAccount::playRequest(const QString& channel,const QString& app,const QString& method,QJsonObject args,Reply done) {
    auto reject=[&](const QString& code){done(409,{{"code",code}});};
    static const QRegularExpression id("^[a-z][a-z0-9_.-]{0,63}$");
    if(state_!="connected"||access_.isEmpty()){reject(player_.isEmpty()?"sign_in_required":"account_busy");return;}
    if(QJsonDocument(args).toJson(QJsonDocument::Compact).size()>16000){reject("body_too_large");return;}
    if(method=="play.rules") {
        if(!args.isEmpty()){reject("invalid_request");return;}
        request("GET","/v1/games",{},std::move(done),access_);return;
    }
    if(method=="play.open") {
        const auto rule=args["ruleset_id"].toString();
        if(args.size()!=1||!id.match(rule).hasMatch()){reject("invalid_request");return;}
        closePlay(channel);playGrants_.insert(channel,{});
        request("POST","/v1/play/grants",{{"app_id",app},{"ruleset_id",rule},{"operation_id",QUuid::createUuid().toString(QUuid::WithoutBraces)}},
            [this,channel,rule,done=std::move(done)](int status,QJsonObject result){
                const auto credential=result["grant_token"].toString().toLatin1();
                if(!playGrants_.contains(channel)) {
                    if(token(QString::fromLatin1(credential)))request("POST","/v1/play/commands",{{"action","revoke"},{"operation_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"payload",QJsonObject{}}},[](int,QJsonObject){},credential);
                    return;
                }
                if(status!=200||!token(QString::fromLatin1(credential))){playGrants_.remove(channel);done(status,{{"code",result["code"].toString("backend_unavailable")}});return;}
                playGrants_[channel]={credential,rule,QDateTime::fromString(result["expires_at"].toString(),Qt::ISODateWithMs)};
                done(200,{{"ruleset",result["ruleset"]},{"trust",result["trust"]},{"expires_at",result["expires_at"]}});
            },access_);return;
    }
    if(!playGrants_.contains(channel)||playGrants_[channel].token.isEmpty()){reject("play_not_open");return;}
    const auto grant=playGrants_[channel];
    if(grant.expires<=QDateTime::currentDateTimeUtc()){closePlay(channel);reject("grant_expired");return;}
    if(method=="play.board") {
        const auto mode=args["mode"].toString(),kind=args["kind"].toString();
        if((mode!="individual"&&mode!="teams")||(kind!="records"&&kind!="wins")){reject("invalid_request");return;}
        const QSet<QString> allowed{"mode","kind","offset","limit","revision","around"};
        QUrlQuery query;
        for(auto i=args.begin();i!=args.end();++i){if(!allowed.contains(i.key())){reject("invalid_request");return;}if(i.key()!="mode"&&i.key()!="kind")query.addQueryItem(i.key(),i.value().toVariant().toString());}
        request("GET","/v1/boards/"+grant.ruleset+"/"+mode+"/"+kind+"?"+query.toString(QUrl::FullyEncoded),{},std::move(done),grant.token);return;
    }
    static const QSet<QString> actions{"create","join","start","cancel","submit","get","history","stats"};
    const auto action=method.mid(5);
    if(!method.startsWith("play.")||!actions.contains(action)||!uuid(args["operation_id"].toString())||!args["payload"].isObject()||args.size()!=2){reject("invalid_request");return;}
    args["action"]=action;
    request("POST","/v1/play/commands",args,[done=std::move(done)](int status,QJsonObject result){done(status,std::move(result));},grant.token);
}
void BackendAccount::read(std::function<void(bool,QByteArray)> done) {
    done=[lease=lock_,callback=std::move(done)](bool ok,QByteArray bytes){callback(ok,std::move(bytes));};
    if(services_.read){services_.read(key_,std::move(done));return;}
    if(encryptedStore_){encrypted_.read(key_,std::move(done));return;}
    if(!vaultRunning()){done(false,{});return;}
    KeychainRequest::run("TrainerOS.Backend",key_,KeychainRequest::Read,{},this,
        [done=std::move(done)](int error,QByteArray bytes){done(error==QKeychain::NoError||error==QKeychain::EntryNotFound,std::move(bytes));});
}
void BackendAccount::write(QByteArray bytes,std::function<void(bool)> done) {
    // Retain the old owner's lease until an asynchronous vault operation settles.
    // Switching back must not race a delayed write of an already rotated token.
    done=[lease=lock_,callback=std::move(done)](bool ok){callback(ok);};
    if(services_.write){services_.write(key_,std::move(bytes),std::move(done));return;}
    if(encryptedStore_){encrypted_.write(key_,std::move(bytes),[done=std::move(done)](bool ok,QByteArray){done(ok);});return;}
    if(!vaultRunning()){done(false);return;}
    KeychainRequest::run("TrainerOS.Backend",key_,KeychainRequest::Write,std::move(bytes),this,
        [done=std::move(done)](int error,QByteArray){done(error==QKeychain::NoError);});
}
}
