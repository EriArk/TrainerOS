#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <functional>

namespace trainer {
// Armada's user-scoped systemd credentials. Only ciphertext reaches disk.
class EncryptedCredentials final : public QObject {
public:
    using Completion = std::function<void(bool, QByteArray)>;
    explicit EncryptedCredentials(QObject* parent = nullptr) : QObject(parent) {}
    ~EncryptedCredentials() override { cancel(); }
    static bool available();
    static QString path(const QString& key);
    static bool remove(const QString& key);
    void read(const QString& key, Completion done);
    void write(const QString& key, QByteArray secret, Completion done);
    void cancel();
private:
    void run(const QString& key, QByteArray input, bool writing, Completion done);
    QPointer<QProcess> process_;
};
}
