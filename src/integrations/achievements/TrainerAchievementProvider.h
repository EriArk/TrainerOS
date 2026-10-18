#pragma once
#include "RetroAchievementsProvider.h"
#include <memory>

namespace trainer {
// Constructed anew for each shell session. No account is read before the store
// supplies the selected owner's directory; a directory can be bound only once.
class TrainerAchievementProvider final : public AchievementProvider {
public:
    explicit TrainerAchievementProvider(LibraryRepository& library) : library_(library) {}
    void bind(const QString& directory) {
        if(provider_ || directory.isEmpty())return;
        provider_=std::make_unique<RetroAchievementsProvider>(library_,directory);
        connect(provider_.get(),&AchievementProvider::snapshotChanged,this,&AchievementProvider::snapshotChanged);
        connect(provider_.get(),&AchievementProvider::achievementsEarned,this,&AchievementProvider::achievementsEarned);
        connect(provider_.get(),&AchievementProvider::historyChanged,this,&AchievementProvider::historyChanged);
        emit snapshotChanged({});provider_->refreshAll();
    }
    AchievementContext context() const override { return provider_?provider_->context():AchievementContext{"retroAchievements",{}}; }
    AchievementAccount launchAccount() const { return provider_ ? provider_->launchAccount() : AchievementAccount{}; }
    QList<AchievementSet> sets() const override {return provider_?provider_->sets():QList<AchievementSet>{};}
    AchievementSnapshot snapshot(const QString& id) const override {return provider_?provider_->snapshot(id):AchievementSnapshot{context(),id,AchievementState::Disconnected,{},{},{}};}
    void refresh(const QString& id) override {if(provider_)provider_->refresh(id);}
    void refreshAdventure(const QString& id,bool notify=false) override {if(provider_)provider_->refreshAdventure(id,notify);}
    void observeRuntimeGame(const QString& id,const QString& hash,const QString& game) override {if(provider_)provider_->observeRuntimeGame(id,hash,game);}
    void acknowledgeLive(const QString& set,const QString& id) override {if(provider_)provider_->acknowledgeLive(set,id);}
    void refreshAll() override {if(provider_)provider_->refreshAll();}
    QVariantList accountHistory() const override {return provider_?provider_->accountHistory():QVariantList{};}
    QString historyStatus() const override {return provider_?provider_->historyStatus():AchievementProvider::historyStatus();}
    bool historyBusy() const override {return provider_&&provider_->historyBusy();}
    bool historyMore() const override {return provider_&&provider_->historyMore();}
    bool hasHistoryKey() const override {return provider_&&provider_->hasHistoryKey();}
    void setHistoryKey(const QString& key) override {if(provider_)provider_->setHistoryKey(key);}
    void fetchHistory(bool more=false) override {if(provider_)provider_->fetchHistory(more);}
    bool canManageAccount() const override {return bool(provider_);}
    bool accountBusy() const override {return provider_&&provider_->accountBusy();}
    QString accountMessage() const override {return provider_?provider_->accountMessage():QString();}
    void login(const QString& name,const QString& secret) override {if(provider_)provider_->login(name,secret);}
    void disconnectAccount() override {if(provider_)provider_->disconnectAccount();}
    bool disconnectForRemoval() {return !provider_ || provider_->disconnectForRemoval();}
private:
    LibraryRepository& library_;
    std::unique_ptr<RetroAchievementsProvider> provider_;
};
}
