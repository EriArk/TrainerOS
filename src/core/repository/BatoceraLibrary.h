#pragma once
#include "LibraryRepository.h"
#include <QHash>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>

namespace trainer {
struct FolderEntry {
    AdventureRegistration record;
    QVariantMap media;
    bool existing = false;
    bool preserveMedia = false;
};
struct FolderScan {
    QList<FolderEntry> entries;
    QStringList warnings;
    bool complete = true;
};
// Read-only filesystem discovery. Reads optional bounded adapter identity probes; does not execute XML commands,
// modify gamelists or use external play counts as Trainer history.
using ExperienceIdentityProbe = std::function<QVariantMap(const QString&,const QString&)>;
FolderScan scanBatoceraLibrary(const QString& roms, const QList<AdventureRegistration>& existing, const ExperienceIdentityProbe& probe = {});
QStringList batoceraPlatforms();

class BatoceraLibrary final : public QObject, public LibraryRepository {
    Q_OBJECT
public:
    BatoceraLibrary(LibraryRepository& library, QString roms, QObject* parent = nullptr);
    ~BatoceraLibrary() override;
    QList<World> worlds() const override { return library_.worlds(); }
    QList<Adventure> adventures() const override { return library_.adventures(); }
    QList<ResumePoint> resumePoints() const override { return library_.resumePoints(); }
    QList<PlaySession> recentSessions() const override { return library_.recentSessions(); }
    QList<PlaySession> gameSessions(const QString& id) const override { return library_.gameSessions(id); }
    std::optional<ExitMedia> exitMedia(const QString& id) const override { return library_.exitMedia(id); }
    std::optional<qint64> recordedSeconds(const QString& id) const override { return library_.recordedSeconds(id); }
    HomeSnapshot home() const override { return library_.home(); }
    QList<AdventureRegistration> registrations() const override { return library_.registrations(); }
    void editLibraryAsync(const LibraryEdit&, QObject*, std::function<void(QString)>) override;
    QString storageRootFor(const QString&) const override;
    bool editable() const override { return library_.editable(); }
    std::optional<AdventureRegistration> registration(const QString& id) const override { return library_.registration(id); }
    void saveAdventureAsync(const AdventureRegistration& r, QObject* c, std::function<void(LibraryWriteResult)> done) override { library_.saveAdventureAsync(r,c,std::move(done)); }
    QVariantMap artwork(const QString& id) const override;
    QVariantMap experienceIdentity(const QString& id) const override { return media_.value(id).value("experienceIdentity").toMap(); }
    void setDisplayPreferences(const QJsonObject& preferences) {display_=preferences;emit changed();}
    void refreshContentAvailability() override;
    void rescan();
    bool setRoot(const QString& root) { if(busy_)return false; roms_=root;media_.clear();lastScan_.invalidate();return true; }
    bool busy() const { return busy_; }
    QString root() const { return roms_; }
    bool writing() const { return writing_; }
    std::function<void(AdventureRegistration&)> prepareInstallation;
    ExperienceIdentityProbe observeIdentity;
    std::function<QString(const AdventureRegistration&,LibraryEdit&)> prepareFileMove;
signals:
    void changed();
    void busyChanged();
    void writingChanged();
    void scanFinished(int added, const QStringList& warnings);
private:
    void importNext();
    LibraryRepository& library_;
    QString roms_;
    QThread* thread_ = nullptr;
    bool busy_ = false;
    bool writing_ = false;
    bool rescanPending_ = false;
    QElapsedTimer lastScan_;
    QTimer deferredScan_;
    FolderScan scan_;
    int index_ = 0, added_ = 0;
    QHash<QString,QVariantMap> media_;
    QJsonObject display_;
    QHash<QString,QVariantMap> nextMedia_;
};
}
