#pragma once
#include "RetroAchievementsApi.h"
#include "core/repository/LibraryRepository.h"
#include <QThread>
#include <QSet>

namespace trainer {
class RetroAchievementsProvider final : public AchievementProvider {
    Q_OBJECT
public:
    RetroAchievementsProvider(LibraryRepository&, QString directory, AchievementTransport transport = {}, QObject* parent = nullptr);
    ~RetroAchievementsProvider() override;
    AchievementContext context() const override { return {"retroAchievements", account_.username}; }
    AchievementAccount launchAccount() const { return account_; } // C++ only; never expose the token to QML.
    QList<AchievementSet> sets() const override;
    AchievementSnapshot snapshot(const QString&) const override;
    void refresh(const QString&) override;
    void refreshAll() override;
    void refreshAdventure(const QString&,bool notify = false) override;
    void observeRuntimeGame(const QString&,const QString&,const QString&) override;
    void acknowledgeLive(const QString& set,const QString& id) override {liveAcknowledged_.insert(set+":"+id);}
    QVariantList accountHistory() const override { return history_; }
    QString historyStatus() const override;
    bool historyBusy() const override { return historyBusy_; }
    bool historyMore() const override { return historyMore_; }
    bool hasHistoryKey() const override {return account_.valid() && webAccount_.valid() && webAccount_.username==account_.username;}
    void setHistoryKey(const QString&) override;
    void fetchHistory(bool more=false) override;
    bool canManageAccount() const override { return true; }
    bool accountBusy() const override { return busy_; }
    QString accountMessage() const override { return message_; }
    void login(const QString& username, const QString& password) override;
    void disconnectAccount() override;
    bool disconnectForRemoval();
private:
    AchievementAccount webAccount_;
    QVariantList history_;
    QString historyMessage_;
    bool historyBusy_=false, historyMore_=false;
    int historyGeneration_=0;
    void clearHistory();
    struct RuntimeIdentity {QString hash,game;int revision; qint64 size,modified;};
    QHash<QString,RuntimeIdentity> runtimeIdentities_;
    QSet<QString> liveAcknowledged_;
    void loadCache();
    void sync(const QString& setId,const QString& adventureId = {},bool notify = false);
    void publish(LinkedAchievementSet,bool notify = false);
    QString cacheDirectory() const;
    LibraryRepository& library_;
    QString directory_, message_, pendingAdventure_;
    bool pendingNotify_ = false;
    AchievementAccount account_;
    QHash<QString, LinkedAchievementSet> records_;
    std::shared_ptr<std::atomic_bool> cancelled_ = std::make_shared<std::atomic_bool>(false);
    AchievementTransport transport_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false, pendingRefresh_ = false;
};
}
