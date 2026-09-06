#pragma once
#include "core/model/Models.h"
#include <QObject>
#include <functional>

namespace trainer {
struct ProfileWriteResult {
    bool success;
    QString error;
};
class TrainerRepository {
public:
    virtual ~TrainerRepository() = default;
    virtual std::optional<TrainerProfile> load() const = 0;
    // Reads are cached. Completion runs on the caller's thread while context lives.
    virtual void saveAsync(const TrainerProfile&, QObject* context,
                           std::function<void(ProfileWriteResult)> completed) = 0;
};
class MockTrainerRepository final : public TrainerRepository {
public:
    explicit MockTrainerRepository(std::optional<TrainerProfile> initial = {}) : profile_(std::move(initial)) {}
    std::optional<TrainerProfile> load() const override { return profile_; }
    ProfileWriteResult save(const TrainerProfile& profile);
    void saveAsync(const TrainerProfile& profile, QObject*, std::function<void(ProfileWriteResult)> completed) override {
        completed(save(profile));
    }
    void failNextSave() { failNextSave_ = true; }
private:
    std::optional<TrainerProfile> profile_;
    bool failNextSave_ = false;
};
}
