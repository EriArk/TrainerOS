#pragma once
#include "core/repository/HallOfFameRepository.h"
#include "core/repository/LibraryRepository.h"
#include "core/input/Action.h"
#include <QVariantList>

namespace trainer {
class ArchiveEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
public:
    ArchiveEditor(HallOfFameRepository&, QObject* parent = nullptr);
    void setLibrary(LibraryRepository* library) { library_ = library; }
    bool isOpen() const { return !route_.isEmpty(); }
    bool saving() const { return saving_; }
    QString route() const { return route_; }
    QString error() const { return error_; }
    QString query() const { return query_; }
    int focusIndex() const { return focus_; }
    QVariantList fields() const;
    QVariantList rows() const;
    void begin(const std::optional<HallOfFameEntry>& = {});
    void cancel();
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
    Q_INVOKABLE void submit() { dispatch(Action::ToggleContinue); }
    Q_INVOKABLE void back() { dispatch(Action::Back); }
    void applyText(const QString&);
signals:
    void changed();
    void textRequested(const QString& title, const QString& initial, int limit);
    void saved(const QString& id);
    void messageRequested(const QString& message);
private:
    QList<Adventure> adventures() const;
    void choose(const Adventure&);
    void save();
    void editLevel();
    HallOfFameRepository& repository_;
    LibraryRepository* library_ = nullptr;
    HallOfFameEntry draft_;
    QString route_, error_, query_, textTarget_;
    int focus_ = 0, textMember_ = 0;
    bool saving_ = false;
};
}
