#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"

namespace trainer {
struct StandaloneInstallation {
    QString program, runtimeFile;
    QStringList prefixArguments, platforms;
    static StandaloneInstallation load(const QString& filename, const QString& adapterId);
};
class StandaloneAdapter final : public AdventureAdapter {
public:
    StandaloneAdapter(QString id, LibraryRepository& library, StandaloneInstallation installation);
    QString id() const override { return id_; }
    AdventureCapabilities capabilities(const Adventure&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    void prepareInstallation(AdventureRegistration&) const override;
    std::function<bool(const ProcessCommand&, const QString&)> requestLaunch;
private:
    bool supports(const QString& platform, const QString& path) const;
    std::optional<ProcessCommand> command(const Adventure&) const;
    QString id_;
    LibraryRepository& library_;
    StandaloneInstallation installation_;
};
}
