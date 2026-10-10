#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>
#include <QTimer>
#include <QWindow>
#include <QJsonObject>

namespace trainer {
// A passive, non-focusing surface. The platform owns its hit region, not an
// emulator or an invitation. Closing a game does not destroy this surface.
class InvitationOverlayService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
public:
    InvitationOverlayService(bool gamescope, QString helper, QObject* parent=nullptr);
    ~InvitationOverlayService() override;
    bool ready() const { return ready_; }
    Q_INVOKABLE void attach(QObject* window);
    Q_INVOKABLE void setRegion(QRect region);
    void setGame(qint64 pid);
signals:
    void changed();
    void activated();
private:
    void send(const QJsonObject&);
    void start();
    void sync();
    void lost();
    bool gamescope_=false,ready_=false;
    QString helper_;
    QPointer<QWindow> window_;
    QRect region_;
    qint64 game_=0;
    QProcess process_;
    QByteArray buffer_;
    QTimer heartbeat_,startup_,retry_,healthy_;
    int attempts_=0;
};
}
