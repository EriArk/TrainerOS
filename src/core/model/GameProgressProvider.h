#pragma once
#include "GameProgress.h"
#include "Models.h"
#include <QObject>

namespace trainer {
class GameProgressProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString adventureId() const = 0;
    virtual GameProgress snapshot() const = 0;
signals:
    void changed();
};
}
