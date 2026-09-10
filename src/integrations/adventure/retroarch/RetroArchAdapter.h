#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"
#include <QHash>
#include <functional>

namespace trainer {
// Machine-specific installation data belongs outside the personal Adventure's
// display model. Loading this small local configuration performs no execution.
struct RetroArchInstallation {
    QString program;
    QStringList prefixArguments;
    QString configFile;
    QHash<QString, QString> cores;
    static RetroArchInstallation load(const QString& filename);
};

class RetroArchAdapter final : public AdventureAdapter {
public:
    RetroArchAdapter(LibraryRepository&, RetroArchInstallation);
    QString id() const override { return "retroarch"; }
    AdventureCapabilities capabilities(const Adventure&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    // The application supplies checkpoint/window/lifecycle coordination.
    std::function<bool(const ProcessCommand&)> requestLaunch;
private:
    std::optional<ProcessCommand> command(const Adventure&) const;
    LibraryRepository& repository_;
    RetroArchInstallation installation_;
};
}
