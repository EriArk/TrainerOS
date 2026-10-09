#pragma once
#include "core/experience/ExperienceModule.h"
#include "features/halloffame/HallOfFameController.h"
#include "core/repository/LibraryRepository.h"
#include <algorithm>
namespace trainer {
class GenericExperience final : public ExperienceModule {
public:
    explicit GenericExperience(ExperienceServices services):services_(services) {}
    ExperienceDescriptor descriptor() const override { return genericExperienceDescriptor(); }
    ExperienceManifest manifest() const override { return {"generic",1,{},{}}; }
    QUrl presenter() const override { return QUrl("qrc:/TrainerOS/src/adapters/generic/GenericExperienceViews.qml"); }
    void show(int slot,const QString& face) override { face_=face;if(slot==1 && face=="ra")services_.achievements.showFace(2); }
    int focusIndex() const override { return face_=="ra"?services_.achievements.focusIndex():focus_; }
    bool modalOpen() const override { return face_=="ra" && services_.achievements.account()->isOpen(); }
    bool dispatch(Action action) override {
        if(face_=="ra"){services_.achievements.dispatch(action==Action::LocalAction?Action::ToggleContinue:action);return true;}
        if(action==Action::Back)emit hostActionRequested("home",context_.generation);
        else if(action==Action::LocalAction || (action==Action::Confirm && face_=="details"))emit hostActionRequested("game-properties",context_.generation);
        else if(action==Action::Up || action==Action::Down)focus_=std::clamp(focus_+(action==Action::Down?1:-1),0,std::max(0,int(services_.library.gameSessions(context_.adventure).size())-1));
        emit changed();return true;
    }
    bool activate(int index,const QString& area) override {
        if(face_=="ra"){if(area.isEmpty())services_.achievements.activate(index);else services_.achievements.activateControl(area,index);}
        else if(face_=="details")emit hostActionRequested("game-properties",context_.generation);
        else focus_=std::clamp(index,0,std::max(0,int(services_.library.gameSessions(context_.adventure).size())-1));
        emit changed();return true;
    }
    QJsonObject navigation() const override {return {{"focus",focus_}};}
    void restoreNavigation(const QJsonObject& state) override {focus_=qMax(0,state["focus"].toInt());}
private:
    ExperienceServices services_;
    QString face_="details";
    int focus_=0;
};
}
