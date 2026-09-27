#pragma once
#include "core/input/Action.h"
#include "integrations/practice/PracticeSession.h"
#include <QVariantList>
#include <QJsonArray>
#include <functional>

namespace trainer {
class PracticeController final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int side READ side NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY changed)
    Q_PROPERTY(QVariantList fighters READ fighters NOTIFY changed)
    Q_PROPERTY(QVariantList moves READ moves NOTIFY changed)
    Q_PROPERTY(QVariantMap event READ event NOTIFY changed)
public:
    explicit PracticeController(QObject* parent=nullptr);
    using Verifier=std::function<void(const PracticeSource&,const GameProgress&,QObject*,std::function<void(bool)>)>;
    void configureRuntime(const QString& root);
    void configureRuntime(const QString& node,const QString& worker,const QString& engine);
    void configureVerification(Verifier verify) {verify_=std::move(verify);}
    void setObservation(const PracticeSource&,const GameProgress&,const QVariantList& actors);
    void setActors(const QVariantList& actors) {if(actors_!=actors){actors_=actors;emit changed();}}
    void enter();
    void leave();
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
    QString stage() const {return stage_;}
    QString message() const;
    int focusIndex() const {return focus_;}
    int side() const {return side_;}
    bool running() const {return session_.active();}
    bool isOpen() const {return open_;}
    bool ready() const;
    QVariantList candidates() const;
    QVariantList fighters() const;
    QVariantList moves() const;
    QVariantMap event() const;
signals:
    void changed();
    void closeRequested();
private:
    void check(std::function<void()> success);
    void begin();
    void consumeState();
    void finishEvents();
    void fail(const QString&);
    QVariantMap presentation(int slot) const;
    QString named(const QString&) const;
    void parseEvents(const QJsonArray&);
    void applyEvent();
    PracticeSession session_;
    PracticeSource source_;
    GameProgress progress_;
    QVariantList actors_,frozen_,events_;
    QJsonObject observed_;
    QJsonArray displayedSides_;
    QTimer sourceTimer_,checkDeadline_;
    Verifier verify_;
    QString node_,worker_,engine_,stage_="first",error_;
    int first_=-1,second_=-1,focus_=0,side_=0,eventIndex_=0;
    std::array<int,2> choices_{};
    quint64 generation_=0;
    bool open_=false,checking_=false,discarding_=false;
};
}
