#include "InvitationOverlayService.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegion>
#include <QLoggingCategory>

namespace trainer {
Q_LOGGING_CATEGORY(invitationOverlayLog,"trainer.invitation")
InvitationOverlayService::InvitationOverlayService(bool gamescope,QString helper,QObject* parent)
    :QObject(parent),gamescope_(gamescope),helper_(std::move(helper)) {
    heartbeat_.setInterval(250);startup_.setSingleShot(true);startup_.setInterval(3000);
    retry_.setSingleShot(true);retry_.setInterval(1000);
    healthy_.setSingleShot(true);healthy_.setInterval(10000);
    // Limit consecutive failures, not the number of recoveries in the entire
    // shell session. A later slow window transition can trip the watchdog too.
    connect(&healthy_,&QTimer::timeout,this,[this]{attempts_=0;});
    connect(&retry_,&QTimer::timeout,this,&InvitationOverlayService::start);
    connect(&heartbeat_,&QTimer::timeout,this,[this]{send({{"command","ping"}});});
    connect(&startup_,&QTimer::timeout,this,[this]{process_.kill();lost();});
    connect(&process_,&QProcess::started,this,[this]{heartbeat_.start();});
    connect(&process_,&QProcess::readyReadStandardError,this,[this]{process_.readAllStandardError();});
    connect(&process_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){qCInfo(invitationOverlayLog)<<"badge helper ended"<<code<<status;lost();});
    connect(&process_,&QProcess::errorOccurred,this,&InvitationOverlayService::lost);
    connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{
        buffer_+=process_.readAllStandardOutput();
        if(buffer_.size()>8192){process_.kill();lost();return;}
        while(buffer_.contains('\n')) {
            const auto end=buffer_.indexOf('\n');const auto event=QJsonDocument::fromJson(buffer_.left(end)).object();buffer_.remove(0,end+1);
            if(event["event"]=="ready"){qCInfo(invitationOverlayLog)<<"badge helper ready"<<attempts_;startup_.stop();healthy_.start();ready_=true;emit changed();sync();}
            else if(event["event"]=="activated"&&ready_&&window_&&window_->isVisible())emit activated();
            else if(event["event"]=="failed"){qCWarning(invitationOverlayLog)<<"badge helper unavailable"<<event["reason"].toString().left(200);process_.kill();lost();return;}
        }
    });
}
InvitationOverlayService::~InvitationOverlayService() {
    retry_.stop();healthy_.stop();attempts_=3;
    process_.closeWriteChannel();
    if(process_.state()!=QProcess::NotRunning&&!process_.waitForFinished(300)){process_.kill();process_.waitForFinished(500);}
}
void InvitationOverlayService::attach(QObject* object) {
    if(window_)return;
    window_=qobject_cast<QWindow*>(object);if(!window_)return;
    window_->setOpacity(0);
    connect(window_,&QWindow::visibleChanged,this,[this]{sync();});
    if(!gamescope_){ready_=true;emit changed();return;}
    if(!QFileInfo(helper_).isFile())return;
    // The compositor role is established before the first map. A visible
    // undecorated normal window could otherwise replace the game as its focus.
    window_->create();start();
}
void InvitationOverlayService::start() {
    if(!window_||!gamescope_||process_.state()!=QProcess::NotRunning||attempts_>=3)return;
    ++attempts_;buffer_.clear();startup_.start();
    process_.start("/usr/bin/python3",{helper_,"--window",QString::number(window_->winId()),
        "--shell",QString::number(QCoreApplication::applicationPid())});
}
void InvitationOverlayService::setRegion(QRect region) {
    region_=region;
    if(window_)window_->setMask(QRegion(region));
    sync();
}
void InvitationOverlayService::setGame(qint64 pid){game_=pid;sync();}
void InvitationOverlayService::send(const QJsonObject& packet) {
    if(process_.state()!=QProcess::Running)return;
    if(process_.bytesToWrite()>8192){process_.kill();lost();return;}
    process_.write(QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n');
}
void InvitationOverlayService::sync() {
    // Gamescope can retain an unmapped external overlay's last frame. Clear
    // its opacity even when the helper has ended and cannot consume a packet.
    if(window_)window_->setOpacity(window_->isVisible()&&ready_?1:0);
    if(!ready_||!window_)return;
    const auto dpr=window_->devicePixelRatio();
    send({{"command","surface"},{"visible",window_->isVisible()},{"game",QString::number(game_)},
        {"x",qRound(region_.x()*dpr)},{"y",qRound(region_.y()*dpr)},
        {"width",qRound(region_.width()*dpr)},{"height",qRound(region_.height()*dpr)}});
}
void InvitationOverlayService::lost(){
    startup_.stop();heartbeat_.stop();healthy_.stop();ready_=false;sync();emit changed();
    if(attempts_<3&&!retry_.isActive())retry_.start();
}
}
