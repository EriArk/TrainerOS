#pragma once
#include "core/model/Models.h"
#include <QObject>
#include <functional>

namespace trainer {
class PlayHistoryRepository {
public:
    virtual ~PlayHistoryRepository() = default;
    virtual void saveSessionAsync(const PlaySession&, QObject*, std::function<void(QString)>) = 0;
};
}
