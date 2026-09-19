#pragma once
#include "core/model/Models.h"
#include "core/model/ExitMedia.h"
#include <QObject>
#include <functional>

namespace trainer {
class PlayHistoryRepository {
public:
    virtual ~PlayHistoryRepository() = default;
    virtual void saveSessionAsync(const PlaySession&, QObject*, std::function<void(QString)>) = 0;
    virtual void saveSessionMediaAsync(const PlaySession& session, const std::optional<ExitMediaSource>&,
                                      const std::optional<ExitCapture>&, QObject* context, std::function<void(QString)> completed) {
        saveSessionAsync(session, context, std::move(completed));
    }
};
}
