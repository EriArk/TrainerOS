#pragma once
#include "core/repository/LibraryRepository.h"
#include "features/library/FilePickerController.h"
#include "platform/files/FilesService.h"
#include <QFutureWatcher>

namespace trainer {
// User-added PC games use the shared library and shared Files task owner.
class PcGamesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(trainer::FilePickerController* picker READ picker CONSTANT)
public:
    PcGamesController(LibraryRepository&, FilesService&, QObject* parent=nullptr);
    void configure(QString directory, QString owner);
    void setCatalog(FileCatalog* catalog) { picker_.setCatalog(catalog); }
    FilePickerController* picker() { return &picker_; }
    bool isOpen() const { return open_; }
    bool busy() const { return busy_; }
    QString stage() const { return stage_; }
    QString message() const { return message_; }
    QString title() const { return title_; }
    QVariantList rows() const;
    int focusIndex() const { return focus_; }
    Q_INVOKABLE void begin();
    Q_INVOKABLE void close();
    Q_INVOKABLE void activate(int);
    void dispatch(Action);
    void applyText(const QString&);
    static QVariantMap inspect(const QString& root);
signals:
    void changed();
    void saved(QString id);
    void textRequested(QString title,QString initial,int limit);
private:
    void selected(const QString&);
    void inspectFolder(const QString&,bool copied);
    void copy();
    void scanComplete(const QVariantMap&,bool copied);
    void registerGame();
    LibraryRepository& library_;
    FilesService& files_;
    FilePickerController picker_{nullptr};
    QString directory_, owner_, stage_="source", message_, source_, destination_, installRoot_, job_, id_, title_, proton_;
    QVariantList candidates_;
    QVariantMap chosen_;
    qint64 bytes_=0;
    int focus_=0, generation_=0;
    bool open_=false,busy_=false,writing_=false;
};
}
