#include "CommunicationSettings.h"
#include "features/social/SocialController.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>

namespace trainer {
CommunicationSettings::CommunicationSettings(QObject* parent):QObject(parent) {
    refresh_.setInterval(5000);testLimit_.setSingleShot(true);testLimit_.setInterval(10000);
    scanLimit_.setSingleShot(true);scanLimit_.setInterval(3000);
    connect(&refresh_,&QTimer::timeout,this,&CommunicationSettings::scan);
    connect(&testLimit_,&QTimer::timeout,this,&CommunicationSettings::stopTest);
    connect(&scanLimit_,&QTimer::timeout,this,[this]{scan_.kill();});
    connect(&scan_,&QProcess::readyReadStandardOutput,this,[this]{scanBytes_+=scan_.readAllStandardOutput();if(scanBytes_.size()>1024*1024)scan_.kill();});
    connect(&scan_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){scanLimit_.stop();error_="Audio devices are unavailable.";emit changed();}});
    connect(&scan_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
        scanLimit_.stop();scanBytes_+=scan_.readAllStandardOutput();
        if(!active_)return;
        if(code||exit!=QProcess::NormalExit||scanBytes_.size()>1024*1024){error_="Couldn't refresh audio devices.";emit changed();return;}
        if(scanStage_<2){
            QVariantList found;
            for(const auto& item:QJsonDocument::fromJson(scanBytes_).array()){
                const auto device=item.toObject();const auto name=device["name"].toString();
                if(name.isEmpty()||(scanStage_==0&&(name.endsWith(".monitor")||device["description"].toString().startsWith("Monitor of",Qt::CaseInsensitive))))continue;
                found.append(QVariantMap{{"id",name},{"name",device["description"].toString(name)}});
            }
            (scanStage_==0?inputs_:outputs_)=found;
            ++scanStage_;scanBytes_.clear();
            scan_.start(QStandardPaths::findExecutable("pactl"),scanStage_==1?QStringList{"-f","json","list","sinks"}:QStringList{"get-default-source"});scanLimit_.start();
        }else {defaultInput_=QString::fromUtf8(scanBytes_).trimmed();emit changed();}
    });
    connect(&capture_,&QProcess::readyReadStandardOutput,this,[this]{
        meterBytes_+=capture_.readAllStandardOutput();int peak=0;
        const auto count=meterBytes_.size()/2;
        for(qsizetype i=0;i<count;++i)peak=qMax(peak,std::abs(int(qFromLittleEndian<qint16>(meterBytes_.constData()+i*2))));
        meterBytes_.remove(0,count*2);level_=qMin(100,peak*100/32767);emit meterChanged();
    });
    connect(&capture_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){if(testing_){error_="Couldn't open the microphone.";stopTest();}});
    connect(&capture_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){if(testing_){error_="Microphone disconnected or unavailable.";stopTest();}});
    connect(&avatar_,&SocialMedia::changed,this,[this]{
        if(pendingAvatar_&&avatar_.state()=="picture"){
            pendingAvatar_=false;
            send("profile-update",{{"field","avatar"},{"value","data:image/jpeg;base64,"+QString::fromLatin1(avatar_.bytes().toBase64())}});
            avatar_.clear();choosingAvatar_=false;focus_=1;
        }else if(pendingAvatar_&&avatar_.state()=="error"){pendingAvatar_=false;error_=avatar_.error();}
        emit changed();
    });
}
CommunicationSettings::~CommunicationSettings(){leave();scan_.kill();scan_.waitForFinished(300);capture_.waitForFinished(300);}
void CommunicationSettings::configure(SocialController* social){
    social_=social;
    connect(social,&SocialController::changed,this,[this]{
        const auto id=account()["userId"].toString();
        if(id!=accountId_){stopTest();pendingAvatar_=false;avatar_.clear();choosingAvatar_=false;textField_.clear();accountId_=id;focus_=0;}
        focus_=qBound(0,focus_,qMax(0,int(rows().size())-1));emit changed();
    });
}
QVariantMap CommunicationSettings::account() const{return social_?social_->account():QVariantMap{};}
QVariantMap CommunicationSettings::profile() const{return account()["profile"].toMap();}
QString CommunicationSettings::status() const{
    if(!error_.isEmpty())return error_;
    if(pendingAvatar_)return "Preparing your picture...";
    if(testing_)return "Speak to test your microphone";
    return account()["profileStatus"].toString();
}
void CommunicationSettings::send(const QString& op,const QVariantMap& args){if(social_)social_->reviewCommand(op,args);}
void CommunicationSettings::begin(){active_=true;error_.clear();scan();refresh_.start();emit changed();}
void CommunicationSettings::leave(){
    if(!active_)return;
    active_=false;refresh_.stop();scanLimit_.stop();scan_.kill();stopTest();
    pendingAvatar_=false;choosingAvatar_=false;avatar_.clear();textField_.clear();
}
void CommunicationSettings::scan(){
    if(!active_||scan_.state()!=QProcess::NotRunning)return;
    scanStage_=0;scanBytes_.clear();
    scan_.start(QStandardPaths::findExecutable("pactl"),{"-f","json","list","sources"});scanLimit_.start();
}
QString CommunicationSettings::deviceLabel(const QString& kind) const{
    const auto id=account()["audio"].toMap()[kind].toString();
    if(id.isEmpty())return "Follow system";
    for(const auto& value:kind=="input"?inputs_:outputs_)if(value.toMap()["id"]==id)return value.toMap()["name"].toString();
    return "Selected device disconnected";
}
QVariantList CommunicationSettings::rows() const{
    const auto state=account();const auto user=profile();const auto audio=state["audio"].toMap();
    auto row=[](QString title,QString detail,QString kind="action",QString section={}){return QVariantMap{{"title",title},{"detail",detail},{"kind",kind},{"section",section}};};
    if(choosingAvatar_){
        QVariantList result;
        for(const auto& item:pictures_){auto value=row(item.toMap()["name"].toString(),"","picture");value["image"]=QUrl::fromLocalFile(item.toMap()["path"].toString());result.append(value);}
        result.append(row("Remove picture","","action"));result.append(row("Cancel","","action"));return result;
    }
    if(state["userId"].toString().isEmpty())return {row(state["code"].toString().isEmpty()?"Sign in":"Cancel sign-in",state["code"].toString().isEmpty()?state["status"].toString():"Open fluxer.app/handoff · "+state["code"].toString())};
    QVariantList result{
        row("Display name",user["name"].toString().isEmpty()?state["name"].toString():user["name"].toString(),"action","Profile"),
        row("Profile picture","Choose from Pictures"),row("About me",user["bio"].toString().isEmpty()?"Add a little about yourself":user["bio"].toString()),
        row("Microphone",deviceLabel("input"),"choice","Audio"),row("Audio output",deviceLabel("output"),"choice"),
        row("Call volume","","volume"),row(testing_?"Stop microphone test":"Test microphone",testing_?"Listening locally · 10 seconds":"See your microphone level"),
        row("Do Not Disturb",state["doNotDisturb"].toBool()?"On":"Off","toggle","Notifications"),
        row("Notification sounds",state.value("notificationSound",true).toBool()?"On":"Off","toggle"),
        row("Hide message previews",state.value("privatePreviews",true).toBool()?"On":"Off","toggle")};
    auto volume=result[5].toMap();volume["level"]=audio.value("volume",100);result[5]=volume;
    for(int i=7;i<=9;++i){auto value=result[i].toMap();value["checked"]=value["detail"]=="On";result[i]=value;}
    const auto voice=state["voice"].toMap();
    const bool calling=voice["state"]=="connected"||voice["state"]=="reconnecting";
    auto mic=row("Microphone on",calling?(voice["muted"].toBool()?"Off":"On"):"No active call",calling?"toggle":"unavailable","Current call");mic["checked"]=!voice["muted"].toBool();mic["operation"]="voice-mute";result.append(mic);
    auto output=row("Hear conversation",calling?(voice["deaf"].toBool()?"Off":"On"):"No active call",calling?"toggle":"unavailable");output["checked"]=!voice["deaf"].toBool();output["operation"]="voice-output";result.append(output);
    auto logout=row("Sign out",user["tag"].toString(),"action","Account");logout["operation"]="logout";result.append(logout);
    return result;
}
void CommunicationSettings::cycleDevice(const QString& kind,int direction){
    const auto list=kind=="input"?inputs_:outputs_;QStringList ids{QString()};
    for(const auto& value:list)ids<<value.toMap()["id"].toString();
    const int index=ids.indexOf(account()["audio"].toMap()[kind].toString());
    if(kind=="input")stopTest();
    send("audio-settings",{{kind,ids[(qMax(0,index)+direction+ids.size())%ids.size()]}});
}
void CommunicationSettings::setVolume(int value){send("audio-settings",{{"volume",qBound(0,value,100)}});}
void CommunicationSettings::stopTest(){
    const bool was=testing_;testing_=false;testLimit_.stop();capture_.kill();meterBytes_.clear();level_=0;emit meterChanged();if(was)emit changed();
}
void CommunicationSettings::startTest(){
    if(capture_.state()!=QProcess::NotRunning)return;
    auto device=account()["audio"].toMap()["input"].toString();if(device.isEmpty())device=defaultInput_;
    bool exists=false;for(const auto& value:inputs_)if(value.toMap()["id"]==device)exists=true;
    if(!exists||device.endsWith(".monitor")){error_="Connect a microphone and try again.";emit changed();return;}
    testing_=true;meterBytes_.clear();error_.clear();
    capture_.start(QStandardPaths::findExecutable("parec"),{"--record","--raw","--format=s16le","--rate=24000","--channels=1","--latency-msec=80","--device="+device,"--client-name=TrainerOS Microphone Test"});
    testLimit_.start();emit changed();
}
void CommunicationSettings::activate(int index){
    if(index<0||index>=rows().size()||pendingAvatar_||account()["profileBusy"].toBool())return;
    focus_=index;error_.clear();
    if(choosingAvatar_){
        if(index<pictures_.size()){pendingAvatar_=true;avatar_.choosePicture(pictures_[index].toMap()["path"].toString());}
        else {if(index==pictures_.size())send("profile-update",{{"field","avatar"},{"value",""}});choosingAvatar_=false;focus_=1;}
    }else if(account()["userId"].toString().isEmpty())send(account()["code"].toString().isEmpty()?"login":"cancel-login");
    else if(index==0||index==2){textField_=index==0?"global_name":"bio";emit textRequested(index==0?"Display name":"About me",profile()[index==0?"name":"bio"].toString(),index==0?32:320);}
    else if(index==1){pictures_=avatar_.pictures();choosingAvatar_=true;focus_=0;}
    else if(index==3||index==4)cycleDevice(index==3?"input":"output",1);
    else if(index==6){if(testing_)stopTest();else startTest();}
    else if(index>=7&&index<=9)send(index==7?"dnd":index==8?"notification-sound":"private");
    else if(index>=10)send(rows()[index].toMap()["operation"].toString());
    emit changed();
}
void CommunicationSettings::applyText(const QString& text){
    if(textField_.isEmpty())return;
    send("profile-update",{{"field",textField_},{"value",text.trimmed()}});textField_.clear();
}
void CommunicationSettings::dispatch(Action action){
    if(action==Action::Back){
        if(choosingAvatar_){pendingAvatar_=false;avatar_.clear();choosingAvatar_=false;focus_=1;emit changed();}
        else {stopTest();emit backRequested();}return;
    }
    if(action==Action::Up)focus_=qMax(0,focus_-1);
    if(action==Action::Down)focus_=qMin(int(rows().size())-1,focus_+1);
    if(action==Action::Confirm){activate(focus_);return;}
    if(!choosingAvatar_&&!account()["userId"].toString().isEmpty()&&(action==Action::Left||action==Action::Right)){
        const int direction=action==Action::Left?-1:1;
        if(focus_==3||focus_==4)cycleDevice(focus_==3?"input":"output",direction);
        else if(focus_==5)setVolume(account()["audio"].toMap().value("volume",100).toInt()+direction*5);
        else if(focus_>=7&&focus_<=9&&rows()[focus_].toMap()["checked"].toBool()!=(direction>0))activate(focus_);
    }
    emit changed();
}
}
