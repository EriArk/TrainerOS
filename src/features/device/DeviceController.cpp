#include "DeviceController.h"
#include <algorithm>

namespace trainer {
namespace {
QString capacity(qint64 free, qint64 total) {
    if (free < 0 || total <= 0) return "Unavailable";
    constexpr double gib = 1024.0 * 1024 * 1024;
    return QString("%1 GiB free / %2 GiB").arg(free / gib, 0, 'f', 1).arg(total / gib, 0, 'f', 1);
}
}
void DeviceController::configure(DeviceService* service, bool powerAvailable) {
    service_ = service; powerAvailable_ = powerAvailable;
    if (service_) connect(service_, &DeviceService::changed, this, [this]{
        if(audioOpen_&&audioFocus()<0){audioFocusId_=inputs_?audioInput():mixer_?QString():audioOutput();if(audioFocus()<0)audioFocusId_=audioRows().value(0).toMap()["id"].toString();}
        emit changed();
    });
    monitor_.setInterval(1500);
    connect(&monitor_, &QTimer::timeout, this, [this] { if (service_ && !busy()) service_->refresh(); });
}
void DeviceController::setMonitoring(bool enabled) {
    if (enabled == monitor_.isActive()) return;
    if (enabled) { monitor_.start(); if (service_) service_->refresh(); }
    else monitor_.stop();
}
void DeviceController::adjustQuick(int index, Action action) {
    if (!service_ || index < 0 || index > 4) return;
    if (index >= 2) {
        const auto radio = radios().at(index - 2).toMap();
        if (radio.value("level").toInt() < 0) return;
        if (action == Action::Confirm) service_->toggleRadio(radio.value("id").toString());
        else if (action == Action::Left || action == Action::Right)
            service_->setValue(radio.value("id").toString(), action == Action::Right);
        return;
    }
    if (action == Action::Confirm && index == 0) service_->toggleMute();
    else if (action == Action::Left || action == Action::Right)
        service_->adjust(index == 0 ? "volume" : "brightness", action == Action::Right ? 5 : -5);
}
void DeviceController::setQuickLevel(int index, int value) {
    if(!service_ || index<0 || index>1) return;
    const auto current=service_->snapshot();
    if((index==0 ? current.volume : current.brightness)<0) return;
    service_->setValue(index==0 ? "volume" : "brightness",std::clamp(value,index==0 ? 0 : 5,100));
}
void DeviceController::begin() { focus_ = 0; if (service_) service_->refresh(); emit changed(); }
int DeviceController::audioFocus() const {
    const auto rows=audioRows();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==audioFocusId_)return i;
    return -1;
}
void DeviceController::beginAudio(){
    audioOpen_=true;mixer_=true;inputs_=false;audioFocusId_=audioStreams().value(0).toMap()["id"].toString();
    if(service_){service_->monitorAudio(true);service_->refresh();}emit changed();
}
void DeviceController::closeAudio(){if(!audioOpen_)return;audioOpen_=false;if(service_)service_->monitorAudio(false);emit changed();}
void DeviceController::showMixer(bool value){inputs_=false;mixer_=value;audioFocusId_=audioRows().value(0).toMap()["id"].toString();emit changed();}
void DeviceController::showInputs(){inputs_=true;mixer_=false;audioFocusId_=audioInput();if(audioFocus()<0)audioFocusId_=audioRows().value(0).toMap()["id"].toString();emit changed();}
void DeviceController::setInput(const QString& id,int level){if(audioOpen_&&service_){audioFocusId_=id;service_->setInput(id,level,"volume");}}
void DeviceController::muteInput(const QString& id){if(!audioOpen_||!service_)return;for(const auto& row:audioInputs())if(row.toMap()["id"]==id){audioFocusId_=id;service_->setInput(id,!row.toMap()["muted"].toBool(),"mute");return;}}
void DeviceController::selectInput(const QString& id){if(audioOpen_&&service_){audioFocusId_=id;service_->setInput(id,0,"default");}}
void DeviceController::setStream(const QString& id,int level){if(audioOpen_&&service_){audioFocusId_=id;service_->setStream(id,level);}}
void DeviceController::muteStream(const QString& id){if(!audioOpen_||!service_)return;for(const auto& row:audioStreams())if(row.toMap()["id"]==id){audioFocusId_=id;service_->setStream(id,!row.toMap()["muted"].toBool(),true);return;}}
void DeviceController::selectOutput(const QString& id){
    if(!audioOpen_||!service_||busy())return;
    for(const auto& row:audioOutputs())if(row.toMap()["id"]==id&&row.toMap()["available"].toBool()){
        audioFocusId_=id;service_->selectOutput(id);return;
    }
}
void DeviceController::dispatchAudio(Action action){
    if(!audioOpen_)return;
    if(action==Action::Back){closeAudio();return;}
    if(action==Action::PreviousFace||action==Action::NextFace){const int page=mixer_?0:inputs_?1:2;const int next=(page+(action==Action::NextFace?1:2))%3;if(next==1)showInputs();else showMixer(next==0);return;}
    const auto rows=audioRows();
    if(action==Action::Up||action==Action::Down){
        const int i=std::clamp(audioFocus()+(action==Action::Down?1:-1),0,std::max(0,int(rows.size())-1));
        audioFocusId_=rows.value(i).toMap()["id"].toString();emit changed();
    }else if(action==Action::Confirm){if(inputs_)muteInput(audioFocusId_);else if(mixer_)muteStream(audioFocusId_);else selectOutput(audioFocusId_);}
    else if(inputs_&&action==Action::LocalAction)selectInput(audioFocusId_);
    else if((mixer_||inputs_)&&(action==Action::Left||action==Action::Right)&&audioFocus()>=0){const int level=std::clamp(rows[audioFocus()].toMap()["level"].toInt()+(action==Action::Right?5:-5),0,100);if(inputs_)setInput(audioFocusId_,level);else setStream(audioFocusId_,level);}
}
QVariantList DeviceController::rows() const {
    const auto value = service_ ? service_->snapshot() : DeviceSnapshot{};
    const QString volume = value.volume < 0 ? "Unavailable" : QString::number(value.volume) + "%" + (value.muted ? " · Muted" : "");
    return {
        QVariantMap{{"title", "Volume"}, {"level", value.volume}, {"muted", value.muted}, {"value", volume}},
        QVariantMap{{"title", "Screen brightness"}, {"level", value.brightness}, {"muted", false}, {"value", (value.brightness < 0 ? QString("Unavailable") : QString::number(value.brightness) + "%")}},
        QVariantMap{{"title", "Refresh status"}, {"value", ""}},
        QVariantMap{{"title", "Restart device"}, {"value", powerAvailable_ ? "Save your place, then restart" : "Available in the ArmadaOS installation"}},
        QVariantMap{{"title", "Power off"}, {"value", powerAvailable_ ? "Save your place, then turn off" : "Available in the ArmadaOS installation"}},
        QVariantMap{{"title", "Back to Settings"}, {"value", ""}}
    };
}
QVariantList DeviceController::status() const {
    const auto value = service_ ? service_->snapshot() : DeviceSnapshot{};
    return {QVariantMap{{"title", "Network"}, {"value", value.network}},
            QVariantMap{{"title", "Trainer storage"}, {"value", capacity(value.internalFree, value.internalTotal)}},
            QVariantMap{{"title", "Adventure storage"}, {"value", capacity(value.libraryFree, value.libraryTotal)}}};
}
QVariantList DeviceController::radios() const {
    const auto value = service_ ? service_->snapshot() : DeviceSnapshot{};
    QVariantList result;
    const QStringList ids{"wifi", "bluetooth", "airplane"}, names{"Wi-Fi", "Bluetooth", "Airplane"};
    const QList<int> levels{value.wifi, value.bluetooth, value.airplane};
    for (int i = 0; i < 3; ++i) result.append(QVariantMap{{"id", ids[i]}, {"title", names[i]}, {"level", levels[i]},
        {"detail", levels[i] < 0 ? "Unavailable" : levels[i] ? "On" : "Off"}});
    return result;
}
void DeviceController::requestPower(bool restart) {
    if (powerAvailable_) emit powerRequested(restart ? "reboot" : "poweroff");
    else emit messageRequested("Power controls are available in the ArmadaOS installation.");
}
void DeviceController::activate(int index) {
    if (index < 0 || index > 5) return;
    focus_ = index;
    if (index == 5) { emit closeRequested(); return; }
    if (index >= 3) {
        requestPower(index == 3);
    } else if (service_) {
        if (index == 2) service_->refresh();
        else if (index == 0) service_->toggleMute();
    }
    emit changed();
}
void DeviceController::dispatch(Action action) {
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::ToggleContinue) { if (service_) service_->refresh(); return; }
    if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = std::min(5, focus_ + 1);
    if (focus_ < 2) adjustQuick(focus_, action);
    emit changed();
}
}
