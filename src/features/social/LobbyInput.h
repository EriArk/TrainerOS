#pragma once
#include "LobbyCoordinator.h"

namespace trainer {
// Local transport admission only. The caller must authenticate the connection,
// prepare the host's emulator port and own its game process. No peer packet or
// spectator API can call this service. Foreground/process checks remain in the
// capture supervisor; both independent permissions are required for input.
class LobbyInput final : public QObject {
    Q_OBJECT
public:
    explicit LobbyInput(LobbyCoordinator&,QObject* parent=nullptr);
    ~LobbyInput() override;
    bool authorize(QString peer,QString connection,QString ticket,int reservedPort);
    void release(const QString& connection);
    bool active() const {return !grants_.isEmpty();}
    // Transfer once to the owned capture supervisor, never expose a socket path.
    int takeDescriptor(const QString& connection);
signals:
    void revoked(QString connection);
private:
    void reconcile();
    void heartbeat();
    void revoke(const QString&);
    struct Grant {
        QString peer,ticket,lobby,admission;
        QJsonObject game;
        qint64 generation=0;
        int port=0,writer=-1,reader=-1;
    };
    bool matches(const Grant&) const;
    LobbyCoordinator& lobby_;
    QTimer timer_{this};
    QMap<QString,Grant> grants_;
};
}
