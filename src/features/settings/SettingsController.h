#pragma once
#include "core/input/Action.h"
#include "core/repository/PreferencesRepository.h"
#include <QVariantList>
#include "core/model/SaveBackup.h"

namespace trainer {
class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme NOTIFY changed)
    Q_PROPERTY(bool worldEditing READ worldEditing NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY changed)
    Q_PROPERTY(bool videoPreviews READ videoPreviews NOTIFY changed)
    Q_PROPERTY(bool readOnlySaves READ readOnlySaves NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString libraryStatus READ libraryStatus NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool creditsOpen READ creditsOpen NOTIFY changed)
    Q_PROPERTY(int category READ category NOTIFY changed)
    Q_PROPERTY(int rowFocus READ rowFocus NOTIFY changed)
    Q_PROPERTY(bool controlsFocused READ controlsFocused NOTIFY changed)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
public:
    using QObject::QObject;
    void setRepository(PreferencesRepository* repository) { repository_ = repository; reload(); }
    void reload();
    void configureSavePolicy(SaveBackupService* service) { savePolicy_=service;connect(service,&SaveBackupService::policyChanged,this,&SettingsController::changed); }
    bool readOnlySaves() const { return savePolicy_ && savePolicy_->readOnly(); }
    void setTrainersAvailable(bool value) { trainersAvailable_ = value; emit changed(); }
    void setLegacyTrashAvailable(bool value) { if(legacyTrash_==value)return;legacyTrash_=value;if(category_==8)row_=0;emit changed(); }
    void begin() { category_ = 0; row_ = 0; pane_ = false; emit changed(); }
    int category() const { return category_; }
    int rowFocus() const { return row_; }
    bool controlsFocused() const { return pane_; }
    QStringList categories() const { return {"Appearance", "Sound", "Media", "Feedback", "Trainer", "System", "Credits", "Controller", "Library", "Saves", "Connections"}; }
    QVariantList controls() const;
    Q_INVOKABLE void selectCategory(int index, bool enter = true);
    Q_INVOKABLE void activateRow(int index);
    Q_INVOKABLE void cycleTheme(int direction = 1);
    bool creditsOpen() const { return category_==6 && pane_; }
    QString theme() const { return value_.theme; }
    bool worldEditing() const { return value_.worldEditing; }
    bool reducedMotion() const { return value_.reducedMotion; }
    bool videoPreviews() const { return value_.videoPreviews; }
    bool saving() const { return saving_; }
    QString error() const { return error_; }
    QString libraryStatus() const { return libraryStatus_; }
    void setLibraryScanState(bool available, bool busy, const QString& result = {});
    int focusIndex() const { return pane_ ? row_ : category_; }
    void dispatch(Action);
    void activate(int index);
signals:
    void changed();
    void closeRequested();
    void deviceRequested(int index);
    void controllerRequested();
    void trashRequested();
    void libraryRefreshRequested();
    void trainerRequested(int index);
    void messageRequested(const QString& message);
    void quickAdjustment(int index, trainer::Action action);
private:
    SaveBackupService* savePolicy_ = nullptr;
    PreferencesRepository* repository_ = nullptr;
    ShellPreferences value_;
    bool saving_ = false;
    bool trainersAvailable_ = false;
    bool legacyTrash_ = false;
    bool libraryAvailable_ = false, libraryScanning_ = false;
    QString libraryStatus_;
    QString error_;
    int category_ = 0, row_ = 0;
    bool pane_ = false;
};
}
