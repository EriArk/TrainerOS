#pragma once
#include <QObject>
#include <QJsonObject>
#include <QDateTime>
#include <QHash>
#include <optional>
#include <QVariantList>

namespace trainer {
enum class AchievementState { Disconnected, Unsupported, Loading, Ready, Offline, Error };
enum class AchievementMode { Standard, Hardcore };
struct AchievementContext {
    QString providerId;
    QString accountId;
    bool operator==(const AchievementContext&) const = default;
};
struct AchievementSet {
    QString id;
    QString gameId;
    QString adventureId;
    QString title;
    QString world;
    bool supported = true;
};
struct AchievementDefinition {
    QString id;
    QString title;
    QString description;
    QString badgeName; // Optional provider artwork identity; never an unlock signal.
    int points = -1;
};
struct AchievementUnlock {
    QString achievementId;
    std::optional<bool> unlocked;
    std::optional<AchievementMode> mode;
    QDateTime earnedAt;
};
// Context and set identity scope every definition/unlock in the snapshot.
struct AchievementSnapshot {
    AchievementContext context;
    QString setId;
    AchievementState state = AchievementState::Disconnected;
    QList<AchievementDefinition> definitions;
    QList<AchievementUnlock> unlocks;
    QDateTime fetchedAt;
};
class AchievementProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual AchievementContext context() const = 0;
    virtual QList<AchievementSet> sets() const = 0;
    virtual AchievementSnapshot snapshot(const QString& setId) const = 0;
    virtual void refresh(const QString& setId) = 0;
    virtual void refreshAll() { for (const auto& set : sets()) refresh(set.id); }
    virtual void refreshAdventure(const QString&, bool = false) { refreshAll(); }
    virtual void observeRuntimeGame(const QString&,const QString&,const QString&) {}
    virtual void acknowledgeLive(const QString&,const QString&) {}
    virtual QVariantList accountHistory() const { return {}; }
    virtual QString historyStatus() const { return "Connect an RA account and add its Web API key to view account history."; }
    virtual bool historyBusy() const { return false; }
    virtual bool historyMore() const { return false; }
    virtual bool hasHistoryKey() const { return false; }
    virtual void setHistoryKey(const QString&) {}
    virtual void fetchHistory(bool = false) {}
    virtual bool canManageAccount() const { return false; }
    virtual bool accountBusy() const { return false; }
    virtual QString accountMessage() const { return {}; }
    virtual void login(const QString&, const QString&) {}
    virtual void disconnectAccount() {}
    // C++ composition only; never a QML/SDK property. Sent only after explicit
    // reward-link consent, transiently to the authenticated TrainerOS backend.
    virtual QJsonObject rewardCredentials() const { return {}; }
signals:
    void historyChanged();
    void snapshotChanged(const QString& setId);
    void achievementsEarned(const QString& title, const QStringList& names);
};
class MockAchievementProvider final : public AchievementProvider {
public:
    explicit MockAchievementProvider(QObject* parent = nullptr);
    AchievementContext context() const override { return context_; }
    QList<AchievementSet> sets() const override;
    AchievementSnapshot snapshot(const QString& setId) const override;
    void refresh(const QString& setId) override;
    void setState(const QString& setId, AchievementState);
    void setAccount(const QString& accountId);
    void setNextResult(AchievementState state) { nextResult_ = state; }
    void holdRequests(bool hold) { hold_ = hold; }
    void finishRefresh(const QString& setId);
    void enableAccountPreview() { accountPreview_ = true; }
    void enableGalleryPreview();
    QVariantList accountHistory() const override;
    bool hasHistoryKey() const override {return galleryPreview_ && !context_.accountId.isEmpty();}
    QString historyStatus() const override {return hasHistoryKey()?"Sample account history":AchievementProvider::historyStatus();}
    bool canManageAccount() const override { return accountPreview_; }
    QString accountMessage() const override { return "Sample account controls. Credentials are never submitted in this preview."; }
private:
    AchievementSnapshot sample(const QString& setId) const;
    AchievementContext context_{"retroAchievements-mock", "sample-trainer"};
    QHash<QString, AchievementSnapshot> snapshots_;
    QHash<QString, AchievementState> pending_;
    AchievementState nextResult_ = AchievementState::Ready;
    int generation_ = 0;
    bool hold_ = false;
    bool accountPreview_ = false;
    bool galleryPreview_ = false;
};
// Normal application startup must never claim sample achievements or an account.
class DisconnectedAchievementProvider final : public AchievementProvider {
public:
    using AchievementProvider::AchievementProvider;
    AchievementContext context() const override { return {"retroAchievements", {}}; }
    QList<AchievementSet> sets() const override { return {}; }
    AchievementSnapshot snapshot(const QString& id) const override { return {context(), id, AchievementState::Disconnected, {}, {}, {}}; }
    void refresh(const QString&) override {}
};
}
