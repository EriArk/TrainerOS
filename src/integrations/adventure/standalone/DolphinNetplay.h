#pragma once
#include "StandaloneAdapter.h"
#include <QJsonArray>
#include <functional>

namespace trainer::dolphin {
struct NetplayRequest {
    QJsonObject expected;
    bool host=false, online=false;
    QString address, token;
    int port=2626, slot=1;
    QJsonArray seats;
};
QString bridgeRoot();
QString bridgeFile();
QString contentIdentity(const QString&,qint64,const std::atomic_bool&,const std::function<void(const QString&)>& observe={});
QJsonObject netplayIdentity(const AdventureRegistration&,const StandaloneInstallation&,const std::atomic_bool&,int players=2,
                           const std::function<void(const QString&)>& observe={});
QString configureNetplay(ProcessCommand&,const StandaloneInstallation&,const QString& content,
                         const NetplayRequest&,const std::atomic_bool&);
}
