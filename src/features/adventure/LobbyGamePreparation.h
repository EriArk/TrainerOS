#pragma once
#include "core/navigation/AdventureLaunchController.h"
#include "AdventureExitPresentation.h"
#include "features/social/LobbyRuntime.h"

namespace trainer {
// Shared consent/save barrier before a lobby replaces the locally owned game.
class LobbyGamePreparation final:public QObject {
    Q_OBJECT
public:
    LobbyGamePreparation(ProcessService&,AdventureLaunchController&,AdventureExitPresentation&,std::function<quint64()> account,QObject* parent=nullptr);
    void prepare(QString operation,LobbyRuntimeService::Completion);
    void prepareReturn(QString operation,qint64 processId,LobbyRuntimeService::Completion);
    void cancel(const QString& operation);
    bool action(const QString&);
    bool pending() const {return bool(completion_);}
signals:
    void optionsRequested();
private:
    void show();
    void complete(bool ok,QString error);
    ProcessService& process_;AdventureLaunchController& lifecycle_;AdventureExitPresentation& overlay_;
    std::function<quint64()> account_;
    QString operation_;LobbyRuntimeService::Completion completion_;
    qint64 processId_=0;quint64 accountId_=0;bool exiting_=false;
};
}
