#include "LinkController.h"
#include "integrations/progress/EmeraldPractice.h"
#include "integrations/progress/EmeraldLink.h"
#include "integrations/progress/EmeraldParty.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUuid>
#include <QSysInfo>
#include <algorithm>
namespace trainer {
namespace {
QString digest(const QJsonObject& value){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(value).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
}
LinkController::LinkController(QObject* parent):QObject(parent),peer_(this),battle_(this) {
    connect(&peer_,&LocalLinkPeer::changed,this,&LinkController::changed);
    connect(&peer_,&LocalLinkPeer::error,this,&LinkController::fail);
    connect(&peer_,&LocalLinkPeer::received,this,&LinkController::receive);
    connect(&peer_,&LocalLinkPeer::connectedToPeer,this,[this]{
        nonce_=uuid();peerId_.clear();peerName_.clear();pin_.clear();peerNonce_.clear();
        accepted_=peerAccepted_=paired_=false;stage_="pair";lastMessage_=QDateTime::currentMSecsSinceEpoch();
        message_="Compare the code on both consoles";
        send("hello",{{"id",peer_.id()},{"name",peer_.name()},{"trainer",trainerName_},{"nonce",nonce_},{"pending",pending()?journal_["id"]:QJsonValue()}});emit changed();
    });
    connect(&peer_,&LocalLinkPeer::disconnectedFromPeer,this,[this]{
        battle_.cancel();paired_=accepted_=peerAccepted_=false;peerId_.clear();pin_.clear();
        stage_="browse";message_=pending()?"Trade paused. Reconnect these same consoles to finish.":"Open Link Counter on both consoles on the same Wi-Fi.";
        resetChoice();emit changed();
    });
    connect(&battle_,&PracticeSession::changed,this,&LinkController::battleChanged);
    connect(&battle_,&PracticeSession::stopped,this,[this](const QString& reason){if(open_ && mode_=="battle" && stage_!="finished" && peer_.connected() && !reason.isEmpty())fail(reason);});
    heartbeat_.setInterval(2000);
    connect(&heartbeat_,&QTimer::timeout,this,[this]{
        if(!peer_.connected())return;
        if(QDateTime::currentMSecsSinceEpoch()-lastMessage_>12000){peer_.disconnectPeer();return;}
        send("ping");
    });
}
void LinkController::configure(const QString& root,const QString& id,const QString& name,Backend backend,
        PracticeController::Verifier verify,const QJsonObject& pending) {
    runtime_=root;backend_=std::move(backend);verify_=std::move(verify);journal_=pending;peer_.configure(id,name);
}
void LinkController::setObservation(const PracticeSource& source,const GameProgress& progress,const QVariantList& actors) {
    if(active() && !busy_ && !pending() && !source.trainerId.isEmpty() &&
        (source.trainerId!=source_.trainerId || source.adventureId!=source_.adventureId))peer_.disconnectPeer();
    // Save-service inspection temporarily emits Checking/Unavailable. It is not
    // a new Party. Actual source verification precedes every action.
    if(progress.availability==ProgressAvailability::Available && (!active() || stage_=="browse" || stage_=="lobby")) {
        source_=source;progress_=progress;actors_=actors;
    } else if(!active()){source_=source;progress_=progress;actors_=actors;}
}
void LinkController::enter() {
    if(!pending())journal_={};
    open_=true;focus_=0;stage_="browse";
    message_=pending()?"Trade paused. Reconnect the same consoles to finish.":"Open Link Counter on both consoles on the same Wi-Fi.";
    if(!backend_){message_="Link requires an installed Emerald Adventure.";emit changed();return;}
    peer_.open();heartbeat_.start();emit changed();
}
void LinkController::leave(){if(busy_)return;open_=false;++generation_;heartbeat_.stop();peer_.close();battle_.cancel();emit changed();}
void LinkController::send(const QString& type,QJsonObject fields){fields["type"]=type;fields["version"]=1;peer_.send(fields);}
void LinkController::fail(const QString& text){message_=text;stage_="error";if(paired_)send("problem",{{"message",text.left(180)}});emit changed();}
void LinkController::resetChoice(){mode_.clear();localChoice_={};remoteChoice_={};remoteJournal_={};transaction_.clear();confirmed_=remoteConfirmed_=false;focus_=0;localMove_=remoteMove_=-1;moveSent_=false;battleState_={};}
void LinkController::verify(std::function<void()> next) {
    if(busy_ || !verify_){if(!verify_)fail("Choose a supported Emerald Adventure first.");return;}
    busy_=true;emit changed();const auto generation=++generation_;
    QTimer::singleShot(6000,this,[this,generation]{if(generation==generation_ && busy_){++generation_;busy_=false;fail("The Party check timed out. Reconnect to try again.");}});
    verify_(source_,progress_,this,[this,generation,next](bool ok){
        if(generation!=generation_)return;busy_=false;++generation_;
        if(!paired_ || !peer_.connected()){emit changed();return;}
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
QVariantList LinkController::rows() const {
    if(stage_=="browse")return peer_.peers();
    if(stage_=="lobby")return {QVariantMap{{"name","Friendly battle"},{"detail","One partner each · Gen III"}},QVariantMap{{"name","Trade Pokemon"},{"detail","Exchange saved Party members"}}};
    if(stage_=="choose") {
        QVariantList out;if(!progress_.party)return out;
        for(int slot=0;slot<progress_.party->party.size();++slot){const auto member=partyMember(slot);if(member.isEmpty())continue;auto row=display(member);row["slot"]=slot;row["detail"]=QString("Lv. %1").arg(row["level"].toInt());out.append(row);}return out;
    }
    if(stage_=="moves") {
        QVariantList out;const auto sides=battleState_["sides"].toArray();if(sides.size()!=2)return out;
        for(const auto& v:sides[host()?0:1].toObject()["moves"].toArray()){auto row=v.toObject().toVariantMap();row["name"]=row["move"];row["detail"]=QString("%1 / %2 PP").arg(row["pp"].toInt()).arg(row["maxPp"].toInt());out.append(row);}return out;
    }
    return {};
}
QVariantList LinkController::fighters() const {
    QVariantList out;const auto sides=battleState_["sides"].toArray();
    const std::array<QJsonObject,2> members{localChoice_,remoteChoice_};
    for(int i=0;i<2;++i){auto row=display(members[i]);
        if(sides.size()==2){const auto side=sides[host()?i:1-i].toObject();row["battleHp"]=side["hp"].toInt();row["maxHp"]=side["maxHp"].toInt();row["hpRatio"]=side["hp"].toDouble()/std::max(1,side["maxHp"].toInt());row["battleStatus"]=side["status"].toString();}
        out.append(row);
    }return out;
}
QString LinkController::turnSummary() const {
    QStringList lines;
    for(const auto& event:battleState_["events"].toArray()) {
        const auto fields=event.toString().split('|');if(fields.size()<3)continue;
        const auto kind=fields[1];
        const bool local=fields[2].startsWith(host()?"p1":"p2");
        const auto name=(local?localChoice_:remoteChoice_)["name"].toString().left(24);
        if(kind=="move" && fields.size()>3)lines<<name+" used "+fields[3].left(32)+"!";
        else if(kind=="faint")lines<<name+" fainted!";
        else if(kind=="-miss")lines<<"The attack missed!";
        else if(kind=="cant")lines<<name+" couldn't move!";
        else if(kind=="-supereffective")lines<<"Super effective!";
        else if(kind=="-immune")lines<<"It had no effect!";
    }
    return lines.mid(0,5).join("  ·  ");
}
void LinkController::pairReady() {
    if(!accepted_ || !peerAccepted_ || paired_)return;paired_=true;pin_.clear();
    if(pending() || !peerPending_.isEmpty()){recover(pending()?journal_["id"].toString():peerPending_);return;}
    stage_="lobby";message_="What shall we do together?";focus_=0;emit changed();
}
void LinkController::startMode(const QString& mode) {
    if(pending() || !paired_ || busy_)return;
    resetChoice();mode_=mode;stage_="choose";message_=mode=="battle"?"Choose your battle partner":"Choose a Pokemon to offer";
    if(rows().isEmpty()){fail("Choose an Emerald Adventure with a saved Party, then reconnect.");return;}
    emit changed();
}
void LinkController::choose(int index) {
    const auto options=rows();if(index<0 || index>=options.size())return;
    const int slot=options[index].toMap()["slot"].toInt();
    verify([this,slot]{
        localChoice_=partyMember(slot);
        if(mode_=="trade")operation("inspect",{{"slot",slot}},[this](QJsonObject result){
            if(!paired_)return;
            localChoice_["pokemon"]=result["pokemon"];localChoice_["save"]=result["save"];
            send("offer",{{"offer",localChoice_},{"mode",mode_}});review();
        });else {send("offer",{{"offer",localChoice_},{"mode",mode_}});review();}
    });
}
QString LinkController::proposal() const {
    return digest({{"host",host()?localChoice_:remoteChoice_},{"guest",host()?remoteChoice_:localChoice_},{"mode",mode_}});
}
void LinkController::review() {
    if(localChoice_.isEmpty() || remoteChoice_.isEmpty()){stage_="waiting";message_="Waiting for your friend's choice";}
    else {stage_="review";message_=mode_=="trade"?"Trade these Pokemon? Both players must confirm.":"Ready for a friendly battle?";}
    focus_=0;emit changed();
}
void LinkController::confirm() {
    if(confirmed_ || busy_ || stage_!="review")return;
    verify([this]{confirmed_=true;send("confirm",{{"proposal",proposal()}});stage_="waiting";message_="Waiting for your friend";
        if(remoteConfirmed_) {
            if(mode_=="battle" && host()) {
                send("begin");stage_="starting";message_="Let the battle begin!";
                std::array<int,4> seed;for(auto& value:seed)value=int(QRandomGenerator::global()->bounded(65536u));
#ifdef Q_OS_WIN
                const auto node=runtime_+"/bin/node.exe";
#else
                const auto node=runtime_+"/bin/node";
#endif
                if(!battle_.beginLink(node,runtime_+"/src/integrations/practice/emerald-worker.cjs",runtime_+"/node_modules/pokemon-showdown",
                    {{"protocol",1},{"members",QJsonArray{localChoice_["battle"],remoteChoice_["battle"]}}},seed))fail("The battle engine could not start.");
            } else if(mode_=="trade" && host()) {transaction_=uuid();send("prepare",{{"id",transaction_},{"proposal",proposal()}});advanceTrade();}
        }emit changed();
    });
}
void LinkController::advanceTrade() {
    if(busy_ || !paired_ || mode_!="trade" || transaction_.isEmpty())return;
    if(journal_.isEmpty() || journal_["stage"]=="complete" && journal_["id"]!=transaction_) {
        if(!confirmed_ || !remoteConfirmed_)return;
        stage_="saving";message_="Protecting both saves…";
        operation("prepare",{{"id",transaction_},{"peer",peerId_},{"proposal",proposal()},
            {"slot",localChoice_["slot"]},{"save",localChoice_["save"]},{"outgoing",localChoice_["pokemon"]},{"incoming",remoteChoice_["pokemon"]}},
            [this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();});return;
    }
    if(journal_["id"]!=transaction_ || remoteJournal_["id"]!=transaction_)return;
    if((remoteJournal_["stage"]=="absent" || remoteJournal_["stage"]=="cancelled") && journal_["stage"]=="prepared") {
        operation("abort-prepared",{{"id",transaction_}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});stage_="finished";message_="Trade cancelled. Neither save was changed.";});return;
    }
    if(journal_["proposal"]!=remoteJournal_["proposal"] || journal_["peer"]!=peerId_ || remoteJournal_["peer"]!=peer_.id()){fail("The recovered trade does not match this partner.");return;}
    const auto local=journal_["stage"].toString(),remote=remoteJournal_["stage"].toString();
    const bool localCommitted=local=="committed" || local=="complete",remoteCommitted=remote=="committed" || remote=="complete";
    if(!localCommitted && (host() || remoteCommitted)) {
        stage_="saving";message_="Saving the exchange…";
        operation("commit",{{"id",transaction_},{"peerAfter",remoteJournal_["after"]}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();});return;
    }
    if(localCommitted && remoteCommitted && local!="complete") {
        operation("finish",{{"id",transaction_},{"peerAfter",remoteJournal_["after"]}},[this](QJsonObject result){journal_=result;send("receipt",{{"receipt",journal_}});stage_="finished";message_="Trade complete! Your new partner is waiting in Emerald.";emit saveChanged();});return;
    }
    if(local=="complete" && remoteCommitted){stage_="finished";message_="Trade complete! Your new partner is waiting in Emerald.";emit changed();}
}
void LinkController::recover(const QString& id) {
    if(QUuid(id).isNull()){fail("The pending trade ID is invalid.");return;}
    mode_="trade";transaction_=id;stage_="saving";message_="Reconnecting the saved exchange…";
    operation("status",{{"id",id}},[this](QJsonObject result){
        if(result.isEmpty()) {
            send("receipt",{{"receipt",QJsonObject{{"id",transaction_},{"peer",peerId_},{"stage","absent"}}}});
            stage_="waiting";message_="Waiting for your friend's recovery receipt";return;
        }
        journal_=result;send("receipt",{{"receipt",journal_}});advanceTrade();
    });
}
void LinkController::battleChanged() {
    if(!host() || mode_!="battle" || battle_.state().isEmpty())return;
    const auto state=battle_.state();if(state==battleState_)return;battleState_=state;
    send("battle",{{"state",state}});localMove_=remoteMove_=-1;moveSent_=false;focus_=0;
    stage_=state["ended"].toBool()?"finished":"moves";
    const auto winner=state["winner"].toString();
    message_=stage_=="finished"?(winner.isEmpty()?"A draw! Well played.":winner=="Partner 1"?"You win! Well played.":"Your friend wins! Well played."):QString("Turn %1 · Choose a move").arg(state["turn"].toInt());emit changed();
}
void LinkController::receive(const QJsonObject& input) {
    lastMessage_=QDateTime::currentMSecsSinceEpoch();if(input["version"].toInt()!=1){peer_.disconnectPeer();return;}
    const auto type=input["type"].toString();if(type=="ping")return;
    if(type=="hello" && peerId_.isEmpty()) {
        const auto id=input["id"].toString(),nonce=input["nonce"].toString();
        if(QUuid(id).isNull() || QUuid(nonce).isNull() || id==peer_.id() || pending() && journal_["peer"]!=id){peer_.disconnectPeer();return;}
        peerId_=id;peerNonce_=nonce;peerName_=input["name"].toString().left(48);peerPending_=input["pending"].toString();
        const auto trainer=input["trainer"].toString().left(32);if(!trainer.isEmpty())peerName_=trainer+" · "+peerName_;
        const auto code=digest({{"a",host()?nonce_:peerNonce_},{"b",host()?peerNonce_:nonce_}}).left(8).toUInt(nullptr,16)%1000000;
        pin_=QString::number(code).rightJustified(6,'0');emit changed();return;
    }
    if(type=="accept" && !peerId_.isEmpty() && input["code"].toString()==pin_){peerAccepted_=true;pairReady();return;}
    if(!paired_)return;
    if(type=="problem"){message_=input["message"].toString().left(180);stage_="error";emit changed();return;}
    if(type=="mode" && !pending() && (stage_=="lobby" || !host() && stage_=="waiting" && mode_.isEmpty()) && (input["mode"]=="trade" || input["mode"]=="battle")){
        // The host resolves simultaneous invitations so the consoles never
        // select different activities and wait for incompatible proposals.
        if(host())send("mode",{{"mode",input["mode"]}});
        startMode(input["mode"].toString());return;
    }
    if(type=="cancel" && !pending() && !busy_){battle_.cancel();resetChoice();stage_="lobby";message_="Your friend returned to the counter.";emit changed();return;}
    if(type=="offer" && input["mode"]==mode_ && !confirmed_ && (stage_=="choose" || stage_=="waiting" || stage_=="review")) {
        const auto offer=input["offer"].toObject();
        if(offer["name"].toString().size()>24 || offer["target"].toString().size()>100 || offer["slot"].toInt(-1)<0 || offer["slot"].toInt()>5)return;
        if(mode_=="trade" && importEmeraldLinkRecord(offer["pokemon"].toObject()).isEmpty()){fail("This Pokemon cannot be exchanged here yet.");return;}
        remoteChoice_=offer;if(!localChoice_.isEmpty())review();return;
    }
    if(type=="confirm" && !localChoice_.isEmpty() && !remoteChoice_.isEmpty() && input["proposal"]==proposal()) {
        remoteConfirmed_=true;
        if(confirmed_ && host() && !busy_) {confirmed_=false;stage_="review";confirm();}return;
    }
    if(type=="prepare" && !host() && mode_=="trade" && confirmed_ && remoteConfirmed_ && input["proposal"]==proposal() && !QUuid(input["id"].toString()).isNull()) {
        transaction_=input["id"].toString();advanceTrade();return;
    }
    if(type=="receipt" && mode_=="trade") {
        const auto receipt=input["receipt"].toObject();
        if(receipt["id"]!=transaction_ || receipt["peer"]!=peer_.id() || !QStringList{"prepared","commit","committed","complete","absent","cancelled"}.contains(receipt["stage"].toString()))return;
        if(receipt["stage"]=="cancelled" && !pending()){stage_="finished";message_="Trade cancelled. Neither save was changed.";emit changed();return;}
        remoteJournal_=receipt;advanceTrade();return;
    }
    if(type=="begin" && !host() && mode_=="battle" && confirmed_ && remoteConfirmed_){stage_="starting";message_="Let the battle begin!";emit changed();return;}
    if(type=="battle" && !host() && mode_=="battle" && confirmed_ && remoteConfirmed_) {
        const auto state=input["state"].toObject();
        if(state["sides"].toArray().size()!=2 || state["turn"].toInt()<1 || state["turn"].toInt()>201)return;
        if(!battleState_.isEmpty() && state["turn"].toInt()<=battleState_["turn"].toInt() && !state["ended"].toBool())return;
        battleState_=state;moveSent_=false;focus_=0;stage_=state["ended"].toBool()?"finished":"moves";
        const auto winner=state["winner"].toString();message_=stage_=="finished"?(winner.isEmpty()?"A draw! Well played.":winner=="Partner 2"?"You win! Well played.":"Your friend wins! Well played."):QString("Turn %1 · Choose a move").arg(state["turn"].toInt());emit changed();return;
    }
    if(type=="move" && host() && mode_=="battle" && input["turn"]==battleState_["turn"] && remoteMove_<0) {
        const int move=input["slot"].toInt(-1);if(move<0 || move>4)return;remoteMove_=move;
        if(localMove_>=0 && !battle_.choose(localMove_,remoteMove_))fail("This battle move is unavailable.");return;
    }
}
void LinkController::activate(int index) {
    if(busy_)return;
    if(stage_=="browse"){peer_.connectPeer(index);return;}
    if(stage_=="pair" && !pin_.isEmpty()){accepted_=true;send("accept",{{"code",pin_}});message_="Waiting for your friend's confirmation";pairReady();return;}
    if(stage_=="lobby"){
        const auto mode=index==0?"battle":"trade";send("mode",{{"mode",mode}});
        if(host())startMode(mode);else {stage_="waiting";message_="Inviting your friend…";emit changed();}return;
    }
    if(stage_=="choose"){choose(index);return;}
    if(stage_=="review"){confirm();return;}
    if(stage_=="moves" && !moveSent_) {
        const auto choices=rows();if(index<0 || index>=choices.size())return;
        const int slot=choices[index].toMap()["slot"].toInt();
        verify([this,slot]{moveSent_=true;stage_="waiting";message_="Waiting for your friend's move";
            if(host()){localMove_=slot;if(remoteMove_>=0 && !battle_.choose(localMove_,remoteMove_))fail("This battle move is unavailable.");}
            else send("move",{{"turn",battleState_["turn"]},{"slot",slot}});emit changed();});return;
    }
    if(stage_=="finished" || stage_=="error") {
        if(pending()){peer_.disconnectPeer();return;}
        // A fresh observation is required after a trade; leave/re-enter refreshes it.
        leave();emit closeRequested();
    }
}
void LinkController::dispatch(Action action) {
    if(action==Action::Back && !busy_) {
        if(pending()){peer_.disconnectPeer();leave();emit closeRequested();return;}
        if(mode_=="trade" && confirmed_){peer_.disconnectPeer();leave();emit closeRequested();return;}
        if(stage_=="choose" || stage_=="review" || stage_=="waiting" || stage_=="moves" || stage_=="finished") {
            send("cancel");battle_.cancel();resetChoice();stage_="lobby";message_="What shall we do together?";emit changed();return;
        }
        leave();emit closeRequested();return;
    }
    if(action==Action::Confirm){activate(focus_);return;}
    const int count=rows().size();if(!count || busy_)return;
    const int columns=stage_=="choose" || stage_=="moves"?2:1;
    if(action==Action::Left)focus_=std::max(0,focus_-1);
    if(action==Action::Right)focus_=std::min(count-1,focus_+1);
    if(action==Action::Up)focus_=std::max(0,focus_-columns);
    if(action==Action::Down)focus_=std::min(count-1,focus_+columns);
    emit changed();
}
}
