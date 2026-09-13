#pragma once
#include "core/model/GameProgressProvider.h"
#include "core/model/Models.h"
#include "core/model/SaveBackup.h"
#include <QThread>
#include <functional>

namespace trainer {
using ProgressSaveResolver = std::function<SaveTarget(const AdventureRegistration&)>;
GameProgress inspectGameProgress(const AdventureRegistration&, const ProgressSaveResolver&);

class GameProgressService final : public GameProgressProvider {
    Q_OBJECT
public:
    explicit GameProgressService(ProgressSaveResolver, QObject* parent = nullptr);
    ~GameProgressService() override;
    void refresh(const AdventureRegistration&);
    void invalidate();
    QString adventureId() const override { return record_.adventure.id; }
    GameProgress snapshot() const override { return snapshot_; }
private:
    void startRead();
    ProgressSaveResolver resolver_;
    AdventureRegistration record_;
    GameProgress snapshot_;
    QThread thread_;
    QObject* worker_;
    quint64 generation_ = 0;
    bool reading_ = false, pending_ = false;
};
}
