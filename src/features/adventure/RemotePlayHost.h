#pragma once
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QJsonObject>

namespace trainer {
// One owned-game media supervisor. Per-guest descriptors are transferred only
// through its inherited Unix socket, never through a path or provider message.
class RemotePlayHost final:public QObject {
    Q_OBJECT
public:
    explicit RemotePlayHost(QObject* parent=nullptr);
    ~RemotePlayHost() override;
    bool add(qint64 pid,const QString& root,const QString& connection,int port,const QString& config,int lease);
    void remove(const QString& connection);
    void stop();
signals:
    void ready(QString connection);
    void ended(QString connection);
    void failed(QString error);
private:
    bool send(const QJsonObject&,int descriptor=-1);
    void read();
    void fail(QString);
    QProcess process_{this};QSet<QString> connections_;QByteArray output_;
    int writer_=-1; qint64 pid_=0;bool stopping_=false;
};
}
