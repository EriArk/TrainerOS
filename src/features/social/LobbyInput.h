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
    bool active() const {return !connection_.isEmpty();}
    // Transfer once to the owned capture supervisor, never expose a socket path.
    int takeDescriptor(const QString& connection);
signals:
    void revoked(QString connection);
private:
    void reconcile();
    void heartbeat();
    void revoke();
    bool matches() const;
    LobbyCoordinator& lobby_;
    QTimer timer_{this};
    QString peer_,connection_,ticket_,lobbyId_,admission_;
    QJsonObject game_;
    qint64 generation_=0;
    int port_=0,writer_=-1,reader_=-1;
};
}
