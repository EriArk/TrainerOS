#include "NetworkService.h"
#include <QJsonDocument>
namespace trainer {
NetworkService::NetworkService(QObject* parent):QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout,this,[this]{error_="Connection timed out. Try again.";process_.closeWriteChannel();process_.kill();});
    connect(&process_, &QProcess::readyReadStandardOutput,this,[this]{
        buffer_ += process_.readAllStandardOutput();
        if(buffer_.size()>1024*1024){error_="Connection response was too large.";process_.kill();return;}
        while(buffer_.contains('\n')) {
            const auto line=buffer_.left(buffer_.indexOf('\n'));buffer_.remove(0,line.size()+1);
            const auto value=QJsonDocument::fromJson(line).object();
            if(value.value("event")=="done") {result_=true;error_=value.value("error").toString();}
            else if(!value.isEmpty()) emit event(value);
        }
    });
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        timeout_.stop();buffer_.clear();
        if(!result_ && error_.isEmpty())error_="Connection service unavailable. Try again.";
        if((code || status!=QProcess::NormalExit) && error_.isEmpty())error_="The connection did not complete.";
        emit finished(error_);
    });
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){timeout_.stop();emit finished("Connection service unavailable.");}
    });
}
NetworkService::~NetworkService(){process_.closeWriteChannel();if(process_.state()!=QProcess::NotRunning){process_.terminate();if(!process_.waitForFinished(500))process_.kill();}}
void NetworkService::request(const QJsonObject& request) {
    if(process_.state()!=QProcess::NotRunning)return;
    buffer_.clear();result_=false;error_.clear();
    process_.setStandardErrorFile(QProcess::nullDevice());
    const bool read=request.value("op")=="list";
    process_.start(read?"/usr/bin/python3":"/usr/bin/sudo",read
        ?QStringList{"-I","/var/opt/traineros/integrations/network-control.py"}
        :QStringList{"-n","/var/opt/traineros/integrations/network-control.py"});
    process_.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');
    timeout_.start(110000);
}
void NetworkService::respond(const QJsonObject& value){process_.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');}
void NetworkService::cancel(){respond({{"cancel",true}});}
}
