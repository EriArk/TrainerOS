#include "DiagnosticsController.h"
#include <algorithm>
#include <QtConcurrent>
#include <QFileInfo>
#include <QDir>
#include "core/repository/CollectionRepository.h"

namespace trainer {
namespace {
QString actionName(Action action) {
    const QStringList names{"Up", "Down", "Left", "Right", "Confirm", "Back", "System menu", "Previous section", "Next section", "Recent games", "Quick Apps", "Home", "Previous face", "Next face", "Local action"};
    return names.value(int(action), "Unknown action");
}
}
void DiagnosticsController::configure(ControllerInput* input, DiagnosticsService* service) {
    if (input_) disconnect(input_, nullptr, this, nullptr);
    if (service_) disconnect(service_, nullptr, this, nullptr);
    input_ = input; service_ = service;
    if (input_) {
        connect(input_, &ControllerInput::sampled, this, &DiagnosticsController::sample);
        connect(input_, &ControllerInput::observedAction, this, [this](Action action, bool controller) {
            if (!recording_ || !controller) return; // Keyboard never satisfies a controller check.
            lastAction_ = actionName(action);
            actions_.append(lastAction_);
            while (actions_.size() > 24) actions_.removeFirst();
            emit changed();
        });
    }
    if (service_) connect(service_, &DiagnosticsService::runtimeChanged, this, [this] {
        if (recording_) { runtime_ = service_->runtime(); emit changed(); }
    });
    if (service_) connect(service_, &DiagnosticsService::completed, this, [this](const QString& error, const QString&) {
        status_ = error.isEmpty() ? "Report saved locally in the diagnostics folder." : error;
        emit changed(); emit messageRequested(status_);
    });
}
void DiagnosticsController::begin() {
    health_=false;
    focus_ = 0;
    if (!recording_) { recording_ = true; reset(); }
    if (service_) runtime_ = service_->runtime();
    sample(); emit changed();
}
void DiagnosticsController::sample() {
    if (!recording_ || !input_) return;
    auto next = input_->sample();
    if (!next.connected) { next.name = sample_.name; next.guid = sample_.guid; next.mapping = sample_.mapping; }
    if (next.connected != sample_.connected) next.connected ? ++connections_ : ++disconnections_;
    // A different mapping starts a new set of button/range observations.
    if (next.connected && (next.guid != sample_.guid || next.mapping != sample_.mapping)) {
        seen_.fill(false); minimum_.fill(0); maximum_.fill(0); axesSeen_.fill(false); actions_ = {};
        lastAction_ = "No controller action recorded";
    }
    const bool changedReadings = next.connected != sample_.connected || next.enabled != sample_.enabled
        || next.awaitingNeutral != sample_.awaitingNeutral || next.name != sample_.name
        || next.buttons != sample_.buttons || next.axes != sample_.axes || next.unmappedDevices != sample_.unmappedDevices;
    sample_ = next;
    if (sample_.connected) {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) seen_[i] = seen_[i] || sample_.buttons[i];
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
            minimum_[i] = axesSeen_[i] ? std::min(minimum_[i], sample_.axes[i]) : sample_.axes[i];
            maximum_[i] = axesSeen_[i] ? std::max(maximum_[i], sample_.axes[i]) : sample_.axes[i]; axesSeen_[i] = true;
        }
    }
    if (changedReadings) emit changed();
}
void DiagnosticsController::reset() {
    seen_.fill(false); minimum_.fill(0); maximum_.fill(0); axesSeen_.fill(false); actions_ = {};
    connections_ = disconnections_ = 0; lastAction_ = "No controller action recorded";
    status_ = "Checks reset. Button marks mean a signal was observed, not that its physical label is correct.";
    emit changed();
}
QVariantList DiagnosticsController::buttons() const {
    const std::pair<int, QString> shown[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP,"Up"}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN,"Down"}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT,"Left"},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,"Right"}, {input_?input_->confirmButton():ControllerInput::ConfirmButton,"A"}, {input_?input_->backButton():ControllerInput::BackButton,"B"},
        {SDL_CONTROLLER_BUTTON_Y,"X"}, {SDL_CONTROLLER_BUTTON_X,"Y"}, {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,"L1"},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,"R1"}, {SDL_CONTROLLER_BUTTON_START,"Start"}, {SDL_CONTROLLER_BUTTON_BACK,"Select"},
        {SDL_CONTROLLER_BUTTON_LEFTSTICK,"L3"}, {SDL_CONTROLLER_BUTTON_RIGHTSTICK,"R3"}, {SDL_CONTROLLER_BUTTON_GUIDE,"Guide"}};
    QVariantList result;
    for (const auto& [id, name] : shown) result.append(QVariantMap{{"label", input_?input_->glyph(name):name}, {"held", sample_.buttons[id]}, {"seen", seen_[id]}});
    return result;
}
QVariantList DiagnosticsController::axes() const {
    QVariantList result;
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) result.append(QVariantMap{{"value", sample_.axes[i] / 32768.0},
        {"minimum", minimum_[i] / 32768.0}, {"maximum", maximum_[i] / 32768.0}});
    return result;
}
QString DiagnosticsController::connection() const {
    if (!input_ || !sample_.initialized) return "Controller input is unavailable";
    if (sample_.connected) return sample_.name.isEmpty() ? "Controller connected" : sample_.name;
    if (!sample_.unmappedDevices.isEmpty()) return "Device detected without a controller mapping";
    return "Connect a controller to begin";
}
QString DiagnosticsController::gate() const {
    if (!sample_.connected) return "No mapped controller";
    if (!sample_.enabled) return "Shell input paused while inactive";
    return sample_.awaitingNeutral ? "Release buttons and center the left stick" : "Shell input ready";
}
QStringList DiagnosticsController::runtimeLines() const {
    if (runtime_.isEmpty()) return {"Display information is unavailable"};
    const auto window = runtime_["window"].toObject(), screen = runtime_["qtReportedScreen"].toObject();
    QStringList lines{runtime_["os"].toString() + " · " + runtime_["cpuArchitecture"].toString(),
        "Qt " + runtime_["qtVersion"].toString() + " · SDL " + runtime_["sdlVersion"].toString(),
        "Display backend: " + runtime_["qpaPlatform"].toString()};
    if (!window.isEmpty()) lines << QString("Window: %1 × %2 · DPR %3").arg(window["width"].toInt()).arg(window["height"].toInt()).arg(window["devicePixelRatio"].toDouble(), 0, 'f', 2);
    if (!screen.isEmpty()) lines << QString("Qt screen: %1 × %2 · %3 Hz").arg(screen["width"].toInt()).arg(screen["height"].toInt()).arg(screen["refreshRate"].toDouble(), 0, 'f', 1);
    return lines;
}
QJsonObject DiagnosticsController::observations() const {
    QJsonArray buttons, axes;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) buttons.append(QJsonObject{{"sdlButton", i},
        {"name", QString::fromLatin1(SDL_GameControllerGetStringForButton(SDL_GameControllerButton(i)))},
        {"held", sample_.buttons[i]}, {"observed", seen_[i]}});
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) axes.append(QJsonObject{{"sdlAxis", i},
        {"name", QString::fromLatin1(SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis(i)))},
        {"rawValue", sample_.connected ? QJsonValue(sample_.axes[i]) : QJsonValue()},
        {"minimum", axesSeen_[i] ? QJsonValue(minimum_[i]) : QJsonValue()},
        {"maximum", axesSeen_[i] ? QJsonValue(maximum_[i]) : QJsonValue()}});
    return {{"connected", sample_.connected}, {"name", sample_.name}, {"guid", sample_.guid}, {"sdlMapping", sample_.mapping},
        {"unmappedDevices", QJsonArray::fromStringList(sample_.unmappedDevices)}, {"gate", gate()}, {"buttons", buttons}, {"axes", axes},
        {"recentControllerActions", actions_}, {"connectionsObserved", connections_}, {"disconnectionsObserved", disconnections_},
        {"normalization", QJsonObject{{"stickEngage", ControllerInput::StickEngage}, {"stickRelease", ControllerInput::StickRelease},
            {"repeatDelayMs", ControllerInput::RepeatDelayMs}, {"repeatIntervalMs", ControllerInput::RepeatIntervalMs}}},
        {"readings", "SDL-mapped state before TrainerOS filtering; action history excludes keyboard input"}};
}
void DiagnosticsController::activate(int index) {
    if(health_){healthAction(index);return;}
    if (index < 0 || index > 3) return;
    focus_ = index;
    if (index == 3) { emit closeRequested(); return; }
    if (index == 0 && service_) { runtime_ = service_->runtime(); status_ = "Display readings refreshed. L1/R1 and Start keep their normal actions."; }
    if (index == 1 && service_ && !saving()) { service_->save(observations()); status_ = "Saving report…"; }
    if (index == 2) reset();
    if (!service_ && index < 2) status_ = "The diagnostic report service isn't available in this fixture.";
    emit changed();
}
void DiagnosticsController::dispatch(Action action) {
    if(health_) {
        if(action==Action::ScrollUp||action==Action::ScrollDown){emit scrollRequested(action==Action::ScrollDown?1:-1);return;}
        if(action==Action::LocalAction){openGameSetup();return;}
        if(action==Action::Back){health_=false;emit closeRequested();return;}
        if(action==Action::Confirm){healthAction(focus_);return;}
        if(action==Action::Up||action==Action::Left)focus_=std::max(0,focus_-1);
        if(action==Action::Down||action==Action::Right)focus_=std::min(int(healthRows_.size())+4,focus_+1);
        emit changed();return;
    }
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::Left) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Right) focus_ = std::min(3, focus_ + 1);
    emit changed();
}
QString DiagnosticsController::healthSummary() const {
    int games=0;for(const auto& row:healthRows_)games+=row.toMap()["games"].toStringList().size();
    return QString("%1 systems · %2 games\n").arg(healthRows_.size()).arg(games)+(checking_?"Checking files…":"Check runtime, storage and firmware. Gameplay compatibility is separate.");
}
void DiagnosticsController::beginHealth(){health_=true;focus_=0;status_.clear();if(service_)runtime_=service_->runtime();refreshHealth();emit changed();}
void DiagnosticsController::refreshHealth() {
    if(checking_||!library_||!adapter_)return;
    QMap<QString,QVariantMap> groups;
    for(auto r:library_->registrations()) {
        if(r.removed||r.adventure.collectionOnly)continue;
        if(r.adventure.adapterId=="unconfigured")adapter_->prepareInstallation(r);
        const auto key=r.adventure.platformId+":"+r.adventure.adapterId;
        auto group=groups.value(key);group["id"]=key;group["platform"]=r.adventure.platformId;group["adapter"]=r.adventure.adapterId;
        group["title"]=platformLabel(r.adventure.platformId).name;auto games=group["games"].toStringList();games<<r.adventure.id;group["games"]=games;
        auto titles=group["titles"].toStringList();titles<<r.adventure.title;group["titles"]=titles;
        auto issue=!r.contentAvailable?QString("Game storage is unavailable. Reconnect the source and recheck."):adapter_->setupIssue(r);
        if(!adapter_->capabilities(r.adventure).launch&&issue.isEmpty())issue="This route needs configuration or device verification.";
        if(!issue.isEmpty()){group["detail"]=issue;group["state"]="Needs attention";}
        else if(!group.contains("state")){group["state"]="Needs verification";group["detail"]="Runtime is configured. Select to check its files and required firmware.";}
        groups[key]=group;
    }
    healthRows_.clear();for(const auto& group:groups)healthRows_<<group;
    focus_=std::clamp(focus_,0,int(healthRows_.size())+4);emit changed();
}
void DiagnosticsController::checkHealth(int index) {
    if(checking_||!library_||!adapter_||index<0||index>=healthRows_.size())return;
    if(canCheck&&!canCheck()){status_="Finish the current game or library operation before checking.";emit changed();return;}
    const auto group=healthRows_[index].toMap();QList<AdventureRegistration> records;QSet<QString> launchable;
    for(const auto& id:group["games"].toStringList())if(auto r=library_->registration(id)){if(r->adventure.adapterId=="unconfigured")adapter_->prepareInstallation(*r);if(adapter_->capabilities(r->adventure).launch)launchable.insert(id);records<<*r;}
    checking_=true;status_="Checking runtime, game files and required firmware…";emit changed();
    checkFuture_=QtConcurrent::run([this,records,launchable,index,key=group["id"].toString()]{
        QStringList issues;int ready=0;
        for(const auto& r:records){const QFileInfo f(r.contentPath);auto issue=(!f.isFile()||!f.isReadable()||f.size()==0)?QString("Game file unavailable. Reconnect storage or repair its location."):adapter_->verifyInstallation(r);if(issue.isEmpty()&&!launchable.contains(r.adventure.id))issue="No verified launch route for this game on this device.";if(issue.isEmpty())++ready;else if(!issues.contains(issue))issues<<issue;}
        QMetaObject::invokeMethod(this,[this,records,index,key,issues,ready]{
            checking_=false;bool stale=false;
            for(const auto& r:records){const auto current=library_->registration(r.adventure.id);if(!current||current->removed||current->revision!=r.revision||current->contentPath!=r.contentPath)stale=true;}
            if(stale||healthRows_.value(index).toMap()["id"]!=key){status_="The library changed. Recheck the current selection.";refreshHealth();return;}
            auto group=healthRows_[index].toMap();group["state"]=issues.isEmpty()?"Setup checked":"Needs attention";
            group["detail"]=issues.isEmpty()?QString("%1 game files and their runtime requirements checked. Gameplay compatibility still depends on the game and device.").arg(ready):issues.join("\n");
            group["verified"]=ready;healthRows_[index]=group;status_="Check finished.";emit changed();
        },Qt::QueuedConnection);
    });
}
void DiagnosticsController::healthAction(int index) {
    if(index<0||index>=healthRows_.size()+5)return;focus_=index;
    if(index>=5){checkHealth(index-5);emit changed();return;}
    if(index==0){if(!checking_){emit recheckRequested();refreshHealth();status_="Requested runtime discovery. Select a system to verify its files.";}}
    if(index==1&&service_&&!saving()){
        QJsonArray systems;for(const auto& v:healthRows_){const auto row=v.toMap();systems.append(QJsonObject{{"platform",row["platform"].toString()},{"runtime",row["adapter"].toString()},{"state",row["state"].toString()},{"games",row["games"].toStringList().size()},{"verified",row.value("verified",0).toInt()}});}
        service_->saveSupport({{"systems",systems},{"controllerConnected",input_&&input_->connected()}});status_="Saving a private-data-free support archive…";
    }
    if(index==2&&service_)emit reportsRequested(service_->reportDirectory());
    if(index==3)status_="Offline help: reconnect removable game storage; place your own BIOS files in the library BIOS folder using Files; then refresh and check again. A configured emulator is not proof of game compatibility. Use game Properties to inspect its setup. ROMs and saves are never changed by checks.";
    if(index==4){health_=false;emit closeRequested();return;}
    emit changed();
}

}
