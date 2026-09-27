#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include "CenterActivities.h"
#include "features/pokedex/ClassicArt.h"
#include "features/pokedex/SpriteArt.h"
#include "core/model/GameProgress.h"
#include "core/model/SaveBackup.h"
#include "core/repository/LibraryRepository.h"

namespace trainer {
// Save layout stays behind the injected protected service; QML receives no paths.
class PartyPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canHoldItems READ canHoldItems NOTIFY changed)
    Q_PROPERTY(bool canRelease READ canRelease NOTIFY changed)
    Q_PROPERTY(QVariantMap releaseSubject READ releaseSubject NOTIFY changed)
    Q_PROPERTY(bool canMove READ canMove NOTIFY changed)
    Q_PROPERTY(bool moveOpen READ moveOpen NOTIFY changed)
    Q_PROPERTY(QString moveStage READ moveStage NOTIFY changed)
    Q_PROPERTY(QString moveTitle READ moveTitle NOTIFY changed)
    Q_PROPERTY(QString moveMessage READ moveMessage NOTIFY changed)
    Q_PROPERTY(QVariantList moveRows READ moveRows NOTIFY changed)
    Q_PROPERTY(QVariantList movePair READ movePair NOTIFY changed)
    Q_PROPERTY(int moveIndex READ moveIndex NOTIFY changed)
    Q_PROPERTY(bool moveParty READ moveParty NOTIFY changed)
    Q_PROPERTY(QString section READ section NOTIFY changed)
    Q_PROPERTY(bool detailOpen READ detailOpen NOTIFY changed)
    Q_PROPERTY(bool sample READ sample CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(int boxCount READ boxCount NOTIFY changed)
    Q_PROPERTY(QString boxName READ boxName NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int box READ box NOTIFY changed)
    Q_PROPERTY(bool activitiesFocused READ activitiesFocused NOTIFY changed)
    Q_PROPERTY(bool boxFocused READ boxFocused NOTIFY changed)
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY changed)
    Q_PROPERTY(trainer::CenterActivities* activities READ activities CONSTANT)
public:
    explicit PartyPresentation(bool sample, QObject* parent = nullptr);
    void configureMovement(SaveBackupService*,LibraryRepository*);
    bool canMove() const;
    bool canRelease() const;
    bool canHoldItems() const;
    Q_INVOKABLE void beginHeldItems();
    QVariantMap releaseSubject() const {return releaseSubject_;}
    Q_INVOKABLE void beginRelease();
    Q_INVOKABLE void confirmRelease();
    bool moveOpen() const {return !moveStage_.isEmpty();}
    bool moving() const {return moveStage_=="writing";}
    QString moveStage() const {return moveStage_;}
    QString moveTitle() const;
    QString moveMessage() const {return moveMessage_;}
    QVariantList moveRows() const;
    QVariantList movePair() const;
    int moveIndex() const {return moveIndex_;}
    bool moveParty() const {return moveTargetBox_<0;}
    Q_INVOKABLE void beginMove();
    Q_INVOKABLE void moveActivate(int);
    QString section() const { return section_; }
    bool detailOpen() const { return detail_; }
    bool sample() const { return sample_; }
    bool available() const { return sample_ || (snapshot_ && snapshot_->error.isEmpty()); }
    int boxCount() const { return sample_ ? 2 : snapshot_ ? snapshot_->boxes.size() : 0; }
    QString boxName() const;
    void setProgress(const QString& adventureId, const GameProgress&);
    QString title() const { return title_; }
    QString status() const;
    QVariantList entries() const;
    QVariantMap detail() const;
    int focusIndex() const { return section_ == "activities" ? activities_.focusIndex() : activitiesFocus_ ? -1 : section_ == "party" ? partyFocus_ : storageFocus_[box_]; }
    bool activitiesFocused() const { return activitiesFocus_; }
    bool boxFocused() const { return boxFocus_; }
    int menuIndex() const { return menuIndex_; }
    void configureArtwork(ClassicArt* art, SpriteArt* sprites);
    Q_INVOKABLE void changeBox(int delta);
    CenterActivities* activities() { return &activities_; }
    void openActivities();
    int box() const { return box_; }
    void setAdventure(const QString& id, const QString& title);
    void dispatch(Action);
    void activate(int);
    void showSection(const QString&);
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
    void openSaves();
    void returnFromSaves();
signals:
    void changed();
    void healingRequested();
    void backupsRequested();
private:
    enum class Operation { Move,Release,HeldItem };
    void beginOperation(Operation);
    void submitOperation();
    void dispatchMove(Action);
    Operation operation_=Operation::Move;
    int heldItemChoice_=0;
    QVariantMap releaseSubject_;
    void cancelMove();
    SaveBackupService* movementService_=nullptr;
    LibraryRepository* movementLibrary_=nullptr;
    QString saveRevision_,moveStage_,moveMessage_,moveName_,moveToken_;
    PartySnapshot moveSnapshot_;
    AdventureRegistration moveRegistration_;
    PartyMove moveRequest_;
    int moveIndex_=0,moveTargetBox_=-1;
    quint64 moveGeneration_=0;
    QVariantMap slot(int) const;
    QVariantMap present(const PokemonRecord&, int) const;
    QVariantMap withArt(QVariantMap) const;
    void syncActors();
    bool sample_, detail_ = false;
    QString section_ = "party", previousSection_ = "party", id_, title_;
    int partyFocus_ = 0, storageFocus_[14] = {}, box_ = 0;
    std::optional<PartySnapshot> snapshot_;
    QString observationKey_;
    QString sourceContext_;
    bool initialBoxSet_ = false;
    ProgressAvailability availability_ = ProgressAvailability::Unsupported;
    CenterActivities activities_;
    bool activitiesFocus_ = false;
    bool boxFocus_ = false;
    int menuIndex_ = 0;
    ClassicArt* art_ = nullptr;
    SpriteArt* sprites_ = nullptr;
    QString managementSection_ = "party";
};
}
