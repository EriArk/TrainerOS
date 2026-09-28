#include "NearbyService.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
namespace trainer {
NearbyService::NearbyService(QObject* parent):QObject(parent) {
    retry_.setSingleShot(true);retry_.setInterval(15000);
    connect(&retry_,&QTimer::timeout,this,[this]{configure(config_["id"].toString(),config_["name"].toString(),config_["visible"].toBool());});
    connect(&process_,&QProcess::started,this,[this]{request(config_);});
    connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{
        buffer_+=process_.readAllStandardOutput();
        if(buffer_.size()>65536){process_.terminate();return;}
        while(buffer_.contains('\n')) {
            const int at=buffer_.indexOf('\n');const auto j=QJsonDocument::fromJson(buffer_.left(at)).object();buffer_.remove(0,at+1);
            if(j["event"]=="peers") {
                const auto peers=j["peers"].toArray().toVariantList();
                if(peers==peers_)continue;
                peers_=peers;
            }
            if(!j.isEmpty())emit event(j);
        }
    });
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{
        buffer_.clear();peers_.clear();emit event({{"event","closed"},{"error","Nearby connection service stopped."}});
        if(config_["visible"].toBool())retry_.start();
    });
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){emit event({{"event","unavailable"}});if(config_["visible"].toBool())retry_.start();}
    });
}
NearbyService::~NearbyService(){
    retry_.stop();disconnect(&process_,nullptr,this,nullptr);
    if(process_.state()==QProcess::NotRunning)return;
    process_.closeWriteChannel();if(!process_.waitForFinished(1500)){process_.terminate();if(!process_.waitForFinished(1000)){process_.kill();process_.waitForFinished(1000);}}
}
void NearbyService::configure(const QString& id,const QString& name,bool visible) {
    const QJsonObject value{{"op","configure"},{"id",id},{"name",name},{"visible",visible}};
    if(value==config_ && (process_.state()!=QProcess::NotRunning || retry_.isActive()))return;
    config_=value;if(!visible)retry_.stop();
#ifdef Q_OS_LINUX
    if(process_.state()==QProcess::NotRunning && visible && QFileInfo::exists("/var/opt/traineros/integrations/nearby-control.py")) {
        process_.setStandardErrorFile(QProcess::nullDevice());
        process_.start("/usr/bin/sudo",{"-n","/var/opt/traineros/integrations/nearby-control.py"});
    } else if(process_.state()==QProcess::Running)request(value);
#endif
}
void NearbyService::request(const QJsonObject& value){if(process_.state()==QProcess::Running)process_.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');}
}
