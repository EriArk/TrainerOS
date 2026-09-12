#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include <QHash>

namespace trainer {
class MockAdventureAdapter final : public AdventureAdapter {
public:
    QString id() const override { return "mock"; }
    MockAdventureAdapter();
    AdventureCapabilities capabilities(const Adventure&) const override;
    ResumeAvailability resumeAvailability(const Adventure&, const ResumePoint&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    void setCapabilities(const QString& adventureId, AdventureCapabilities capabilities);
    void failNextLaunch() { failNextLaunch_ = true; }
    void setSource(const QString& sourceId, const QString& revision) { sources_.insert(sourceId, revision); }
    void setIntegrationRevision(const QString& revision) { integrationRevision_ = revision; }
private:
    QHash<QString, QString> sources_;
    QString integrationRevision_ = "mock-v1";
    QHash<QString, AdventureCapabilities> overrides_;
    bool failNextLaunch_ = false;
};
}
