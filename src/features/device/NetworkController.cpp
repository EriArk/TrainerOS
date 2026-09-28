#include "NetworkController.h"
#include <QJsonArray>
#include <algorithm>
namespace trainer {
NetworkController::NetworkController(QObject* parent):QObject(parent){
    poll_.setInterval(8000);
    connect(&poll_,&QTimer::timeout,this,[this]{if(active_&&!busy_&&prompt_.isEmpty()&&textMode_.isEmpty())run("list");});
}
void NetworkController::configure(NetworkService* service){
    service_=service;
    connect(service_,&NetworkService::event,this,&NetworkController::receive);
    connect(service_,&NetworkService::finished,this,[this](const QString& error){
        busy_=false;prompt_.clear();confirmation_.clear();textMode_.clear();pending_={};
        emit closeKeyboardRequested();
        status_=error;emit changed();
    });
}
void NetworkController::setActive(bool active){
    if(active_==active)return;
    active_=active;
    if(active){poll_.start();if(!busy_)run("list");}
    else {poll_.stop();leave();}
}
void NetworkController::leave(){
    if(busy_&&service_)service_->cancel();
    textMode_.clear();pending_={};prompt_.clear();confirmation_.clear();emit closeKeyboardRequested();emit changed();
}
void NetworkController::run(const QString& op,const QJsonObject& extra){
    if(busy_)return;
    if(!service_){status_="Connections are available on your handheld.";emit changed();return;}
    auto request=extra;request["op"]=op;request["kind"]=bluetooth_?"bluetooth":"wifi";
    busy_=true;status_=op=="list"?"Updating…":op=="scan"?"Looking nearby…":"Connecting…";
    emit changed();service_->request(request);
}
void NetworkController::receive(const QJsonObject& value){
    const auto type=value.value("event").toString();
    if(type=="snapshot"){
        const auto id=selected().value("id").toString();rows_=value.value("rows").toArray().toVariantList();
        focus_=std::clamp(focus_,0,std::max(0,int(rows_.size())-1));
        for(int i=0;i<rows_.size();++i)if(rows_[i].toMap().value("id").toString()==id){focus_=i;break;}
    }else if(type=="prompt"){
        if(!active_){if(service_)service_->cancel();return;}
        const auto kind=value.value("kind").toString();
        if(kind=="pin"||kind=="passkey"){
            textMode_=kind;emit textRequested(kind=="pin"?"Pairing PIN":"Pairing code",kind=="pin"?16:6,false);
        }else{
            confirmation_=kind=="display"?"display":"pair";
            prompt_=value.value("text").toString();
        }
    }
    emit changed();
}
QString NetworkController::confirmLabel()const{return confirmation_=="display"?"Wait":confirmation_=="forget"?"Forget":confirmation_=="disconnect"?"Disconnect":"Confirm";}
void NetworkController::selectFace(bool bluetooth){
    if(busy_||!prompt_.isEmpty()||!textMode_.isEmpty()||bluetooth_==bluetooth)return;
    bluetooth_=bluetooth;rows_.clear();focus_=0;run("list");
}
void NetworkController::activate(int index){
    if(!prompt_.isEmpty()){confirm();return;}
    if(busy_||index<0||index>=rows_.size())return;
    focus_=index;const auto row=selected();pending_={{"id",row.value("id").toString()}};
    if(row.value("connected").toBool()){
        confirmation_="disconnect";prompt_="Disconnect from "+row.value("title").toString()+"?";
    }else if(!bluetooth_&&!row.value("saved").toBool()&&row.value("security")=="unsupported"){
        status_="Set up this network in Desktop Mode first.";
    }else if(!bluetooth_&&!row.value("saved").toBool()&&row.value("security")!="open"){
        textMode_="password";emit textRequested("Wi-Fi password",64,true);
    }else {run(bluetooth_&&!row.value("saved").toBool()?"pair":"connect",pending_);}
    emit changed();
}
void NetworkController::confirm(){
    const auto action=confirmation_;
    if(action=="display")return;
    confirmation_.clear();prompt_.clear();
    if(action=="pair"){if(service_)service_->respond({{"accept",true}});}
    else if(action=="forget"||action=="disconnect")run(action,pending_);
    emit changed();
}
void NetworkController::applyText(const QString& text){
    const auto mode=textMode_;textMode_.clear();
    if(mode=="password"){
        if(text.toUtf8().size()<8||text.toUtf8().size()>64){status_="Use an 8–63 character password or a 64-digit hexadecimal key.";emit changed();return;}
        auto value=pending_;value["password"]=text;run("connect",value);
    }else if((mode=="pin"||mode=="passkey")&&service_)service_->respond({{"value",text}});
}
void NetworkController::cancelText(){
    if(textMode_=="pin"||textMode_=="passkey"){if(service_)service_->cancel();}
    textMode_.clear();pending_={};emit changed();
}
void NetworkController::dispatch(Action action){
    if(action==Action::Back){
        if(busy_){if(service_)service_->cancel();status_="Cancelling…";emit changed();return;}
        if(!prompt_.isEmpty()){prompt_.clear();confirmation_.clear();pending_={};emit changed();return;}
        emit backRequested();return;
    }
    if(!prompt_.isEmpty()){if(action==Action::Confirm)confirm();return;}
    if(busy_)return;
    if(action==Action::Left||action==Action::Right){selectFace(!bluetooth_);return;}
    if(action==Action::Secondary){emit radioRequested(bluetooth_?3:2);return;}
    if(action==Action::ToggleContinue){run("scan");return;}
    if(action==Action::LocalAction&&selected().value("saved").toBool()){
        pending_={{"id",selected().value("id").toString()}};confirmation_="forget";
        prompt_="Forget "+selected().value("title").toString()+"?";
    }
    if(action==Action::Up)focus_=std::max(0,focus_-1);
    if(action==Action::Down)focus_=std::min(std::max(0,int(rows_.size())-1),focus_+1);
    if(action==Action::Confirm){activate(focus_);return;}
    emit changed();
}
}
