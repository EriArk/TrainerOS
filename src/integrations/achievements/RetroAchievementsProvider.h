#pragma once
#include "RetroAchievementsApi.h"
#include "core/repository/LibraryRepository.h"
#include <QThread>

namespace trainer {
class RetroAchievementsProvider final : public AchievementProvider {
    Q_OBJECT
public:
    RetroAchievementsProvider(LibraryRepository&, QString directory, AchievementTransport transport = {}, QObject* parent = nullptr);
    ~RetroAchievementsProvider() override;
    AchievementContext context() const override { return {"retroAchievements", account_.username}; }
    QList<AchievementSet> sets() const override;
    AchievementSnapshot snapshot(const QString&) const override;
    void refresh(const QString&) override;
    void refreshAll() override;
    bool canManageAccount() const override { return true; }
    bool accountBusy() const override { return busy_; }
    QString accountMessage() const override { return message_; }
    void login(const QString& username, const QString& password) override;
    void disconnectAccount() override;
private:
    void loadCache();
    void sync(const QString& setId);
    void publish(LinkedAchievementSet);
    QString cacheDirectory() const;
    LibraryRepository& library_;
    QString directory_, message_;
    AchievementAccount account_;
    QHash<QString, LinkedAchievementSet> records_;
    std::shared_ptr<std::atomic_bool> cancelled_ = std::make_shared<std::atomic_bool>(false);
    AchievementTransport transport_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false, pendingRefresh_ = false;
};
}
