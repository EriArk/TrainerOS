#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"
#include <QHash>
#include <functional>
#include <memory>
#include <mutex>

namespace trainer {
// Bound once after authenticated entry; worker snapshots never read UI-owned state.
struct RetroArchSaveOwner { QString id, directory; bool legacy = false; };
class RetroArchSaveSession final {
public:
    bool bind(const RetroArchSaveOwner& owner) {
        std::lock_guard lock(mutex_);
        if(owner.id.isEmpty() || owner.directory.isEmpty())return false;
        if(owner_)return owner_->id==owner.id && owner_->directory==owner.directory && owner_->legacy==owner.legacy;
        owner_=owner;return true;
    }
    std::optional<RetroArchSaveOwner> owner() const { std::lock_guard lock(mutex_);return owner_; }
private:
    mutable std::mutex mutex_;
    std::optional<RetroArchSaveOwner> owner_;
};
// Machine-specific installation data belongs outside the personal Adventure's
// display model. Loading this small local configuration performs no execution.
struct RetroArchInstallation {
    QString program;
    QStringList prefixArguments;
    QString configFile;
    QHash<QString, QString> cores;
    // Legacy migration/test evidence only; normal launch never resumes states.
    QString resumeDirectory;
    // Verified runtime identity is also required by ordinary-save backups.
    QString runtimeFile;
    bool saveBackups = false;
    std::shared_ptr<RetroArchSaveSession> saves;
    static RetroArchInstallation load(const QString& filename);
};

class RetroArchAdapter final : public AdventureAdapter {
public:
    RetroArchAdapter(LibraryRepository&, RetroArchInstallation);
    QString id() const override { return "retroarch"; }
    AdventureCapabilities capabilities(const Adventure&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    void prepareInstallation(AdventureRegistration&) const override;
    // The application supplies checkpoint/window/lifecycle coordination.
    std::function<bool(const ProcessCommand&, const QString& adventureId)> requestLaunch;
private:
    std::optional<ProcessCommand> command(const Adventure&) const;
    LibraryRepository& repository_;
    RetroArchInstallation installation_;
};
}
