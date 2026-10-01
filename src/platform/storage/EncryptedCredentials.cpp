#include "EncryptedCredentials.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

namespace trainer {
bool EncryptedCredentials::available() {
#ifdef Q_OS_LINUX
    return QFileInfo("/usr/bin/systemd-creds").isExecutable();
#else
    return false;
#endif
}
QString EncryptedCredentials::path(const QString& key) {
    static const QRegularExpression valid("^[a-f0-9]{64}$");
    if(!valid.match(key).hasMatch())return {};
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/credentials/fluxer-"+key+".cred";
}
bool EncryptedCredentials::remove(const QString& key) {
    const auto file=path(key);if(file.isEmpty())return false;
    return !QFileInfo::exists(file)||QFile::remove(file);
}
void EncryptedCredentials::cancel() {
    if(!process_)return;
    auto* p=process_.data();process_=nullptr;p->disconnect(this);
    if(p->state()!=QProcess::NotRunning)p->kill();
    p->deleteLater();
}
void EncryptedCredentials::read(const QString& key,Completion done) {
    cancel();const auto name=path(key);QFile file(name);
    if(name.isEmpty()||QFileInfo(name).isSymLink()){done(false,{});return;}
    if(!file.exists()){done(true,{});return;}
    if(file.size()>65536||!file.open(QIODevice::ReadOnly)){done(false,{});return;}
    run(key,file.readAll(),false,std::move(done));
}
void EncryptedCredentials::write(const QString& key,QByteArray secret,Completion done) {
    cancel();if(secret.isEmpty()||secret.size()>16384||path(key).isEmpty()){done(false,{});return;}
    run(key,std::move(secret),true,std::move(done));
}
void EncryptedCredentials::run(const QString& key,QByteArray input,bool writing,Completion done) {
    if(!available()){done(false,{});return;}
    auto* p=new QProcess(this);process_=p;
    p->setProgram("/usr/bin/systemd-creds");
    p->setArguments({writing?"encrypt":"decrypt","--user",writing?"--with-key=host":"--refuse-null","--name=traineros-fluxer-"+key,"-","-"});
    // Neither secrets nor ciphertext are command arguments, environment or logs.
    connect(p,&QProcess::started,this,[p,input=std::move(input)]{p->write(input);p->closeWriteChannel();});
    auto finish=[this,p,key,writing,done=std::move(done)](bool success) {
        if(process_!=p)return;
        process_=nullptr;auto output=p->readAllStandardOutput();p->deleteLater();
        success=success&&!output.isEmpty()&&output.size()<65536;
        if(writing&&success) {
            const auto name=path(key);const auto dir=QFileInfo(name).absolutePath();
            success=!QFileInfo(dir).isSymLink()&&!QFileInfo(name).isSymLink()&&QDir().mkpath(dir)
                &&QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
            QSaveFile file(name);
            success=success&&file.open(QIODevice::WriteOnly)&&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
                &&file.write(output)==output.size()&&file.commit();
            output.clear();
        }
        done(success,success?output:QByteArray());
    };
    connect(p,&QProcess::finished,this,[finish](int code,QProcess::ExitStatus status){finish(code==0&&status==QProcess::NormalExit);});
    connect(p,&QProcess::errorOccurred,this,[finish](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(false);});
    QTimer::singleShot(20000,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});
    p->start();
}
}
