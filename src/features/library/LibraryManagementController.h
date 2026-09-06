#pragma once
#include "core/repository/LibraryRepository.h"
#include "FilePickerController.h"

namespace trainer {
class LibraryManagementController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int rowIndex READ rowIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(trainer::FilePickerController* files READ files CONSTANT)
public:
    LibraryManagementController(LibraryRepository&, FileCatalog*, QObject* parent = nullptr);
    void begin(const QString& worldId = {});
    void close();
    void refresh();
    void applyText(const QString&);
    void dispatch(Action);
    void activate(int index, const QString& area = {});
    void setInitialFolder(const QString& path) { initialFolder_ = path; }
    QString route() const { return files_.isOpen() ? "files" : route_; }
    QString zone() const { return zone_; }
    int focusIndex() const { return route_ == "list" && zone_ == "list" ? rowIndex() : focus_; }
    int rowIndex() const;
    QVariantList rows() const;
    QVariantList fields() const;
    QVariantList choices() const;
    QString title() const;
    QString error() const { return error_; }
    bool saving() const { return saving_; }
    FilePickerController* files() { return &files_; }
    const FilePickerController* files() const { return &files_; }
signals:
    void changed();
    void rowsChanged();
    void closeRequested();
    void textRequested(const QString& title, const QString& initial, int limit);
    void messageRequested(const QString& message);
    void saved();
private:
    void edit(const QString& id = {});
    void save();
    void back();
    QString worldName(const QString&) const;
    LibraryRepository& repository_;
    FilePickerController files_;
    QList<Adventure> records_;
    AdventureRegistration draft_;
    QString selectedId_, preferredWorld_, initialFolder_, error_;
    QString route_ = "list", zone_ = "actions";
    QStringList extraDraft_;
    int focus_ = 0, textField_ = -1;
    bool open_ = false, saving_ = false;
};
}
