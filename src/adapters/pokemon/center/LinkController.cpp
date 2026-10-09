#include "LinkController.h"
#include "integrations/progress/EmeraldPractice.h"
#include "integrations/progress/EmeraldLink.h"
#include "integrations/progress/EmeraldParty.h"
#include "integrations/progress/Gen3Progress.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUuid>
#include <QSysInfo>
#include <QSettings>
#include <algorithm>
namespace trainer {
namespace {
QString digest(const QJsonObject& value){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(value).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
}
LinkController::~LinkController(){
    disconnect(&peer_,nullptr,this,nullptr);disconnect(&nearby_,nullptr,this,nullptr);
    disconnect(&battle_,nullptr,this,nullptr);peer_.close();battle_.cancel();
}
LinkController::LinkController(QObject* parent):NativeActivityProvider(parent),peer_(this),battle_(this),nearby_(this) {
    connect(&peer_,&LocalLinkPeer::changed,this,[this]{
        if(!online_ && !directPeer_.isEmpty() && !directIncoming_ && !directInterface_.isEmpty())peer_.connectId(directPeer_,directInterface_);
        emit changed();
    });
    connect(&nearby_,&NearbyService::event,this,&LinkController::directEvent);
    invitationTimer_.setSingleShot(true);invitationTimer_.setInterval(45000);
    invitationTimer_.setParent(this);invitationTimer_.setObjectName("link-invitation-deadline");
    connect(&invitationTimer_,&QTimer::timeout,this,[this]{answerInvitation(false);});
    connectionTimer_.setParent(this);connectionTimer_.setObjectName("link-connection-deadline");connectionTimer_.setSingleShot(true);
    connect(&connectionTimer_,&QTimer::timeout,this,[this]{endConnectionAttempt("Could not finish connecting. Invite your friend again.");});
    connect(&peer_,&LocalLinkPeer::error,this,[this](const QString& reason){
        if(!directPeer_.isEmpty() && !paired_)endConnectionAttempt(reason);else fail(reason);
    });
    connect(&peer_,&LocalLinkPeer::received,this,&LinkController::receive);
    connect(&peer_,&LocalLinkPeer::connectedToPeer,this,[this]{
        if(online_){peer_.disconnectPeer();return;}
        nonce_=uuid();peerId_.clear();peerName_.clear();pin_.clear();peerNonce_.clear();
        accepted_=peer_.outgoing();peerAccepted_=paired_=false;stage_="pair";lastMessage_=QDateTime::currentMSecsSinceEpoch();
        message_="Waiting for your friend";
        if(directPeer_.isEmpty())invitationTimer_.start();else connectionTimer_.start(15000);
        send("hello",{{"id",localId()},{"name",peer_.name()},{"trainer",trainerName_},{"nonce",nonce_},{"pending",pending()?journal_["id"]:QJsonValue()}});emit changed();
    });
    connect(&peer_,&LocalLinkPeer::disconnectedFromPeer,this,[this]{
        if(online_)return;
        battle_.cancel();paired_=accepted_=peerAccepted_=false;peerId_.clear();pin_.clear();
        stage_="browse";message_=pending()?"Trade paused. Reconnect these same consoles to finish.":"Choose a nearby Trainer";
        invitationTimer_.stop();connectionTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();
        if(!directPeer_.isEmpty()){nearby_.request({{"op","disconnect"}});directPeer_.clear();directInterface_.clear();directAccepted_=false;}
        directConnecting_=false;resetChoice();emit changed();
    });
    connect(&battle_,&PracticeSession::changed,this,&LinkController::battleChanged);
    connect(&battle_,&PracticeSession::stopped,this,[this](const QString& reason){if(open_ && mode_=="battle" && stage_!="finished" && transportConnected() && !reason.isEmpty())fail(reason);});
    playbackTimer_.setSingleShot(true);
    connect(&playbackTimer_,&QTimer::timeout,this,[this]{
        if(stage_!="events")return;
        if(playback_.advance())playbackTimer_.start(playback_.event()["duration"].toInt());
        else finishPlayback();
        emit changed();
    });
    heartbeat_.setInterval(2000);
    connect(&heartbeat_,&QTimer::timeout,this,[this]{
        if(online_ || !transportConnected())return;
        if(QDateTime::currentMSecsSinceEpoch()-lastMessage_>12000){disconnectTransport();return;}
        send("ping");
    });
}

QJsonArray LinkController::onlineCapabilities() const {
    if(!backend_||!verify_||source_.trainerId.isEmpty()||gen3Edition(progress_.contentRevision)!=Gen3Edition::Emerald
        ||progress_.availability!=ProgressAvailability::Available||!progress_.party||!progress_.party->error.isEmpty()
        ||progress_.saveRevision.isEmpty()||progress_.contextRevision.isEmpty())return {};
    QJsonArray out;
    for(const auto& mode:QStringList{"trade","sale","gift"})out.append(QJsonObject{
        {"family","systemActivity"},{"id","org.traineros.emerald."+mode},{"version",1},
        {"build",progress_.contentRevision},{"schema","emerald-individual-1"},{"rules","emerald-link-v2"},
        {"effect","bilateralSave"},{"participants",2},{"delivery","fluxer-dm-v1"},
        {"label",mode=="trade"?"Exchange Pokemon":mode=="sale"?"Sell Pokemon":"Give a Pokemon"}});
    return out;
}
bool LinkController::beginOnline(QString self,QString peer,QString name,QString activity,bool initiator) {
    if(online_||peer_.connected()||busy_||!mode_.isEmpty()||invitationOpen()||QUuid(self).isNull()||QUuid(peer).isNull()
        ||(pending()&&(journal_["peer"]!=peer||journal_["kind"]=="battle")))return false;
    bool supported=false;for(const auto& c:onlineCapabilities())if(c.toObject()["id"]==activity)supported=true;
    if(!supported)return false;
    online_=true;onlineSelf_=self;peerId_=peer;peerName_=name.left(100);paired_=accepted_=peerAccepted_=true;
    stage_="lobby";open_=true;updatePresence();
    send("online-ready",{{"pending",pending()?journal_["id"]:QJsonValue()}});
    if(pending())recover(journal_["id"].toString());
    else {sellerId_=initiator?self:peer;startMode(activity.section('.',-1));}
    emit workspaceRequested();emit changed();return true;
}
void LinkController::receiveOnline(QJsonObject frame) {
    if(!online_)return;
    if(frame["version"].toInt()!=2){disconnectTransport();return;}
    if(frame["type"]=="online-ready") {
        const auto id=frame["pending"].toString();
        if(!id.isEmpty()&&!QUuid(id).isNull()&&!pending()&&!busy_){resetChoice();recover(id);}
        return;
    }
    static const QStringList allowed{"activity-invite","activity-accept","activity-cancel","offer","confirm","prepare","receipt","cancel","problem"};
    if(!allowed.contains(frame["type"].toString()))return;
    if(frame["type"]=="receipt" && frame["receipt"].toObject()["kind"]=="battle")return;
    receive(frame);
}
void LinkController::endOnline() {
    if(!online_)return;
    online_=false;paired_=accepted_=peerAccepted_=false;peerId_.clear();onlineSelf_.clear();
    invitationTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();
    resetChoice();stage_="browse";message_=pending()?"Exchange paused. Reconnect with the same friend to recover.":"Online connection ended";
    updatePresence();emit changed();
}
void LinkController::showOnline(){if(online_){enter();emit workspaceRequested();}}
void LinkController::disconnectTransport(){if(online_){endOnline();emit onlineClosed();}else peer_.disconnectPeer();}

void LinkController::configure(const QString& root,const QString& id,const QString& name,Backend backend,
        PracticeController::Verifier verify,const QJsonObject& pending) {
    runtime_=root;backend_=std::move(backend);verify_=std::move(verify);journal_=pending;
    if(trainerName_.isEmpty())trainerName_=name.left(32);
    peer_.configure(id,trainerName_);visibleNearby_=QSettings().value("nearby/visible",true).toBool();updatePresence();
}
void LinkController::setTrainerName(const QString& name) {
    if(trainerName_==name.left(32))return;trainerName_=name.left(32);
    peer_.configure(peer_.id(),trainerName_);updatePresence();
    if(paired_)send("identity",{{"name",trainerName_}});
}
void LinkController::setVisibleNearby(bool value) {
    if(visibleNearby_==value)return;visibleNearby_=value;QSettings().setValue("nearby/visible",value);
    if(!value && !active())disconnectSession();updatePresence();emit changed();
}
void LinkController::setInvitationsAllowed(bool value) {
    if(invitationsAllowed_==value)return;invitationsAllowed_=value;
    if(!value && invitationOpen())answerInvitation(false);updatePresence();
}
void LinkController::updatePresence() {
    if(!backend_ || QUuid(peer_.id()).isNull() || trainerName_.isEmpty())return;
    const bool show=!online_ && visibleNearby_ && invitationsAllowed_;
    peer_.setVisible(show);
    if(show){peer_.open();heartbeat_.start();}else if(!transportConnected() && !pending())peer_.close();
    nearby_.configure(peer_.id(),trainerName_,show);
}
bool LinkController::invitationOpen() const {
    return (stage_=="pair" && !peerId_.isEmpty()) || !inviteId_.isEmpty() || (!directPeer_.isEmpty() && !paired_);
}
bool LinkController::invitationIncoming() const {
    if(!inviteId_.isEmpty())return inviteOwner_==peerId_;
    if(!directPeer_.isEmpty())return directIncoming_ && !directAccepted_;
    return stage_=="pair" && !accepted_;
}
QString LinkController::invitationText() const {
    const auto name=!directPeer_.isEmpty() && peerName_.isEmpty()?directName_:peerName_;
    if(!paired_ && !directPeer_.isEmpty() && directConnecting_)return "Connecting to "+name+"…";
    if(!invitationIncoming())return "Waiting for "+name+"...";
    if(inviteMode_.isEmpty())return name+" invites you to connect";
    const QString activity=inviteMode_=="battle"?"a battle":inviteMode_=="trade"?"an exchange":inviteMode_=="sale"?"a sale":"a gift";
    return name+" invites you to "+activity;
}
void LinkController::disconnectSession() {
    if(busy_)return;
    invitationTimer_.stop();connectionTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();
    disconnectTransport();nearby_.request({{"op","disconnect"}});directPeer_.clear();directInterface_.clear();directAccepted_=false;
    directConnecting_=false;resetChoice();stage_="browse";peerName_.clear();paired_=false;emit changed();
}
void LinkController::endConnectionAttempt(const QString& reason) {
    const bool waiting = invitationOpen() && !paired_;
    disconnectTransport();
    disconnectSession();
    if(waiting && !pending()){message_=reason;emit changed();emit connectionFailed(reason);}
}
void LinkController::answerInvitation(bool accept) {
    if(!invitationOpen())return;
    if(!inviteId_.isEmpty()) {
        const auto id=inviteId_,mode=inviteMode_,owner=inviteOwner_;
        if(accept && owner!=peerId_)return;
        send(accept?"activity-accept":"activity-cancel",{{"id",id}});
        invitationTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();
        if(accept){sellerId_=owner;startMode(mode);emit workspaceRequested();}
        else {stage_="lobby";message_="What shall we do together?";emit changed();}
        return;
    }
    if(!directPeer_.isEmpty() && !transportConnected()) {
        if(accept && directIncoming_ && !directAccepted_){
            directAccepted_=directConnecting_=true;invitationTimer_.stop();connectionTimer_.start(75000);
            nearby_.request({{"op","accept"},{"peer",directPeer_}});emit changed();
        }
        else if(!accept)disconnectSession();return;
    }
    if(!accept){disconnectSession();return;}
    accepted_=true;send("accept",{{"code",pin_}});pairReady();emit changed();
}
void LinkController::directEvent(const QJsonObject& event) {
    const auto kind=event["event"].toString();
    if(kind=="identity")peer_.setBluetoothDiscoveryId(event["discovery"].toString());
    if(kind=="searching")directSearching_=event["active"].toBool();
    if(kind=="unavailable" || kind=="closed" || kind=="error")directSearching_=false;
    if(kind=="invite") {
        if(!directPeer_.isEmpty() || paired_)return;
        if(!invitationsAllowed_ || !visibleNearby_ || busy_ || !mode_.isEmpty() || invitationOpen() || paired_
            || (pending() && journal_["peer"].toString()!=event["peer"].toString())) {
            nearby_.request({{"op","disconnect"}});return;
        }
        if(QUuid(event["peer"].toString()).isNull())return;
        directPeer_=event["peer"].toString();directName_=event["name"].toString().left(32);directIncoming_=true;directAccepted_=directConnecting_=false;
        invitationTimer_.stop();connectionTimer_.start(60000);
    } else if(kind=="connecting" && event["peer"].toString()==directPeer_ && event["phase"]=="network" && !directConnecting_) {
        directConnecting_=true;connectionTimer_.start(75000);
    } else if(kind=="ready" && event["peer"].toString()==directPeer_) {
        if(!directInterface_.isEmpty() || paired_)return;
        if(event["transport"]=="bluetooth") {
            if(!directIncoming_) {
                const auto identity=event["identity"].toString();const int port=event["port"].toInt();
                if(QUuid(identity).isNull() || identity==localId() || port<=0 || port>65535 || port==47845) {
                    endConnectionAttempt("Bluetooth connection could not start.");return;
                }
                // Discovery addresses are not persistent Trainer/save identity.
                // Bind the accepted transport to its handshake before Link hello.
                directPeer_=identity;directName_=event["name"].toString().left(32);
                connectionTimer_.start(15000);peer_.connectBridge(quint16(port));
            }else connectionTimer_.start(15000);
            emit changed();return;
        }
        directInterface_=event["interface"].toString();
        connectionTimer_.start(15000);
        if(!directIncoming_)peer_.connectId(directPeer_,directInterface_);
    } else if((kind=="closed" || kind=="error" || (kind=="unavailable" && !paired_)) && !directPeer_.isEmpty()) {
        endConnectionAttempt(event["error"].toString().isEmpty()?"Nearby connection is unavailable. Try again.":event["error"].toString());
    }
    emit changed();
}
void LinkController::inviteActivity(const QString& mode) {
    if(!paired_ || pending() || busy_ || !inviteId_.isEmpty() || !mode_.isEmpty() || (online_ && mode=="battle"))return;
    inviteId_=uuid();inviteMode_=mode;inviteOwner_=localId();invitationTimer_.start();
    send("activity-invite",{{"id",inviteId_},{"mode",mode}});emit changed();
}
void LinkController::setObservation(const PracticeSource& source,const GameProgress& progress,const QVariantList& actors) {
    if((active() || paired_) && !busy_ && !pending() && !source.trainerId.isEmpty() &&
        (source.trainerId!=source_.trainerId || source.adventureId!=source_.adventureId))disconnectTransport();
    // Save-service inspection temporarily emits Checking/Unavailable. It is not
    // a new Party. Actual source verification precedes every action.
    if(progress.availability==ProgressAvailability::Available && (!active() || stage_=="browse" || stage_=="lobby" || stage_=="finished")) {
        source_=source;progress_=progress;actors_=actors;
    } else if(!active()){source_=source;progress_=progress;actors_=actors;}
}
void LinkController::enter() {
    if(!pending())journal_={};
    open_=true;focus_=0;if(!paired_ && !invitationOpen())stage_="browse";
    if(stage_=="browse")message_=pending()?"Trade paused. Reconnect the same consoles to finish.":"Choose a nearby Trainer";
    if(!backend_){stage_="error";message_="Link requires an installed Emerald Adventure.";emit changed();return;}
    peer_.open();heartbeat_.start();emit changed();
}
void LinkController::leave(){if(!open_ || busy_ || pending() || !mode_.isEmpty())return;open_=false;emit changed();}
void LinkController::send(const QString& type,QJsonObject fields){
    fields["type"]=type;fields["version"]=2;
    if(online_){
        // Local owner and registration IDs have no meaning to an internet peer.
        if(type=="receipt"){auto r=fields["receipt"].toObject();r.remove("owner");r.remove("adventure");fields["receipt"]=r;}
        emit onlineSend(fields);
    }else peer_.send(fields);
}
void LinkController::fail(const QString& text){playbackTimer_.stop();playback_.reset();message_=text;stage_="error";if(paired_)send("problem",{{"message",online_?QString("The activity could not continue on your friend's device."):text.left(180)}});emit changed();}
void LinkController::resetChoice(){playbackTimer_.stop();playback_.reset();turnSubmitted_=false;mode_.clear();localChoice_={};remoteChoice_={};remoteJournal_={};transaction_.clear();confirmed_=remoteConfirmed_=false;focus_=0;localMove_=remoteMove_=-1;moveSent_=false;battleState_={};bench_=bag_=recovering_=battleStarting_=false;bagItem_=0;forfeitSide_=-1;collection_=-1;stake_="none";stakeAmount_=1000;}
void LinkController::verify(std::function<void()> next) {
    if(busy_ || !verify_){if(!verify_)fail("Choose a supported Emerald Adventure first.");return;}
    busy_=true;emit changed();const auto generation=++generation_;
    QTimer::singleShot(6000,this,[this,generation]{if(generation==generation_ && busy_){++generation_;busy_=false;fail("The Party check timed out. Reconnect to try again.");}});
    verify_(source_,progress_,this,[this,generation,next](bool ok){
        if(generation!=generation_)return;busy_=false;++generation_;
        if(!paired_ || !transportConnected()){emit changed();return;}
        if(!ok){send("cancel");fail("Your Party changed. Return to Center, then reconnect.");return;}
        next();emit changed();
    });
}
void LinkController::operation(const QString& op,QJsonObject args,std::function<void(QJsonObject)> next) {
    if(busy_ || !backend_)return;busy_=true;emit changed();
    args["adventure"]=journal_.isEmpty()?source_.adventureId:journal_["adventure"].toString();
    backend_(op,args,this,[this,next](const QJsonObject& value){
        busy_=false;
        if(!value["error"].toString().isEmpty()){
            if(value.contains("pending"))journal_=value["pending"].toObject();
            fail(value["error"].toString());return;
        }
        next(value);emit changed();
        if(forfeitSide_>=0 && !busy_ && host() && journal_["stage"]=="reserved") {
            const int side=forfeitSide_;forfeitSide_=-1;concede(side);
        }
        if(!busy_ && pending() && !remoteJournal_.isEmpty())advanceTrade();
    });
}
QJsonObject LinkController::partyMember(int slot) const {
    if(!progress_.party || slot<0 || slot>=progress_.party->party.size())return {};
    const auto battle=emeraldPracticeMember(progress_,slot);if(battle.isEmpty())return {};
    const auto& mon=progress_.party->party[slot];QString target;
    for(const auto& actor:actors_)if(actor.toMap()["index"].toInt()==slot)target=actor.toMap()["target"].toString();
    return {{"slot",slot},{"name",(mon.nickname.isEmpty()?mon.speciesName:mon.nickname).left(24)},{"level",mon.level},{"target",target},{"battle",battle}};
}
QVariantMap LinkController::display(const QJsonObject& value) const {
    if(value.contains("pokemon")) {
        const auto record=importEmeraldLinkRecord(value["pokemon"].toObject());
        if(!record.isEmpty()) {
            const auto mon=readEmeraldPartyMember(record);
            QVariantMap row{{"name",mon.nickname.isEmpty()?mon.speciesName:mon.nickname},{"level",mon.level},
                {"target",mon.speciesId+"/"+mon.formId},{"kind","known"}};
            return artwork_?artwork_(row):row;
        }
    }
    const auto target=value["target"].toString();
    static const QRegularExpression allowed("^[a-z0-9-]+/[a-zA-Z0-9._-]+$");
    QVariantMap row{{"name",value["name"].toString().left(24)},{"level",value["level"].toInt()},
        {"target",allowed.match(target).hasMatch()?target:QString()},{"kind","known"}};
    return artwork_?artwork_(row):row;
}
QString LinkController::collectionName() const {
    return collection_<0?"Party":progress_.party && collection_<progress_.party->boxes.size()?progress_.party->boxes[collection_].name:QString();
}
QString LinkController::stakeText() const {
    if(stake_=="money")return QString("Stake · ₽ %1 each").arg(stakeAmount_);
    if(stake_=="pokemon")return "Stake · one Pokemon each";
    return "No stake";
}
QVariantList LinkController::savedMembers() const {
    QVariantList out;if(!progress_.party)return out;
    const auto members=collection_<0?progress_.party->party:progress_.party->boxes.value(collection_).members;
    for(int i=0;i<members.size();++i){const auto& mon=members[i];if(mon.kind!=PokemonSlotKind::Known)continue;
        const int position=collection_<0?i:6+collection_*30+i;
        QVariantMap row{{"name",mon.nickname.isEmpty()?mon.speciesName:mon.nickname},{"level",mon.level},
            {"target",mon.speciesId+"/"+mon.formId},{"kind","known"},{"slot",position},
            {"detail",QString("Lv. %1 · %2").arg(mon.level).arg(mon.speciesName)}};
        out.append(artwork_?artwork_(row):row);
    }return out;
}
QVariantList LinkController::previewTeam() const {
    QVariantList out;if(!progress_.party)return out;
    for(int i=0;i<progress_.party->party.size();++i){auto m=partyMember(i);if(!m.isEmpty())out.append(display(m));}return out;
}
QVariantList LinkController::team() const {
    QVariantList out;const auto sides=playback_.active()?playback_.sides():battleState_["sides"].toArray();const auto members=localChoice_["team"].toArray();
    if(sides.size()!=2)return previewTeam();
    for(const auto& v:sides[host()?0:1].toObject()["team"].toArray()) {
        const auto mon=v.toObject();const int index=mon["member"].toInt(-1);if(index<0 || index>=members.size())continue;
        auto row=display(members[index].toObject());for(auto it=mon.begin();it!=mon.end();++it)row[it.key()]=it.value().toVariant();
        out.append(row);
    }return out;
}
QVariantList LinkController::rows() const {
    if(stage_=="browse") {
        auto rows=nearby_.peers();QSet<QString> ids;
        for(auto& v:rows){auto row=v.toMap();ids.insert(row["id"].toString());row["direct"]=true;row["detail"]=row["transport"]=="bluetooth"?"Bluetooth":"Nearby";v=row;}
        for(const auto& v:peer_.peers()) {
            const auto row=v.toMap();
            if(!ids.contains(row["id"].toString()) && !ids.contains(row["bluetooth"].toString()))rows.append(v);
        }
        std::sort(rows.begin(),rows.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["name"].toString()<b.toMap()["name"].toString();});return rows;
    }
    if(stage_=="lobby") {
        QVariantList out;
        if(!online_)out.append(QVariantMap{{"name","Friendly battle"},{"detail","Your saved teams · Gen III"}});
        out.append(QVariantMap{{"name","Trade Pokemon"},{"detail","Party and Boxes"}});
        out.append(QVariantMap{{"name","Sell Pokemon"},{"detail","For in-game money"}});
        out.append(QVariantMap{{"name","Give a Pokemon"},{"detail","A gift for your friend"}});return out;
    }
    if(stage_=="stake" || stage_=="choose" && mode_!="battle")return savedMembers();
    if(stage_=="choose") {
        QVariantList out;if(!progress_.party)return out;
        for(int slot=0;slot<progress_.party->party.size();++slot){const auto member=partyMember(slot);if(member.isEmpty())continue;auto row=display(member);row["slot"]=slot;row["detail"]=QString("Lv. %1").arg(row["level"].toInt());out.append(row);}return out;
    }
    if(stage_=="moves") {
        QVariantList out;const auto sides=battleState_["sides"].toArray();if(sides.size()!=2)return out;
        const auto side=sides[host()?0:1].toObject();QSet<int> items;
        for(const auto& v:side["moves"].toArray()){auto row=v.toObject().toVariantMap();
            const bool switching=row["switch"].toBool(),item=row["bag"].toBool();
            if(side["forceSwitch"].toBool()){if(!switching)continue;}
            else if(bag_){if(!item || bagItem_ && row["item"].toInt()!=bagItem_)continue;}
            else if(item || switching!=bench_)continue;
            row["name"]=row["move"];
            if(item && !bagItem_){const int id=row["item"].toInt();if(items.contains(id))continue;items.insert(id);row["detail"]=QString("× %1").arg(row["pp"].toInt());}
            else if(item || switching){const int index=item?row["target"].toInt():row["slot"].toInt()-10;
                const auto members=localChoice_["team"].toArray();if(index<0 || index>=members.size())continue;
                const auto picture=display(members[index].toObject());for(auto it=picture.begin();it!=picture.end();++it)row[it.key()]=it.value();
                const auto health=side["team"].toArray();for(const auto& value:health)if(value.toObject()["member"].toInt()==index){const auto mon=value.toObject();row["detail"]=QString("%1 / %2 HP").arg(mon["hp"].toInt()).arg(mon["maxHp"].toInt());}
            } else row["detail"]=QString("%1 / %2 PP").arg(row["pp"].toInt()).arg(row["maxPp"].toInt());
            out.append(row);
        }return out;
    }
    return {};
}
QVariantList LinkController::fighters() const {
    QVariantList out;const auto sides=playback_.active()?playback_.sides():battleState_["sides"].toArray();
    const std::array<QJsonObject,2> members{localChoice_,remoteChoice_};
    for(int i=0;i<2;++i){auto shown=members[i];
        if(sides.size()==2 && shown.contains("team")){const auto team=shown["team"].toArray();const int index=sides[host()?i:1-i].toObject()["member"].toInt();if(index>=0 && index<team.size())shown=team[index].toObject();}
        if(mode_=="battle" && sides.isEmpty() && stake_=="pokemon" && !shown["stakePokemon"].toObject().isEmpty()) {
            const auto offered=shown["stakePokemon"].toObject();
            shown.insert("pokemon",offered);
        }
        auto row=display(shown);
        if(sale() && members[i]["role"]=="buyer") {
            const auto offer=seller()?localChoice_:remoteChoice_;
            row={{"payment",true},{"name",mode_=="gift"?QString("Thank you!"):QString("₽ %1").arg(offer["price"].toInt())},
                {"detail",members[i]["account"].toObject()["destination"].toString()}};
        }
        if(sides.size()==2){const auto side=sides[host()?i:1-i].toObject();row["battleHp"]=side["hp"].toInt();row["maxHp"]=side["maxHp"].toInt();row["hpRatio"]=side["hp"].toDouble()/std::max(1,side["maxHp"].toInt());row["battleStatus"]=side["status"].toString();}
        const auto event=playback_.event();
        if(playback_.active() && event["actor"].toInt()==(host()?i:1-i)) {
            row["effectBeat"]=event["serial"];row["effectKind"]=event["effect"];row["effectElement"]=event["element"];
            if(event["effect"]=="move")row["attackBeat"]=event["serial"];
        }
        out.append(row);
    }return out;
}
QString LinkController::turnSummary() const {
    if(playback_.active()) {
        auto text=playback_.event()["text"].toString();
        text.replace(host()?"Partner 1":"Partner 2","You");
        text.replace(host()?"Partner 2":"Partner 1","Your friend");return text;
    }
    return stage_=="moves"?QString("Choose your action for turn %1").arg(battleTurn()):message_;
}
void LinkController::pairReady() {
    if(!accepted_ || !peerAccepted_ || paired_)return;paired_=true;pin_.clear();invitationTimer_.stop();connectionTimer_.stop();
    directConnecting_=false;
    if(pending() || !peerPending_.isEmpty()){recover(pending()?journal_["id"].toString():peerPending_);emit workspaceRequested();return;}
    stage_="lobby";message_="What shall we do together?";focus_=0;emit changed();
}
void LinkController::startMode(const QString& mode) {
    if(pending() || !paired_ || busy_)return;
    resetChoice();mode_=mode;stage_="choose";message_=mode=="battle"?"Choose your lead Pokemon":"Choose a Pokemon to offer";
    if(sale() && !seller()) {
        stage_="waiting";message_="Waiting for your friend's offer";
        verify([this]{operation("inspect",{{"slot",-1}},[this](QJsonObject result){
            if(!paired_)return;
            if(result["account"].toObject()["box"].toInt()==-2){fail("Make room in your Party or boxes first.");return;}
            localChoice_={{"role","buyer"},{"save",result["save"]},{"account",result["account"]}};publishOffer();
        });});return;
    }
    if(rows().isEmpty()){fail("Choose an Emerald Adventure with a saved Party, then reconnect.");return;}
    emit changed();
}
void LinkController::choose(int index) {
    const auto options=rows();if(index<0 || index>=options.size())return;
    const int slot=options[index].toMap()["slot"].toInt();
    const bool staking=stage_=="stake";
    verify([this,slot,staking]{
        if(staking){operation("inspect",{{"slot",slot}},[this,slot](QJsonObject result){
            localChoice_["stakePokemon"]=result["pokemon"];localChoice_["stakeSlot"]=slot;
            localChoice_["save"]=result["save"];publishOffer();});return;}
        localChoice_=partyMember(slot);
        if(localChoice_.isEmpty())localChoice_={{"slot",slot}};
        if(mode_=="trade" || sale())operation("inspect",{{"slot",slot}},[this](QJsonObject result){
            if(!paired_)return;
            localChoice_["pokemon"]=result["pokemon"];localChoice_["save"]=result["save"];
            const auto shown=display(localChoice_);localChoice_["name"]=shown["name"].toString();localChoice_["target"]=shown["target"].toString();localChoice_["level"]=shown["level"].toInt();
            localChoice_["account"]=result["account"];
            if(sale()) {
                localChoice_["role"]="seller";
                if(mode_=="sale"){stage_="price";message_="Set your price";price_=1000;emit changed();return;}
                localChoice_["price"]=0;
            }
            publishOffer();
        });else {
            QJsonArray team{localChoice_};
            for(int i=0;i<progress_.party->party.size();++i)if(i!=slot){const auto member=partyMember(i);if(!member.isEmpty())team.append(member);}
            localChoice_["team"]=team;
            operation("inspect",{{"slot",-1}},[this](QJsonObject result){localChoice_["save"]=result["save"];localChoice_["account"]=result["account"];localChoice_["bag"]=result["bag"];localChoice_["stake"]=stake_;localChoice_["amount"]=stakeAmount_;publishOffer();});}
    });
}
QString LinkController::proposal() const {
    return digest({{"host",host()?localChoice_:remoteChoice_},{"guest",host()?remoteChoice_:localChoice_},{"mode",mode_},{"seller",sale()?sellerId_:QString()}});
}
void LinkController::publishOffer(){send("offer",{{"offer",localChoice_},{"mode",mode_}});review();}
void LinkController::review() {
    if(localChoice_.isEmpty() || remoteChoice_.isEmpty()){stage_="waiting";message_="Waiting for your friend's choice";}
    else {stage_="review";message_=mode_=="trade"?"Trade these Pokemon?":"Ready for a friendly battle?";}
    if(mode_=="battle" && stage_=="review") {
        if(stake_=="pokemon" && localChoice_["stakePokemon"].toObject().isEmpty()){stage_="stake";message_="Choose your stake";}
        else if(stake_=="pokemon" && remoteChoice_["stakePokemon"].toObject().isEmpty()){stage_="waiting";message_="Your friend is choosing a stake";}
        else message_="Used medicine is taken from your Bag.";
    }
    if(sale() && stage_=="review") {
        const auto offered=seller()?localChoice_:remoteChoice_;const auto buying=seller()?remoteChoice_:localChoice_;
        const int price=offered["price"].toInt(-1),balance=buying["account"].toObject()["money"].toInt(-1),wallet=offered["account"].toObject()["money"].toInt(-1);
        if(price<0 || price>999999 || balance<price || wallet<0 || price>999999-wallet){fail("The price exceeds the buyer's money or the seller's wallet limit.");return;}
        message_=QString("%1 · %2\n%3 money: %4 → %5 · %6")
            .arg(mode_=="gift"?"Gift":seller()?"Sell":"Buy",offered["name"].toString(),"Your")
            .arg(seller()?wallet:balance).arg(seller()?wallet+price:balance-price).arg(buying["account"].toObject()["destination"].toString());
        if(price)message_+=QString(" · Price %1").arg(price);
    }
    if(stage_=="review" && mode_!="battle") {
        for(const auto& choice:{localChoice_,remoteChoice_}) {
            const auto input=choice["pokemon"].toObject();if(input.isEmpty())continue;
            const auto prepared=prepareEmeraldReceived(input);
            if(!prepared.error.isEmpty()){fail(prepared.error);return;}
            if(input["species"]!=prepared.pokemon["species"]) {
                const auto evolved=readEmeraldPartyMember(importEmeraldLinkRecord(prepared.pokemon));
                message_+=QString("\n%1 → %2 · Keep existing moves").arg(choice["name"].toString(),evolved.speciesName);
            }
        }
    }
    focus_=0;emit changed();
}
void LinkController::confirm() {
    if(confirmed_ || busy_ || stage_!="review")return;
    verify([this]{confirmed_=true;send("confirm",{{"proposal",proposal()}});stage_="waiting";message_="Waiting for your friend";
        if(remoteConfirmed_) {
            if((mode_=="battle" || mode_=="trade" || sale()) && host()) {transaction_=uuid();send("prepare",{{"id",transaction_},{"proposal",proposal()}});advanceTrade();}
        }emit changed();
    });
}
void LinkController::advanceTrade() {
    if(playback_.active() || busy_ || !paired_ || (mode_!="trade" && !sale() && mode_!="battle") || transaction_.isEmpty())return;
    if(journal_.isEmpty() || journal_["stage"]=="complete" && journal_["id"]!=transaction_) {
        if(!confirmed_ || !remoteConfirmed_)return;
        stage_="saving";message_="Protecting both saves…";
        const auto offered=seller()?localChoice_:remoteChoice_;
        QJsonObject request{{"id",transaction_},{"peer",peerId_},{"proposal",proposal()},{"kind",mode_},{"seller",seller()},
            {"price",sale()?offered["price"]:QJsonValue(0)},
            {"slot",localChoice_["slot"]},{"save",localChoice_["save"]},{"outgoing",localChoice_["pokemon"]},{"incoming",remoteChoice_["pokemon"]}};
        if(mode_=="battle") {
            request["host"]=host();request["bag"]=localChoice_["bag"];
            request["terms"]=QJsonObject{{"stake",stake_},{"amount",stakeAmount_},{"slot",localChoice_["stakeSlot"]},
                {"outgoing",localChoice_["stakePokemon"]},{"incoming",remoteChoice_["stakePokemon"]}};
        }
        operation("prepare",request,[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();});return;
    }
    if(journal_["id"]!=transaction_ || remoteJournal_["id"]!=transaction_)return;
    if((remoteJournal_["stage"]=="absent" || remoteJournal_["stage"]=="cancelled") && (journal_["stage"]=="prepared" || journal_["stage"]=="reserved" && journal_["checkpoint"].toObject().isEmpty())) {
        operation("abort-prepared",{{"id",transaction_}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});stage_="finished";message_="Trade cancelled. Neither save was changed.";});return;
    }
    if(journal_["proposal"]!=remoteJournal_["proposal"] || journal_["peer"]!=peerId_ || remoteJournal_["peer"]!=localId()){fail("The recovered trade does not match this partner.");return;}
    const auto local=journal_["stage"].toString(),remote=remoteJournal_["stage"].toString();
    if(mode_=="battle" && (local=="reserved" || remote=="reserved")) {
        if(local=="reserved" && remote=="reserved") {
            if(recovering_){if(host())finishBattle(journal_["checkpoint"].toObject());}
            else if(host() && !battleStarting_)beginBattle();
        } else if(local=="reserved")finishBattle(remoteJournal_["checkpoint"].toObject());
        return;
    }
    if(mode_=="battle" && journal_["checkpoint"]!=remoteJournal_["checkpoint"]){fail("Reconnect to agree on the same battle result.");return;}

    const bool localCommitted=local=="committed" || local=="complete",remoteCommitted=remote=="committed" || remote=="complete";
    if(!localCommitted && (host() || remoteCommitted)) {
        stage_="saving";message_="Saving the exchange…";
        operation("commit",{{"id",transaction_},{"peerAfter",remoteJournal_["after"]}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();});return;
    }
    if(localCommitted && remoteCommitted && local!="complete") {
        operation("finish",{{"id",transaction_},{"peerAfter",remoteJournal_["after"]}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});stage_="finished";message_=mode_=="battle"?battleResult():mode_=="sale"?"Sale complete! Pokemon and payment saved.":mode_=="gift"?"Gift delivered!":"Trade complete! Your new partner is waiting in Emerald.";emit saveChanged();});return;
    }
    if(local=="complete" && remoteCommitted){stage_="finished";message_=mode_=="battle"?battleResult():mode_=="sale"?"Sale complete! Pokemon and payment saved.":mode_=="gift"?"Gift delivered!":"Trade complete! Your new partner is waiting in Emerald.";emit changed();}
}
void LinkController::recover(const QString& id) {
    if(QUuid(id).isNull()){fail("The pending trade ID is invalid.");return;}
    recovering_=true;mode_="trade";transaction_=id;stage_="saving";message_="Reconnecting the saved exchange…";
    operation("status",{{"id",id}},[this](QJsonObject result){
        if(online_&&!result.isEmpty()&&(result["peer"]!=peerId_||result["kind"]=="battle")) {
            fail("This saved exchange belongs to a different session.");return;
        }
        if(result.isEmpty()) {
            send("receipt",{{"receipt",QJsonObject{{"id",transaction_},{"peer",peerId_},{"stage","absent"}}}});
            stage_="waiting";message_="Waiting for your friend's recovery receipt";return;
        }
        journal_=result;mode_=journal_["kind"].toString("trade");send("receipt",{{"receipt",journal_}});advanceTrade();
    });
}
QString LinkController::battleResult() const {
    const auto result=journal_["outcome"].toString();
    return result=="win"?"You win! Battle saved.":result=="loss"?"Your friend wins! Battle saved.":result=="cancel"?"Battle ended. Stakes kept; used medicine saved.":"A draw! Battle saved.";
}
void LinkController::beginBattle() {
    battleStarting_=true;send("begin");stage_="starting";message_="Let the battle begin!";
    std::array<int,4> seed;for(auto& value:seed)value=int(QRandomGenerator::global()->bounded(65536u));
#ifdef Q_OS_WIN
    const auto node=runtime_+"/bin/node.exe";
#else
    const auto node=runtime_+"/bin/node";
#endif
    QJsonArray teams,bags;
    for(const auto& choice:{localChoice_,remoteChoice_}){QJsonArray team;for(const auto& member:choice["team"].toArray())team.append(member.toObject()["battle"]);teams.append(team);bags.append(choice["bag"]);}
    if(!battle_.beginLink(node,runtime_+"/src/integrations/practice/emerald-worker.cjs",runtime_+"/node_modules/pokemon-showdown",
        {{"protocol",1},{"teams",teams},{"bags",bags}},seed))fail("The battle engine could not start.");
    emit changed();
}
void LinkController::showBattle(const QJsonObject& state) {
    playbackTimer_.stop();
    const QJsonArray teams=host()?QJsonArray{localChoice_["team"],remoteChoice_["team"]}:QJsonArray{remoteChoice_["team"],localChoice_["team"]};
    playback_.load(battleState_,state,teams);
    battleState_=state;localMove_=remoteMove_=-1;moveSent_=turnSubmitted_=false;focus_=0;bench_=bag_=false;bagItem_=0;
    if(playback_.active()) {
        stage_="events";message_="The turn is playing out";
        playbackTimer_.start(playback_.event()["duration"].toInt());
    } else finishPlayback();
    emit changed();
}
void LinkController::finishPlayback() {
    playbackTimer_.stop();
    const auto sides=battleState_["sides"].toArray();if(sides.size()!=2)return;
    if(battleState_["ended"].toBool()){finishBattle(battleState_);return;}
    if(host() && forfeitSide_>=0) {const int side=forfeitSide_;forfeitSide_=-1;concede(side);return;}
    bench_=sides[host()?0:1].toObject()["forceSwitch"].toBool();
    if(sides[host()?0:1].toObject()["wait"].toBool()){localMove_=9;moveSent_=true;}
    if(host() && sides[1].toObject()["wait"].toBool())remoteMove_=9;
    stage_=moveSent_?"waiting":"moves";
    message_=moveSent_?"Your friend is choosing a replacement":bench_?"Choose your next Pokemon":QString("Turn %1").arg(battleTurn());
    tryBattleTurn();
}
void LinkController::tryBattleTurn() {
    if(!host() || playback_.active() || turnSubmitted_ || localMove_<0 || remoteMove_<0 || journal_["stage"]!="reserved")return;
    turnSubmitted_=true;stage_="waiting";message_="Playing this turn...";
    if(!battle_.choose(localMove_,remoteMove_))fail("This battle move is unavailable.");
}
void LinkController::finishBattle(const QJsonObject& state) {
    if(busy_ || journal_["stage"]!="reserved")return;
    if(!state.isEmpty())battleState_=state;
    stage_="saving";message_="Saving the battle result…";
    operation("battle-finish",{{"id",transaction_},{"state",state}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();});
}
void LinkController::battleChanged() {
    if(journal_["stage"]!="reserved")return;
    if(!host() || mode_!="battle" || battle_.state().isEmpty() || busy_)return;
    const auto state=battle_.state();if(state==battleState_)return;
    operation("battle-checkpoint",{{"id",transaction_},{"state",state}},[this,state](QJsonObject result){
        journal_=result;showBattle(state);send("battle",{{"state",state}});
    });
}
void LinkController::battleRules(const QString& kind,int amount) {
    if(pending() || localChoice_.isEmpty() || !QStringList{"none","money","pokemon"}.contains(kind))return;
    // A new proposal invalidates even an in-flight Ready verification. Its
    // callback must not silently approve terms that were not on that screen.
    if(busy_){++generation_;busy_=false;}
    confirmed_=remoteConfirmed_=false;stake_=kind;stakeAmount_=std::clamp(amount,1,999999);localChoice_["stake"]=kind;localChoice_["amount"]=stakeAmount_;
    localChoice_.remove("stakePokemon");localChoice_.remove("stakeSlot");remoteChoice_={};focus_=0;
    if(kind=="pokemon"){stage_="stake";collection_=-1;message_="Choose your stake";emit changed();}
    else publishOffer();
}
void LinkController::concede(int side) {
    if(!host() || journal_["stage"]!="reserved" || side<0 || side>1)return;
    if(busy_ || playback_.active() || turnSubmitted_){forfeitSide_=side;return;}
    auto state=journal_["checkpoint"].toObject();
    if(state["ended"].toBool() || state["sides"].toArray().size()!=2)return;
    state["request"]=state["request"].toInt()+1;state["ended"]=true;
    state["winner"]=side==0?"Partner 2":"Partner 1";
    state["events"]=QJsonArray{QString("|message|Partner %1 conceded the battle.").arg(side+1)};
    finishBattle(state);
}
void LinkController::submitMove(int slot) {
    moveSent_=true;stage_="waiting";message_="Waiting for your friend's move";
    if(host()){localMove_=slot;tryBattleTurn();}
    else send("move",{{"turn",battleState_["turn"]},{"request",battleState_["request"]},{"slot",slot}});
    emit changed();
}
void LinkController::receive(const QJsonObject& input) {
    lastMessage_=QDateTime::currentMSecsSinceEpoch();if(input["version"].toInt()!=2){disconnectTransport();return;}
    const auto type=input["type"].toString();if(type=="ping")return;
    if(type=="hello" && peerId_.isEmpty()) {
        const auto id=input["id"].toString(),nonce=input["nonce"].toString();
        if(QUuid(id).isNull() || QUuid(nonce).isNull() || id==localId() || pending() && journal_["peer"]!=id){disconnectTransport();return;}
        if(!invitationsAllowed_ || !visibleNearby_ || (!directPeer_.isEmpty() && directPeer_!=id)){disconnectTransport();return;}
        peerId_=id;peerNonce_=nonce;peerName_=input["name"].toString().left(32);peerPending_=input["pending"].toString();
        const auto code=digest({{"a",host()?nonce_:peerNonce_},{"b",host()?peerNonce_:nonce_}}).left(8).toUInt(nullptr,16)%1000000;
        pin_=QString::number(code).rightJustified(6,'0');
        if(directPeer_==id && directAccepted_)accepted_=true;
        if(accepted_)send("accept",{{"code",pin_}});emit changed();return;
    }
    if(type=="accept" && !peerId_.isEmpty() && input["code"].toString()==pin_){peerAccepted_=true;pairReady();return;}
    if(!paired_)return;
    if(type=="identity"){peerName_=input["name"].toString().left(32);emit changed();return;}
    if(type=="activity-invite") {
        const auto id=input["id"].toString(),mode=input["mode"].toString();
        if(QUuid(id).isNull() || !QStringList{"battle","trade","sale","gift"}.contains(mode) || (online_ && mode=="battle"))return;
        if(!invitationsAllowed_ || pending() || busy_ || !mode_.isEmpty() || (!inviteId_.isEmpty() && (inviteOwner_==peerId_ || host()))) {
            send("activity-cancel",{{"id",id}});return;
        }
        if(!inviteId_.isEmpty())send("activity-cancel",{{"id",inviteId_}});
        inviteId_=id;inviteMode_=mode;inviteOwner_=peerId_;invitationTimer_.start();emit changed();return;
    }
    if(type=="activity-cancel" && input["id"]==inviteId_ && !inviteId_.isEmpty()) {
        invitationTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();emit changed();return;
    }
    if(type=="activity-accept" && input["id"]==inviteId_ && inviteOwner_==localId() && !inviteId_.isEmpty()) {
        const auto mode=inviteMode_;invitationTimer_.stop();inviteId_.clear();inviteMode_.clear();inviteOwner_.clear();
        sellerId_=localId();startMode(mode);emit workspaceRequested();return;
    }
    if(type=="problem"){message_=input["message"].toString().left(180);stage_="error";emit changed();return;}
    if(type=="rules" && !host() && mode_=="battle" && !pending()) {battleRules(input["stake"].toString(),input["amount"].toInt());return;}
    if(type=="forfeit" && host() && mode_=="battle" && pending()){concede(1);return;}
    if(type=="cancel" && !pending() && !busy_){battle_.cancel();resetChoice();stage_="lobby";message_="Your friend returned to the counter.";emit changed();return;}
    if(type=="offer" && input["mode"]==mode_ && !confirmed_ && (stage_=="choose" || stage_=="stake" || stage_=="price" || stage_=="waiting" || stage_=="review")) {
        const auto offer=input["offer"].toObject();
        const bool remoteBuyer=sale() && seller();
        if(offer["name"].toString().size()>24 || offer["target"].toString().size()>100 || (!remoteBuyer && (offer["slot"].toInt(-1)<0 || offer["slot"].toInt()>(mode_=="battle"?5:425))))return;
        if((mode_=="trade" || sale() && !seller()) && importEmeraldLinkRecord(offer["pokemon"].toObject()).isEmpty()){fail("This Pokemon cannot be exchanged here yet.");return;}
        if(sale() && offer["role"].toString()!=(remoteBuyer?"buyer":"seller"))return;
        if(mode_=="battle" && (offer["team"].toArray().isEmpty() || offer["team"].toArray().size()>6)){fail("Your friend needs the team-battle update.");return;}
        if(mode_=="battle" && (offer["stake"].toString("none")!=stake_ || offer["amount"].toInt(1000)!=stakeAmount_))return;
        remoteChoice_=offer;if(!localChoice_.isEmpty() && stage_!="price" && stage_!="stake")review();return;
    }
    if(type=="confirm" && !localChoice_.isEmpty() && !remoteChoice_.isEmpty() && input["proposal"]==proposal()) {
        remoteConfirmed_=true;
        if(confirmed_ && host() && !busy_) {confirmed_=false;stage_="review";confirm();}return;
    }
    if(type=="prepare" && !host() && (mode_=="trade" || sale() || mode_=="battle") && confirmed_ && remoteConfirmed_ && input["proposal"]==proposal() && !QUuid(input["id"].toString()).isNull()) {
        transaction_=input["id"].toString();advanceTrade();return;
    }
    if(type=="receipt" && (mode_=="trade" || sale() || mode_=="battle")) {
        const auto receipt=input["receipt"].toObject();
        if(receipt["id"]!=transaction_ || receipt["peer"]!=localId() || !QStringList{"reserved","prepared","commit","committed","complete","absent","cancelled"}.contains(receipt["stage"].toString()))return;
        if(receipt["stage"]=="cancelled" && !pending()){stage_="finished";message_="Trade cancelled. Neither save was changed.";emit changed();return;}
        remoteJournal_=receipt;advanceTrade();return;
    }
    if(type=="begin" && !host() && mode_=="battle" && confirmed_ && remoteConfirmed_){battleStarting_=true;stage_="starting";message_="Let the battle begin!";emit changed();return;}
    if(type=="battle" && !host() && mode_=="battle" && confirmed_ && remoteConfirmed_) {
        const auto state=input["state"].toObject();
        if(state["sides"].toArray().size()!=2 || state["turn"].toInt()<1 || state["turn"].toInt()>201)return;
        if(!battleState_.isEmpty() && state["request"].toInt()<=battleState_["request"].toInt())return;
        operation("battle-checkpoint",{{"id",transaction_},{"state",state}},[this,state](QJsonObject result){journal_=result;showBattle(state);});return;
    }
    if(type=="move" && host() && mode_=="battle" && input["request"]==battleState_["request"] && input["turn"]==battleState_["turn"] && remoteMove_<0) {
        const int move=input["slot"].toInt(-1);if(move<0 || move>255)return;remoteMove_=move;
        tryBattleTurn();return;
    }
}
void LinkController::activate(int index) {
    if(busy_ || stage_=="events")return;
    if(invitationOpen()){answerInvitation(true);return;}
    if(stage_=="browse"){
        const auto options=rows();if(index<0 || index>=options.size())return;const auto row=options[index].toMap();
        if(row["direct"].toBool()) {
            directPeer_=row["id"].toString();directName_=row["name"].toString();directIncoming_=false;directAccepted_=true;
            directConnecting_=false;invitationTimer_.stop();connectionTimer_.start(60000);
            nearby_.request({{"op","invite"},{"peer",directPeer_}});emit changed();
        }else peer_.connectId(row["id"].toString());return;
    }
    if(stage_=="lobby"){
        const auto mode=activityModes().value(index);if(mode.isEmpty())return;
        inviteActivity(mode);return;
    }
    if(stage_=="choose" || stage_=="stake"){choose(index);return;}
    if(stage_=="price"){
        localChoice_["price"]=price_;publishOffer();return;
    }
    if(stage_=="review"){confirm();return;}
    if(stage_=="concede") {
        if(host())concede(0);else {send("forfeit");stage_="waiting";message_="Finishing the battle…";emit changed();}return;
    }
    if(stage_=="moves" && !moveSent_) {
        const auto choices=rows();if(index<0 || index>=choices.size())return;
        const int slot=choices[index].toMap()["slot"].toInt();
        if(bag_ && !bagItem_ && choices[index].toMap()["bag"].toBool()){bagItem_=choices[index].toMap()["item"].toInt();focus_=0;emit changed();return;}
        submitMove(slot);return;
    }
    if(stage_=="finished" || stage_=="error") {
        if(pending()){disconnectTransport();return;}
        if(stage_=="error" && !paired_) {
            resetChoice();leave();return; // Return to Center, not an empty peer list.
        }
        battle_.cancel();resetChoice();stage_=paired_?"lobby":"browse";message_="What shall we do together?";emit changed();
    }
}
void LinkController::dispatch(Action action) {
    if(invitationOpen()){if(action==Action::Confirm)answerInvitation(true);else if(action==Action::Back)answerInvitation(false);return;}
    if(action==Action::Secondary && stage_=="lobby"){disconnectSession();return;}
    if(stage_=="events")return;
    if(!busy_ && stage_=="moves") {
        const auto sides=battleState_["sides"].toArray();
        const bool forced=sides.size()==2 && sides[host()?0:1].toObject()["forceSwitch"].toBool();
        if(forced && (action==Action::Secondary || action==Action::ToggleContinue)){bench_=true;bag_=false;emit changed();return;}
        if(action==Action::Secondary){bench_=!bench_;bag_=false;bagItem_=0;focus_=0;emit changed();return;}
        if(action==Action::ToggleContinue){bag_=!bag_;bench_=false;bagItem_=0;focus_=0;emit changed();return;}
        if(action==Action::Back && (bag_ || bench_)){if(bagItem_)bagItem_=0;else bag_=bench_=false;focus_=0;emit changed();return;}
    }
    if(!busy_ && canSetTerms()) {
        if(action==Action::LocalAction) {
            const auto kind=stake_=="none"?"money":stake_=="money"?"pokemon":"none";
            send("rules",{{"stake",kind},{"amount",stakeAmount_}});battleRules(kind,stakeAmount_);return;
        }
        if(stake_=="money" && (action==Action::Up || action==Action::Down || action==Action::Left || action==Action::Right)) {
            if(action==Action::Left)priceStep_=std::max(1,priceStep_/10);
            if(action==Action::Right)priceStep_=std::min(100000,priceStep_*10);
            if(action==Action::Up || action==Action::Down) {
                const int amount=std::clamp(stakeAmount_+(action==Action::Up?priceStep_:-priceStep_),1,999999);
                send("rules",{{"stake","money"},{"amount",amount}});battleRules("money",amount);
            }emit changed();return;
        }
    }
    if(!busy_ && (stage_=="stake" || stage_=="choose" && mode_!="battle") && (action==Action::Secondary || action==Action::ToggleContinue)) {
        collection_=(collection_+1+(action==Action::Secondary?-1:1)+15)%15-1;focus_=0;emit changed();return;
    }
    if(action==Action::Back && !busy_) {
        if(stage_=="concede"){stage_=moveSent_?"waiting":"moves";message_=QString("Turn %1").arg(battleTurn());emit changed();return;}
        if(stage_=="finished" && !pending()){activate(0);return;}
        if(pending() && mode_=="battle" && journal_["stage"]=="reserved" && transportConnected()) {
            if(!battleState_.isEmpty()){stage_="concede";message_="Concede? Your friend wins this battle and the agreed stake.";emit changed();return;}
            disconnectTransport();leave();emit closeRequested();return;
        }
        if(pending()){disconnectTransport();leave();emit closeRequested();return;}
        if((mode_=="trade" || sale()) && confirmed_){disconnectTransport();leave();emit closeRequested();return;}
        if(stage_=="choose" || stage_=="stake" || stage_=="price" || stage_=="review" || stage_=="waiting" || stage_=="moves" || stage_=="finished") {
            send("cancel");battle_.cancel();resetChoice();stage_="lobby";message_="What shall we do together?";emit changed();return;
        }
        leave();emit closeRequested();return;
    }
    if(action==Action::Confirm){activate(focus_);return;}
    if(stage_=="price" && !busy_) {
        if(action==Action::Right)priceStep_=std::min(100000,priceStep_*10);
        if(action==Action::Left)priceStep_=std::max(1,priceStep_/10);
        const int delta=action==Action::Up?priceStep_:action==Action::Down?-priceStep_:0;
        price_=std::clamp(price_+delta,1,999999);emit changed();return;
    }
    const int count=rows().size();if(!count || busy_)return;
    const int columns=stage_=="choose" || stage_=="stake" || stage_=="moves"?2:1;
    if(action==Action::Left)focus_=std::max(0,focus_-1);
    if(action==Action::Right)focus_=std::min(count-1,focus_+1);
    if(action==Action::Up)focus_=std::max(0,focus_-columns);
    if(action==Action::Down)focus_=std::min(count-1,focus_+columns);
    emit changed();
}
}
