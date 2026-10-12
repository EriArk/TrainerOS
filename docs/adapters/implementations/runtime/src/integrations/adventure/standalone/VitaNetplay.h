#pragma once
#include "StandaloneAdapter.h"

namespace trainer::vita {
QString bundleRoot();
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
struct Request {
    QJsonObject expected,endpoint;
    bool host=false,online=false;
    QString identity,address;
};
ProcessCommand command(const AdventureRegistration&,const Request&,const QStringList& inputPrefix={});
QJsonObject event(const QByteArray&);
}
