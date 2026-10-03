#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"

namespace trainer {
namespace ppsspp { struct NetplayRequest; }
struct StandaloneInstallation {
    QString program, runtimeFile;
    QStringList prefixArguments, platforms;
    QString configFile;
    bool melonDsSaveBackups = false;
    bool operator==(const StandaloneInstallation&) const = default;
    static StandaloneInstallation load(const QString& filename, const QString& adapterId);
    static StandaloneInstallation fromJson(const QJsonObject&, const QString& adapterId);
};
class StandaloneAdapter final : public AdventureAdapter {
public:
    StandaloneAdapter(QString id, LibraryRepository& library, StandaloneInstallation installation);
    QString id() const override { return id_; }
    AdventureCapabilities capabilities(const Adventure&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult launchNetplay(const Adventure&, const ppsspp::NetplayRequest&);
    const StandaloneInstallation& installation() const { return installation_; }
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    void prepareInstallation(AdventureRegistration&) const override;
    QString setupIssue(const AdventureRegistration&) const override;
    QString verifyInstallation(const AdventureRegistration&) const override;
    bool updateInstallation(const StandaloneInstallation& value) {
        if (installation_ == value) return false;
        installation_ = value; return true;
    }
    std::function<AdventureResult(const ProcessCommand&, const QString&)> requestLaunch;
private:
    bool supports(const QString& platform, const QString& path) const;
    std::optional<ProcessCommand> command(const Adventure&) const;
    QString id_;
    LibraryRepository& library_;
    StandaloneInstallation installation_;
};
}
