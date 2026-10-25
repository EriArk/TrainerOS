#pragma once
#include "core/input/ControllerInput.h"
#include "platform/diagnostics/DiagnosticsService.h"
#include <QVariantList>
#include <QPointer>
#include <QJsonArray>
#include "core/repository/LibraryRepository.h"
#include "integrations/adventure/AdventureAdapter.h"
#include <QFuture>

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
    Q_PROPERTY(bool health READ health NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QVariantList healthRows READ healthRows NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedHealth READ selectedHealth NOTIFY changed)
    Q_PROPERTY(QString healthSummary READ healthSummary NOTIFY changed)
public:
    explicit DiagnosticsController(QObject* parent = nullptr) : QObject(parent) {}
    ~DiagnosticsController() override {checkFuture_.waitForFinished();}
    void configureLibrary(LibraryRepository* library,AdventureAdapter* adapter){library_=library;adapter_=adapter;}
    std::function<bool()> canCheck;
    bool health() const{return health_;}
    bool checking() const{return checking_;}
    QVariantList healthRows() const{return healthRows_;}
    QVariantMap selectedHealth() const{return healthRows_.value(std::max(0,focus_-5)).toMap();}
    QString healthSummary() const;
    void beginHealth();
    Q_INVOKABLE void healthAction(int index);
    Q_INVOKABLE void openGameSetup(){const auto games=selectedHealth()["games"].toStringList();if(!games.isEmpty()&&!checking_)emit setupRequested(games.first());}
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
    void reportsRequested(QString directory);
    void setupRequested(QString game);
    void recheckRequested();
    void scrollRequested(int direction);
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
    void refreshHealth();
    void checkHealth(int index);
    bool health_=false,checking_=false;
    QVariantList healthRows_;
    LibraryRepository* library_=nullptr;
    AdventureAdapter* adapter_=nullptr;
    QFuture<void> checkFuture_;
};
}
