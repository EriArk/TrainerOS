#pragma once
#include "core/experience/ExperienceNavigation.h"
#include "core/experience/ExperienceManifest.h"
#include "core/input/Action.h"
#include "core/model/GameProgressProvider.h"
#include <QObject>
#include <QUrl>
#include <QPointer>
#include <utility>
#include <memory>
#include <vector>

namespace trainer {
class LibraryRepository;
class TrainerController;
class TrainerRepository;
class HallOfFameController;
class HallOfFameRepository;
struct ExperienceServices {
    LibraryRepository& library;
    TrainerController& profile;
    HallOfFameController& achievements;
    HallOfFameRepository& archive;
    TrainerRepository& profiles;
};
struct ExperienceContext {
    QString owner, adventure;
    std::optional<Adventure> game;
    QVariantMap profile;
    HomeSnapshot home;
    QPointer<GameProgressProvider> progress;
    quint64 generation = 0;
};
struct ExperienceLiveContext {
    QString owner, adventure, session, module;
    int revision=0, version=0;
    bool operator==(const ExperienceLiveContext&) const = default;
};
// Trusted in-process modules. All mutations still go through the existing
// authorized services; a descriptor or view cannot grant semantic save access.
class ExperienceModule : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { if(descriptor().id=="generic" || enabled_==enabled)return;enabled_=enabled;emit changed(); }
    virtual ExperienceDescriptor descriptor() const = 0;
    virtual ExperienceManifest manifest() const = 0;
    bool supports(const std::optional<Adventure>& game) const { return manifest().match(game)>0; }
    int match(const std::optional<Adventure>& game,const QVariantMap& evidence) const { const auto identity=manifest();return identity.id==descriptor().id?identity.match(game,evidence):0; }
    virtual QUrl presenter() const = 0;
    virtual void refresh(const ExperienceContext& context) { context_=context; }
    virtual QVariantMap homeProgress() const { return {}; }
    virtual QStringList capabilityNotes() const { return {}; }
    virtual QVariantList liveActions(const ExperienceLiveContext&) const { return {}; }
    virtual void invokeLiveAction(const QString&,const ExperienceLiveContext&) {}
    virtual void show(int, const QString&) {}
    virtual void leave(bool) {}
    virtual bool dispatch(Action) { return false; }
    virtual bool activate(int, const QString&) { return false; }
    virtual void activateHome(int,const QString&) {}
    virtual int focusIndex() const { return 0; }
    virtual bool modalOpen() const { return false; }
    virtual bool navigationBlocked(bool) const { return false; }
    virtual bool activityBusy(bool = false) const { return false; }
    virtual bool writing() const { return false; }
    virtual bool recentsAllowed() const { return true; }
    virtual bool intercept(Action) { return false; }
    virtual QVariantMap invitation() const { return {}; }
    virtual QJsonObject navigation() const { return {}; }
    virtual void restoreNavigation(const QJsonObject&) {}
    virtual QJsonObject legacyState() const { return {}; }
    virtual QStringList restoreLegacy(const QJsonObject&) { return {}; }
    virtual void reload() {}
    void applyText(const QString& text) { auto apply=std::exchange(textApply_,{});if(apply)apply(text); }
    virtual void cancelText() { textApply_={}; }
signals:
    void changed();
    void notice(QString message);
    void faceRequested(int slot, QString face, bool acceptedActivity, quint64 generation);
    void hostActionRequested(QString action, quint64 generation);
    void textRequested(QString title, QString initial, int limit, quint64 generation);
    void textCancelled();
protected:
    ExperienceContext context_;
    std::function<void(QString)> textApply_;
private:
    bool enabled_ = true;
};
using ExperienceModules = std::vector<std::unique_ptr<ExperienceModule>>;
using ExperienceFactory = std::function<ExperienceModules(ExperienceServices)>;
}
