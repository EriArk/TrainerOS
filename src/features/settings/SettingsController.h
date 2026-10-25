#pragma once
#include "LibraryStorageController.h"
#include "ClockController.h"
#include "GameInputProfiles.h"
#include "CommunicationSettings.h"
#include "core/input/Action.h"
#include "core/input/ControllerInput.h"
#include "core/repository/PreferencesRepository.h"
#include <QVariantList>
#include <QPointer>
#include "core/model/SaveBackup.h"

namespace trainer {
class ScrapeController;
class NativeActivityProvider;
class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(trainer::LibraryStorageController* storage READ storage CONSTANT)
    Q_PROPERTY(trainer::ClockController* clock READ clock CONSTANT)
    Q_PROPERTY(trainer::CommunicationSettings* communication READ communication CONSTANT)
    Q_PROPERTY(QString theme READ theme NOTIFY changed)
    Q_PROPERTY(QString displayProfile READ displayProfile NOTIFY changed)
    Q_PROPERTY(int interfaceScale READ interfaceScale NOTIFY changed)
    Q_PROPERTY(bool worldEditing READ worldEditing NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY changed)
    Q_PROPERTY(bool videoPreviews READ videoPreviews NOTIFY changed)
    Q_PROPERTY(bool previewSound READ previewSound NOTIFY changed)
    Q_PROPERTY(int previewDelay READ previewDelay NOTIFY changed)
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
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY changed)
    Q_PROPERTY(int sectionIndex READ sectionIndex NOTIFY changed)
    Q_PROPERTY(QString categoryId READ categoryId NOTIFY changed)
    Q_PROPERTY(trainer::GameInputProfiles* gameInput READ gameInput CONSTANT)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
public:
    GameInputProfiles* gameInput() {return &gameInput_;}
    void setController(ControllerInput* value) { input_=value;connect(value,&ControllerInput::profileChanged,this,&SettingsController::changed);
        connect(&gameInput_,&GameInputProfiles::captureRequested,value,&ControllerInput::beginCapture);
        connect(value,&ControllerInput::buttonCaptured,&gameInput_,[this](QString button){if(category_==16&&gameInput_.editing())gameInput_.selectBinding(button);});
        connect(value,&ControllerInput::keyCaptured,&gameInput_,[this](QString chord){if(category_==16)gameInput_.assignTarget(chord);}); }
    void setScraper(ScrapeController* value) { scraper_=value; }
    CommunicationSettings* communication() { return &communication_; }
    void configureNearby(NativeActivityProvider*);
    ClockController* clock() { return &clock_; }
    const ClockController* clock() const { return &clock_; }
    LibraryStorageController* storage() { return &storage_; }
    const LibraryStorageController* storage() const { return &storage_; }
    using QObject::QObject;
    void setRepository(PreferencesRepository* repository) { repository_ = repository; reload(); }
    void reload();
    void configureSavePolicy(SaveBackupService* service) { savePolicy_=service;connect(service,&SaveBackupService::policyChanged,this,&SettingsController::changed); }
    bool readOnlySaves() const { return savePolicy_ && savePolicy_->readOnly(); }
    void setTrainersAvailable(bool value) { trainersAvailable_ = value; emit changed(); }
    void setLegacyTrashAvailable(bool value) { if(legacyTrash_==value)return;legacyTrash_=value;if(category_==8)row_=0;emit changed(); }
    void begin() { storage_.close(); category_ = 0; row_ = 0; pane_ = false; emit changed(); }
    int category() const { return category_; }
    int rowFocus() const { return category_==16?gameInput_.focusIndex():row_; }
    bool controlsFocused() const { return pane_; }
    QStringList categories() const { return {"Appearance", "Sound", "Media", "Feedback", "Trainer", "System", "Credits", "Controller", "Library", "Saves", "Connections", "Date & time", "Nearby play", "Communication", "ScreenScraper"}; }
    QVariantList controls() const;
    QVariantList sections() const;
    QVariantList pages() const;
    int sectionIndex() const;
    QString categoryId() const;
    Q_INVOKABLE void selectSection(int index);
    Q_INVOKABLE void selectPage(const QString& id, bool enter=true);
    Q_INVOKABLE void selectCategory(int index, bool enter = true);
    Q_INVOKABLE void activateRow(int index);
    Q_INVOKABLE void cycleTheme(int direction = 1);
    bool creditsOpen() const { return category_==6 && pane_; }
    QString theme() const { return value_.theme=="dark"?"dark":"light"; }
    QString displayProfile() const;
    int interfaceScale() const;
    Q_INVOKABLE void adjustDisplay(int row, int direction);
    Q_INVOKABLE void resetDisplayScale();
    Q_INVOKABLE double displayDensity(double screenWidth, double screenHeight,
        double windowWidth, double windowHeight, const QString& profile, int percent) const;
    bool worldEditing() const { return value_.worldEditing; }
    bool reducedMotion() const { return value_.reducedMotion; }
    bool videoPreviews() const { return value_.videoPreviews; }
    bool previewSound() const;
    int previewDelay() const;
    void setMediaOwner(const QString& owner);
    bool saving() const { return saving_; }
    QString error() const { return error_; }
    QString libraryStatus() const { return libraryStatus_; }
    void setLibraryScanState(bool available, bool busy, const QString& result = {});
    int focusIndex() const { return pane_ ? rowFocus() : sectionIndex(); }
    void dispatch(Action);
    void activate(int index);
signals:
    void changed();
    void closeRequested();
    void deviceRequested(int index);
    void controllerRequested();
    void healthRequested();
    void softwareRequested(QString destination);
    void trashRequested();
    void collectionsRequested();
    void libraryRefreshRequested();
    void trainerRequested(int index);
    void messageRequested(const QString& message);
    void quickAdjustment(int index, trainer::Action action);
private:
    QString mediaPrefix_="media/preview/";
    GameInputProfiles gameInput_;
    ControllerInput* input_=nullptr;
    ScrapeController* scraper_=nullptr;
    QPointer<NativeActivityProvider> nearby_;
    ClockController clock_;
    CommunicationSettings communication_;
    LibraryStorageController storage_;
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
