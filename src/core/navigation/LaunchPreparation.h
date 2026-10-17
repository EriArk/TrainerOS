#pragma once
#include "core/repository/LibraryRepository.h"
#include "integrations/adventure/AdventureAdapter.h"
#include <QThread>

namespace trainer {
// Silent recovery for an installed game without a ready binding. One launch
// request owns verification, optional binding repair and the normal launch.
class LaunchPreparation final : public QObject {
    Q_OBJECT
public:
    LaunchPreparation(LibraryRepository& library, AdventureAdapter& adapter, QObject* parent=nullptr)
        : QObject(parent), library_(library), adapter_(adapter) {}
    ~LaunchPreparation() override;
    bool busy() const { return busy_; }
    bool cancelling() const { return cancelling_; }
    QString adventureId() const { return adventureId_; }
    QString stage() const { return cancelling_ ? "Cancelling…" : stage_; }
    void launch(const QString& id);
    void cancel();
signals:
    void changed();
    void libraryChanged();
    void messageRequested(const QString& message);
private:
    void fail(const QString&);
    LibraryRepository& library_;
    AdventureAdapter& adapter_;
    QThread* worker_=nullptr;
    bool busy_=false;
    bool cancelling_=false;
    QString adventureId_,stage_;
};
}
