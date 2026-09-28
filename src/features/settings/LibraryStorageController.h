#pragma once
#include "platform/storage/LibraryStorage.h"
#include "core/input/Action.h"
#include <QObject>
#include <QThread>
#include <QVariantList>
#include <functional>

namespace trainer {
// Mounted library selection, reused inside Settings without another service page.
class LibraryStorageController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString root READ root NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    using QObject::QObject;
    ~LibraryStorageController() override;
    void configure(const QString& root) { root_=root; emit changed(); }
    void begin();
    void close();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    bool isOpen() const { return open_; }
    bool busy() const { return busy_; }
    QString root() const { return root_; }
    QString error() const { return error_; }
    int focusIndex() const { return focus_; }
    QVariantList rows() const;
    std::function<QString(const QString&)> apply;
    std::function<QList<LibraryLocation>(const QString&)> locations=libraryLocations;
    std::function<QString(const LibraryLocation&)> prepare=prepareLibraryLocation;
signals:
    void changed();
private:
    QString root_, error_;
    QList<LibraryLocation> locations_;
    int focus_=0;
    bool open_=false, busy_=false;
    QThread* worker_=nullptr;
};
}
