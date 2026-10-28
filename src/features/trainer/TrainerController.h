#pragma once
#include "core/input/Action.h"
#include "core/repository/TrainerRepository.h"
#include <QObject>
#include <QVariantMap>
#include <QStringList>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QTemporaryDir>

namespace trainer {
class TrainerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exists READ exists NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY changed)
    Q_PROPERTY(QString draftName READ draftName NOTIFY changed)
    Q_PROPERTY(QString draftEmblem READ draftEmblem NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList editRows READ editRows NOTIFY changed)
    Q_PROPERTY(bool avatarEditing READ avatarEditing NOTIFY changed)
    Q_PROPERTY(bool avatarAvailable READ avatarAvailable NOTIFY changed)
    Q_PROPERTY(bool avatarRendererAvailable READ avatarRendererAvailable CONSTANT)
    Q_PROPERTY(bool avatarBusy READ avatarBusy NOTIFY changed)
    Q_PROPERTY(QString avatarPreview READ avatarPreview NOTIFY changed)
    Q_PROPERTY(QVariantList avatarRows READ avatarRows NOTIFY changed)
    Q_PROPERTY(QString avatarError READ avatarError NOTIFY changed)
public:
    static constexpr int NameLimit = 24;
    explicit TrainerController(TrainerRepository&, QObject* parent = nullptr);
    ~TrainerController() override;
    bool exists() const { return profile_.has_value(); }
    bool editing() const { return editing_; }
    bool saving() const { return saving_; }
    void reload();
    QVariantList editRows() const;
    int focusIndex() const { return focus_; }
    QVariantMap profile() const;
    QString draftName() const { return draft_.name; }
    QString draftEmblem() const { return draft_.emblemId; }
    QString error() const { return error_; }
    void beginEdit();
    void cancel();
    void setDraftName(const QString& name);
    void dispatch(Action, bool vertical = false);
    Q_INVOKABLE void activate(int index);
    bool avatarEditing() const { return avatarEditing_; }
    bool avatarAvailable() const;
    bool avatarRendererAvailable() const;
    bool avatarBusy() const { return avatarJob_.isRunning(); }
    QString avatarPreview() const;
    QString avatarError() const { return avatarError_; }
    QVariantList avatarRows() const;
    QJsonObject avatarDescriptor() const { return liveAvatar_; }
    bool applyAppearance(const QJsonObject& appearance);
    Q_INVOKABLE void beginAvatarEdit();
    Q_INVOKABLE void cycleAvatar(int index, int delta = 1);
    Q_INVOKABLE void chooseAvatar(int index, const QString& value);
    Q_INVOKABLE void finishAvatar(bool saveChanges);
signals:
    void changed();
    void nameRequested(const QString& initial);
    void messageRequested(const QString& message);
private:
    void save();
    void refreshAvatar();
    void startAvatarJob();
    QFutureWatcher<QJsonObject> avatarJob_;
    QTemporaryDir avatarDraftCache_;
    QJsonObject liveAvatar_, previewAvatar_;
    QString avatarError_;
    quint64 avatarGeneration_ = 0, avatarJobGeneration_ = 0;
    bool avatarEditing_ = false, avatarJobDraft_ = false;
protected:
    virtual int extraRows() const { return 0; }
    virtual QVariantList extraEditRows() const { return {}; }
    virtual QVariantMap extraProfile(const TrainerProfile&) const { return {}; }
    virtual void activateExtra(int) {}
    virtual bool dispatchExtra(Action) { return false; }
    virtual bool activateOverlay(int) { return false; }
    virtual void cancelExtra() {}
    TrainerRepository& repository_;
    std::optional<TrainerProfile> profile_;
    TrainerProfile draft_;
    bool editing_ = false;
    bool saving_ = false;
    int focus_ = 0;
    QString error_;
};
}
