#pragma once
#include "core/input/Action.h"
#include "platform/storage/FileCatalog.h"
#include <QVariantList>
#include <QHash>

namespace trainer {
class FilePickerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int rowIndex READ rowIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
public:
    explicit FilePickerController(FileCatalog* catalog, QObject* parent = nullptr) : QObject(parent), catalog_(catalog) {}
    bool isOpen() const { return open_; }
    bool busy() const { return busy_; }
    QString path() const { return current_.path.isEmpty() ? "Locations" : current_.path; }
    QString error() const { return current_.error; }
    QString zone() const { return zone_; }
    int focusIndex() const { return zone_ == "list" ? row_ : action_; }
    int rowIndex() const { return row_; }
    QVariantList rows() const;
    QVariantList actions() const;
    void begin(const QString& startingFolder = {});
    void setCatalog(FileCatalog* catalog) { cancel(); catalog_ = catalog; }
    void cancel();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index, const QString& zone);
signals:
    void changed();
    void rowsChanged();
    void selected(const QString& path);
private:
    void load(const QString&, int page = 0, const QString& selection = {});
    void back();
    struct Location { QString path; int page; QString selection; };
    FileCatalog* catalog_;
    DirectoryPage current_;
    QList<Location> history_;
    QString zone_ = "list";
    int row_ = 0, action_ = 0, generation_ = 0;
    bool open_ = false, busy_ = false;
};
}
