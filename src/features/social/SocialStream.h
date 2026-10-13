#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QVideoSink>

namespace trainer {
// Local decoded-frame handoff only. Fluxer owns media networking and admission.
class SocialStream final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY changed)
public:
    explicit SocialStream(QObject* parent=nullptr);
    bool active() const {return server_.isListening();}
    bool hasFrame() const {return hasFrame_;}
    QString open();
    Q_INVOKABLE void setVideoSink(QObject* sink);
    void close();
signals:
    void changed();
    void ended();
private:
    friend class LobbySmokeScenario;
    void read();
    void fail();
    QTemporaryDir directory_;
    QLocalServer server_{this};
    QPointer<QLocalSocket> socket_;
    QPointer<QVideoSink> sink_;
    QTimer deadline_{this};
    QByteArray bytes_;
    bool hasFrame_=false;
};
}
