#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QPointF>
#include <QVariantList>
#include <QTimer>

namespace trainer {
class SocialController;
class TextEntryController;
class AdventureExitPresentation;
// One UI session bound to an owned process and account generation. Messages,
// unread state and drafts remain owned by SocialController.
class PlayerOverlayController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList players READ players NOTIFY changed)
    Q_PROPERTY(QPointF position READ position NOTIFY changed)
    Q_PROPERTY(bool chatOpen READ chatOpen NOTIFY changed)
    Q_PROPERTY(bool conversationReady READ conversationReady NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QVariantMap voice READ voice NOTIFY changed)
    Q_PROPERTY(QVariantList balloons READ balloons NOTIFY changed)
    Q_PROPERTY(bool spectatorComments READ spectatorComments NOTIFY changed)
    Q_PROPERTY(int unread READ unread NOTIFY changed)
public:
    PlayerOverlayController(SocialController&,TextEntryController&,AdventureExitPresentation&,QObject* parent=nullptr);
    QVariantList players() const;
    QPointF position() const {return position_;}
    bool chatOpen() const {return open_;}
    bool conversationReady() const;
    QString name() const;
    QVariantMap voice() const;
    QVariantList balloons() const {return balloons_;}
    bool spectatorComments() const {return spectators_;}
    int unread() const;
    Q_INVOKABLE void presented(QString message);
    Q_INVOKABLE void setDisplay(QString display);
    Q_INVOKABLE void voiceAction(QString action);
    Q_INVOKABLE void mutePlayer(QString id);
    Q_INVOKABLE void toggleSpectators();
    void setGame(qint64 pid, bool party = true);
    bool openNotification(const QString& channel);
    void dispatch(Action);
    Q_INVOKABLE void openPlayer(QString id);
    Q_INVOKABLE void close();
    Q_INVOKABLE void back() {dispatch(Action::Back); }
    Q_INVOKABLE void setPosition(QPointF position);
    Q_INVOKABLE void compose();
    Q_INVOKABLE void send();
    void showPlayers();
    bool menuAction(const QString&);
    Q_INVOKABLE void requestPlayers();
signals:
    void changed();
    void optionsRequested();
private:
    void sync();
    void clear();
    bool validPeer() const;
    SocialController& social_;
    TextEntryController& keyboard_;
    AdventureExitPresentation& view_;
    qint64 game_=0,chatGame_=0;
    quint64 accountGeneration_=0;
    QString account_,peer_,settingsKey_,room_,channel_,observedRoom_,display_;
    QString rosterSelection_;
    QVariantList balloons_;
    QTimer balloonTimer_;
    bool spectators_=true;
    QPointF position_{0,0};
    bool notification_=false, partyGame_=false;
    QString notificationTitle_;
    bool open_=false,pending_=false,pendingList_=false,syncing_=false;
    QTimer deadline_;
};
}
