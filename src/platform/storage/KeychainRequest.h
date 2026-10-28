#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <functional>

namespace trainer {
// QtKeychain has one process-wide job executor. All jobs must share a thread,
// even when their account/controller lives on a short-lived worker thread.
class KeychainRequest final : public QObject {
    Q_OBJECT
public:
    enum Operation { Read, Write, Remove };
    using Completion = std::function<void(int, QByteArray)>;
    static void run(QString service, QString key, Operation operation, QByteArray data,
                    QObject* receiver, Completion completed);
signals:
    void finished(int error, QByteArray data);
private:
    void start();
    QString service_, key_;
    QByteArray data_;
    Completion keepAlive_; // Retain account leases until the OS operation settles.
    Operation operation_ = Read;
};
}
