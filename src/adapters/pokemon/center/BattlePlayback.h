#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QList>

namespace trainer {
// Presentation-only snapshots in simulator log order. Never decides turn order
// or writes a save. The authoritative result remains separate until playback ends.
class BattlePlayback {
public:
    void reset();
    void load(const QJsonObject& before,const QJsonObject& after,const QJsonArray& teams);
    bool active() const {return index_>=0 && index_<frames_.size();}
    bool advance();
    QJsonArray sides() const;
    QVariantMap event() const;
    int turn() const {return turn_;}
private:
    struct Frame {QVariantMap event;QJsonArray sides;};
    QList<Frame> frames_;
    QJsonArray final_;
    int index_=-1,turn_=0,serial_=0;
};
}
