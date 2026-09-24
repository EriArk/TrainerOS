#pragma once
#include "Models.h"
#include <QObject>
#include <functional>

namespace trainer {
struct SaveTarget {
    QString adventureId, title, savePath, contentRevision, contextRevision, error;
    bool supported = false;
    QString backupOwner; // Empty preserves the legacy shelf; otherwise an exact Trainer ID.
};
struct SaveBackup {
    QString id, revision;
    QDateTime createdAt;
    qint64 bytes = 0;
    bool valid = false, hasSave = false, protection = false;
    QString reason;
};
struct SaveBackupSnapshot {
    bool supported = false, hasSave = false;
    QString token, error;
    QList<SaveBackup> copies;
    bool canHeal = false, needsHealing = false;
    int partyCount = 0;
    QString healingError;
};
struct SaveHealing {
    QByteArray data;
    QString error;
    int partyCount = 0;
};
using SaveHealer = std::function<SaveHealing(const QByteArray&, const QString& contentHash)>;
struct SaveBackupResult {
    bool success = false, restored = false;
    QString message;
    SaveBackupSnapshot snapshot = {};
};
class SaveBackupService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool busy() const = 0;
    virtual bool supports(const AdventureRegistration&) const = 0;
    virtual void inspect(const AdventureRegistration&, QObject*, std::function<void(SaveBackupSnapshot)>) = 0;
    virtual void create(const AdventureRegistration&, const QString& token, QObject*, std::function<void(SaveBackupResult)>) = 0;
    virtual void restore(const AdventureRegistration&, const SaveBackup&, const QString& token, QObject*, std::function<void(SaveBackupResult)>) = 0;
    virtual void heal(const AdventureRegistration&, const QString&, QObject*, std::function<void(SaveBackupResult)> done) {
        done({false,false,"Healing is not available for this Adventure."});
    }
signals:
    void busyChanged();
    void operationFailed();
};
}
