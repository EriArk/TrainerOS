#pragma once
#include "core/input/Action.h"
#include "platform/storage/FileCatalog.h"
#include <QVariantList>
#include <algorithm>

namespace trainer {
class FilePickerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool choosingFolder READ choosingFolder NOTIFY changed)
    Q_PROPERTY(bool grid READ grid NOTIFY changed)
    Q_PROPERTY(bool canChoose READ canChoose NOTIFY changed)
    Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString selectionName READ selectionName NOTIFY changed)
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int rowIndex READ rowIndex NOTIFY changed)
    Q_PROPERTY(int placeIndex READ placeIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY changed)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY changed)
public:
    explicit FilePickerController(FileCatalog* catalog, QObject* parent = nullptr) : QObject(parent), catalog_(catalog) {}
    bool isOpen() const { return open_; }
    bool busy() const { return busy_; }
    bool choosingFolder() const { return chooseFolder_; }
    bool grid() const { return grid_; }
    bool canChoose() const;
    bool canBack() const { return !history_.isEmpty(); }
    bool hasMore() const { return current_.page+1<current_.pages; }
    QString path() const { return current_.path; }
    QString error() const { return current_.error; }
    QString selectionName() const;
    QString zone() const { return zone_; }
    int focusIndex() const { return zone_=="places"?place_:zone_=="list"?row_:control_; }
    int rowIndex() const { return row_; }
    int placeIndex() const { return place_; }
    QVariantList rows() const;
    QVariantList places() const { return places_; }
    QVariantList breadcrumbs() const;
    void begin(const QString& startingFolder = {}, bool chooseFolder = false);
    void setCatalog(FileCatalog* catalog) { cancel(); catalog_ = catalog; }
    Q_INVOKABLE void cancel();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index, const QString& zone);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void choose();
    Q_INVOKABLE void navigate(const QString& path);
    Q_INVOKABLE void back();
    Q_INVOKABLE void parentFolder();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void toggleView();
    Q_INVOKABLE void setColumns(int value) { columns_=std::max(1,value); }
signals:
    void changed();
    void rowsChanged();
    void selected(const QString& path);
    void scrollRequested(int direction);
private:
    void load(const QString&, int page = 0, const QString& selection = {});
    struct Location { QString path; int page; QString selection; };
    FileCatalog* catalog_;
    DirectoryPage current_;
    QList<Location> history_;
    QVariantList places_;
    QString zone_="list", selectedPath_;
    int row_=0, control_=0, place_=0, generation_=0, placesGeneration_=0, columns_=4;
    bool open_=false, busy_=false, chooseFolder_=false, grid_=false, advanceAfterLoad_=false;
};
}
