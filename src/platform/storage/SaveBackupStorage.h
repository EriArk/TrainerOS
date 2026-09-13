#pragma once
#include "core/model/SaveBackup.h"
#include <QThread>

namespace trainer {
using SaveTargetResolver = std::function<SaveTarget(const AdventureRegistration&)>;
// All filesystem work below runs on a worker. Targets come from verified adapters.
SaveBackupSnapshot inspectSaveBackups(const QString& root, const SaveTarget&);
SaveBackupResult createSaveBackup(const QString& root, const AdventureRegistration&, const QString& token, const SaveTargetResolver&);
SaveBackupResult restoreSaveBackup(const QString& root, const AdventureRegistration&, const SaveBackup&, const QString& token, const SaveTargetResolver&);

class LocalSaveBackupService final : public SaveBackupService {
    Q_OBJECT
public:
    LocalSaveBackupService(QString root, SaveTargetResolver,
        std::function<bool(const AdventureRegistration&)> supports, QObject* parent = nullptr);
    ~LocalSaveBackupService() override;
    bool busy() const override { return busy_; }
    bool supports(const AdventureRegistration& r) const override { return supports_(r); }
    void inspect(const AdventureRegistration&, QObject*, std::function<void(SaveBackupSnapshot)>) override;
    void create(const AdventureRegistration&, const QString&, QObject*, std::function<void(SaveBackupResult)>) override;
    void restore(const AdventureRegistration&, const SaveBackup&, const QString&, QObject*, std::function<void(SaveBackupResult)>) override;
private:
    void run(std::function<SaveBackupResult()>, QObject*, std::function<void(SaveBackupResult)>);
    QString root_;
    SaveTargetResolver resolve_;
    std::function<bool(const AdventureRegistration&)> supports_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false;
};
}
