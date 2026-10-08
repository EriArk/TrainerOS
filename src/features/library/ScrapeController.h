#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include "integrations/scraper/ScreenScraper.h"
#include <QObject>
#include <QFuture>
#include <QVariantList>
#include <QSet>

namespace trainer {
// Explicit jobs only. Snapshots library records on the GUI thread; hashing,
// HTTP, XML and cache work run sequentially off that thread.
class ScrapeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList settingsRows READ settingsRows NOTIFY changed)
    Q_PROPERTY(int settingsFocus READ settingsFocus NOTIFY changed)
    Q_PROPERTY(bool selectingSystems READ selectingSystems NOTIFY changed)
    Q_PROPERTY(QVariantList systemRows READ systemRows NOTIFY changed)
    Q_PROPERTY(int selectedGameCount READ selectedGameCount NOTIFY changed)
public:
    explicit ScrapeController(LibraryRepository&,QObject* parent=nullptr);
    ~ScrapeController() override;
    void configure(const QString& stateDirectory);
    void setTransport(scraper::Transport transport,scraper::Delay delay={});
    QJsonObject displayPreferences() const {return display_;}
    std::function<bool()> canStart;
    bool isOpen() const {return open_;}
    bool busy() const {return active_ || working_;}
    QString title() const;
    QString detail() const;
    QString status() const {return status_;}
    QVariantList rows() const;
    QVariantList settingsRows() const;
    int focusIndex() const {return focus_;}
    int settingsFocus() const {return settingsFocus_;}
    void begin(const QString& game={},const QString& world={});
    void beginSystems();
    bool selectingSystems() const {return scope_==4 && !active_ && !finished_;}
    QVariantList systemRows() const;
    int selectedGameCount() const;
    void close();
    void hide(){open_=false;emit changed();}
    QVariantList downloadTasks() const;
    void downloadCommand(const QString& task,const QString& command);
    void dispatch(Action);
    void dispatchSettings(Action);
    Q_INVOKABLE void activate(int);
    Q_INVOKABLE void activateSetting(int);
    void applyText(const QString&);
signals:
    void changed();
    void saved();
    void settingsBackRequested();
    void displayChanged();
    void textRequested(QString title,QString initial,bool secret);
    void jobStarted();
    void systemSelectionClosed();
private:
    struct Item {QString id,title,path,platform,root;};
    struct Outcome {
        scraper::Status status=scraper::Status::Ready;
        scraper::Fingerprint file;
        QList<scraper::Game> games;
        QMap<QString,QString> local;
        QString error,cache,cachedId;
        bool cached=false,success=false;
        scraper::Quota quota;
    };
    void run(std::function<Outcome()>,std::function<void(Outcome)>);
    void checkAccount(bool startJob);
    void start(bool retry=false);
    void next();
    void found(Outcome);
    void apply(const scraper::Game&);
    void complete(const QString& error={},bool skipped=false);
    void finish();
    bool handleCancellation(bool committed=false);
    void savePreferences();
    QList<Item> items() const;
    LibraryRepository& library_;
    QString directory_,credentialsPath_,status_,phase_,gameId_,worldId_,textTarget_;
    scraper::Credentials credentials_;
    scraper::Preferences preferences_;
    QJsonObject display_;
    scraper::Transport transport_=scraper::httpsTransport();
    scraper::Delay delay_;
    std::shared_ptr<scraper::Client> client_;
    scraper::Cancellation cancel_;
    QFuture<void> future_;
    QList<Item> queue_,failures_;
    Outcome pending_;
    QStringList results_;
    QMap<QString,QString> taskStates_,taskErrors_;
    QVariantList systems_;
    QSet<QString> selectedSystems_;
    int systemListFocus_=0;
    bool skipCurrent_=false;
    int scope_=0,focus_=0,settingsFocus_=0,index_=0,done_=0,failed_=0,skipped_=0;
    bool open_=false,active_=false,working_=false,paused_=false,choosing_=false,finished_=false,wrote_=false;
};
}
