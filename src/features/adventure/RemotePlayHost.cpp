#include "RemotePlayHost.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <memory>
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

namespace trainer {
RemotePlayHost::RemotePlayHost(QObject* parent):QObject(parent) {
    connect(&process_,&QProcess::readyReadStandardOutput,this,&RemotePlayHost::read);
    connect(&process_,&QProcess::readyReadStandardError,this,[this]{process_.readAllStandardError();});
    connect(&process_,&QProcess::finished,this,[this]{if(!stopping_)fail("The Remote Play stream ended. Your game is kept.");});
    connect(&process_,&QProcess::errorOccurred,this,[this](auto error){if(error==QProcess::FailedToStart&&!stopping_)fail("Couldn't open Remote Play.");});
}
RemotePlayHost::~RemotePlayHost(){stop();}
bool RemotePlayHost::add(qint64 pid,const QString& root,const QString& connection,int port,const QString& config,int lease) {
#ifdef Q_OS_LINUX
    const auto closeLease=[lease]{if(lease>=0)::close(lease);};
    if(pid<=1||lease<3||port<2||port>4||QUuid(connection).isNull()||connections_.contains(connection)||connections_.size()>=3) {closeLease();return false;}
    if(process_.state()==QProcess::NotRunning) {
        stop();int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,pair)){closeLease();return false;}
        writer_=pair[0];const auto child=std::make_shared<int>(pair[1]);
        process_.setChildProcessModifier([child]{if(*child!=3)dup2(*child,3);fcntl(3,F_SETFD,0);});
        auto release=[child]{if(*child>=0){::close(*child);*child=-1;}};
        connect(&process_,&QProcess::started,this,release,Qt::SingleShotConnection);
        connect(&process_,&QProcess::errorOccurred,this,[release](auto){release();},Qt::SingleShotConnection);
        auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_GROUP_FD","3");env.insert("LD_LIBRARY_PATH",root);process_.setProcessEnvironment(env);
        stopping_=false;pid_=pid;
        process_.start("/usr/bin/python3",{root+"/run-group.py",QString::number(pid),root+"/stream_host"});
    }
    if(pid!=pid_||!send({{"kind","add"},{"connection",connection},{"port",port},{"config",config}},lease)){closeLease();return false;}
    closeLease();connections_.insert(connection);return true;
#else
    Q_UNUSED(pid);Q_UNUSED(root);Q_UNUSED(connection);Q_UNUSED(port);Q_UNUSED(config);Q_UNUSED(lease);return false;
#endif
}
bool RemotePlayHost::send(const QJsonObject& object,int descriptor) {
#ifdef Q_OS_LINUX
    if(writer_<0)return false;
    const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact);if(bytes.size()>4096)return false;
    iovec data{const_cast<char*>(bytes.constData()),size_t(bytes.size())};msghdr message{};message.msg_iov=&data;message.msg_iovlen=1;
    char control[CMSG_SPACE(sizeof(int))]{};
    if(descriptor>=0){message.msg_control=control;message.msg_controllen=sizeof(control);auto c=CMSG_FIRSTHDR(&message);
        c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));std::memcpy(CMSG_DATA(c),&descriptor,sizeof(int));}
    return sendmsg(writer_,&message,MSG_DONTWAIT|MSG_NOSIGNAL)==bytes.size();
#else
    Q_UNUSED(object);Q_UNUSED(descriptor);return false;
#endif
}
void RemotePlayHost::remove(const QString& connection) {
    if(!connections_.remove(connection))return;
    if(connections_.isEmpty()){stop();return;}
    if(!send({{"kind","remove"},{"connection",connection}}))fail("Couldn't release a Remote Play controller.");
}
void RemotePlayHost::read() {
    output_+=process_.readAllStandardOutput();if(output_.size()>8192){fail("Invalid Remote Play response.");return;}
    while(output_.contains('\n')) {
        const int at=output_.indexOf('\n');const auto value=QJsonDocument::fromJson(output_.left(at)).object();output_.remove(0,at+1);
        const auto id=value["connection"].toString();
        if(value.size()!=2||QUuid(id).isNull()||(value["event"]!="ready"&&value["event"]!="ended")){fail("Invalid Remote Play response.");return;}
        if(!connections_.contains(id))continue;
        if(value["event"]=="ready")emit ready(id);else emit ended(id);
    }
}
void RemotePlayHost::fail(QString message){if(stopping_)return;stop();emit failed(message);}
void RemotePlayHost::stop() {
    stopping_=true;connections_.clear();pid_=0;
#ifdef Q_OS_LINUX
    if(writer_>=0)::close(writer_);
    writer_=-1;
#endif
    if(process_.state()!=QProcess::NotRunning&&!process_.waitForFinished(6500)) {
        process_.terminate();if(!process_.waitForFinished(3000)){process_.kill();process_.waitForFinished(1000);}
    }
#ifdef Q_OS_LINUX
    process_.setChildProcessModifier({});
#endif
    output_.clear();
}
}
