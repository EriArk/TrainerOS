#pragma once
#include "core/input/Action.h"
#include "features/social/SocialMedia.h"
#include <QProcess>
#include <QTimer>
#include <QVariantList>

namespace trainer {
class SocialController;
class CommunicationSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int microphoneLevel READ microphoneLevel NOTIFY meterChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY changed)
    Q_PROPERTY(bool choosingAvatar READ choosingAvatar NOTIFY changed)
public:
    explicit CommunicationSettings(QObject* parent=nullptr);
    ~CommunicationSettings() override;
    void configure(SocialController* social);
    void begin();
    void leave();
    void dispatch(Action action);
    void applyText(const QString& text);
    QVariantList rows() const;
    QVariantMap profile() const;
    QString status() const;
    int focusIndex() const { return focus_; }
    int microphoneLevel() const { return level_; }
    bool testing() const { return testing_; }
    bool choosingAvatar() const { return choosingAvatar_; }
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void setVolume(int value);
signals:
    void changed();
    void meterChanged();
    void backRequested();
    void textRequested(QString title,QString initial,int limit);
private:
    friend class SocialTests;
    void scan();
    void stopTest();
    void startTest();
    void cycleDevice(const QString& kind,int direction);
    void send(const QString& operation,const QVariantMap& args={});
    QVariantMap account() const;
    QString deviceLabel(const QString& kind) const;
    SocialController* social_=nullptr;
    SocialMedia avatar_{this};
    QProcess scan_{this},capture_{this};
    QTimer refresh_{this},testLimit_{this},scanLimit_{this};
    QVariantList inputs_,outputs_,pictures_;
    QString defaultInput_,error_,textField_,accountId_;
    QByteArray scanBytes_,meterBytes_;
    int focus_=0,level_=0,scanStage_=0;
    bool active_=false,testing_=false,choosingAvatar_=false,pendingAvatar_=false;
};
}
