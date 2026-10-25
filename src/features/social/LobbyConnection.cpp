#include "LobbyConnection.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QUuid>
#include <QDir>

namespace trainer {
namespace {
bool candidates(const QJsonArray& batch,int previous) {
    if(batch.isEmpty()||batch.size()+previous>32)return false;
    for(const auto& entry:batch){const auto c=entry.toObject();const auto text=c["candidate"].toString(),mid=c["mid"].toString();
        if(c.size()!=2||text.isEmpty()||text.size()>512||text.contains('\n')||text.contains('\r')||mid.isEmpty()||mid.size()>32)return false;
    }
    return true;
}
QJsonObject binding(const QJsonObject& s,const QString& guest) {
    QJsonArray players;
    for(const auto& v:s["members"].toArray()) {const auto m=v.toObject();if(!m["spectator"].toBool()&&(m["id"]==s["owner"]||m["id"]==guest))players.append(QJsonObject{
        {"id",m["id"]},{"seat",m["seat"]},{"connected",m["connected"]},{"admission",m["admission"]}});}
    QJsonObject result{{"lobby",s["id"]},{"generation",s["generation"]},{"owner",s["owner"]},{"game",s["game"]},{"ticket",s["ticket"]},{"players",players}};
    // Preserve the existing Remote Play identity contract with older clients.
    if(s["relay"]=="psp-host")result["relay"]="psp-host";
    return result;
}
}
LobbyConnection::LobbyConnection(LobbyCoordinator& lobby,QObject* parent):QObject(parent),lobby_(lobby) {
    connect(&lobby_,&LobbyCoordinator::changed,this,&LobbyConnection::reconcile);
    connect(&lobby_,&LobbyCoordinator::runtimeSignal,this,&LobbyConnection::receive);
    connect(&process_,&QProcess::readyReadStandardOutput,this,&LobbyConnection::read);
    connect(&process_,&QProcess::readyReadStandardError,this,[this]{process_.readAllStandardError();});
    connect(&process_,&QProcess::started,this,[this]{
        if(!matches()){fail("The lobby game changed.");return;}
        write({{"host",host_},{"identity",identity_},{"seconds",0},{"ice",ice_},{"mode",mode_}});
        initialized_=true;
        if(!pendingDescription_.isEmpty()){auto packet=pendingDescription_;pendingDescription_={};write(packet);}
        if(!pendingCandidates_.isEmpty()){auto batch=pendingCandidates_;pendingCandidates_={};write({{"identity",identity_},{"candidates",batch}});}
    });
    connect(&process_,&QProcess::errorOccurred,this,[this](auto error){if(error==QProcess::FailedToStart&&!stopping_)fail("Remote Play transport is unavailable.");});
    connect(&process_,&QProcess::finished,this,[this](int,QProcess::ExitStatus){if(!stopping_&&active())fail(ready_?"The Remote Play connection ended. Your game is kept.":"Couldn't connect directly on these networks. Try another network.");});
    deadline_.setSingleShot(true);deadline_.setInterval(90000);
    connect(&deadline_,&QTimer::timeout,this,[this]{fail("Couldn't connect directly on these networks. Try another network.");});
}
LobbyConnection::~LobbyConnection() {stop();}
bool LobbyConnection::matches() const {
    const auto s=lobby_.state();return active()&&!lobby_.recoveryUncertain()&&!s["recovering"].toBool()
        &&s["connection"].toString().isEmpty()&&(s["phase"]=="launching"||s["phase"]=="playing")&&binding(s,guest_)==context_;
}
bool LobbyConnection::start(QString self,QString ticket,QString helper,QJsonArray ice,QString mode,QString selectedGuest) {
    if(active()||process_.state()!=QProcess::NotRunning||!QFileInfo(helper).isExecutable())return false;
    const auto s=lobby_.state();const auto game=s["game"].toObject();
    const int capacity=game["seats"].toInt();
    const bool live=s["phase"]=="playing";
    const bool psp=mode=="psp"&&game["mode"].toString().startsWith("runtime.ppsspp.")&&s["relay"]=="psp-host"&&!live;
    const bool bba=mode=="bba"&&game["mode"]=="runtime.dolphin.bba.v1"&&s["relay"].toString("auto")=="auto"&&!live;
    const bool ps2=mode=="ps2"&&game["mode"]=="runtime.armsx2.lan.v1"&&s["relay"].toString("auto")=="auto"&&!live;
    if((s["phase"]!="launching"&&!live)||s["ticket"]!=ticket||ticket.isEmpty()
       ||(!(mode=="game"&&game["mode"]=="remoteplay.v1")&&!psp&&!ps2&&!bba)||capacity<2||capacity>(mode=="game"?4:2))return false;
    mode_=mode;
    if(self!=s["owner"])selectedGuest=self;
    if(selectedGuest.isEmpty())for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();if(!m["spectator"].toBool()&&m["id"]!=s["owner"]) {
            if(!selectedGuest.isEmpty())return false;
            selectedGuest=m["id"].toString();
        }
    }
    int players=0;QString owner,guest,admission;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();if(m["id"]!=s["owner"]&&m["id"]!=selectedGuest)continue;
        if(m["spectator"].toBool())return false;
        ++players;
        if(!m["connected"].toBool())return false;
        if(m["seat"]==1&&m["id"]==s["owner"])owner=m["id"].toString();
        else if(m["seat"].toInt()>=2&&m["seat"].toInt()<=capacity&&m["id"]==selectedGuest) {
            if(live&&QUuid(m["admission"].toString()).isNull())return false;
            guest=m["id"].toString();admission=m["admission"].toString();
        }else return false;
    }
    if(players!=2||owner.isEmpty()||guest.isEmpty()||(self!=owner&&self!=guest))return false;
    self_=self;host_=self==owner;peer_=host_?guest:owner;guest_=guest;ticket_=ticket;helper_=helper;ice_=ice;context_=binding(s,guest);stopping_=false;admission_=admission;
    if(!matches()){stop();return false;}
    deadline_.start();
    // Guest initiates after its launch callback exists; this also avoids losing
    // an owner's offer before the guest receives the launching snapshot.
    if(!host_) {
        connection_=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto connection=connection_;
        QTimer::singleShot(0,this,[this,connection]{
            QJsonObject request{{"kind","connect"}};
            if(!admission_.isEmpty())request["admission"]=admission_;
            if(active()&&connection_==connection&&!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,request))
                fail("Couldn't reach the other player.");
        });
    }
    return true;
}
void LobbyConnection::launch() {
    auto identity=context_;identity["connection"]=connection_;
    identity_=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    directory_=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/traineros-connection-XXXXXX");
    if(!directory_->isValid()){fail("Couldn't protect connection details.");return;}
    auto env=QProcessEnvironment::systemEnvironment();env.insert("TMPDIR",directory_->path());process_.setProcessEnvironment(env);
    process_.start(helper_,{});
}
void LobbyConnection::receive(QString peer,QString ticket,QString connection,const QJsonObject& payload) {
    if(!matches()||peer!=peer_||ticket!=ticket_)return;
    const auto kind=payload["kind"].toString();
    if(kind=="connect"&&host_&&((payload.size()==1&&admission_.isEmpty())
       ||(payload.size()==2&&payload["admission"]==admission_))) {
        if(!connection_.isEmpty())return;
        connection_=connection;
        launch();if(!active())return;
        if(!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,{{"kind","accepted"}})){fail("Couldn't agree the Remote Play connection.");return;}
        return;
    }
    if(connection!=connection_)return;
    if(kind=="media-ready"&&payload.size()==1&&!host_&&ready_){emit mediaReady();return;}
    if(kind=="accepted"&&!host_&&payload.size()==1&&identity_.isEmpty()){launch();return;}
    if(kind=="candidates"&&payload.size()==3&&payload["identity"]==identity_&&!identity_.isEmpty()) {
        const auto batch=payload["candidates"].toArray();if(!candidates(batch,candidatesReceived_)){fail("Invalid Remote Play addresses.");return;}
        candidatesReceived_+=batch.size();const QJsonObject packet{{"identity",identity_},{"candidates",batch}};
        if(initialized_)write(packet);else for(const auto& entry:batch)pendingCandidates_.append(entry);
        return;
    }
    if(kind=="description"&&payload.size()==3&&payload["identity"]==identity_&&!identity_.isEmpty()) {
        const auto description=payload["description"].toString();
        if(descriptionReceived_||description.isEmpty()||description.size()>16384)return;
        descriptionReceived_=true;
        const QJsonObject packet{{"identity",identity_},{"description",description}};
        if(initialized_)write(packet);else pendingDescription_=packet;
    }
}
bool LobbyConnection::announceMedia() {
    return host_&&ready_&&matches()&&lobby_.sendRuntimeSignal(peer_,ticket_,connection_,{{"kind","media-ready"}});
}
void LobbyConnection::write(QJsonObject value) {
    if(process_.state()==QProcess::NotRunning||process_.bytesToWrite()>32768){fail("Remote Play stopped responding.");return;}
    const auto bytes=QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n';
    if(process_.write(bytes)!=bytes.size())fail("Couldn't prepare Remote Play.");
}
void LobbyConnection::read() {
    buffer_+=process_.readAllStandardOutput();
    if(buffer_.size()>65536){fail("Invalid Remote Play response.");return;}
    while(buffer_.contains('\n')&&active()) {
        const int at=buffer_.indexOf('\n');const auto event=QJsonDocument::fromJson(buffer_.left(at)).object();buffer_.remove(0,at+1);
        if(!matches()){fail("The lobby game changed.");return;}
        if(event["event"]=="description"&&!descriptionSent_&&event.size()==3&&event["identity"]==identity_
           &&event["description"].toString().size()<=16384&&!event["description"].toString().isEmpty()) {
            descriptionSent_=true;auto packet=event;packet.remove("event");packet["kind"]="description";
            if(!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,packet)){fail("Couldn't reach the other player.");return;}
        } else if(event["event"]=="candidates"&&event.size()==3&&event["identity"]==identity_&&descriptionSent_) {
            const auto batch=event["candidates"].toArray();
            if(!candidates(batch,candidatesSent_)){fail("Invalid Remote Play addresses.");return;}
            candidatesSent_+=batch.size();auto packet=event;packet.remove("event");packet["kind"]="candidates";
            if(!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,packet)){fail("Couldn't reach the other player.");return;}
        } else if(event["event"]=="ready"&&event.size()==2&&!ready_&&descriptionSent_&&descriptionReceived_) {
            const auto path=event["config"].toString();const QFileInfo file(path);
            // Only the launched local helper can supply a config path. Still
            // reject symlinks/world-readable material before exposing it locally.
            if(!directory_||!file.canonicalFilePath().startsWith(directory_->path()+"/")
               ||!file.isAbsolute()||!file.isFile()||file.isSymLink()||file.size()>4096
               ||(file.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther))) {fail("Couldn't protect connection details.");return;}
            config_=path;ready_=true;deadline_.stop();emit connected(path);
        }else {fail("Invalid Remote Play response.");return;}
    }
}
void LobbyConnection::reconcile(){if(active()&&!matches())fail("The lobby connection changed. Your game is kept.");}
void LobbyConnection::fail(QString reason){if(!active()||stopping_)return;stop();emit ended(reason);}
void LobbyConnection::stop() {
    stopping_=true;deadline_.stop();ticket_.clear();
    if(process_.state()!=QProcess::NotRunning) {
        process_.write("{\"close\":true}\n");process_.closeWriteChannel();
        if(!process_.waitForFinished(1000)){process_.terminate();if(!process_.waitForFinished(1000)){process_.kill();process_.waitForFinished(1000);}}
    }
    // Only the exact private file returned by our child, never a path from a peer.
    if(!config_.isEmpty())QFile::remove(config_);
    config_.clear();connection_.clear();identity_.clear();admission_.clear();context_={};buffer_.clear();
    pendingDescription_={};initialized_=false;directory_.reset();
    pendingCandidates_={};candidatesSent_=candidatesReceived_=0;
    ready_=descriptionSent_=descriptionReceived_=false;
}
}
