#pragma once
#include "integrations/adventure/AdventureAdapter.h"
#include <QHash>

namespace trainer {
class MockAdventureAdapter final : public AdventureAdapter {
public:
    QString id() const override { return "mock"; }
    MockAdventureAdapter();
    AdventureCapabilities capabilities(const Adventure&) const override;
    AdventureResult launch(const Adventure&) override;
    AdventureResult resume(const Adventure&, const ResumePoint&) override;
    void setCapabilities(const QString& adventureId, AdventureCapabilities capabilities);
    void failNextLaunch() { failNextLaunch_ = true; }
private:
    QHash<QString, AdventureCapabilities> overrides_;
    bool failNextLaunch_ = false;
};
}
