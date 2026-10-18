#pragma once
#include <QJsonObject>
#include "core/input/Action.h"
#include "core/repository/HallOfFameRepository.h"
#include "integrations/achievements/AchievementProvider.h"
#include "ArchiveEditor.h"
#include "core/experience/ExperienceProviders.h"
#include "AchievementAccountController.h"
#include <QVariantList>
#include "core/model/GameProgress.h"

namespace trainer {
class HallOfFameController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool historyOpen READ historyOpen NOTIFY historyChanged)
    Q_PROPERTY(QVariantList historyRows READ historyRows NOTIFY historyChanged)
    Q_PROPERTY(QString historyStatus READ historyStatus NOTIFY historyChanged)
    Q_PROPERTY(bool historyConfigured READ historyConfigured NOTIFY historyChanged)
    Q_PROPERTY(bool historyBusy READ historyBusy NOTIFY historyChanged)
    Q_PROPERTY(bool historyMore READ historyMore NOTIFY historyChanged)
    Q_PROPERTY(int historyFocus READ historyFocus NOTIFY historyChanged)
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(bool overview READ overview NOTIFY changed)
    Q_PROPERTY(QVariantMap championPreview READ championPreview NOTIFY changed)
    Q_PROPERTY(QVariantMap journey READ journey NOTIFY changed)
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int rowIndex READ rowIndex NOTIFY changed)
    Q_PROPERTY(bool archive READ isArchive NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList team READ team NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString emptyMessage READ emptyMessage NOTIFY changed)
    Q_PROPERTY(QVariantMap gallery READ gallery NOTIFY changed)
    Q_PROPERTY(bool galleryMenuOpen READ galleryMenuOpen NOTIFY changed)
    Q_PROPERTY(QVariantList galleryOptions READ galleryOptions NOTIFY changed)
    Q_PROPERTY(int galleryMenuFocus READ galleryMenuFocus NOTIFY changed)
    Q_PROPERTY(trainer::ArchiveEditor* editor READ editor CONSTANT)
    Q_PROPERTY(trainer::AchievementAccountController* account READ account CONSTANT)
    Q_PROPERTY(bool editable READ editable CONSTANT)
public:
    HallOfFameController(HallOfFameRepository&, AchievementProvider&, QObject* parent = nullptr);
    QString route() const { return route_; }
    bool overview() const { return route_ == "archive-journey" || route_ == "archive-champions" || route_ == "archive-champion-detail"; }
    void showJourney();
    void setCurrentAdventure(const QString&);
    void configureArtwork(ExperienceArtProvider* art) { art_=art; emit changed(); }
    void enableSampleJourney() { sampleJourney_ = true; }
    QVariantMap championPreview() const;
    QVariantMap journey() const;
    void setProgress(const QString& adventure, const GameProgress&);
    QString zone() const { return zone_; }
    bool isArchive() const { return route_.startsWith("archive"); }
    int focusIndex() const;
    int rowIndex() const;
    QVariantList rows() const;
    QVariantMap detail() const;
    QVariantList team() const;
    QVariantList actions() const;
    QString status() const;
    QString emptyMessage() const;
    ArchiveEditor* editor() { return &editor_; }
    AchievementAccountController* account() { return &account_; }
    const AchievementAccountController* account() const { return &account_; }
    bool editable() const { return repository_.archiveEditable(); }
    void beginMemory(bool edit);
    void dispatch(Action);
    void activate(int index);
    void activateControl(const QString& zone, int index);
    void refreshArchive();
    void switchFace();
    int faceIndex() const {return overview()?0:isArchive()?1:2;}
    void cycleFace(int delta);
    void showFace(int index);
    void showCurrentAchievements();
    void showAchievementLibrary();
    bool historyOpen() const {return historyOpen_;}
    QVariantList historyRows() const {return provider_.accountHistory();}
    QString historyStatus() const {return provider_.historyStatus();}
    bool historyConfigured() const {return provider_.hasHistoryKey();}
    bool historyBusy() const {return provider_.historyBusy();}
    bool historyMore() const {return provider_.historyMore();}
    int historyFocus() const {return historyFocus_;}
    Q_INVOKABLE void openHistory();
    Q_INVOKABLE void closeHistory();
    Q_INVOKABLE void loadHistory(bool more=false) {provider_.fetchHistory(more);}

    bool browsingGames() const {return gameBrowser_;}
    void setGameLibrary(LibraryRepository* library) {gameLibrary_=library;}
    Q_INVOKABLE void showAchievementGames();
    Q_INVOKABLE void focusAchievementGame(int index);
    Q_INVOKABLE void openAchievementAccount() {account_.begin();}
    QVariantMap gallery() const;
    QVariantList galleryOptions() const;
    bool galleryMenuOpen() const { return galleryMenuOpen_; }
    int galleryMenuFocus() const { return galleryMenuFocus_; }
    Q_INVOKABLE void openGalleryMenu();
    Q_INVOKABLE void closeGalleryMenu();
    Q_INVOKABLE void chooseGalleryOption(int);
    Q_INVOKABLE void focusAchievement(int);
    void setMoments(const QVariantList& moments) { moments_=moments;emit changed(); }
    Q_INVOKABLE void openMoment();
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
signals:
    void historyChanged();
    void momentRequested(const QString& capture);
    void detailScrollRequested(int delta);
    void changed();
    void rowsChanged();
    void messageRequested(const QString& message);
private:
    QList<AchievementSet> browsableSets() const;
    LibraryRepository* gameLibrary_=nullptr;
    bool gameBrowser_=false,historyOpen_=false;
    int historyFocus_=0;
    QVariantList moments_;
    struct Row { QString id; QString title; QString subtitle; };
    QList<Row> currentRows() const;
    AchievementSet selectedSet() const;
    AchievementSnapshot currentSnapshot() const;
    AchievementSnapshot checkedSnapshot(const AchievementSet&) const;
    QString selectedRowId() const;
    void selectRow(const QString& id);
    void reconcile();
    void normalizeActions();
    void back();
    bool isDetail() const { return !overview() && route_.endsWith("detail"); }
    ExperienceArtProvider* art_ = nullptr;
    GameProgress progress_;
    QList<ChampionRecord> championRecords() const;
    QString championId_;
    QSet<QString> preserving_;
    bool sampleJourney_ = false;
    struct FaceView { QString route, zone; int action = 0; };
    FaceView archiveView_{"archive-list", "list"};
    FaceView journeyView_{"archive-journey", "actions"};
    FaceView achievementView_{"sets", "list"};
    HallOfFameRepository& repository_;
    AchievementProvider& provider_;
    ArchiveEditor editor_;
    AchievementAccountController account_;
    QList<HallOfFameEntry> archive_;
    QString archiveError_;
    QString archiveId_, setId_, currentAdventure_;
    bool preferCurrent_ = false;
    bool galleryMode_ = false, galleryMenuOpen_ = false;
    int galleryMenuFocus_ = 0, galleryFilter_ = 0, gallerySort_ = 0;
    AchievementContext galleryContext_;
    QHash<QString, QString> achievementIds_;
    QString route_ = "archive-list", zone_ = "list";
    int actionFocus_ = 0;
};
}
