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
        emit snapshotChanged({});provider_->refreshAll();
    }
    AchievementContext context() const override { return provider_?provider_->context():AchievementContext{"retroAchievements",{}}; }
    QList<AchievementSet> sets() const override {return provider_?provider_->sets():QList<AchievementSet>{};}
    AchievementSnapshot snapshot(const QString& id) const override {return provider_?provider_->snapshot(id):AchievementSnapshot{context(),id,AchievementState::Disconnected,{},{},{}};}
    void refresh(const QString& id) override {if(provider_)provider_->refresh(id);}
    void refreshAll() override {if(provider_)provider_->refreshAll();}
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
