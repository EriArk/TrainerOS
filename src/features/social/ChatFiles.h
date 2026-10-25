#pragma once
#include "features/library/FilePickerController.h"
#include <QVariantMap>
#include <QSet>

namespace trainer {
class FilesService;
// UI-owned selection only. Transfer IO and authentication stay on Fluxer's worker.
class ChatFiles final : public QObject {
    Q_OBJECT
    Q_PROPERTY(trainer::FilePickerController* picker READ picker CONSTANT)
    Q_PROPERTY(QVariantList selected READ selected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
public:
    explicit ChatFiles(QObject* parent=nullptr);
    FilePickerController* picker(){return &picker_;}
    QVariantList selected() const;
    bool busy() const {return busy_;}
    QString status() const {return status_;}
    double progress() const {return progress_;}
    void context(QString account,QString channel);
    void receive(QVariantMap event);
    bool send(QString text);
    Q_INVOKABLE void choose();
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void cancel();
    void dispatch(Action action){picker_.dispatch(action);}
    QVariantList downloads() const;
    void control(QString id,QString action);
    void setFilesService(FilesService*);
    void setDownloadDestination(QString id,QString folder);
signals:
    void changed();
    void request(QString operation,QVariantMap args);
    void sent(QString channel,QString text);
    void folderRequested(QString path);
private:
    void relocateDownloads();
    FilesService* fileService_=nullptr;
    QHash<QString,QString> moving_;
    QSet<QString> preparing_;
    LocalFileCatalog catalog_;
    FilePickerController picker_{&catalog_};
    QHash<QString,QVariantList> selections_;
    QVariantList jobs_;
    QString account_,channel_,uploadChannel_,nonce_,text_,status_;
    bool busy_=false,uncertain_=false;
    double progress_=0;
    qint64 nonceTime_=0;
};
}
