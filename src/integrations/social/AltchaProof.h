#pragma once
#include <QObject>
#include <QJsonObject>
#include <QElapsedTimer>
#include <functional>

namespace trainer {
// ALTCHA v2 PBKDF2/SHA-256 client protocol. Runs in short worker-event-loop
// slices so authentication never stalls the UI or Gateway heartbeats.
class AltchaProof final : public QObject {
public:
    using Completed = std::function<void(QByteArray)>;
    AltchaProof(QJsonObject challenge, Completed completed, QObject* parent);
private:
    void step();
    void finish(QByteArray token = {});
    QJsonObject challenge_;
    Completed completed_;
    QByteArray nonce_, salt_, prefix_;
    int cost_ = 0, length_ = 0;
    quint32 counter_ = 0;
    QElapsedTimer elapsed_;
};
}
