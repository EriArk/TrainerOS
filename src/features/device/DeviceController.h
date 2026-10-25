#pragma once
#include "core/input/Action.h"
#include "platform/device/DeviceService.h"
#include <QVariantList>
#include <QTimer>

namespace trainer {
class DeviceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList radios READ radios NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool audioOpen READ audioOpen NOTIFY changed)
    Q_PROPERTY(QVariantList audioOutputs READ audioOutputs NOTIFY changed)
    Q_PROPERTY(QString audioOutput READ audioOutput NOTIFY changed)
    Q_PROPERTY(int audioFocus READ audioFocus NOTIFY changed)
    Q_PROPERTY(bool mixer READ mixer NOTIFY changed)
    Q_PROPERTY(QVariantList audioStreams READ audioStreams NOTIFY changed)
    Q_PROPERTY(bool inputs READ inputs NOTIFY changed)
    Q_PROPERTY(QVariantList audioInputs READ audioInputs NOTIFY changed)
    Q_PROPERTY(QString audioInput READ audioInput NOTIFY changed)
public:
    using QObject::QObject;
    void configure(DeviceService* service, bool powerAvailable);
    void begin();
    void setMonitoring(bool enabled);
    void adjustQuick(int index, Action action);
    Q_INVOKABLE void setQuickLevel(int index, int value);
    void requestPower(bool restart);
    QVariantList rows() const;
    QVariantList status() const;
    QVariantList radios() const;
    int focusIndex() const { return focus_; }
    bool busy() const { return service_ && service_->busy(); }
    QString error() const { return service_ ? service_->error() : QString(); }
    void dispatch(Action);
    void activate(int index);
    bool audioOpen() const{return audioOpen_;}
    QVariantList audioOutputs() const{return service_?service_->snapshot().audioOutputs:QVariantList{};}
    QString audioOutput() const{return service_?service_->snapshot().audioOutput:QString();}
    int audioFocus() const;
    bool mixer() const{return mixer_;}
    bool inputs() const{return inputs_;}
    QVariantList audioInputs() const{return service_?service_->snapshot().audioInputs:QVariantList{};}
    QString audioInput() const{return service_?service_->snapshot().audioInput:QString();}
    Q_INVOKABLE void showInputs();
    Q_INVOKABLE void setInput(const QString& id,int level);
    Q_INVOKABLE void muteInput(const QString& id);
    Q_INVOKABLE void selectInput(const QString& id);
    QVariantList audioStreams() const{return service_?service_->snapshot().audioStreams:QVariantList{};}
    Q_INVOKABLE void showMixer(bool value);
    Q_INVOKABLE void setStream(const QString& id,int level);
    Q_INVOKABLE void muteStream(const QString& id);
    void beginAudio();
    void dispatchAudio(Action action);
    Q_INVOKABLE void closeAudio();
    Q_INVOKABLE void selectOutput(const QString& id);
signals:
    void changed();
    void closeRequested();
    void powerRequested(const QString& mode);
    void messageRequested(const QString& message);
private:
    DeviceService* service_ = nullptr;
    bool powerAvailable_ = false;
    int focus_ = 0;
    QTimer monitor_;
    bool audioOpen_=false;
    bool mixer_=true;
    bool inputs_=false;
    QVariantList audioRows() const{return inputs_?audioInputs():mixer_?audioStreams():audioOutputs();}
    QString audioFocusId_;
};
}
