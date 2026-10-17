#pragma once
#include "core/storage/LocalStateStore.h"
#include "integrations/achievements/AchievementProvider.h"
#include "integrations/achievements/RetroArchAchievementEvents.h"
#include <QAudioSink>
#include <QBuffer>
#include <QQueue>
#include <QThread>
#include <QTimer>

namespace trainer {
// The emulator recognizes achievements. This service only receives owned live
// events, persists their provenance, and presents them without taking input.
class AchievementCelebration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
public:
    AchievementCelebration(LocalStateStore*,AchievementProvider&,QObject* parent=nullptr);
    ~AchievementCelebration() override;
    void begin(const QString& adventure,qint64 pid,const QVariantMap& controls);
    void end();
    void finish();
    void feed(const QByteArray&);
    QVariantMap current() const { return current_; }
    Q_INVOKABLE void setPresentable(bool);
    static QByteArray jinglePcm(int sampleRate=24000);
signals:
    void changed();
    void notice(const QString&);
private:
    struct Pending { AchievementMoment moment; bool captured=false,reading=false,failed=false; };
    bool valid() const;
    void resolve();
    void collect(const QString&);
    void next();
    void sound();
    LocalStateStore* store_;
    AchievementProvider& provider_;
    RetroArchAchievementEvents parser_;
    QString owner_,account_,session_,adventure_,directory_,game_,hash_,mode_;
    bool loaded_=false,presentable_=false;
    quint64 generation_=0;
    QMap<QString,Pending> pending_;
    QSet<QString> seen_;
    QQueue<QVariantMap> queue_;
    QVariantMap current_;
    QTimer duration_;
    QThread thread_;
    QObject* worker_;
    QAudioSink* sink_=nullptr;
    QBuffer audio_;
};
}
