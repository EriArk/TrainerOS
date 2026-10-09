#pragma once
#include "core/experience/ExperienceModule.h"
namespace trainer::test {
// Independent compiled module: no Pokemon services, shell subclass or title rule.
class CounterExperience final : public ExperienceModule {
public:
    explicit CounterExperience(QString id):id_(std::move(id)) {}
    ExperienceDescriptor descriptor() const override {
        return {id_,version,id_+"-home",{id_+" tools",{{"counter","Counter",id_+"-counter"},{"second","Second",id_+"-second"}}},{id_+" history",{{"records","Records",id_+"-records"}}}};
    }
    ExperienceManifest manifest() const override { return {id_,1,{{"fixture",{},{{matchGame.isEmpty()?"legacy.domain":"local.game",matchGame.isEmpty()?id_:matchGame}}}}, {}}; }
    QUrl presenter() const override {return QUrl("qrc:/TrainerOS/tests/fixtures/adapters/CounterExperience.qml");}
    QVariantMap homeProgress() const override {return {{"fixtureValue",count},{"fixtureOwner",context_.owner},{"fixtureGame",context_.adventure}};}
    QVariantList liveActions(const ExperienceLiveContext&) const override {return {QVariantMap{{"id","inspect"},{"label","Inspect fixture session"}}};}
    void invokeLiveAction(const QString&,const ExperienceLiveContext& context) override {lastLiveGame=context.adventure;++liveCalls;}
    void show(int slot,const QString& face) override {lastSlot=slot;lastFace=face;}
    bool dispatch(Action action) override {if(action==Action::Confirm){++count;emit changed();}return true;}
    bool activate(int index,const QString&) override {count=index;emit changed();return true;}
    void activateHome(int index,const QString&) override {count=index;emit changed();}
    int focusIndex() const override {return count;}
    QJsonObject navigation() const override {return {{"count",count}};}
    void restoreNavigation(const QJsonObject& state) override {count=state["count"].toInt();}
    void requestText(){textApply_=[this](QString text){lastText=text;};emit textRequested("Fixture text",{},24,context_.generation);}
    quint64 token() const {return context_.generation;}
    void requestFace(){emit faceRequested(0,"second",false,context_.generation);}
    int count=0,lastSlot=-1,version=1,liveCalls=0;
    QString lastFace,lastText,matchGame,lastLiveGame;
private:
    QString id_;
};
}
