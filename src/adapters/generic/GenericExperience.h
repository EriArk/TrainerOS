#pragma once
#include "core/experience/ExperienceModule.h"
namespace trainer {
// The fallback contributes Home only. Overview/RA/Lobby belong to the host.
class GenericExperience final : public ExperienceModule {
public:
    explicit GenericExperience(ExperienceServices) {}
    ExperienceDescriptor descriptor() const override { return genericExperienceDescriptor(); }
    ExperienceManifest manifest() const override { return {"generic",1,{},{}}; }
    QUrl presenter() const override { return QUrl("qrc:/TrainerOS/src/adapters/generic/GenericExperienceViews.qml"); }
};
}
