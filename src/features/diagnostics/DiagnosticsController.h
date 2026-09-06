#pragma once
#include "core/input/ControllerInput.h"
#include "platform/diagnostics/DiagnosticsService.h"
#include <QVariantList>
#include <QPointer>
#include <QJsonArray>

namespace trainer {
class DiagnosticsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY changed)
    Q_PROPERTY(QVariantList axes READ axes NOTIFY changed)
    Q_PROPERTY(QString connection READ connection NOTIFY changed)
    Q_PROPERTY(QString gate READ gate NOTIFY changed)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY changed)
    Q_PROPERTY(QStringList runtimeLines READ runtimeLines NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    explicit DiagnosticsController(QObject* parent = nullptr) : QObject(parent) {}
    void configure(ControllerInput*, DiagnosticsService*);
    void begin();
    void dispatch(Action);
    void activate(int index);
    void reset();
    QJsonObject observations() const;
    QVariantList buttons() const;
    QVariantList axes() const;
    QString connection() const;
    QString gate() const;
    QString lastAction() const { return lastAction_; }
    QStringList runtimeLines() const;
    QString status() const { return status_; }
    bool saving() const { return service_ && service_->saving(); }
    int focusIndex() const { return focus_; }
signals:
    void changed();
    void closeRequested();
    void messageRequested(const QString& message);
private:
    void sample();
    QPointer<ControllerInput> input_;
    QPointer<DiagnosticsService> service_;
    ControllerSample sample_;
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX> seen_{};
    std::array<int, SDL_CONTROLLER_AXIS_MAX> minimum_{}, maximum_{};
    std::array<bool, SDL_CONTROLLER_AXIS_MAX> axesSeen_{};
    QJsonArray actions_;
    QJsonObject runtime_;
    QString lastAction_ = "No controller action recorded", status_;
    bool recording_ = false;
    int focus_ = 0, connections_ = 0, disconnections_ = 0;
};
}
