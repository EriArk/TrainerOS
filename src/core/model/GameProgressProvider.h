#pragma once
#include "GameProgress.h"
#include "Models.h"
#include <QObject>
#include <functional>
#include <QStringList>

namespace trainer {
class GameProgressProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString adventureId() const = 0;
    virtual GameProgress snapshot() const = 0;
    virtual void verifySnapshot(const AdventureRegistration&,const GameProgress&,QObject*,std::function<void(bool)> done) {done(false);}
    virtual void inspectCapabilities(const AdventureRegistration&, QObject*, std::function<void(QStringList)> done) {done({});}
signals:
    void changed();
};
}
