#pragma once
#include "StandaloneAdapter.h"
#include <QJsonArray>

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
QJsonObject netplayIdentity(const AdventureRegistration&,const StandaloneInstallation&,const std::atomic_bool&);
QString configureNetplay(ProcessCommand&,const StandaloneInstallation&,const QString& content,
                         const NetplayRequest&,const std::atomic_bool&);
}
