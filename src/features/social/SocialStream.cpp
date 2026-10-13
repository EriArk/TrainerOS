#include "SocialStream.h"
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QtEndian>
#include <cstring>

namespace trainer {
SocialStream::SocialStream(QObject* parent):QObject(parent) {
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    deadline_.setSingleShot(true);
    connect(&deadline_,&QTimer::timeout,this,&SocialStream::fail);
    connect(&server_,&QLocalServer::newConnection,this,[this]{
        while(server_.hasPendingConnections()) {
            auto* connection=server_.nextPendingConnection();
            if(socket_){connection->abort();connection->deleteLater();continue;}
            socket_=connection;connection->setReadBufferSize(1280*720*4+16);
            connect(connection,&QLocalSocket::readyRead,this,&SocialStream::read);
            connect(connection,&QLocalSocket::disconnected,this,[this,connection]{if(socket_==connection)fail();});
        }
    });
}
void SocialStream::setVideoSink(QObject* sink) {sink_=qobject_cast<QVideoSink*>(sink);}
QString SocialStream::open() {
    close();
    if(!directory_.isValid()||!server_.listen(directory_.filePath("frames")))return {};
    deadline_.start(15000);emit changed();return server_.fullServerName();
}
void SocialStream::close() {
    deadline_.stop();server_.close();
    if(socket_) {auto* old=socket_.data();socket_=nullptr;old->disconnect(this);old->abort();old->deleteLater();}
    bytes_.clear();hasFrame_=false;if(sink_)sink_->setVideoFrame({});emit changed();
}
void SocialStream::fail() {close();emit ended();}
void SocialStream::read() {
    if(!socket_)return;
    bytes_+=socket_->readAll();
    // The helper is trusted, but dimensions and framing still need bounds.
    if(bytes_.size()>2*(1280*720*4+16)){fail();return;}
    while(bytes_.size()>=16) {
        const auto* data=reinterpret_cast<const uchar*>(bytes_.constData());
        const auto magic=qFromLittleEndian<quint32>(data),w=qFromLittleEndian<quint32>(data+4),
            h=qFromLittleEndian<quint32>(data+8),size=qFromLittleEndian<quint32>(data+12);
        if(magic!=0x31565254||!w||!h||w>1280||h>720||size!=w*h*4){fail();return;}
        if(bytes_.size()<16+size)return;
        QVideoFrame frame(QVideoFrameFormat(QSize(w,h),QVideoFrameFormat::Format_BGRX8888));
        if(!frame.map(QVideoFrame::WriteOnly)){fail();return;}
        for(quint32 y=0;y<h;++y)std::memcpy(frame.bits(0)+y*frame.bytesPerLine(0),bytes_.constData()+16+y*w*4,w*4);
        frame.unmap();if(sink_)sink_->setVideoFrame(frame);
        bytes_.remove(0,16+size);deadline_.start(10000);
        if(!hasFrame_){hasFrame_=true;emit changed();}
    }
}
}
