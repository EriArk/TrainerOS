#pragma once
#include "AdventureAdapter.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"
#include <QFileInfo>
#include <QJsonDocument>

namespace trainer {
// Ordinary process binding, deliberately independent of Experience modules.
class PcAdventureAdapter final : public AdventureAdapter {
public:
    LibraryRepository* library = nullptr;
    QString helper = "/var/opt/traineros/pc/launch.py";
    std::function<AdventureResult(const ProcessCommand&, const QString&)> requestLaunch;
    QString id() const override { return "pc-local"; }
    AdventureCapabilities capabilities(const Adventure& a) const override {
        return {a.adapterId == id() && !a.collectionOnly, false, true};
    }
    QString setupIssue(const AdventureRegistration&) const override { return {}; }
    QString verifyInstallation(const AdventureRegistration& r) const override {
        if (!QFileInfo(r.contentPath).isFile()) return "This PC game is unavailable. Reconnect its storage or repair its location.";
        if (!QFileInfo(helper).isFile()) return "PC launch support is not installed on this device.";
        const auto c = r.integrationConfig;
        if (c["runner"] == "native")
            return QFileInfo(r.contentPath).isExecutable() ? QString() : QString("The game's executable permission is missing.");
        if (c["runner"] != "proton") return "Choose a compatible PC runtime.";
        if (!QFileInfo(c["proton"].toString()+"/proton").isFile() || !QFileInfo(c["umu"].toString()).isFile())
            return "The selected Windows runtime is unavailable. Reconnect its storage or select another runtime.";
        return {};
    }
    AdventureResult launch(const Adventure& a) override {
        const auto r = library ? library->registration(a.id) : std::nullopt;
        if (!r) return {false,"PC game registration is unavailable."};
        auto config = r->integrationConfig;
        config["executable"] = r->contentPath;
        ProcessCommand command;
        command.program = "/usr/bin/python3";
        command.arguments = {helper, QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact))};
        command.workingDirectory = QFileInfo(r->contentPath).absolutePath();
        command.runtimeControls = {{"kind","native"}};
        return requestLaunch ? requestLaunch(command,a.id) : AdventureResult{false,"PC launch is unavailable."};
    }
    AdventureResult resume(const Adventure& a, const ResumePoint&) override { return launch(a); }
};
}
