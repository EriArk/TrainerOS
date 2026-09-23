#pragma once
#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <array>
#include <memory>

namespace trainer {
// Ephemeral, non-copyable input. Never exposed to QML, serialized or logged.
class PinInput final {
public:
    ~PinInput();
    PinInput() = default;
    PinInput(const PinInput&) = delete;
    PinInput& operator=(const PinInput&) = delete;
    void digit(int);
    void erase();
    void clear();
    int size() const { return size_; }
    const char* data() const { return digits_.data(); }
    bool matches(const PinInput&) const;
private:
    std::array<char,6> digits_{};
    int size_ = 0;
};
using SecretPin = std::shared_ptr<PinInput>;
inline SecretPin emptyPin() { return std::make_shared<PinInput>(); }
struct PinRecord { int version=0; QByteArray salt, verifier; };
struct PinResult { QString error; bool success() const { return error.isEmpty(); } };
QString migrateTrainerPins(QSqlDatabase&);
QString makePinRecord(const SecretPin&, PinRecord&);
QString writePinRecord(QSqlDatabase&, const QString&, const PinRecord&, bool family = false);
// Must run on the database worker. Failed attempts are committed before returning.
PinResult verifyTrainerPin(QSqlDatabase&, const QString&, const SecretPin&, qint64 now, bool family = false);
}
