#include "PowerControl.h"
#include <QJsonDocument>
#include <QFileInfo>
#include <QPointer>

namespace trainer {
PowerControl::~PowerControl(){
    process_.closeWriteChannel();
    if(process_.state()!=QProcess::NotRunning&&!process_.waitForFinished(300)){
        process_.kill();process_.waitForFinished(300);
    }
}
void PowerControl::start(){
    if(process_.state()!=QProcess::NotRunning)return;
    const QString path="/var/opt/traineros/integrations/power-service.py";
    const QFileInfo file(path),directory(file.absolutePath());
    if(!file.isFile()||file.isSymLink()||!file.isExecutable()||file.ownerId()!=0||directory.ownerId()!=0
       ||((file.permissions()|directory.permissions())&(QFile::WriteGroup|QFile::WriteOther)))return;
    connect(&process_,&QProcess::readyReadStandardOutput,this,&PowerControl::receive);
    connect(&process_,&QProcess::readyReadStandardError,this,[this]{process_.readAllStandardError();});
    connect(&process_,&QProcess::finished,this,[this]{available_=busy_=keepAwake_=false;error_="Power service stopped.";emit changed();});
    process_.start("/usr/bin/sudo",{"-n",path});
}
void PowerControl::send(const QJsonObject& value){if(process_.state()==QProcess::Running)process_.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');}
void PowerControl::toggleKeepAwake(){if(available_)send({{"command","keep-awake"},{"enabled",!keepAwake_}});}
void PowerControl::sleep(){if(available_&&!busy_){error_.clear();send({{"command","request"}});}}
void PowerControl::receive(){
    buffer_+=process_.readAllStandardOutput();
    while(buffer_.contains('\n')){
        const auto at=buffer_.indexOf('\n');const auto row=QJsonDocument::fromJson(buffer_.left(at)).object();buffer_.remove(0,at+1);
        const auto event=row["event"].toString();
        if(event=="ready"){available_=row["canSleep"].toBool();error_=available_?QString():"Sleep is unavailable on this device.";}
        else if(event=="menu")emit menuRequested();
        else if(event=="state")keepAwake_=row["keepAwake"].toBool();
        else if(event=="awake"){busy_=false;emit woke();}
        else if(event=="error"){busy_=false;error_=row["message"].toString();emit menuRequested();}
        else if(event=="sleep-request"){
            if(busy_){send({{"command","reject"}});continue;}
            busy_=true;error_.clear();
            QPointer<PowerControl> self(this);
            const auto ready=[self](QString error){
                if(!self)return;
                self->error_=error;
                if(error.isEmpty())self->send({{"command","sleep"}});
                else {self->busy_=false;self->send({{"command","reject"}});emit self->menuRequested();}
                emit self->changed();
            };
            if(prepareSleep)prepareSleep(ready);else ready("Sleep preparation is unavailable.");
        }
        emit changed();
    }
}
}
