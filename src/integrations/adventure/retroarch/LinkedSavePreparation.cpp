#include "LinkedSavePreparation.h"
#include <QJsonDocument>
#include <QTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <memory>

namespace trainer::retroarch {
void LinkedSavePreparation::stop() {
    auto* p=process_;process_=nullptr;
    if(!p)return;
    p->disconnect(this);
    if(p->state()==QProcess::NotRunning)p->deleteLater();
    else {connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),p,&QObject::deleteLater);p->kill();}
}
void LinkedSavePreparation::start(const QString& python,const QString& helper,const QJsonObject& config) {
    stop();
    if(!QFileInfo(python).isExecutable()||!QFileInfo(helper).isFile()) {emit failed();return;}
    auto* p=new QProcess(this);process_=p;
    auto* timeout=new QTimer(p);timeout->setSingleShot(true);
    const auto failure=[this,p]{if(process_==p){stop();emit failed();}};
    connect(timeout,&QTimer::timeout,this,failure);timeout->start(75000);
    struct State {QByteArray buffer,data; bool ready=false,done=false;};
    auto state=std::make_shared<State>();
    const bool host=config["host"].toBool();const int size=config["size"].toInt();
    connect(p,&QProcess::started,this,[p,config]{p->write(QJsonDocument(config).toJson(QJsonDocument::Compact)+'\n');});
    connect(p,&QProcess::readyReadStandardError,this,[p]{p->readAllStandardError();});
    connect(p,&QProcess::readyReadStandardOutput,this,[this,p,state,host,size,failure]{
        if(process_!=p)return;
        state->buffer+=p->readAllStandardOutput();
        if(state->buffer.size()>262144){failure();return;}
        while(state->buffer.contains('\n')) {
            const int end=state->buffer.indexOf('\n');
            const auto event=QJsonDocument::fromJson(state->buffer.left(end)).object();state->buffer.remove(0,end+1);
            if(event["event"]=="ready"&&host&&!state->ready&&!state->done) {
                const auto endpoint=event["endpoint"].toObject();
                const bool valid=endpoint["mode"]=="online"?
                    QRegularExpression("^[0-9]+(?:-[a-z]+){8}$").match(endpoint["code"].toString()).hasMatch()&&endpoint["code"].toString().size()<=160:
                    endpoint["mode"]=="nearby"&&endpoint["port"].toInt()>0&&endpoint["port"].toInt()<=65535&&
                    QRegularExpression("^[0-9a-f]{64}$").match(endpoint["fingerprint"].toString()).hasMatch()&&
                    QRegularExpression("^[0-9a-f]{32}$").match(endpoint["token"].toString()).hasMatch();
                if(!valid){failure();return;}
                state->ready=true;emit ready(endpoint);
                if(process_!=p)return;
            } else if(event["event"]=="done"&&!state->done&&(!host||state->ready)) {
                const auto encoded=event["data"].toString().toLatin1();
                const auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
                if(!decoded||(host?decoded.decoded.size()!=size:!decoded.decoded.isEmpty())){failure();return;}
                state->data=decoded.decoded;state->done=true;
            } else {failure();return;}
        }
    });
    connect(p,&QProcess::errorOccurred,this,[failure](QProcess::ProcessError error){if(error==QProcess::FailedToStart)failure();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p,state,failure](int code,QProcess::ExitStatus status){
        if(process_!=p)return;
        if(code||status!=QProcess::NormalExit||!state->done||!state->buffer.isEmpty()){failure();return;}
        process_=nullptr;p->deleteLater();emit completed(state->data);
    });
    p->start(python,{"-I",helper});
}
}
