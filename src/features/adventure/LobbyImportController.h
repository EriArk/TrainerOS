#pragma once
#include "features/library/FilePickerController.h"
#include "core/repository/LibraryRepository.h"
#include <atomic>
#include <memory>
namespace trainer {
class SocialController;class RuntimeMultiplayer;class AdventureAdapter;
class LobbyImportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QObject* files READ files CONSTANT)
public:
    LobbyImportController(LibraryRepository&,AdventureAdapter&,FileCatalog*,SocialController&,RuntimeMultiplayer&);
    ~LobbyImportController() override {if(cancelled_)*cancelled_=true;}
    bool open() const {return open_;} bool busy() const {return busy_;} bool saving() const {return saving_;}
    QString error() const {return error_;} QString title() const {return context_["game"].toObject()["title"].toString();}
    QObject* files(){return &files_;}
    void begin();void dispatch(Action);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void chooseAgain();
signals:
    void changed();void imported();
private:
    friend class RuntimeMultiplayerTests;
    QJsonObject context() const;
    void select(QString);
    LibraryRepository& library_;AdventureAdapter& adapter_;SocialController& social_;RuntimeMultiplayer& runtime_;
    FilePickerController files_;QJsonObject context_;QString error_;
    bool open_=false,busy_=false,saving_=false;quint64 generation_=0;
    std::shared_ptr<std::atomic_bool> cancelled_;
};
}
