#pragma once
#include "AdventureAdapter.h"

namespace trainer {
// Non-owning composition boundary. Feature pages keep one capability interface.
class AdapterRouter final : public AdventureAdapter {
public:
    explicit AdapterRouter(const QList<AdventureAdapter*>& adapters) {
        for (auto* adapter : adapters) if (adapter) adapters_.append(adapter);
    }
    QString id() const override { return "router"; }
    AdventureCapabilities capabilities(const Adventure& a) const override {
        const auto* adapter = find(a); return adapter ? adapter->capabilities(a) : AdventureCapabilities{};
    }
    ResumeAvailability resumeAvailability(const Adventure& a, const ResumePoint& p) const override {
        const auto* adapter = find(a); return adapter ? adapter->resumeAvailability(a, p) : ResumeAvailability::Incompatible;
    }
    AdventureResult launch(const Adventure& a) override {
        auto* adapter = find(a); return adapter ? adapter->launch(a) : unavailable_.launch(a);
    }
    AdventureResult resume(const Adventure& a, const ResumePoint& p) override {
        auto* adapter = find(a); return adapter ? adapter->resume(a, p) : unavailable_.resume(a, p);
    }
    void prepareInstallation(AdventureRegistration& record) const override {
        if (auto* adapter = find(record.adventure)) { adapter->prepareInstallation(record); return; }
        if (record.adventure.adapterId != "unconfigured") return;
        for (auto* adapter : adapters_) {
            adapter->prepareInstallation(record);
            if (record.adventure.adapterId != "unconfigured") return;
        }
    }
private:
    AdventureAdapter* find(const Adventure& a) const {
        for (auto* adapter : adapters_) if (adapter->id() == a.adapterId) return adapter;
        return nullptr;
    }
    QList<AdventureAdapter*> adapters_;
    UnconfiguredAdventureAdapter unavailable_;
};
}
