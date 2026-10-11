#pragma once
#include "SaveBackupStorage.h"

namespace trainer {
// Opaque emulator-owned progress only. This grants no semantic save operations.
// Runtime adapters must verify their own return receipt and clean session exit.
struct RuntimeSaveSeed {
    SaveTarget target;
    QByteArray bytes;
    QString token, error;
    bool existed = false;
};
RuntimeSaveSeed captureRuntimeSave(const QString& backupRoot, const AdventureRegistration&, const SaveTargetResolver&);
QString returnRuntimeSave(const QString& backupRoot, const AdventureRegistration&, const RuntimeSaveSeed&,
                         const QByteArray& ownedResult, const SaveTargetResolver&);
}
